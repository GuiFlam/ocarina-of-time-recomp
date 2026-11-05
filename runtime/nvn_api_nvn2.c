// NVN2 (SDK 22, Switch 2) procs that Ocarina of Time asks for at its graphics bring-up and the Switch 1 front end
// (nvn_api_core.c, nvn_api_obj.c) does not have: the HDR, variable-refresh and acquire-timeout settings of the window,
// ray tracing memory of the queue, fast-clear value registration of the device, and the program builder, which NVN2
// puts between the device and nvnProgramInitialize. Ours (2026-11-05); the names come from the game's proc loader
// (tools/gen_nvn_procs.py NVN2_OOT), the semantics from the names and NVN's conventions, as no NVN2 header is at hand.
//
// The host window is one fixed-rate SDR surface, Vulkan has no fast-clear registry and the back end does no ray
// tracing, so the setters accept and keep nothing, and the getters answer zero or the default. The program builder
// keeps two fields inside the game's NVNprogramBuilder object (its size is not known; two words fit any builder): the
// device at +0 and the sample shading flag at +8. nvnProgramInitialize (nvn_api_obj.c) ignores its second argument, so
// a builder there, where Switch 1 passed the device, changes nothing.
#include <stdint.h>
#include <string.h>

#include "host.h"
#include "nvn_api.h"

static void accept(Cpu *c) { (void)c; }           // a setter the host keeps nothing from
static void zero(Cpu *c) { ret_x(c, 0); }         // a getter answering 0 / false / none

// ---- device: fast clear registration. int nvnDeviceRegisterFastClear{Color,Colori,Colorui}(d, value, format),
// ...Depth(d, float), ...Stencil(d, int), and the ...Index forms with an explicit index: the slot of a clear value the
// hardware clears to without a shader. Vulkan clears to any value, so every registration lands in slot 0.
NVN_API(nvnDeviceRegisterFastClearColorIndex) { zero(c); }
NVN_API(nvnDeviceRegisterFastClearColoriIndex) { zero(c); }
NVN_API(nvnDeviceRegisterFastClearColoruiIndex) { zero(c); }
NVN_API(nvnDeviceRegisterFastClearDepthIndex) { zero(c); }
NVN_API(nvnDeviceRegisterFastClearStencil) { zero(c); }
NVN_API(nvnDeviceRegisterFastClearStencilIndex) { zero(c); }

// ---- queue: ray tracing memory and the primary queue mark
NVN_API(nvnQueueGetTotalRayTracingMemoryUsed) { zero(c); }
NVN_API(nvnQueueBuilderSetRayTracingMemorySize) { accept(c); }
NVN_API(nvnQueueBuilderGetRayTracingMemorySize) { zero(c); }
NVN_API(nvnQueueBuilderSetPrimaryQueue) { accept(c); }
NVN_API(nvnQueueBuilderGetPrimaryQueue) { zero(c); }

// ---- window: acquire timeout, colour space, HDR display and content metadata
NVN_API(nvnWindowBuilderSetAcquireTimeout) { accept(c); }
NVN_API(nvnWindowBuilderGetAcquireTimeout) { zero(c); }
NVN_API(nvnWindowBuilderSetColorSpace) { accept(c); }
NVN_API(nvnWindowBuilderGetColorSpace) { zero(c); }
NVN_API(nvnWindowBuilderSetHdrDisplayMetadata) { accept(c); }
NVN_API(nvnWindowBuilderGetHdrDisplayMetadata) { zero(c); }
NVN_API(nvnWindowBuilderSetHdrContentMetadata) { accept(c); }
NVN_API(nvnWindowBuilderGetHdrContentMetadata) { zero(c); }
NVN_API(nvnWindowSetAcquireTimeout) { accept(c); }
NVN_API(nvnWindowGetAcquireTimeout) { zero(c); }
NVN_API(nvnWindowSetHdrDisplayMetadata) { accept(c); }
NVN_API(nvnWindowGetHdrDisplayMetadata) { zero(c); }
NVN_API(nvnWindowSetHdrContentMetadata) { accept(c); }
NVN_API(nvnWindowGetHdrContentMetadata) { zero(c); }

// ---- program builder (NVN2)
typedef struct { uint64_t device; int32_t sample_shading, pad; } prog_builder_t;
#define PB(c) ((prog_builder_t *)arg_p(c, 0))
NVN_API(nvnProgramBuilderSetDefaults) { prog_builder_t *b = PB(c); b->device = 0; b->sample_shading = 0; }
NVN_API(nvnProgramBuilderSetDevice) { PB(c)->device = arg_x(c, 1); }
NVN_API(nvnProgramBuilderGetDevice) { ret_x(c, PB(c)->device); }
NVN_API(nvnProgramBuilderSetSampleShading) { PB(c)->sample_shading = (int32_t)arg_x(c, 1); }
NVN_API(nvnProgramBuilderGetSampleShading) { ret_x(c, (uint32_t)PB(c)->sample_shading); }
// void nvnProgramBuilderSetDebugDataHashes(b, const void *hashes, int count): identifies shader debug data for tools
NVN_API(nvnProgramBuilderSetDebugDataHashes) { accept(c); }
