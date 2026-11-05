# From metroid-prime-4-recomp tests/flags_check.py @ 586e91e1b6f38c9e512e39f2d47a046fdf273cf0 (MIT, Copyright (c) 2026 Jeff Aigner), copied unchanged.
# From fe3h-pc-recomp tests/flags_check.py @ 47ef99126793dbd13e4a475feb910a13ef6639b8 (MIT, Copyright (c) 2026 Jeff Aigner), copied unchanged.
# From totk-pc-recomp tests/flags_check.py @ c944f9f94520874f2a669add30dd08d29ee9f7b5 (MIT, Copyright (c) 2026 Valiantsin Dzerakh), copied unchanged.
"""Build and run tests/flags_check.c: runtime/cpu.h's function-local flag helpers against its packed flag helpers.

Usage: flags_check.py
"""
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests' / 'diff'))
from common import MSYS_BIN, compiler  # noqa: E402


def main():
    work = ROOT / 'build' / 'test' / 'flags'
    work.mkdir(parents=True, exist_ok=True)
    exe = work / ('flags_check.exe' if os.name == 'nt' else 'flags_check')
    env = dict(os.environ)
    if os.name == 'nt' and MSYS_BIN.exists():
        env['PATH'] = str(MSYS_BIN) + os.pathsep + env.get('PATH', '')
    for opt in ('-O0', '-O1'):
        subprocess.check_call([compiler(), opt, '-fno-strict-aliasing', '-I', str(ROOT / 'runtime'),
                               str(ROOT / 'tests' / 'flags_check.c'), '-o', str(exe)], env=env)
        print(opt, end=': ', flush=True)
        if subprocess.run([str(exe)], env=env).returncode:
            return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
