# From metroid-prime-4-recomp tools/prepare_shaders.py @ 586e91e1b6f38c9e512e39f2d47a046fdf273cf0 (MIT, Copyright (c) 2026 Jeff Aigner), copied with mp4 renamed to oot.
# Adapted from totk-pc-recomp tools/prepare_shaders.py @ c944f9f94520874f2a669add30dd08d29ee9f7b5 (MIT, Copyright (c) 2026
# Valiantsin Dzerakh): the seed files and the --prepare-shaders run. The list is this project's own (shader answers met in
# play, below) instead of MacNativeGames' TSV, the programs come from tools/shader_inventory.py, and there are no default
# variants.
"""Translate the shader variants a list names, once, before play, so the first time a scene is seen does not stutter.

The list names shader variants players met: a code hash (runtime/gfx_shader.c build_code_hash) and the translator
answers it was translated for. It is a file in the layout of shader-answers.bin, which oot.exe writes next to
pipeline-keys.bin as it plays ("answers met in play" in gfx_shader.c), with every input id zeroed (input ids depend on
the translator; oot.exe computes them): it holds no game code, so it can ship with a release. This tool finds each
listed program in the game's own files (tools/shader_inventory.py), writes next to the build's oot.exe

    seed-programs.bin  "TOTKSPRG": stage, length, c1 length, code hash, control section, code, c1
    seed-answers.bin   "TOTKSANS": stage, code hash, zero input id, answers, priority

and runs `oot.exe --prepare-shaders`, which translates every record into the build's shader cache, writes
shader-prepared.bin and builds every variant's stage library once, so the GPU driver's own disk cache holds them
(vk_pipeline.c "stage library prebuild"). The seed files are deleted afterwards.

Making the list from play (the files of one or more builds that played; records of programs not in the game are
dropped only at preparation time):

    python tools/prepare_shaders.py --make-list build/shader-list.bin build/v131072/shader-answers.bin [more...]

Preparing a build (Python 3.14 for compression.zstd: .venv-py314 or the installer's embeddable Python):

    python tools/prepare_shaders.py build/v131072 --list build/shader-list.bin [--romfs DIR] [--no-run] [--keep-seed]
"""
import argparse
import struct
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CONTROL = 0x880
ANS_MAGIC, PRG_MAGIC = b'TOTKSANS', b'TOTKSPRG'
ANS_REC = 4 + 32 + 32 + 147 + 1                            # gfx_shader.c ANS_REC
LIST_PRIORITY = 1                                          # shader-prepared.bin: warm-up (0 title, 2 other)
BUNDLED_LIST = ROOT / 'data' / 'shader-list.bin'           # release folder layout (tools/make_release.py)


def read_records(path):
    """The records of an answers file (shader-answers.bin or a list): [bytes of ANS_REC]."""
    b = Path(path).read_bytes()
    if len(b) < 16 or b[:8] != ANS_MAGIC:
        raise SystemExit(f'{path}: not a shader answers file')
    version, size = struct.unpack_from('<II', b, 8)
    if version != 1 or size != ANS_REC:
        raise SystemExit(f'{path}: answers version {version}, record size {size}; this tool reads 1 and {ANS_REC}')
    return [b[o:o + ANS_REC] for o in range(16, len(b) - ANS_REC + 1, ANS_REC)]


def write_records(path, recs):
    with open(path, 'wb') as f:
        f.write(ANS_MAGIC + struct.pack('<II', 1, ANS_REC))
        for r in recs:
            f.write(r)


def make_list(out, sources):
    """The union of the sources' variants, in the order first met, with input ids zeroed and one record per
    (stage, code hash, answers)."""
    seen, recs = set(), []
    for src in sources:
        for r in read_records(src):
            stage, h, packed = r[:4], r[4:36], r[68:68 + 147]
            if (stage, h, packed) in seen:
                continue
            seen.add((stage, h, packed))
            recs.append(stage + h + bytes(32) + packed + bytes([LIST_PRIORITY]))
    write_records(out, recs)
    print(f'shader list: {len(recs)} variants of {len({r[4:36] for r in recs})} programs from {len(sources)} file(s) '
          f'-> {out}')
    return 0


def prepare(a):
    sys.path.insert(0, str(ROOT / 'tools'))
    from shader_inventory import programs   # needs Python 3.14 (compression.zstd)
    t0 = time.monotonic()
    listed = read_records(a.list or BUNDLED_LIST)
    want = {r[4:36] for r in listed}
    progs = {}
    for h, stage, control, code in programs(a.romfs):
        if h in want and h not in progs:
            c1_off, length = struct.unpack_from('<II', control, 0x700)
            progs[h] = struct.pack('<III', stage, length, length - c1_off) + h + control + code[:length] + \
                code[c1_off:length]
    t1 = time.monotonic()
    recs = [r for r in listed if r[4:36] in progs]
    out = a.build.resolve()
    with open(out / 'seed-programs.bin', 'wb') as f:
        f.write(PRG_MAGIC + struct.pack('<II', 1, 0))
        for body in progs.values():
            f.write(body)
    write_records(out / 'seed-answers.bin', recs)
    print(f'{len(listed)} listed variants of {len(want)} programs: {len(recs)} variants of {len(progs)} programs found '
          f'in the game ({len(want) - len(progs)} programs not); inventory {t1 - t0:.1f} s', flush=True)
    if a.no_run:
        return 0
    r = subprocess.run([str(out / 'oot.exe'), '--prepare-shaders'], cwd=str(out))
    if not a.keep_seed:
        for name in ('seed-programs.bin', 'seed-answers.bin'):
            (out / name).unlink(missing_ok=True)
    print(f'oot.exe --prepare-shaders: rc={r.returncode}; total {time.monotonic() - t0:.1f} s', flush=True)
    return r.returncode


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('build', type=Path, nargs='?', help='build directory holding oot.exe (build\\v131072)')
    ap.add_argument('--version', default='v131072', help='build/extracted/<version> (default v131072)')
    ap.add_argument('--romfs', type=Path, help='merged RomFS directory with romfs.bin and index.json '
                                               '(default build/extracted/<version>/romfs)')
    ap.add_argument('--list', type=Path, help=f'the shader list (default {BUNDLED_LIST.relative_to(ROOT)})')
    ap.add_argument('--no-run', action='store_true', help='write the seed files only')
    ap.add_argument('--keep-seed', action='store_true', help='keep the seed files after oot.exe ran')
    ap.add_argument('--make-list', type=Path, metavar='OUT', help='write a list from the answers files given after it')
    ap.add_argument('sources', type=Path, nargs='*', help='with --make-list: shader-answers.bin files')
    a = ap.parse_args()
    if a.make_list:
        sources = ([a.build] if a.build else []) + a.sources
        if not sources:
            ap.error('--make-list needs at least one shader-answers.bin')
        return make_list(a.make_list, sources)
    if not a.build:
        ap.error('the build directory is required')
    a.romfs = a.romfs or ROOT / 'build' / 'extracted' / a.version / 'romfs'
    return prepare(a)


if __name__ == '__main__':
    sys.exit(main())
