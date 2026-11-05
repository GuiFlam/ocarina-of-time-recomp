# From metroid-prime-4-recomp tools/resolve_map.py @ 586e91e1b6f38c9e512e39f2d47a046fdf273cf0 (MIT, Copyright (c) 2026 Jeff Aigner), copied with mp4 renamed to oot.
# From fe3h-pc-recomp tools/resolve_map.py @ 47ef99126793dbd13e4a475feb910a13ef6639b8 (MIT, Copyright (c) 2026 Jeff Aigner), copied with fe3h renamed to mp4 only.
"""Resolve oot.exe+<offset> addresses from a log (runtime/kernel.c thread dump "host:" lines) with the linker map.

  python tools/resolve_map.py build/v262144/oot.map < log

Every "oot.exe+0x..." in the input is replaced by the symbol that covers it plus the offset into it; other modules'
addresses stay as they are. The map is lld's (--Map): lines of "RVA size align out in symbol".
"""
import bisect
import re
import sys
from pathlib import Path

ADDR = re.compile(r'oot\.exe\+0x([0-9a-f]+)')


def load_map(path):
    syms = []
    for line in Path(path).read_text(encoding='utf-8', errors='replace').splitlines():
        parts = line.split()
        if len(parts) != 4 or not re.fullmatch(r'[0-9a-f]{8,16}', parts[0]):
            continue   # sections, object files and input sections have more fields
        if parts[2] != '0':
            continue
        syms.append((int(parts[0], 16), parts[3]))
    syms.sort()
    return [a for a, _ in syms], [n for _, n in syms]


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    addrs, names = load_map(sys.argv[1])

    def resolve(m):
        rva = int(m.group(1), 16)
        i = bisect.bisect_right(addrs, rva) - 1
        if i < 0:
            return m.group(0)
        return f'{names[i]}+{rva - addrs[i]:#x}'

    for line in sys.stdin:
        sys.stdout.write(ADDR.sub(resolve, line))


if __name__ == '__main__':
    main()
