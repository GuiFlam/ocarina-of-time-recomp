# Plan

## Phase 0: survey

What the dump is, before any code is lifted. metroid-prime-4-recomp's `docs/plan.md` (Phase 0) is the model.

| Question | Answer |
|---|---|
| Dump | `The Legend of Zelda Ocarina of TIme [04004860224DEC000].nup`, 25,020,741,061 bytes |
| Title id | `04004860224DEC00` (file name; the same value sits at offset 0xB0 of the header). The `0400` range is the Switch 2's |
| Container | Magic `NUP\x01` at 0, a `TOC\x01` table at 0x80, entry records from 0x10000 on; the data past the header is encrypted (no readable strings). Not XCI or NSP: `tools/extract_exefs.py` and hactool do not read it |
| Keys | Open. Switch 2 keys, from the user's own console, presumably; which ones the container needs is open |
| CPU, executable format | Open. Expected ARM64 NSOs, as the Switch 2 runs Switch code; the lifter's coverage of its instruction set is open |
| SDK version | Open |
| Graphics API and shaders | Open. MP4's NVN front end and Maxwell translator assume a Switch 1 GPU; the Switch 2's NVN and shader ISA may differ |
| Engine, archives | Open |
| Updates, DLC | Open |

First steps:

1. Read the `.nup` container: header, TOC, entry records; what is encrypted with what. Write `tools/extract_nup.py`
   (our own code; read GPL tools for behaviour only).
2. From its contents, the ExeFS and RomFS, and whether `recomp/nso.py` loads the modules.
3. Run the lifter over every module and count unsupported instructions.
4. The SDK version, the NVN version, the shader format.

## Phase 1: build

Rewrite the game-bound tables still holding Metroid Prime 4's values, listed in `CLAUDE.md` ("Where to get code
from"), then build `oot.exe` and run `--selftest`, the differential tests and the host probes as MP4 did.

## Shared core

The copy of 2026-11-05, from metroid-prime-4-recomp @ `586e91e`: 606 tracked files, 119 changed only by the rename of
`mp4` to `oot` (`MP4_` to `OOT_`, the window title and installer text naming the game), the rest unchanged; each
file's first line says which. Not taken: `recomp/patches-v131072.txt`, `tools/data/expected-v1.0.0.json`,
`tools/data/expected-v1.1.0.json`. Unrenamed on purpose: `tools/vendor/` and `tools/xlate/patches/`, whose "MP4"
comments record where a change was made.
