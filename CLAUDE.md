@AGENTS.md

# Session guide

## What this project is

A static recompilation of *The Legend of Zelda: Ocarina of Time*, from the user's dump
`The Legend of Zelda Ocarina of TIme [04004860224DEC000].nup` (title id `04004860224DEC00`). The goal is the same as
the predecessors': lift the game's ARM64 code to C, compile it for x86-64 and link it with a host runtime that stands
in for the OS and GPU. Players bring their own dump and keys; nothing from the game is ever committed.

This is the fourth game on the totk-pc-recomp code, and the first from the Switch 2: the title id is in the `0400`
range, and the dump is a `.nup`, not an XCI or NSP. metroid-prime-4-recomp deliberately left its Switch 2 build
alone (its `docs/plan.md`, Phase 0), so everything below the container is unknown until Phase 0 answers it.

## Where to get code from

- `refs/metroid-prime-4-recomp/`: the MP4 port at `586e91e`, the direct predecessor. Every file in this tree was
  copied from there on 2026-11-05 with `mp4` renamed to `oot` (`MP4_` to `OOT_`); each file's first line names
  its source, and the lines after it carry the older attributions. Its `docs/plan.md` records what broke on the third
  game and why.
- `refs/fe3h-pc-recomp/` and `refs/totk-pc-recomp/`: the older ports. Read `docs/architecture.md` in TotK first.
- `CLAUDE.local.md` (ignored) names the local working checkouts.

Still MP4's and to be rewritten for this game once Phase 0 knows its modules and versions (they would mislead if
trusted): `runtime/boot.c` (modules, init_array sizes, stack, `nvnBootstrapLoader` return addresses, SDK init form),
the main stack in `runtime/kernel.c`, the version tables in `tools/build_game.py`, `installer/layout.py`,
`tools/make_release.py` and `tools/prepare_shaders.py` (`v131072`), the probe
table in `runtime/loader.c`, `runtime/frame_rate.c` (host half of MP4's frame-time patch), and `runtime/ov_oot.c`
(MP4's SDK 19.3 overrides: keep what this game reaches, drop the rest). Not taken: MP4's `recomp/patches-v131072.txt`
and `tools/data/expected-*.json`.

## Status

Phase 0 (`docs/plan.md`). The `.nup` is a plain container (no keys needed): `python3.14 tools/extract_nup.py --list`,
then `--exefs-only --verify` wrote `build/extracted/v131072/exefs` (title id `0400486024DEC000`, v131072, display
1.1.1). `main` is ZBIC-compressed (NSO flag bit 7): `python3 tools/build_zbic.py` builds our decoder (upstream zstd plus
`tools/zbic/`), and `extract_nup.py` then writes plain NSOs to `exefs/` (stored ones in `exefs-zbic/`). `--romfs-only`
leaves the ExeFS alone. The lifter now takes the
Switch 2's ARMv8.3 code (PAC branches and ops, `LDAPR`, `UDOT`, FP16 `FNEG`): 0 unsupported instructions in every
module, differential tests pass. Survey C in `generated/survey-v131072/`. Run the lifter tools with a Python 3.12 venv
holding `tools/requirements.txt` (MP4's `.venv` works for now) and the differential tests with a Windows venv
(MP4's `.venv-win`). Next: RomFS serving, NVN2.

## Environment notes

- The user works in WSL (Linux), where the interpreter is `python3`. Builds run with Windows Python and MSYS2 UCRT64
  Clang through interop, as in MP4 (`.venv-win/Scripts/python.exe tools/build_game.py`); make the venvs from
  `tools/requirements.txt`.
- The dump is 25 GB on the Windows desktop; `config.local.toml` points at it in place rather than copying it.
- Never commit without the content check passing; never bypass the hook.
