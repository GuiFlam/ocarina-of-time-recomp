"""Extract the RomFS of the game version an update brings: base RomFS merged with the update's BKTR patch by hactool.

  .venv/bin/python tools/extract_romfs.py [--config config.local.toml] [--hactool toolchain/hactool/hactool]

Writes build/extracted/v<update version>/romfs/romfs.bin and index.json ({status, romfs_size, romfs_sha256, files:
[{path, offset, size}]}), which tools/build_game.py turns into the runtime's RomFS index (tools/romfs_index.py). The
version folder is the one tools/extract_exefs.py wrote for that update. Needs room for both Program NCAs and the merged
image (twice the RomFS size, plus the merged image) next to the output.

Copied unchanged from fe3h-pc-recomp tools/extract_romfs.py @ 47ef99126793dbd13e4a475feb910a13ef6639b8 (MIT, Copyright (c) 2026 Jeff
Aigner), apart from this header. That file is adapted from totk-pc-recomp tools/extract.py merge_update_romfs() @ c944f9f94520874f2a669add30dd08d29ee9f7b5 (MIT,
Copyright (c) 2026 Valiantsin Dzerakh). copy_range(), hactool_prod_keys() and index_romfs() follow MacNativeGames
games/totk/scripts/extract_update_romfs.py and extract_romfs.py @ b75be8cfa704df3701e86130c8ced268e79d6b11 (MIT,
Copyright (c) 2026 MacNativeGames contributors), as vendored by totk-pc-recomp. Changes: no TotK hash list (the
merged image is verified by hactool's six IVFC levels and recorded by its SHA-256), our extract_exefs.program_nca()
with the title id taken from the package, and the output folder named after the update's CNMT version.

Key values stay in two temporary 0600 files (hactool's keys and the title keys), never in arguments or output.
"""
import argparse
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
from extract_exefs import (common_tickets, container_entries, content_meta, load_keys, nca_inventory,  # noqa: E402
                           program_nca, read_at, record_offset, require)

LONG_HEX = re.compile(r'[0-9A-Fa-f]{32,}')
# hactool checks two signatures with Nintendo public keys it does not ship; these lines always read (FAIL).
UNSIGNED = re.compile(r'\s*(?:Fixed-Key Signature|NPDM Signature) \(FAIL\)')
HACTOOL_ERROR = re.compile(r'\(FAIL\)|\(INVALID\)|Failed to|Error:')
LOCKPICK_KEK_SOURCE = re.compile(r'(?:mariko_)?master_kek_source_[0-9a-f]{2}\Z')


def say(message):
    print(message, flush=True)


def copy_range(source, offset, size, target):
    with source.open('rb') as src, target.open('wb') as dst:
        src.seek(offset)
        while size:
            data = src.read(min(size, 8 << 20))
            require(bool(data), 'Truncated NCA')
            dst.write(data)
            size -= len(data)


def hactool_prod_keys(source, target):
    """prod.keys as hactool reads it: only `name = value` lines (hactool takes a trailing comment as part of the
    value); Lockpick's 17-byte master_kek_source values lose their trailing NUL."""
    lines = []
    for number, line in enumerate(Path(source).read_text().splitlines(), 1):
        body = line.split('#', 1)[0]
        if '=' not in body:
            continue
        name, value = (part.strip() for part in body.split('=', 1))
        if LOCKPICK_KEK_SOURCE.fullmatch(name.lower()) is not None:
            require(re.fullmatch(r'[0-9a-fA-F]+', value) is not None, f'Invalid key encoding at line {number}')
            if len(value) == 34 and value[-2:].lower() == '00':
                value = value[:32]
            require(len(value) == 32, f'{name.lower()} must be 16 bytes')
        lines.append(f'{name} = {value}')
    fd = os.open(target, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, 'w') as f:
        f.write('\n'.join(lines) + ('\n' if lines else ''))


def index_romfs(path):
    """[{path, offset, size}] of a RomFS image, sorted by path."""
    with path.open('rb') as f:
        h = struct.unpack('<10Q', read_at(f, 0, 80))
        require(h[0] == 80, 'RomFS header')
        length = path.stat().st_size
        for off, size in ((h[1], h[2]), (h[3], h[4]), (h[5], h[6]), (h[7], h[8])):
            require(off + size <= length, 'RomFS table bounds')
        dirs, entries = read_at(f, h[3], h[4]), read_at(f, h[7], h[8])
    files, seen_dirs, seen_files, paths, todo = [], set(), set(), set(), [(0, '')]

    def name(table, off, n):
        require(off + n <= len(table), 'Name bounds')
        s = table[off:off + n].decode('utf-8')
        require(s not in ('.', '..') and '/' not in s and '\\' not in s and '\0' not in s, 'Unsafe path')
        return s

    while todo:
        off, parent = todo.pop()
        require(off not in seen_dirs and off + 24 <= len(dirs), 'Invalid or cyclic directory')
        seen_dirs.add(off)
        _, sibling, child, file, _, n = struct.unpack_from('<6I', dirs, off)
        part = name(dirs, off + 24, n)
        prefix = parent + part + '/' if part else parent
        if sibling != 0xFFFFFFFF:
            todo.append((sibling, parent))
        if child != 0xFFFFFFFF:
            todo.append((child, prefix))
        while file != 0xFFFFFFFF:
            require(file not in seen_files and file + 32 <= len(entries), 'Invalid or cyclic file')
            seen_files.add(file)
            _, next_file, data, size, _, n = struct.unpack_from('<IIQQII', entries, file)
            filename = prefix + name(entries, file + 32, n)
            require(filename not in paths and h[9] + data + size <= length, 'Duplicate or out-of-bounds file')
            paths.add(filename)
            files.append({'path': filename, 'offset': h[9] + data, 'size': size})
            file = next_file
    return sorted(files, key=lambda item: item['path'])


def sha256(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def program_ncas(base, update, keys_path):
    """(tickets, update version, [(source, record, header)] of the base and the update Program NCA)."""
    keys = load_keys(keys_path)
    tickets, inputs, version = {}, [], None
    for source, is_update in ((base, False), (update, True)):
        with source.open('rb') as f:
            kind, entries = container_entries(f, source.stat().st_size)
            require(not is_update or kind == 'nsp', 'Update must be an NSP')
            for rights, key in common_tickets(f, entries).items():
                require(tickets.get(rights, key) == key, 'Conflicting common tickets')
                tickets[rights] = key
            records = nca_inventory(f, entries, keys)
            title = inputs[0][1]['title_id'] if inputs else None
            record, header = program_nca(records, update=is_update, title_id=title)
            rights = header[0x230:0x240]
            require(rights == bytes(16) or rights in tickets, 'Missing common ticket')
            if is_update:
                _, version = content_meta(f, records, keys, tickets)
            inputs.append((source, record, header))
    return tickets, version, inputs


def run_hactool(command, environment):
    """(exit code, output with long hex strings removed)."""
    done = subprocess.run(command, env=environment, capture_output=True, encoding='utf-8', errors='replace')
    return done.returncode, LONG_HEX.sub('<hex removed>', done.stdout + done.stderr)


def merge(base, update, keys_path, hactool, extracted, logs, out=None, phase=None):
    """out: the romfs folder to write (default extracted/v<version>/romfs). phase(name): 'copy', 'merge', 'index'
    (tools/extract.py, for the installer's progress)."""
    tickets, version, inputs = program_ncas(base, update, keys_path)
    output = Path(out) if out else extracted / f'v{version}' / 'romfs'
    phase = phase or (lambda name: None)
    if (output / 'index.json').is_file() and (output / 'romfs.bin').is_file():
        say(f'{output}: already extracted')
        return output
    if output.exists():
        shutil.rmtree(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    sections = [i for i in range(4) if inputs[1][2][0x400 + i * 512 + 4] == 4]
    require(len(sections) == 1, 'Expected one BKTR section in the update Program NCA')
    need = inputs[0][1]['size'] + inputs[1][1]['size'] + inputs[0][1]['size']
    free = shutil.disk_usage(output.parent).free
    require(free > need, f'Need {need / (1 << 30):.0f} GiB free in {output.parent}, have {free / (1 << 30):.0f}')
    log_path = logs / f'merge_romfs-v{version}.log'
    with tempfile.TemporaryDirectory(prefix='update-merge-', dir=output.parent) as tmp:
        tmp = Path(tmp)
        title_keys = tmp / 'title.keys'
        fd = os.open(title_keys, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(fd, 'w') as f:
            for rights, key in tickets.items():
                f.write(f'{rights.hex()} = {key.hex()}\n')
        hactool_keys = tmp / 'hactool.keys'
        hactool_prod_keys(keys_path, hactool_keys)
        say('  copying the base and update Program NCAs')
        phase('copy')
        base_nca, update_nca = tmp / 'base.nca', tmp / 'update.nca'
        for (source, record, _), target in zip(inputs, (base_nca, update_nca)):
            copy_range(source, record_offset(record), record['size'], target)
        result = tmp / 'result'
        result.mkdir()
        say('  merging with hactool')
        phase('merge')
        command = [str(hactool), '-x', '-y', '--disablekeywarns', '--suppresskeys', '-k', str(hactool_keys),
                   f'--basenca={base_nca}', f'--section{sections[0]}={result / "romfs.bin"}', str(update_nca)]
        code, log = run_hactool(command, dict(os.environ, MNG_TITLE_KEYS=str(title_keys)))
        log_path.write_text(log, encoding='utf-8')
        checked = '\n'.join(line for line in log.splitlines() if not UNSIGNED.match(line))
        require(code == 0 and not HACTOOL_ERROR.search(checked), f'hactool merge failed; see {log_path}')
        require(all(re.search(rf'Level {i} \(GOOD\)', log) for i in range(6)), 'hactool did not verify all six IVFC levels')
        image = result / 'romfs.bin'
        say('  hashing and indexing the merged image')
        phase('index')
        files = index_romfs(image)
        report = {'status': 'pass', 'version': version, 'romfs_size': image.stat().st_size,
                  'romfs_sha256': sha256(image), 'files': files,
                  'integrity': 'BKTR merge by hactool; all six IVFC levels verified'}
        (result / 'index.json').write_text(json.dumps(report, indent=2) + '\n')
        result.rename(output)
    return output


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--config', type=Path, default=ROOT / 'config.local.toml')
    ap.add_argument('--hactool', type=Path, default=ROOT / 'toolchain' / 'hactool' /
                    ('hactool.exe' if os.name == 'nt' else 'hactool'))
    args = ap.parse_args()
    config = tomllib.loads(args.config.read_text(encoding='utf-8'))
    base, keys = Path(config['input']['base']), Path(config['input']['keys'])
    updates = [Path(u) for u in config['input'].get('updates', [])]
    require(updates, 'input.updates is empty: the base RomFS alone is read by tools/romfs_reader.py')
    work = ROOT / config.get('work', {}).get('dir', 'build')
    extracted, logs = work / 'extracted', work / 'logs'
    logs.mkdir(parents=True, exist_ok=True)
    require(args.hactool.is_file(), f'{args.hactool} not found: run tools/build_hactool.py')
    for update in updates:
        say(f'Update {update.name}')
        out = merge(base, update, keys, args.hactool, extracted, logs)
        report = json.loads((out / 'index.json').read_text())
        say(f'  RomFS v{report["version"]}: {report["romfs_size"]:,} bytes, {len(report["files"]):,} files')


if __name__ == '__main__':
    try:
        main()
    except ValueError as error:
        sys.exit(f'error: {error}')
