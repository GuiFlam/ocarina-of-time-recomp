// From metroid-prime-4-recomp runtime/vk_crumbs.c @ 586e91e1b6f38c9e512e39f2d47a046fdf273cf0 (MIT, Copyright (c) 2026 Jeff Aigner), copied with mp4 renamed to oot.
// From fe3h-pc-recomp runtime/vk_crumbs.c @ 47ef99126793dbd13e4a475feb910a13ef6639b8 (MIT, Copyright (c) 2026 Jeff Aigner), copied with fe3h renamed to mp4 only.
// From totk-pc-recomp runtime/vk_crumbs.c @ c944f9f94520874f2a669add30dd08d29ee9f7b5 (MIT, Copyright (c) 2026 Valiantsin Dzerakh), copied unchanged.
// GPU breadcrumbs for a lost device (vk.h vk_crumb). VK_AMD_buffer_marker writes a 32-bit code into a buffer from
// the command stream: at TOP_OF_PIPE once the command processor reaches the marker, at BOTTOM_OF_PIPE once every
// command before it has finished. Each crumb writes both, into two words of one host-coherent buffer, so after a
// device loss word 0 names the newest point the GPU reached and word 1 the newest point it had finished: the hang lies
// between them. A code is the crumb kind in the top 4 bits and its number in the low 28. The markers cost two small
// writes per crumb, and crumbs mark submissions, upload batches, ASTC chunks and presents, not draws.
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cfg.h"
#include "host.h"
#include "vk.h"

static PFN_vkCmdWriteBufferMarkerAMD write_marker;
static PFN_vkCmdSetCheckpointNV set_checkpoint;          // VK_NV_device_diagnostic_checkpoints: the per-op crumbs there
static PFN_vkGetQueueCheckpointDataNV get_checkpoints;
static VkQueue crumb_queue;
static int nvidia;
static VkBuffer buf;
static VmaAllocation alloc;
static volatile uint32_t *words;

void vk_crumbs_init(VkCtx *c) {
    nvidia = c->props.vendorID == 0x10de;
    if (c->has_nv_checkpoints) {
        set_checkpoint = (PFN_vkCmdSetCheckpointNV)vkGetDeviceProcAddr(c->device, "vkCmdSetCheckpointNV");
        get_checkpoints = (PFN_vkGetQueueCheckpointDataNV)vkGetDeviceProcAddr(c->device, "vkGetQueueCheckpointDataNV");
        crumb_queue = c->gfx_queue;
    }
    if (!c->has_buffer_marker) return;
    write_marker = (PFN_vkCmdWriteBufferMarkerAMD)vkGetDeviceProcAddr(c->device, "vkCmdWriteBufferMarkerAMD");
    VkBufferCreateInfo bi = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = 16,
                             .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT};
    VmaAllocationCreateInfo ai = {.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
                                  .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                                  .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
    VmaAllocationInfo info;
    if (!write_marker || vmaCreateBuffer(c->vma, &bi, &ai, &buf, &alloc, &info) != VK_SUCCESS) {
        rt_log("vk: GPU breadcrumbs unavailable");
        write_marker = NULL;
        return;
    }
    words = info.pMappedData;
    words[0] = words[1] = 0;
    rt_log("vk: GPU breadcrumbs on (VK_AMD_buffer_marker)");
}

void vk_crumbs_release(void) {
    VkCtx *c = vk_ctx();
    if (buf && c) vmaDestroyBuffer(c->vma, buf, alloc);
    buf = VK_NULL_HANDLE, alloc = NULL, words = NULL, write_marker = NULL;
}

// OOT_CRUMBS=ops: a crumb at every pass begin and end and before every draw and dispatch outside a pass, each with a
// line saying what it was, so a device loss names where the GPU was (for finding a fault, not for play). A buffer
// marker between draws loses the device on NVIDIA (RTX 4070, driver 1.4.351: an "invalid write at 0" at the first
// frame, inside a pass or not), and so does a checkpoint (VK_NV_device_diagnostic_checkpoints, which is tried first
// where it exists), so the option is refused there; elsewhere it is untested.
#define OP_RING 1024
static int ops_on = -1;
static uint64_t op_serial;
static char op_desc[OP_RING][160];

int vk_crumbs_ops(void) {
    if (ops_on < 0) {
        const char *e = cfg_get("OOT_CRUMBS");
        ops_on = e && !strcmp(e, "ops") && (set_checkpoint != NULL || write_marker != NULL);
        if (ops_on && nvidia) {   // measured on an RTX 4070: the device is lost at the first frame, markers or checkpoints
            rt_log("vk: OOT_CRUMBS=ops is off: a crumb between draws loses the device on NVIDIA (markers and checkpoints alike)");
            ops_on = 0;
        }
        if (ops_on) rt_log("vk: OOT_CRUMBS=ops: a %s at every pass boundary and before draws and dispatches outside a pass",
                           set_checkpoint ? "checkpoint" : "breadcrumb");
    }
    return ops_on;
}

void vk_crumb_op(VkCommandBuffer cb, const char *desc) {
    uint64_t n = ++op_serial;
    snprintf(op_desc[n % OP_RING], sizeof op_desc[0], "%s", desc);
    if (set_checkpoint) set_checkpoint(cb, (const void *)(uintptr_t)n);   // the marker is the serial, opaque to the driver
    else vk_crumb(cb, VK_CRUMB_OP, n);
}

// After a device loss: the newest checkpoints the queue reached, per pipeline stage, and what they were.
static void checkpoints_report(void) {
    if (!get_checkpoints || !crumb_queue) return;
    uint32_t n = 0;
    get_checkpoints(crumb_queue, &n, NULL);
    if (!n) { rt_log("vk: checkpoints: none reached"); return; }
    VkCheckpointDataNV d[16];
    if (n > 16) n = 16;
    for (uint32_t i = 0; i < n; i++) d[i] = (VkCheckpointDataNV){.sType = VK_STRUCTURE_TYPE_CHECKPOINT_DATA_NV};
    get_checkpoints(crumb_queue, &n, d);
    for (uint32_t i = 0; i < n; i++) {
        uint64_t k = (uint64_t)(uintptr_t)d[i].pCheckpointMarker;
        rt_log("vk: checkpoint reached at stage %#x: op %llu (%s)", (unsigned)d[i].stage, (unsigned long long)k,
               k && op_serial - k < OP_RING ? op_desc[k % OP_RING] : "description no longer kept");
    }
}

void vk_crumb(VkCommandBuffer cb, int kind, uint64_t n) {
    if (!write_marker || !cb) return;
    uint32_t code = (uint32_t)kind << 28 | (uint32_t)(n & 0x0fffffff);
    write_marker(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, buf, 0, code);
    write_marker(cb, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, buf, 4, code);
}

static void describe(uint32_t code, char *o, size_t size) {
    static const char *const kinds[] = {"nothing", "the start of a part of submission", "the end of submission",
                                        "the start of upload batch", "the end of upload batch",
                                        "ASTC decode chunk", "the start of present",
                                        "the end of present"};
    unsigned k = code >> 28;
    if (!code) snprintf(o, size, "nothing yet");
    else if (k == VK_CRUMB_OP) {
        uint32_t n = code & 0x0fffffff;
        snprintf(o, size, "op %u (%s)", n, op_serial - n < OP_RING ? op_desc[n % OP_RING] : "description no longer kept");
    } else if (k < sizeof kinds / sizeof *kinds) snprintf(o, size, "%s %u", kinds[k], code & 0x0fffffff);
    else snprintf(o, size, "unknown code %#x", code);
}

void vk_crumbs_report(void) {
    checkpoints_report();
    if (!words) return;
    char reached[256], finished[256];
    describe(words[0], reached, sizeof reached);
    describe(words[1], finished, sizeof finished);
    rt_log("vk: GPU breadcrumbs: the GPU last reached %s; everything before %s had finished", reached, finished);
}
