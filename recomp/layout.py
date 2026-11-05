# From metroid-prime-4-recomp recomp/layout.py @ 586e91e1b6f38c9e512e39f2d47a046fdf273cf0 (MIT, Copyright (c) 2026 Jeff Aigner), copied unchanged.
# From fe3h-pc-recomp recomp/layout.py @ 47ef99126793dbd13e4a475feb910a13ef6639b8 (MIT, Copyright (c) 2026 Jeff Aigner), copied unchanged.
# From totk-pc-recomp recomp/layout.py @ c944f9f94520874f2a669add30dd08d29ee9f7b5 (MIT, Copyright (c) 2026 Valiantsin Dzerakh), copied unchanged.
"""Guest address layout of the loaded modules, computed from their sizes.

Guest addresses equal host addresses, and the lifter writes absolute addresses into the generated C, so the
generator and the runtime must agree on every module base. This is the single place that decides them: the generator
calls layout() and writes the result into generated/<version>/layout.h and layout.json, which the runtime and the
build read.

Modules are placed in Horizon's load order (rtld is dropped; the host links the modules):
main, subsdk0..9, sdk. Each base is aligned to 2 MiB. Everything must stay below 2^39, the limit of the addresses
IPC buffers can encode.
"""
import json
from pathlib import Path

from nso import Nso

FIRST_BASE = 0x7000000000
ALIGN = 0x200000
ADDRESS_LIMIT = 1 << 39
ORDER = ['main'] + [f'subsdk{i}' for i in range(10)] + ['sdk']


def align_up(v, a=ALIGN):
    return (v + a - 1) & ~(a - 1)


def layout(exefs):
    """[(name, base, size)] for the modules present in the ExeFS directory, in load order."""
    exefs = Path(exefs)
    out, base = [], FIRST_BASE
    for name in ORDER:
        path = exefs / name
        if not path.exists():
            continue
        size = Nso(str(path)).size
        out.append((name, base, size))
        base = align_up(base + size)
    if base > ADDRESS_LIMIT:
        raise ValueError(f'modules end at {base:#x}, above 2^39')
    return out


def write(mods, outdir):
    """layout.json for Python tools and layout.h for the runtime."""
    outdir = Path(outdir)
    outdir.mkdir(parents=True, exist_ok=True)
    (outdir / 'layout.json').write_text(json.dumps(
        [{'name': n, 'base': f'{b:#x}', 'size': f'{s:#x}'} for n, b, s in mods], indent=1) + '\n')
    lines = ['// Module layout chosen by recomp/layout.py. Generated; do not edit.', '#pragma once', '']
    for n, b, s in mods:
        lines.append(f'#define MOD_{n.upper()}_BASE {b:#x}ull')
        lines.append(f'#define MOD_{n.upper()}_SIZE {s:#x}ull')
    lines += ['', f'#define MOD_COUNT {len(mods)}',
              '#define MOD_LIST(X) ' + ' '.join(f'X({n}, {n.upper()})' for n, _, _ in mods), '']
    (outdir / 'layout.h').write_text('\n'.join(lines))


def read(outdir):
    """[(name, base, size)] from a layout.json written by write()."""
    data = json.loads((Path(outdir) / 'layout.json').read_text())
    return [(m['name'], int(m['base'], 16), int(m['size'], 16)) for m in data]
