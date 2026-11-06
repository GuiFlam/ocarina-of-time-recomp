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

Phase 1 reached (2026-11-05): `oot.exe` builds (`.venv-win/Scripts/python.exe tools/build_game.py` from MP4's venv for
now), `--selftest` and the host probes pass, and `--boot` passes to the first `nvnBootstrapLoader` call. `docs/plan.md`
Phase 1 lists the Switch 2 and SDK 22 differences met on the way (GetInfo 29-33, the thread handle at TLR+0x110, the
PACIBSP prologue, nn::oe and nn::vi additions in `ov_oot.c`, `nv::InitializeGraphics(0, 0)`). Extraction:
`python3.14 tools/extract_nup.py --verify` (needs `toolchain/zbic/nso_unzbic` from `tools/build_zbic.py`) writes
`build/extracted/v131072/{exefs,romfs,extraction-manifest.json}`. Logs of runs go to `build/logs/`. Phase 2 (`oot.exe
--seconds 15`) runs the whole start-up without a crash: audio, saves, NPLN offline, 3,661 shader programs, three frames
presented; it then stalls because no SM86 shader translates (`docs/plan.md`, "Open: shader translation"), which is
Phase 3: `docs/phase3.md` is the hand-off for it (what blocks, the measured NVN2 control-section fields, the lowering
approach, the order of work). Start there, with its dated Status sections: as of the night of 2026-11-06 the SM86
lowering covers 94.1% of the corpus, the title cinematic decodes on the host (`runtime/ov_movie22.c`, dav1d) and
plays on screen, upscaled to 2560x1440 by the game's neural network through the emulated tensor-core instructions
(HMMA, LDSM, MOVM), at 21 fps. The shader cache keys carry the translator identity and `KEY_VERSION`
(`gfx_shader.c`): bump the latter after a lowering change. Test runs: `scratchpad/bin/run.sh` kills every run; the
first run after a translator or lowering change spends a minute building the kernels' pipelines (the driver's disk
cache keeps them); about one run in two dies right after the first frame (an SDK abort in MultiWait, see the docs),
so run until one survives. Discovery tools that paid off: `OOT_BREAK_AT`/`OOT_BREAK_BRIEF`/`OOT_BREAK_WATCH`, the
crash dump's memory and TLS peeks (`OOT_CRASH_PEEK`), the NVN2 logging stubs (`nvn_procs_stub.inc`),
`build/logs/nvn-proc-got.json` (GOT slot -> proc name, from the loader at `main+0x129e290`), `--probe-tensor` and
`--probe-cs` (a kernel in isolation), `OOT_DUMP=dispatch`, `OOT_SKIP_DISPATCH_FROM`, `OOT_MQ_LOG`, `OOT_THREAD_DUMP`.

## Environment notes

- The user works in WSL (Linux), where the interpreter is `python3`. Builds run with Windows Python and MSYS2 UCRT64
  Clang through interop, as in MP4 (`.venv-win/Scripts/python.exe tools/build_game.py`); make the venvs from
  `tools/requirements.txt`.
- The dump is 25 GB on the Windows desktop; `config.local.toml` points at it in place rather than copying it.
- `oot.exe` is a Windows program run from WSL through interop: WSL environment variables do NOT reach it unless named
  in `WSLENV`. Always run it as `WSLENV=OOT_TRACE:OOT_VOLUME:OOT_APP_POOL:OOT_CRASH_PEEK:OOT_BREAK_AT:OOT_BREAK_BRIEF:OOT_BREAK_WATCH
  OOT_TRACE=gfx build/v131072/oot.exe ...`
  (add every `OOT_*` variable used); without it the variable is silently ignored. Half a day of traces in this
  session were empty for this reason.
- Never commit without the content check passing; never bypass the hook.
