# Ocarina of Time Recomp

Static recompilation of *The Legend of Zelda: Ocarina of Time* (title id `04004860224DEC00`, a Switch 2 title) into a
native PC program. It starts from metroid-prime-4-recomp, the third port of the lifter, host layer and NVN/Vulkan
renderer that began in totk-pc-recomp and went through fe3h-pc-recomp. Plan: [docs/plan.md](docs/plan.md).

## Game content boundary

This repository is public. Game data lives outside it and every derived file stays in ignored folders.

- The user's game and key paths are in `config.local.toml` (ignored). Pass the keys file path to tools; the tools read it themselves. Key values stay out of your output, logs, commands and files.
- Everything extracted or generated goes under `build/` or `generated/`.
- Every file the code generator emits carries the marker defined as `GENERATED_MARKER` in `tools/check_content.py`.
- Run `python tools/check_content.py --staged` before proposing a commit. CI runs the same check.
- Enable the hook once per clone: `git config core.hooksPath .githooks`.

## Reusing code

`refs/` holds the upstream checkouts (`tools/fetch_refs.sh` recreates it). Read-only.

- Copy only MIT, BSD, ISC, zlib or Apache-2.0 code, keep its header, and add a row to `THIRD_PARTY_NOTICES.md`.
- yuzu, suyu, Ryujinx and other GPL code: read for behaviour, write our own.
- Code taken from metroid-prime-4-recomp, fe3h-pc-recomp or totk-pc-recomp is attributed like any other upstream,
  naming the commit it came from. When a piece is copied unchanged, note it so it can later move into a shared library.

## Code and writing

- Generated C is never edited by hand: change the generator.
- Comments say what the code does and why. Cite our code by file plus function, upstream code by project, file and function.
- Code, comments, docs and commit messages are in English. Commit messages are plain sentences without type prefixes.
