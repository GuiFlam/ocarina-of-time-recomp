"""Read the base game's RomFS in place, without extracting it: list its files and read byte ranges.

The RomFS lives in IVFC level 5 of the Program NCA's RomFS section. RomFS.read() decrypts just the requested range
(AES-CTR), so a file deep inside a 16 GB XCI costs only its own size. The IVFC hash levels are not checked here; use
a full extraction when the data must be verified.

Only the base game: an update's RomFS is a BKTR patch over the base and needs hactool (see totk-pc-recomp
tools/extract.py, merge_update_romfs).

Copied unchanged from fe3h-pc-recomp tools/romfs_reader.py @ 47ef99126793dbd13e4a475feb910a13ef6639b8 (MIT, Copyright (c)
2026 Jeff Aigner), apart from this header. The directory walk follows MacNativeGames games/totk/scripts/extract_romfs.py index_romfs() @
b75be8cfa704df3701e86130c8ced268e79d6b11 (MIT, Copyright (c) 2026 MacNativeGames contributors), as vendored by
totk-pc-recomp @ c944f9f94520874f2a669add30dd08d29ee9f7b5.
"""
import struct
from pathlib import Path

from cryptography.hazmat.primitives.ciphers import modes

from extract_exefs import (common_tickets, container_entries, decrypt, load_keys, nca_ctr_key, nca_inventory,
                           program_nca, read_at, record_offset, require)

NONE = 0xFFFFFFFF
READ_STEP = 64 << 20   # read_at() refuses single reads above 512 MiB


class RomFS:
    """game: base game XCI or NSP; keys: prod.keys. Key values stay in this object."""

    def __init__(self, game, keys, title_id=None):
        game = Path(game)
        self.f = game.open('rb')
        keys = load_keys(Path(keys))
        _, entries = container_entries(self.f, game.stat().st_size)
        tickets = common_tickets(self.f, entries)
        record, hdr = program_nca(nca_inventory(self.f, entries, keys), title_id=title_id)
        self.title_id = record['title_id']
        self._key = nca_ctr_key(hdr, keys, tickets)
        self._nca = record_offset(record)
        for i in range(4):
            start, end = struct.unpack_from('<II', hdr, 0x240 + i * 16)
            fs = hdr[0x400 + 512 * i:0x600 + 512 * i]
            if start and fs[2:5] == bytes([0, 3, 3]):   # RomFS, hierarchical integrity, AES-CTR
                break
        else:
            raise ValueError('No RomFS section in the Program NCA')
        require(fs[8:12] == b'IVFC', 'Unsupported RomFS integrity layout')
        self._fs, self._start, self._size = fs, start * 512, (end - start) * 512
        self._level5, self.size = struct.unpack_from('<QQ', fs, 0x18 + 5 * 24)
        header = struct.unpack('<10Q', self.read(0, 80))
        require(header[0] == 80, 'RomFS header')
        self._dirs = self.read(header[3], header[4])
        self._entries = self.read(header[7], header[8])
        self._data = header[9]
        self._files = None

    def close(self):
        self.f.close()

    def read(self, offset, size):
        """size bytes of the RomFS image at offset."""
        require(0 <= offset and 0 <= size and offset + size <= self.size, 'RomFS read out of bounds')
        absolute = self._start + self._level5 + offset
        require(absolute + size <= self._start + self._size, 'Section bounds')
        aligned = absolute & ~15
        total = size + absolute - aligned
        out = bytearray()
        for pos in range(0, total, READ_STEP):
            iv = self._fs[0x140:0x148][::-1] + ((aligned + pos) // 16).to_bytes(8, 'big')
            out += decrypt(self._key, modes.CTR(iv), read_at(self.f, self._nca + aligned + pos,
                                                             min(READ_STEP, total - pos)))
        return bytes(out[absolute - aligned:])

    def files(self):
        """{path: (offset in the image, size)}."""
        if self._files is not None:
            return self._files
        files, seen, todo = {}, set(), [(0, '')]
        while todo:
            off, parent = todo.pop()
            require(off not in seen and off + 24 <= len(self._dirs), 'Invalid or cyclic RomFS directory')
            seen.add(off)
            _, sibling, child, file, _, n = struct.unpack_from('<6I', self._dirs, off)
            part = self._dirs[off + 24:off + 24 + n].decode('utf-8')
            prefix = parent + part + '/' if part else parent
            if sibling != NONE:
                todo.append((sibling, parent))
            if child != NONE:
                todo.append((child, prefix))
            while file != NONE:
                require(file + 32 <= len(self._entries), 'Invalid RomFS file entry')
                _, nxt, data, size, _, n = struct.unpack_from('<IIQQII', self._entries, file)
                path = prefix + self._entries[file + 32:file + 32 + n].decode('utf-8')
                require(path not in files and self._data + data + size <= self.size, 'Bad RomFS file entry')
                files[path] = (self._data + data, size)
                file = nxt
        self._files = dict(sorted(files.items()))
        return self._files

    def read_file(self, path, offset=0, size=None):
        """Bytes of a RomFS file, or of [offset, offset + size) within it."""
        start, length = self.files()[path]
        require(0 <= offset <= length, 'Offset beyond the file')
        size = length - offset if size is None else min(size, length - offset)
        return self.read(start + offset, size)
