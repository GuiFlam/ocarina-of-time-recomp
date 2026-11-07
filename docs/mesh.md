# Mesh and task shaders

Hand-off for the work that draws the geometry the Kokiri Forest's colour pass sends through mesh shaders. Written
2026-11-06, before any code for it beyond diagnostics; update the Status section as it moves.

## Symptom and evidence

On the level route (`python3 tools/run.py --seconds 150 --replay build/logs/a-spam.txt --skip-movies --env
OOT_SKIP_CS=1a62084a --shot ...`) most of the scene is flat black or white. A frame dump (`--dump 900`,
`build/logs/dump-361`, `docs/phase3.md` evening status) shows:

- pass 136, a depth prepass: 217 ordinary draws, colour masked, covering the whole view;
- pass 168, the colour pass (depth EQUAL, no write): its ordinary draws render correctly (textured bark and moss),
  but most of the scene, the ground among it, is missing.

The missing part is drawn with mesh shaders, which are stubs:

- `nvnCommandBufferDrawMeshTasksIndirect` is called thousands of times a run, `DrawMeshTasks` too (both in
  `runtime/nvn_procs_stub.inc`; `nvn_procs.c` logs a stub's call count at powers of two from 1024).
- The game's programs come in four stage sets by NVN2's stage byte (control +0x56; `nvn_api_obj.c prog_set_shaders`
  logs each set once): vertex + fragment (0 1), compute (5), mesh + fragment (7 1) and task + mesh + fragment
  (6 7 1).
- `OOT_MESH_DUMP=<dir>` (a Windows path, e.g. from `wslpath -w build/logs/mesh`) writes the control block (0x400
  bytes) and code of every stage of the first 12 mesh programs as `<k>_s<stage>.ctl/.code`. The first 12 are mesh +
  fragment (no task stage among them); `tools/sm86_dis.py <file>.code` lists them.

## What the programs contain

Over the 12 captured mesh stages (320-2200 instructions each), only two opcodes are new to the lowering; the rest is
the instruction set the vertex, fragment and compute programs already use:

- `0x923` ISBERD, once per program, at the start (reads the stage buffer: the mesh group's index, the payload);
- `0x927` ISBEWR, about 800 times (writes the stage buffer: the outputs), e.g. `ISBEWR [R7 + 0x2001c]`.

All 12 stop in `sm86_lower` at the first ISBERD/ISBEWR ("opcode"). Bank 4 constants appear too (`c[4][0x5c]`).

## Layout, from Lichii's notes (GPL-3.0: behaviour only, write our own code)

`/mnt/c/dev/Lichii/NX2_NOTES.md`, row 89 (the same scheme in another Switch 2 title):

- Task program: `ISBERD.ATTR.SKEW [RZ]` is the workgroup index; it writes the payload with `ISBEWR.O.ATTR` at
  `rank * stride + 0x10` and the number of mesh workgroups to launch at `[0x4]`.
- Mesh program: input `[0]` is its workgroup index, the payload from 0x10. Outputs: vertices in groups of 32, one
  128-byte line per output component in the mesh header's output-map order, then the per-primitive groups; the
  primitive map has the count at `ISBEWR.O.32 [0x3]` and U8 vertex indices from 4. Addresses are linear bytes.
- On the GPU side, while SET_MESH_CONTROL (method 0x114c) is on, the task program sits in the vertex slot, the mesh
  program in the tessellation slot, and a header-only pass-through geometry program supplies the per-primitive output
  map. The draw count is the number of task workgroups.
- Their approach: VK_EXT_mesh_shader; ISBE reads and writes kept as a pseudo instruction through the lowering, an
  emulated stage buffer (task payload / workgroup memory), and every EXIT branching to one epilogue that emits
  EmitMeshTasks (task) or SetMeshOutputs plus the copy of the buffer into the mesh outputs (mesh).

To confirm against our captures before relying on it: the field meanings of `ISBEWR` (address register, the
immediate's offset and mode bits) and where the control section declares the output count, the maximum vertices
and primitives and the output map of a mesh program.

## Plan

1. **Decode** ISBERD/ISBEWR on the captured programs (`tools/sm86_dis.py`; Mesa NAK has no encoder for them, so the
   fields come from the captures and the notes above) and the mesh control section fields (max vertices,
   primitives, workgroup size, output map). Capture task programs too (raise the dump's limit or filter on stage 6).
2. **Lowering** (`runtime/sm86_lower.c`): keep ISBERD/ISBEWR as a pseudo instruction the translator understands
   (a translator patch decodes it), or lower them to shared-memory loads and stores into an emulated stage buffer
   (workgroup memory sized from the control section), with every EXIT jumping to an epilogue marker.
3. **Translator** (a patch series in `tools/xlate/patches`): task and mesh execution models in Ryujinx's SPIR-V back
   end (`TaskEXT`, `MeshEXT`, `SPV_EXT_mesh_shader`): the payload, the epilogue (EmitMeshTasksEXT;
   SetMeshOutputsEXT plus copying positions, attributes and primitive indices out of the emulated buffer), local
   size from the control section. The fragment stage stays as it is (it reads the mesh's attributes as varyings).
4. **Host**: mesh pipelines (no vertex input; task and mesh stages; `VK_EXT_mesh_shader` enabled in
   `vk_device.c` and checked in `vk_caps.c`), the reflection of task and mesh shaders in `gfx_shader.c`, and the
   draws: record `DrawMeshTasks*` as replay ops (`nvn_api_cmd.c`) and replay them as `vkCmdDrawMeshTasks*EXT`.
5. **Check** with the level route's screenshot and a frame dump: pass 168 should have the ground.

An alternative to 3 and 4 is running task and mesh programs as compute that writes vertex and index buffers, then
an indirect draw with a pass-through vertex shader. It avoids a new execution model in the translator, but needs a
chain of indirect dispatches and draws per call and buffers sized for the worst case; VK_EXT_mesh_shader is on both
target GPUs (RTX 4070, RX 9070), so the native way is the recommendation.

## Status

- 2026-11-06: findings above; diagnostics `OOT_MESH_DUMP`, the stub call counts and the stage-set log in place.
