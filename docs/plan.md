# Plan

## Phase 0: survey

What the dump is, before any code is lifted. metroid-prime-4-recomp's `docs/plan.md` (Phase 0) is the model.

| Question | Answer |
|---|---|
| Dump | `The Legend of Zelda Ocarina of TIme [04004860224DEC000].nup`, 25,020,741,061 bytes. One package: kind update, flagged standalone (it carries the whole game), title id `0400486024DEC000` (the file name has one digit too many), version 131072 (`v131072`), display version 1.1.1, title "The Legend of Zelda Ocarina of Time" |
| Container | NUP, header version 1: loose ExeFS and RomFS trees, each file raw or zstd, with a SHA-256 per file and over the TOC; nothing encrypted, so no keys are needed to read it. Layout in `tools/extract_nup.py` (ours, written from Lichii's reader, GPL-3.0, read for behaviour only). ExeFS extracted to `build/extracted/v131072/exefs`, every hash verified |
| ExeFS | `main` 29.9 MB, `sdk` 7.8 MB, `subsdk0` 0.5 MB, `rtld`, `main.npdm`. RomFS: 186,587 files, 24.43 GiB (not extracted yet) |
| Executable format | NSO0, but `main`'s flags are 0xFF: bit 7 marks ZBIC segments, zstd frames with the magic "ZBIC" whose FSE table headers (NCount) are coded by binary interpolative coding instead of zstd's own form (first seen in Switch firmware 22.0.0). `sdk`, `subsdk0`, `rtld` are LZ4 as before. Our decoder: `tools/zbic/zbic_ncount.c` (the format is described there) hooked into upstream zstd 1.5.7 (BSD) by `tools/zbic/zbic.patch`, built by `tools/build_zbic.py` into `toolchain/zbic/nso_unzbic`; `tools/extract_nup.py` runs it on ZBIC modules and checks every segment against the SHA-256 in the NSO header. Written from kinnay/zbic's behaviour (GPL-2.0, Atmosphère's zstd), no code taken. Checks (2026-11-05): `main` decompresses byte for byte as kinnay/zbic does, its three segment hashes match, on Linux and on Windows (MSYS2 UCRT64 gcc); and 600 frames made locally by kinnay/zbic's encoder (levels 1 to 19, 1 byte to 2 MB, random, skewed and repetitive data; 2,502 table headers, 1,134 of them with low-probability symbols) all decode. The encoder stays out of the repository, so that check is not reproducible from it |
| Images | `main` 0x4EA0000 (60 MB of code), `subsdk0` 0xE0000, `sdk` 0x1080000; `sdk` 28,707 symbols, `subsdk0` 1,083, `main` 1,249 |
| CPU | ARMv8.3 code: return address signing (`PACIBSP` ... `RETAB` in every function), `LDAPR`, plus a few ARMv8.2 `UDOT`/`SDOT` and FP16 `FNEG`/`FABS`. All now lifted (`recomp/lifter.py`, `recomp/simd_c.py`); see below |
| Engine | Nintendo EPD's TotK-era engine by its file names (`.byml.zs`, `.blarc`, `.bfarc`), Havok 2025.2 (Physics, Cloth, AI, Animation), NPLN Google APIs 1.55.1 |
| Graphics | NVN2 (`Nin_Ounce_NVN2` in asset names; Ounce is the Switch 2). The NVN front end and the Maxwell translator assume the Switch 1 GPU: the largest open question |
| SDK version | Open (the `nnSdk` version string is not in plain form) |
| Updates, DLC | None in the dump |

**Lifter run (2026-11-05)**, `recomp/gen.py` over v131072: `main` 249,589 functions, 1,061 MB of C; `sdk` 45,907, 151 MB; `subsdk0` 1,547, 9 MB; 47 s. As copied from MP4, 223,448 instructions were unsupported: `RETAB` 149,725, `LDAPR` 73,664, `PACGA` 58, `PACIA`/`PACIB`/`AUTIB`/`XPACI` 10, `UDOT` 4, FP16 `FNEG` 4. Now 0 in every module:

- PAC branches (`BRAA`, `BLRAA`, `RETAA` and their B and Z forms) lift as the plain branch, and `PACIA`...`AUTDB`, `XPACI`/`XPACD` as identities. The signing instructions in hint space already lifted as no-ops, so no pointer is ever signed and authentication always succeeds. `PACGA` gives a deterministic mix of its two inputs in the upper half, as `sdk` only recomputes and compares it.
- `LDAPR` lifts like `LDAR`.
- `UDOT`/`SDOT` (vector) and FP16 `FABS`/`FNEG` (vector) are lowered in `simd_c.py`.

Trap sites left: 394,261 in `main`, all zero padding (393,731), `udf` words (528) and 2 data words, the same class MP4 had.

Differential tests (Windows venv): `tests/diff/lift.py` over this game, 72,000 encodings and 215,928 cases, 0 failures (A57 model). It now takes the module bases from `recomp/layout.py`, and has `--model max` and `--only` to test later-architecture forms: every distinct `LDAPR`, `UDOT`/`SDOT` and FP16 `FNEG`/`FABS` word of the game, 320 encodings and 16,000 cases on Unicorn's max CPU, 0 failures. `tests/diff/simd.py` passes, its lifter-only list being exactly the new DotProd and FP16 forms. The PAC lowering is not checked against Unicorn, as its max CPU signs pointers for real. Not run yet: `func.py`, `jumptables_check.py`. The harness's `fnprof_hit` stub, stale in MP4 too, follows `cpu.h` again.

Next:

1. RomFS: `runtime/romfs.c` reads one image through an index of (offset, size, path) and never parses the RomFS
   format, so `tools/extract_nup.py` writes the NUP's loose files back to back, in the index's path order, into
   `romfs/romfs.bin` with the `index.json` `tools/romfs_index.py` reads (status "pass" when every file was checked
   against its SHA-256). No runtime change; one file instead of 186,587 opens. `--romfs-loose` still writes loose files.
2. Done: ZBIC without GPL code (see the table).
3. The SDK and NVN versions; the NVN2 entry points `main` imports (`tools/survey_imports.py`).
4. Rewrite the game-bound tables and reach a build (Phase 1).

## Phase 1: build

Rewrite the game-bound tables still holding Metroid Prime 4's values, listed in `CLAUDE.md` ("Where to get code
from"), then build `oot.exe` and run `--selftest`, the differential tests and the host probes as MP4 did.

## Shared core

The copy of 2026-11-05, from metroid-prime-4-recomp @ `586e91e`: 606 tracked files, 119 changed only by the rename of
`mp4` to `oot` (`MP4_` to `OOT_`, the window title and installer text naming the game), the rest unchanged; each
file's first line says which. Not taken: `recomp/patches-v131072.txt`, `tools/data/expected-v1.0.0.json`,
`tools/data/expected-v1.1.0.json`. Unrenamed on purpose: `tools/vendor/` and `tools/xlate/patches/`, whose "MP4"
comments record where a change was made.
