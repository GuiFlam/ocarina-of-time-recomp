# Phase 3: SM86 shaders

Hand-off written 2026-11-05 at commit `b46d663`, for a fresh session. Read `CLAUDE.md` and `AGENTS.md` first; the
history is in `docs/plan.md` (Phases 0 to 2 and the NVN2 survey). This file says where the port stands, exactly what
blocks it, what is already measured, and the order to do the work in.

## Status (2026-11-05, later the same day)

Steps 1 to 3 of the order of work below are done; step 4 is at 86% of the corpus; step 5 is where the work is.

- The translator builds (`build/xlate/oot_xlate.exe`, `python tools/build_xlate.py` from the Windows venv, 20 s).
  `tools/xlate/Program.cs` now echoes the job's `c1` into the `.res.json` (the host binds the lowered jump tables).
- `tools/shader_census.py` (NVN2 version; `python3.14`, Linux) reads the archives and writes
  `build/v131072/shaders/{ctl,code}/<key>`, `programs.json` and `opcodes.txt`: 14,363 programs (10,994 pixel, 3,106
  vertex, 251 compute, 9 mesh, 3 task) from the four `Shader/*.bfsha.zs`, the `Layout2Shader` bfsha and the two agl
  `.sharcb` of the ApplicationPackage sarc; the ExeFS modules hold none. `--lower` builds `runtime/sm86_lower.c` with
  gcc (`tools/sm86_lower_main.c`) and lowers every program, grouping the failures with the failing instruction in
  `lowered.txt`; `--translate` runs the translator over the lowered ones through WSL interop (`failures.txt`).
  `tools/sm86_dis.py <key>` lists a program's fields. The corpus is regenerated from the RomFS in 2 min; later runs
  reuse `programs.json` (`--extract` reads the archives again).
- The control section's fields are settled (the table below, columns "Reading", and `sm86_control_parse`): size at
  `+0xd4` (the BNSH archives repeat it at `+0x30` and `+0x98`, the agl packages leave those 0), instruction end at
  `+0x38`, jump-table constants at `+0x40` with their length at `+0x3c`, code size at `+0x44`, the stage at byte
  `+0x56` (NVN's enum plus 6 task, 7 mesh), the compute workgroup size at `+0x58`; the tail past `+0xe0` holds a u32
  count and 16-byte records `{BRX instruction offset, entry count, 0x10000, byte offset of the table in c1}`.
  Graphics code starts with the 0x80-byte Turing+ header, whose first 0x50 bytes are the Maxwell header the
  translator reads. The early-fragment-tests flag is not identified (candidates: `+0xa4` bits 0 and 8).
- `runtime/sm86_lower.c` lowers 13,444 of 14,363 programs (93.6%). Registers: the lowering's uniform registers
  and scratch live above the program's own; a uniform register loaded by a single direct `ULDC` (or `UMOV`), or by
  one earlier in the same basic block, is forwarded as that constant and needs no register; when the program leaves
  no room, a liveness analysis over the basic blocks (`insn_regs`, exact destination widths, reads over-stated only
  for texture coordinates) finds scratch among the registers dead at each instruction and a uniform register's
  register among those dead over its span. Uniform predicates take free predicates, or are swapped through a
  register around each instruction naming them. What is left: 656 vertex shaders (skinning: up to 247 registers
  live at once, no adjacent pair free where a load needs one), the surface stores (`SUST`, 86 compute programs),
  `ISBERD`/`ISBEWR` (task and mesh stages), a few rare forms (`lowered.txt`).
- The host (`runtime/gfx_shader.c`, `vk_exec_draw.c`, `vk_dump.c`, the shader probe and the pipeline self-test)
  reads the NVN2 control through `sm86_control_parse`, hashes the control section (its own size) and the whole code
  block for the cache key (`KEY_VERSION` 5, `IN_VERSION` 2, `PRG_VERSION` 2), lowers before translating, and binds
  the lowered program's c1 (`gfx_shader_c1`, uploaded once per shader) as uniform slot 1 instead of the game's, since
  the jump tables' targets moved with the code.

## Status (2026-11-06)

The game plays its title cinematic through a host decoder, and the picture is still black for a reason outside the
shaders lowered so far: the game's video path runs on the tensor cores.

- `runtime/ov_movie22.c` decodes the cinematics with dav1d (linked statically from MSYS2, BSD-2, row in
  `THIRD_PARTY_NOTICES.md`): the title is "video/av01" 1920x1080 at 60 fps, a 64 s stream with a key frame every
  321 samples (`nn::movie::Buffer` flag 4 marks those, not the end of the stream, which is an empty flagged buffer).
  Frames come back to the game as NV12 in the buffer it registered (it reads planes 0 and 1 only), with the
  sample's time stamp. Audio is still dropped. The decode runs on the game's decode thread; the game then runs
  at about 14 fps there, to be profiled (dav1d on 4 threads, the NV12 interleave, or the game waiting on frames).
- The lowering grew: SUST (bindless store, the handle loaded from the constant word), LEA and IMAD.X carries
  through comparisons and predicate adds (the translator ignores Maxwell LEA's carry predicate and X), the
  uniform register budget counts registers read before any definition, uniform pairs are allocated adjacently
  in both the pool and the liveness mode, the swap register for uniform predicates comes from the liveness
  allocator when the pool is full, and only the predicate fields an op has are rewritten. Corpus: 13,514 of 14,363
  lower (94.1%), all translate.
- What blocks the picture: every shader of the video path (19 compute programs, dumped by the host to
  `shader-cache/<key>.fail.{ctl,code}` for `tools/sm86_dis.py`) uses the tensor-core instructions: HMMA (0x03c,
  4,478 instructions), LDSM (0x03b, 1,670) and MOVM (0x03a, 24). They are the game's neural upscaler (the SDK
  list names "SMAA_For_NDSR"; the swapchain is 2560x1440 while the movie is 1080p), and they are not in the RomFS
  archives the census reads (nor in the ExeFS modules; the raw `romfs.bin` has no match either, so they sit in a
  compressed file the census does not visit, or the game builds them). NVN2 device property `0x67`, kept by the
  game as a capability flag and required by its video surface setup, is most likely "supports tensor cores".
  Next: lower HMMA.16816 / 1688 f16 (LDSM's matrix loads into per-lane fragments, the warp-cooperative product
  through SHFL and FFMA, MOVM's transpose) in front of the translator, or find the non-tensor path if the game has
  one (with `0x67` = 0 the player aborted in its surface setup; the shader's decision may lie elsewhere).
- Reaching the game proper: `OOT_INPUT_SCRIPT` (lines "<ticks> A 10") passes the title; the file select and the
  opening are cinematics too, so they need the same path.
- The intermittent death right after the first frame (one run in three, exit 40 or 5): the crash handler's dump
  (`main.c`) caught two. One executed from the thread's own stack while logging a guest `Break` from
  `sdk+0x1a5560` (the SDK abort), the log line itself garbled; the other executed from the heap region of the
  `Cpu` structures with `rt_svc+0x59` (after `svc_body`: the host-depth bookkeeping, `kern_preempt_pending` /
  `kern_host_left`) as the return address and lifted `main+0x3779bc0` below it. Both point at the asynchronous
  preemption (`kernel.c`, the interrupt trampoline that pushes the interrupted thread's registers on its stack).
  Test: `OOT_ASYNC_PREEMPT=0` over several runs; then read the trampoline against a thread suspended inside host
  code.

## Status (2026-11-06, later): the title frame decodes, the upscaler does not

The movie frame reaches the GPU and converts correctly: the first compute pass of the game's video path writes a
1920x1080 RGBA8 image of the title cinematic (Link on Epona by the river; dump `build/logs/dump-93`, pass 0000,
147,050 distinct colours). What follows it, the neural upscaler's twelve kernels and the final 2560x1440 pass, still
produces a near-constant image and then zeros, so the screen stays black. Found and done on the way:

- The tensor-core instructions are emulated in the lowering (`runtime/sm86_lower.c`, "tensor-core instructions"):
  LDSM (1, 2 or 4 matrices, plain or transposed) through SHFL.IDX of the row addresses and LDS; HMMA.16816.F16.F16
  through the PTX fragment layouts (lane = 4g + t), SHFL.IDX and HFMA2 (f16 accumulation, the tensor core accumulates
  wider); MOVM through two SHFL and a PRMT. Also added: SUST (bindless store, the handle a dword index in the
  constant word as TEX's), HFMA2 (all forms, .RELU through HSET2.GT.BF and HMUL2), F2FP (two F2F.F16.F32 and a
  PRMT), HMNMX2 (HSET2, HADD2, HFMA2), BAR.SYNC. All 19 kernels lower and translate; the corpus stays at 13,514
  lowered, 0 translation failures.
- NVN2's constant bank 0 for compute is not Maxwell's driver bank. `nvnCommandBufferPushComputeConstants(cmd, 0,
  offset, bytes, data)` (an NVN2 entry point the host only stubbed) pushes the kernel's parameter block, and the bank
  reads as: 0x10 reserved bytes (the dispatch size is put there), the pushed constants, then the bound resources in
  slot order, storage buffers {u64 address, u32 size, u32 0} for control[0xb2] slots, texture handles (u64) for
  control[0xb3] slots, image handles (u64) for control[0xb6] slots (the counts are bytes of the control section;
  control[0xb8..0xba] holds the bank's size). Measured on the kernels: the final pass pushes 0x54 bytes and reads its
  storage slot 2 at 0x90, textures 0 and 3 at 0xa0 and 0xb8, image 1 at 0xc8; the NV12 pass pushes 0x40 bytes and
  reads textures 1 and 2 at 0x58 and 0x60, image 0 at 0x68; the kernels without a push read storage slots 0 and 1 at
  0x10 and 0x20. The host records the push as OP_PUSH_CC, builds the bank at each dispatch (`vk_exec_draw.c`
  nvn2_c0_fill_cs), consumes the push with the dispatch, and resolves the shader's bank-0 words back to the bound
  slots for its descriptors (nvn2_c0_slot, cbuf_handle, ssbo_info). The graphics stages still get Maxwell's driver
  bank; the game's vertex shaders read 64-bit addresses at c0+0x60, which is the same scheme and will need the push
  (nvnCommandBufferPushGraphicsConstants is still a stub) when a draw depends on it.
- Diagnostics added: `OOT_BIND_LOG=1` logs the compute stage's binds, pushes, dispatches and inline uniform updates
  from frame 290 on; `OOT_PROG_DUMP=1` hex-dumps the memory around compute programs' control sections at
  SetShaders; the frame dump prints each stage's bound texture, image and uniform slots per pass and resolves texture
  declarations as the replay does (`vk_exec_res_handle`), so storage images written by dispatches get their PNGs.
- The death after the first frame, one run in three, is the SDK's own abort in
  `nn::os::detail::MultiWaitImpl::InternalWaitAnyImpl` (sdk+0x167be0): after a holder's virtual IsSignaled (vtable
  +0x18) returns true, it aborts when its count of found holders reaches the caller's capacity (the compare before
  the jump to sdk+0x167ec4, which calls `nn::diag::detail::AbortImpl()` through the GOT). The host then dies with
  the abort's return address as the instruction pointer (`executing 0x7005367ec8`, the LR of that call, rcx holding
  the Cpu), before `AbortImpl(const char *, ...)` is reached (an observer on it, `ov_oot.c` obs_abort_impl, never
  fires). Two bugs: more holders report signalled than the game expects (an nn::os event or timer the host leaves
  signalled at start-up, to find), and the abort path crashing instead of reporting. `OOT_ASYNC_PREEMPT=0` does not
  prevent it.

Next: find where the upscaler's values go wrong (`OOT_DUMP_BUF` snapshots of its feature-map buffers after each
pass, the shapes of the HMMA operands against the PTX layouts, the LDSM addresses), then the frame rate (the decode
on the game's thread and the emulated kernels).

## Where the port stands

`oot.exe --seconds 15` runs the whole start-up of OoT 1.1.1 without a crash: 82 threads, 10,988 IPC messages, 1,485
file opens, saves mounted, NPLN offline, the audio renderer at 48 kHz, 3,661 shader programs set up through NVN2's
program builder, three frames presented at 2560x1440 (`build/logs/run-45.log`). Then it stalls in the third frame
(51.7 s) because no shader translates:

```
[host] gfx_shader: translation failed (-): bad control section
[host] vk: a stage 0 shader of program 4136 failed to translate: draws with it are skipped
```

Draws with failed programs are skipped, the game waits on GPU work that never completes, nothing is drawn. Everything
below the graphics API is believed to work; every remaining known problem is in this one area or is minor (see "After
the shaders").

## Why every shader fails

The host's shader path (`runtime/gfx_shader.c`, `runtime/vk_exec_draw.c`, `tools/xlate/`) is TotK's, for Switch 1:

- `gfx_shader.c` `req_init()` reads the c1 offset at `control+0x700` and the code length at `control+0x704`, and
  `CONTROL_SIZE` is `0x880`: the cache key, the `.in` files, `build_code_hash()` and the async job copies all take
  0x880 bytes from the control pointer. `vk_dump.c:585` reads `+0x700` too.
- The NVN2 control section is 224, 248 or 280 bytes (`0xe0`, `0xf8`, `0x118`). Reading `+0x700` reads whatever lies
  after it, so the length check fails and every program is rejected before translation.
- The code is Ampere SM86 (the Switch 2's T239 GPU): 128-bit instructions, opcode in the low 12 bits of word 0, no
  Maxwell scheduling words. The vendored translator (`tools/vendor/ryujinx`, Ryujinx 1.3.2, MIT) decodes Maxwell and
  Pascal 64-bit forms only (`src/Ryujinx.Graphics.Shader/Decoders/InstDecoders.cs`).
- The translator itself is not built here yet: the log says `translator identity 0000000000000000 (none: no
  translator)`. `.NET 10.0.403` is installed; `python tools/build_xlate.py` (Windows venv) builds
  `build/xlate/oot_xlate.exe`. MP4's `mp4_xlate.exe` next door is the same source and would do for a first smoke test.

So there are two separate jobs: the control-section layout (small, data-driven) and an SM86 front end (large).

## What is measured about the NVN2 control section

From the decompressed `Shader/Model.Product.110.product.Nin_Ounce_NVN2.bfsha` (320 MB; zstd with the `zs.zsdic`
dictionary from `Pack/ZsDic.pack.zs`): 8,517 blocks found by the magic `0x98761234`, 20,000 of them surveyed by
per-dword variance. The first 0xe0 bytes are the fixed part; the block's own size sits at `+0x30` and is repeated at
`+0x98` and `+0xd4`; the tail after `+0xe0` is 0, 0x18 or 0x38 bytes of extra records.

| Offset | Seen | Reading |
|---|---|---|
| `+0x00` | `0x98761234` | magic, as on Switch 1 |
| `+0x04`, `+0x08`, `+0x0c` | `0xc9`, `0x14`, `0xe0170` in every block | constants (version words) |
| `+0x18`, `+0x1c` | `0x220014`, `0x288040` in every block | constants (SDK 22.x / NVN2 version?) |
| `+0x20..+0x27` | unique per block | a 64-bit hash |
| `+0x28..+0x2f` | 7,832 distinct over 8,517 | a second 64-bit hash, shared by a few blocks |
| `+0x30` | `0xf8` (5,900), `0xe0` (2,177), `0x118` (315) | **control section size**; repeated at `+0x98`, `+0xd4` |
| `+0x38` | 402 values, `0x6e80..0x7000` typical | code size candidate (multiple of 0x80) |
| `+0x3c` | `0x14` (5,900), `0` (1,906), `0x20` (394) | tracks `+0x30`; `+0xdc` has the same classes |
| `+0x40`, `+0x44` | rise with `+0x38` (`0x9e80, 0x9f00, 0xa000`) | sizes with headers, or offsets of c1 and end |
| `+0x4c` | 150 values, `0x5a..0x60` common | register count or similar small per-shader number |
| `+0x50..+0x60` | small flags, `0x10000` common | stage/feature flags |
| `+0x64..+0x70` | `0x3f800000` in a few blocks | floats (1.0f) |
| `+0xa4` | `0x1000001` or `0x1000101` | flags |
| `+0xbc`, `+0xc4`, `+0xc8`, `+0xd8` | `0xe0` always | size of the fixed part |

The survey script is a few lines: find each magic, measure to the next, tabulate each dword. Not yet known and needed
first: which fields are the **code length**, the **c1 offset** (the constant bank the translator needs for `BRX`
jump tables) and the **stage**, and whether an early-fragment-tests flag exists (Switch 1: `control[0x718] & 1`).
Two ways to pin them, both cheap:

1. The archive's own tables. BNSH's shader code section records per program the code and control offsets and sizes
   (TotK's `refs/totk-pc-recomp/tools/shader_census.py`, from MacNativeGames' `prepare_shader_seed.py`, MIT, parses
   Switch 1 BNSH; the container is unchanged here). Match those sizes against `+0x38`/`+0x40`/`+0x44`.
2. The game at run time. `prog_set_shaders()` (`nvn_api_obj.c`) logs `data0`; add a once-only dump of the 0x120 bytes
   behind `control` and the size of the pool mapping behind `data`, and compare with the bytes that follow the code
   (the program's own end is where the code size says).

Note the game hands over the Switch 1 `NVNshaderData` form, `{data, control}` of 16 bytes
(`nvnProgramBuilderSetShaders`, built on the stack at `main+0x14e2468`), so the host side needs no ABI change.

## What is known about the code

- Every stage ends in a 128-bit `EXIT`, opcode `0x94d`; none has Maxwell's. Instruction words are 16 bytes, the opcode
  is `word0 & 0xfff` (Lichii's `instruction.h` uses exactly this `Bits(0, 12)`).
- Phase 0 measured a 0x60-byte program header before the instructions, against Maxwell's 0x50-byte SPH. Lichii's
  environment reads `sizeof(sph)` (0x50) from the program base for Switch 2 titles too, so settle the header size on
  the corpus before anything else: the first instruction is where a plausible opcode stream begins.
- The translator job format (`tools/xlate/Program.cs`) is "graphics: 0x50-byte SPH + instructions, compute:
  instructions"; `gfx_shader.c` skips `CODE_SKIP 0x30` of NVN code-block header before that for graphics stages.
  Both constants must be checked against the NVN2 layout.

## Approach: lower SM86 to Maxwell in front of the existing translator

Recommended. Lichii (`src/shader_recompiler/frontend/ampere/lower_to_maxwell.h`, GPL-3.0, read for behaviour only) runs
four Switch 2 titles this way: it does not reinterpret the bytes, it emits new Maxwell instructions for each SM86 one,
inserts the scheduling word every three instructions, maps the uniform registers (`UR0..`) onto high general registers
(R216 and up), keeps an address map so branch targets and `BRX` tables survive the re-layout, and hands the result to
the unchanged yuzu Maxwell front end. The alternative, a native SM86 decoder inside Ryujinx's translator, would mean
new decoder structs and emitter signatures for every instruction (Ryujinx's emitters are bound to the 64-bit field
layout), many times the work for the same output.

Ours would be a C module with no dependency on the translator, usable both in `runtime/gfx_shader.c` (lower, then
request the translation of the Maxwell code; cache keys stay over the original bytes) and in an offline tool over the
whole corpus. Sources of truth that may be copied, with a header and a `THIRD_PARTY_NOTICES.md` row:

- Mesa's NAK compiler, `src/nouveau/compiler/nak/sm70_encode.rs` (MIT): the Volta/Turing/Ampere encoding, field by
  field, and the reference Lichii itself names for its encoding knowledge.
- Ryujinx's `Decoders/InstDecoders.cs` and `InstTable.cs` (MIT, already vendored): the Maxwell target encoding.

From Lichii, only behaviour: which SM86 forms Nintendo's compiler emits for these engines, the pitfalls it records in
`NX2_NOTES.md` (row 54: widen until the whole dump lowers; row 93: save and restore P6 around uniform guards; row 121:
translation time), and its test cases under `src/tests/video_core/ampere_*.cpp` as a checklist of forms (uniform
datapath `R2UR`/`UP2UR`/`ULDC`, `LDSM`, `IPA` modes, formatted surfaces, `CALL.REL.NOINC`, 64-bit `UIADD3` carry,
`TMML`, attribute `0x5c`).

## Order of work

1. **Build the translator** (`python tools/build_xlate.py` from the Windows venv), so a lowered shader can be checked
   at once. Confirm with MP4's census shaders if needed.
2. **Corpus tool**: port `refs/totk-pc-recomp/tools/shader_census.py` to the NVN2 archives (`Model.bfsha`, the
   `Layout2Shader`, `ApplicationPackage` sarc, `.vsetb`, fonts; `tools/shader_inventory.py` is MP4's Retro-format
   tool and is replaced, not extended). Output under `build/v131072/shaders/`: control and code per program, plus an
   opcode histogram (`word0 & 0xfff`) over all code. The histogram is the work list for step 4.
3. **Control section** in `gfx_shader.c`: one `ctl_parse()` giving size, code length, c1 offset, stage and flags, used
   by `req_init`, `build_code_hash`, the `.in` writer/reader, the async job copies and `vk_dump.c`; `CONTROL_SIZE`
   becomes the block's own size. Bump `KEY_VERSION`/`IN_VERSION` (the formats change). The `--probe-gfx`-style census
   self-test in `vk_pipeline.c` (`build/v121/shaders`) and `gfx_shader_probe.c` need the new corpus path.
4. **SM86 lowering**, corpus-driven: implement by opcode frequency, with the offline tool reporting the share of
   programs that lower and translate; stop at every unknown form rather than guessing. Hard parts in order: the
   address map (sched words every three instructions change every branch target; `BRX` tables live in c1), the
   uniform datapath, predicates and the P6 guard, then texture and surface forms.
5. **Run the game** with `OOT_TRACE=gfx`. Expect the first pixels, then new failures: NVN2 procs still stubbed
   (`nvn_procs_stub.inc` logs the first three calls of each; 160 return 0, only three were reached so far), state
   objects whose NVN2 layout differs, and whatever frame 3 was waiting for.

## After the shaders

Known and parked, none blocking: the C++ unwinder reads past `subsdk0` into the gap before `sdk` (`QueryMemory`
should report it free); `--probe-ipc`'s sdk heap constructor address is MP4's (`ipc.c` `ctors_143`); the
`--probe-services` override list; installer tables (`installer/layout.py`, `tools/make_release.py`,
`tools/prepare_shaders.py`); `tests/diff/func.py` and `tests/jumptables_check.py` not run; MP4's stale `fnprof_hit`
stub and `"\mp4"` escape to report upstream.

## Working facts

- Build: `/mnt/c/dev/metroid-prime-4-recomp/.venv-win/Scripts/python.exe tools/build_game.py` (257 s), output
  `build/v131072/oot.exe`. Lift: `recomp/gen.py --exefs build/extracted/v131072/exefs --out generated/v131072
  --modules main,subsdk0,sdk`.
- Run, always with WSLENV or every `OOT_*` variable is silently dropped:
  `export WSLENV=OOT_TRACE:OOT_VOLUME:OOT_APP_POOL:OOT_CRASH_PEEK:OOT_BREAK_AT:OOT_BREAK_BRIEF:OOT_BREAK_WATCH;
  OOT_VOLUME=0 OOT_TRACE=gfx build/v131072/oot.exe --seconds 15 > build/logs/run-N.log 2>&1`.
- Debug: `OOT_BREAK_AT=main+0x...`, `OOT_BREAK_WATCH`, `OOT_CRASH_PEEK`, `build/logs/nvn-proc-got.json` (GOT slot to
  proc name), `OOT_TRACE=svc,host,ipc,fs,gfx`.
- Extraction: `python3.14 tools/extract_nup.py --verify` (needs `toolchain/zbic/nso_unzbic` from `tools/build_zbic.py`).
- Lichii is at `/mnt/c/dev/Lichii` (GPL-3.0): `NX2_NOTES.md`, `src/shader_recompiler/frontend/ampere/`,
  `src/video_core/shader_environment.cpp`. Behaviour only, never code.
- Nothing from the game is committed; everything extracted or decompressed stays under `build/` (the decompressed
  bfsha of this session lived in the scratchpad and is gone; regenerate it from the RomFS image).
