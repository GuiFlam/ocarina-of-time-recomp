#!/usr/bin/env python3
"""Read a .nup container (NX Package: loose ExeFS and RomFS trees of a base program, its updates and add-on content in
one file) and list or extract its packages.

    python3.14 tools/extract_nup.py [--config config.local.toml] --list
    python3.14 tools/extract_nup.py [--config config.local.toml] [--package N] [--exefs-only | --romfs-only] [--verify]

Extraction writes the package's ExeFS to build/extracted/v<version>/exefs and its RomFS as the image the runtime reads
(runtime/romfs.c), build/extracted/v<version>/romfs/romfs.bin and index.json: the files back to back in path order,
which tools/romfs_index.py indexes like an extracted Switch 1 RomFS. --romfs-loose writes loose files to romfs_files/
instead. nup.json beside them records the package (title id, version, kind, title name, display version) and every
loose file's path, size and SHA-256. The NPDM from the package's metadata goes to exefs/main.npdm when the
ExeFS tree lacks one. The default package is the highest-versioned update, else the base.

Switch 2 modules whose segments are ZBIC (NSO flag bit 7) are rewritten as plain NSOs, which recomp/nso.py reads, by
toolchain/zbic/nso_unzbic (tools/build_zbic.py); the module as stored goes to exefs-zbic/, and each rewritten
segment is checked against the SHA-256 in its NSO header.

Needs Python 3.14 or later for the standard library's compression.zstd; no keys are needed, as the container holds
the files decrypted.

Layout, as Lichii (GPL-3.0, read for behaviour only, src/core/file_sys/nup_container.cpp) reads it; all integers
little endian:

- Superblock at 0, 0x80 bytes: magic "NUP\\x01", u16 header size, u16 header version, u64 TOC offset, u64 TOC size,
  32-byte SHA-256 of the TOC, u64 blob offset, u64 file size, u32 package count, u32 flags (0), 16-byte uuid.
- TOC: header (0x30: magic "TOC\\x01", u32 package count, u64 dir count, u64 file count, u64 string pool offset and
  size, all relative to the TOC), then package records (0x40), dir records (0x18), file records (0x50), the children
  array (u32 node indices, a value below the dir count is a dir, otherwise file index + dir count), the string pool.
- Package record: u64 title id, u32 root, ExeFS and RomFS dir (0xFFFFFFFF for none), u32 version, u8 kind (0 base, 1
  update, 2 add-on), u8 flags (1 standalone update), at 0x20 u64 meta offset and at 0x28 u32 meta size (absolute).
- Dir record: u32 name offset and length in the pool, u32 first child slot and child count.
- File record: u32 name offset and length, u64 size, u64 extent offset (absolute, 16-aligned) and extent size, 32-byte
  SHA-256 of the contents, u8 codec (0 raw, 1 zstd).
- Meta: TLVs (u16 type, u16 reserved, u32 size, payload): 1 NPDM, 3 icon, 4 display version, 5 title name,
  6 publisher, 8 source dump, 9 localized titles, 10 localized icons.
"""
import argparse
import hashlib
import json
import os
import struct
import sys
import subprocess
import tomllib
from pathlib import Path

try:
    from compression import zstd
except ImportError:
    zstd = None

ROOT = Path(__file__).resolve().parent.parent
UNZBIC = ROOT / 'toolchain' / 'zbic' / ('nso_unzbic.exe' if os.name == 'nt' else 'nso_unzbic')
NO_DIR = 0xFFFFFFFF
KINDS = {0: 'base', 1: 'update', 2: 'addon'}
META_NAMES = {1: 'npdm', 3: 'icon', 4: 'display_version', 5: 'title_name', 6: 'publisher', 8: 'source_dump',
              9: 'localized_titles', 10: 'localized_icons'}


class NupError(Exception):
    pass


def require(cond, what):
    if not cond:
        raise NupError(what)


class Nup:
    def __init__(self, path):
        self.path = Path(path)
        self.f = open(self.path, 'rb')
        size = os.fstat(self.f.fileno()).st_size
        sb = self.read(0, 0x80)
        require(sb[:4] == b'NUP\x01', 'not a NUP container (magic)')
        hdr_size, hdr_ver, toc_off, toc_size = struct.unpack_from('<HHQQ', sb, 4)
        toc_sha = sb[0x18:0x38]
        self.blob_off, file_size, pkg_count, flags = struct.unpack_from('<QQII', sb, 0x38)
        self.header_version = hdr_ver
        require(hdr_size >= 0x80 and flags == 0, 'unknown superblock header size or flags')
        require(file_size == size, f'file size {size} differs from the superblock\'s {file_size} (truncated?)')
        require(0x80 <= toc_off <= self.blob_off <= size and toc_size <= self.blob_off - toc_off, 'TOC out of bounds')
        toc = self.read(toc_off, toc_size)
        require(hashlib.sha256(toc).digest() == toc_sha, 'TOC SHA-256 mismatch')
        magic, n_pkg, n_dir, n_file, pool_off, pool_size = struct.unpack_from('<IIQQQQ', toc, 0)
        require(magic == 0x01434F54 and n_pkg == pkg_count and n_pkg > 0, 'bad TOC header')
        pos = 0x30
        recs_end = pos + n_pkg * 0x40 + n_dir * 0x18 + n_file * 0x50
        require(recs_end <= pool_off and pool_off + pool_size <= len(toc), 'TOC records out of bounds')
        pool = toc[pool_off:pool_off + pool_size]

        def name(off, length):
            require(off + length <= len(pool), 'name out of bounds')
            return pool[off:off + length].decode('utf-8')

        self.packages = []
        for _ in range(n_pkg):
            tid, root, exefs, romfs, ver, kind, pflags = struct.unpack_from('<QIIIIBB', toc, pos)
            meta_off, meta_size = struct.unpack_from('<QI', toc, pos + 0x20)
            require(kind in KINDS, f'unknown package kind {kind}')
            self.packages.append({'title_id': f'{tid:016X}', 'version': ver, 'kind': KINDS[kind],
                                  'standalone_update': bool(pflags & 1), 'root_dir': root,
                                  'exefs_dir': None if exefs == NO_DIR else exefs,
                                  'romfs_dir': None if romfs == NO_DIR else romfs,
                                  'meta': self.read_meta(meta_off, meta_size)})
            pos += 0x40
        self.dirs = []
        for _ in range(n_dir):
            no, nl, first, count = struct.unpack_from('<IIII', toc, pos)
            self.dirs.append((name(no, nl), first, count))
            pos += 0x18
        self.files = []
        for _ in range(n_file):
            no, nl, fsize, ext_off, ext_size = struct.unpack_from('<IIQQQ', toc, pos)
            sha = toc[pos + 0x20:pos + 0x40]
            codec = toc[pos + 0x40]
            require(codec in (0, 1), f'unknown codec {codec}')
            require(fsize == 0 or (ext_off >= self.blob_off and ext_off + ext_size <= size), 'extent out of bounds')
            self.files.append({'name': name(no, nl), 'size': fsize, 'offset': ext_off, 'extent': ext_size,
                               'sha256': sha, 'codec': codec})
            pos += 0x50
        n_children = (pool_off - recs_end) // 4
        self.children = struct.unpack_from(f'<{n_children}I', toc, recs_end)
        require(all(c < n_dir + n_file for c in self.children), 'child index out of bounds')

    def read(self, off, size):
        self.f.seek(off)
        data = self.f.read(size)
        require(len(data) == size, 'short read')
        return data

    def read_meta(self, off, size):
        meta = {}
        if not off or not size:
            return meta
        block = self.read(off, size)
        pos = 0
        while pos + 8 <= len(block):
            kind, _, length = struct.unpack_from('<HHI', block, pos)
            payload = block[pos + 8:pos + 8 + length]
            require(len(payload) == length, 'meta TLV out of bounds')
            pos += 8 + length
            key = META_NAMES.get(kind, f'type_{kind}')
            meta[key] = payload if key in ('npdm', 'icon') or key.startswith('type_') or key.startswith('localized') \
                else payload.decode('utf-8', 'replace')
        return meta

    def walk(self, dir_index, prefix='', depth=0):
        """(relative path, file record) for every file under a dir, depth first."""
        require(depth < 128, 'directory tree too deep or cyclic')
        _, first, count = self.dirs[dir_index]
        for c in self.children[first:first + count]:
            if c < len(self.dirs):
                yield from self.walk(c, f'{prefix}{self.dirs[c][0]}/', depth + 1)
            else:
                rec = self.files[c - len(self.dirs)]
                yield prefix + rec['name'], rec

    def stream_file(self, rec, out, name, verify, also=None):
        """Write one member's contents to the open file out; also, if given, is a hash fed the same bytes."""
        h = hashlib.sha256() if verify else None
        if rec['size'] == 0:
            pass
        elif rec['codec'] == 0:
            self.f.seek(rec['offset'])
            left = rec['size']
            while left:
                chunk = self.f.read(min(left, 64 << 20))
                require(chunk, 'short read')
                out.write(chunk)
                h and h.update(chunk)
                also and also.update(chunk)
                left -= len(chunk)
        else:
            require(zstd is not None, 'zstd members need Python 3.14+ (compression.zstd)')
            self.f.seek(rec['offset'])
            dec = zstd.ZstdDecompressor()
            left, written = rec['extent'], 0
            while left:
                chunk = self.f.read(min(left, 16 << 20))
                require(chunk, 'short read')
                left -= len(chunk)
                data = dec.decompress(chunk)
                out.write(data)
                h and h.update(data)
                also and also.update(data)
                written += len(data)
            require(written == rec['size'], f'{name}: decompressed {written} bytes, expected {rec["size"]}')
        if verify and rec['size']:
            require(h.digest() == rec['sha256'], f'{name}: SHA-256 mismatch')

    def extract_file(self, rec, out_path, verify):
        out_path.parent.mkdir(parents=True, exist_ok=True)
        with open(out_path, 'wb') as out:
            self.stream_file(rec, out, out_path.name, verify)

    def write_romfs_image(self, dir_index, dest, verify, version):
        """The RomFS as runtime/romfs.c reads it: dest/romfs.bin, every file back to back in the byte order of its
        path (the index's order, tools/romfs_index.py), and dest/index.json with each file's offset and size and the
        image's size and SHA-256. The status is "pass" only when every file was checked against its stored SHA-256
        (verify). Written to .part names and renamed at the end, so an interrupted run leaves no image that looks
        whole."""
        entries = sorted(self.walk(dir_index), key=lambda e: e[0].encode())
        dest.mkdir(parents=True, exist_ok=True)
        image, part = dest / 'romfs.bin', dest / 'romfs.bin.part'
        total = sum(r['size'] for _, r in entries)
        whole = hashlib.sha256()
        files, offset = [], 0
        with open(part, 'wb') as out:
            for n, (path, rec) in enumerate(entries, 1):
                require('..' not in path.split('/') and not path.startswith('/'), f'unsafe path {path!r}')
                self.stream_file(rec, out, path, verify, whole)
                files.append({'path': path, 'offset': offset, 'size': rec['size']})
                offset += rec['size']
                if n % 2000 == 0 or n == len(entries):
                    print(f'romfs image: {n}/{len(entries)} files, {offset / 2**30:.2f}/{total / 2**30:.2f} GiB',
                          flush=True)
        require(offset == total == part.stat().st_size, 'image size differs from the sum of its files')
        index = {'status': 'pass' if verify else 'unverified', 'version': version, 'source': self.path.name,
                 'romfs_size': total, 'romfs_sha256': whole.hexdigest(),
                 'integrity': ('every file checked against its SHA-256 in the NUP TOC, whose own SHA-256 the superblock '
                               'holds') if verify else 'not checked (run with --verify)',
                 'files': files}
        (dest / 'index.json.part').write_text(json.dumps(index), encoding='utf-8')
        os.replace(part, image)
        os.replace(dest / 'index.json.part', dest / 'index.json')
        return len(files), total


def unzbic_exefs(exefs):
    """Rewrite every ZBIC module of an extracted ExeFS as a plain NSO, keeping the stored one in exefs-zbic/."""
    stored_dir = exefs.parent / 'exefs-zbic'
    for path in sorted(exefs.iterdir()):
        head = path.read_bytes()[:0x10] if path.is_file() else b''
        if head[:4] != b'NSO0' or not struct.unpack_from('<I', head, 0xC)[0] & 0x80:
            continue
        require(UNZBIC.is_file(), f'{path.name} is ZBIC-compressed: build {UNZBIC} first (tools/build_zbic.py)')
        stored_dir.mkdir(exist_ok=True)
        stored = stored_dir / path.name
        os.replace(path, stored)
        done = subprocess.run([str(UNZBIC), str(stored), str(path)], capture_output=True, text=True)
        require(done.returncode == 0, f'{path.name}: {done.stderr.strip()}')
        raw = path.read_bytes()
        flags = struct.unpack_from('<I', raw, 0xC)[0]
        for i in range(3):
            off, _, size = struct.unpack_from('<III', raw, 0x10 + 0x10 * i)
            if flags >> (3 + i) & 1:
                require(hashlib.sha256(raw[off:off + size]).digest() == raw[0xA0 + 0x20 * i:0xC0 + 0x20 * i],
                        f'{path.name}: segment {i} SHA-256 mismatch after ZBIC decompression')
        print(f'exefs/{path.name}: ZBIC decompressed, segment hashes verified (stored module in exefs-zbic/)')


def load_config(path):
    with open(path, 'rb') as f:
        return tomllib.load(f)


def win_to_wsl(p):
    if os.name != 'nt' and len(p) > 2 and p[1] == ':':
        return '/mnt/' + p[0].lower() + p[2:].replace('\\', '/')
    return p


def describe(nup):
    for i, p in enumerate(nup.packages):
        m = p['meta']
        files = {}
        for key in ('exefs_dir', 'romfs_dir'):
            if p[key] is not None:
                recs = [r for _, r in nup.walk(p[key])]
                files[key[:5]] = (len(recs), sum(r['size'] for r in recs))
        print(f'[{i}] {p["kind"]} {p["title_id"]} v{p["version"]}'
              f' "{m.get("title_name", "")}" {m.get("display_version", "")} publisher "{m.get("publisher", "")}"'
              f'{" standalone" if p["standalone_update"] else ""}')
        for k, (n, s) in files.items():
            print(f'    {k}: {n} files, {s / 2**30:.2f} GiB')
        if p['exefs_dir'] is not None:
            for path, r in nup.walk(p['exefs_dir']):
                print(f'      exefs/{path}  {r["size"]:,} bytes  {"zstd" if r["codec"] else "raw"}')
        if 'npdm' in m:
            print(f'    meta npdm: {len(m["npdm"])} bytes')


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--config', default=str(ROOT / 'config.local.toml'))
    ap.add_argument('--input', help='the .nup (default: [input] base from the config)')
    ap.add_argument('--list', action='store_true', help='list the packages and ExeFS files, extract nothing')
    ap.add_argument('--package', type=int, help='package index from --list')
    group = ap.add_mutually_exclusive_group()
    group.add_argument('--exefs-only', action='store_true')
    group.add_argument('--romfs-only', action='store_true')
    ap.add_argument('--verify', action='store_true', help='check every extracted file against its SHA-256')
    ap.add_argument('--romfs-loose', action='store_true',
                    help='write the RomFS as loose files in romfs_files/ instead of the romfs/ image the runtime reads')
    ap.add_argument('--out', help='output folder (default build/extracted/v<version>)')
    args = ap.parse_args()

    src = args.input or win_to_wsl(load_config(args.config)['input']['base'])
    try:
        nup = Nup(src)
        if args.list:
            describe(nup)
            return 0
        if args.package is not None:
            pi = args.package
        else:
            ups = [i for i, p in enumerate(nup.packages) if p['kind'] == 'update']
            bases = [i for i, p in enumerate(nup.packages) if p['kind'] == 'base']
            pi = max(ups, key=lambda i: nup.packages[i]['version']) if ups else bases[0]
        pkg = nup.packages[pi]
        out = Path(args.out) if args.out else ROOT / 'build' / 'extracted' / f'v{pkg["version"]}'
        manifest = {'source': Path(src).name, 'header_version': nup.header_version,
                    **{k: v for k, v in pkg.items() if k not in ('meta', 'root_dir', 'exefs_dir', 'romfs_dir')},
                    'title_name': pkg['meta'].get('title_name'), 'display_version': pkg['meta'].get('display_version'),
                    'files': {}}
        trees = [] if args.romfs_only else [('exefs', pkg['exefs_dir'], out / 'exefs')]
        if not args.exefs_only and args.romfs_loose:
            trees.append(('romfs', pkg['romfs_dir'], out / 'romfs_files'))
        for label, d, dest in trees:
            if d is None:
                print(f'{label}: none in this package')
                continue
            entries = list(nup.walk(d))
            total = sum(r['size'] for _, r in entries)
            done = 0
            for n, (path, rec) in enumerate(entries, 1):
                require('..' not in path.split('/') and not path.startswith('/'), f'unsafe path {path!r}')
                nup.extract_file(rec, dest / path, args.verify)
                done += rec['size']
                manifest['files'][f'{label}/{path}'] = {'size': rec['size'], 'sha256': rec['sha256'].hex()}
                if n % 500 == 0 or n == len(entries):
                    print(f'{label}: {n}/{len(entries)} files, {done / 2**30:.2f}/{total / 2**30:.2f} GiB', flush=True)
        if not args.exefs_only and not args.romfs_loose:
            if pkg['romfs_dir'] is None:
                print('romfs: none in this package')
            else:
                n, size = nup.write_romfs_image(pkg['romfs_dir'], out / 'romfs', args.verify, pkg['version'])
                print(f'romfs: {n} files in {out / "romfs" / "romfs.bin"} ({size / 2**30:.2f} GiB)')
        if not args.romfs_only and pkg['exefs_dir'] is not None:
            npdm = pkg['meta'].get('npdm')
            if npdm and not (out / 'exefs' / 'main.npdm').exists():
                (out / 'exefs' / 'main.npdm').write_bytes(npdm)
                print('exefs/main.npdm written from the package metadata')
            unzbic_exefs(out / 'exefs')
        # A partial run keeps the other tree's entries of an earlier manifest.
        old = out / 'nup.json'
        if (args.exefs_only or args.romfs_only) and old.is_file():
            kept = json.loads(old.read_text()).get('files', {})
            prefix = 'romfs/' if args.exefs_only else 'exefs/'
            manifest['files'] = {**{k: v for k, v in kept.items() if k.startswith(prefix)}, **manifest['files']}
        old.write_text(json.dumps(manifest, indent=1))
        print(f'wrote {out}')
    except NupError as e:
        print(f'extract_nup: {e}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
