// From metroid-prime-4-recomp runtime/game_patch_stubs.c @ 586e91e1b6f38c9e512e39f2d47a046fdf273cf0 (MIT, Copyright (c) 2026 Jeff Aigner), copied unchanged.
// From fe3h-pc-recomp runtime/game_patch_stubs.c @ 47ef99126793dbd13e4a475feb910a13ef6639b8 (MIT, Copyright (c) 2026 Jeff Aigner), copied; the frame step part moved to runtime/frame_rate.c.
// Stand-ins for totk-pc-recomp runtime/resolution.c, which holds the host half of TotK's resolution patch set and was
// not taken over (this game's frame step is runtime/frame_rate.c). The rest of the runtime still calls its interface
// (host.h); these definitions keep the game at its own resolution:
//
// - resolution: no extra heap, no grown HeapDef table and no wide UI overlay; the output and scene sizes the overlay
//   shows are a placeholder until FE3H's own sizes are measured.
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

#include "host.h"

// ---------- Resolution

uint64_t res_heap_extra_kib(void) { return 0; }

uint64_t res_heap_extra_bytes(void) { return 0; }

int res_heapdef_build(const void *in, size_t n, void **out, size_t *out_n) {
    (void)in, (void)n, (void)out, (void)out_n;
    return 0;   // never reached: romfs.c heapdef_overlay() returns first when res_heap_extra_kib() is 0
}

void res_heapdef_served(void) {}

void res_init(void) {}

int res_is_16x9(unsigned w, unsigned h) {
    // Within 1% of 16:9, the test resolution.c uses.
    double r = h ? (double)w / h : 0, want = 16.0 / 9.0;
    return r > want * 0.99 && r < want * 1.01;
}

int res_wants_wide_overlay(const unsigned out[2]) {
    (void)out;
    return 0;
}

void res_wanted(unsigned out[2]) { out[0] = 1920, out[1] = 1080; }

void res_sizes(unsigned out[2], unsigned scene[2]) {
    out[0] = scene[0] = 1920;
    out[1] = scene[1] = 1080;
}
