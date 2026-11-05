# From metroid-prime-4-recomp installer/cli.py @ 586e91e1b6f38c9e512e39f2d47a046fdf273cf0 (MIT, Copyright (c) 2026 Jeff Aigner), copied with mp4 renamed to oot.
# From fe3h-pc-recomp installer/cli.py @ 47ef99126793dbd13e4a475feb910a13ef6639b8 (MIT, Copyright (c) 2026 Jeff Aigner), renamed to mp4; MP4 changes are noted where made.
# From totk-pc-recomp installer/cli.py @ c944f9f94520874f2a669add30dd08d29ee9f7b5 (MIT, Copyright (c) 2026 Valiantsin Dzerakh), renamed to mp4; MP4 changes are noted where made.
"""Command line: python -m installer --base FILE --update FILE --keys FILE --lang CODE --dest DIR [--root DIR]
                                     [--temp DIR] [--verify] [--openh264 yes|no]

A first install needs every option. On a folder with oot-install.json (resume after a cancel or failure, or update
with a newer release) only --dest is needed: --lang defaults to the recorded language, and the game files are needed
only when the extraction has to run again. Prints JSON events on stdout (events.py). Exit code: 0 done, 1 failed,
2 bad command line, 3 cancelled (Ctrl+C or Ctrl+Break).
"""
import argparse
import hashlib
import signal
import sys
import time
from pathlib import Path

from . import layout as layouts
from . import state as statefile
from .events import Reporter
from .runner import Context, run_steps
from .steps import all_steps

EXIT = {'ok': 0, 'failed': 1, 'cancelled': 3}


class Parser(argparse.ArgumentParser):
    def error(self, message):
        Reporter().error('cli.usage', {'detail': message})
        sys.exit(2)


def temp_folder(base, dest):
    """One temp folder per install folder, so two installs never share generated files."""
    tag = hashlib.sha256(str(dest).lower().encode()).hexdigest()[:8]
    return base / f'{dest.name or "root"}-{tag}'


def cancel(signum, frame):
    raise KeyboardInterrupt


def main(argv=None):
    ap = Parser(prog='python -m installer', description=__doc__.splitlines()[0])
    ap.add_argument('--base', type=Path, help='base game, XCI or NSP')
    ap.add_argument('--update', type=Path, help='the 1.1.0 update NSP')
    ap.add_argument('--keys', type=Path, help='prod.keys')
    ap.add_argument('--lang', help='game language, a Switch language code (en-US, en-GB, ja, de, ru, ...)')
    ap.add_argument('--dest', type=Path, required=True, help='install folder')
    ap.add_argument('--root', type=Path, help='release folder or repository (default: the folder above installer/)')
    ap.add_argument('--temp', type=Path, help='temp folder (default: temp/ of the release, build/install-temp in a '
                                              'repository)')
    ap.add_argument('--verify', action='store_true', help='hash every kept file, also those this release made')
    # OOT: the cinematics' H.264 decoder, Cisco's openh264 binary, downloaded from Cisco during the setup (its patent
    # licence covers only that binary, fetched at install time and under the player's control); default: as recorded,
    # else yes.
    ap.add_argument('--openh264', choices=('yes', 'no'),
                    help="download Cisco's OpenH264 video codec for the cinematics (default: as recorded, else yes)")
    args = ap.parse_args(argv)

    # The window cancels with Ctrl+Break to the installer's process group (CREATE_NEW_PROCESS_GROUP), which also reaches
    # the tools it runs; Python's default for it ends the process without recording anything.
    for name in ('SIGBREAK', 'SIGTERM'):
        if hasattr(signal, name):
            signal.signal(getattr(signal, name), cancel)
    layout = layouts.find(args.root)
    dest = args.dest.resolve()
    temp = temp_folder((args.temp or layout.temp).resolve(), dest)
    log = temp / 'logs' / time.strftime('install-%Y%m%d-%H%M%S.log')
    rep = Reporter(log_path=log)
    t0 = time.monotonic()
    try:
        state = statefile.load(dest)
    except statefile.StateError as e:
        rep.error(e.id, {'path': str(e.path)})
        rep.emit('end', result='failed', seconds=0)
        return 1
    ctx = Context(layout, dest, temp, rep, args, state=state, verify=args.verify)
    steps = all_steps()
    rep.emit('start', release=layout.release, mode=ctx.mode, dev=layout.dev, dest=str(dest), log=str(log),
             steps=[s.id for s in steps])
    result = run_steps(steps, ctx)
    rep.emit('end', result=result, seconds=round(time.monotonic() - t0, 1))
    rep.close()
    return EXIT[result]
