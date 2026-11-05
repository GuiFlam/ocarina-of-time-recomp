# From metroid-prime-4-recomp installer/__init__.py @ 586e91e1b6f38c9e512e39f2d47a046fdf273cf0 (MIT, Copyright (c) 2026 Jeff Aigner), copied with mp4 renamed to oot.
# From fe3h-pc-recomp installer/__init__.py @ 47ef99126793dbd13e4a475feb910a13ef6639b8 (MIT, Copyright (c) 2026 Jeff Aigner), renamed to mp4; MP4 changes are noted where made.
# From totk-pc-recomp installer/__init__.py @ c944f9f94520874f2a669add30dd08d29ee9f7b5 (MIT, Copyright (c) 2026 Valiantsin Dzerakh), renamed to mp4; MP4 changes are noted where made.
"""The OOT installer: builds a playable folder from the player's own game files (docs/installer.md).

    python -m installer --base FILE --update FILE --keys FILE --lang CODE --dest DIR [--root DIR] [--temp DIR]

Modules: layout (where the bundled tools are, release or repository), events (JSON progress on stdout and the log
file), state (oot-install.json), runner (which steps run: resume and update), steps (the six steps), ini (oot.ini),
cli (the command line). text/en.json holds the English text of every message id the events carry.

Standard library only, apart from what the extraction tools import (cryptography). Runs on Python 3.14.
"""
