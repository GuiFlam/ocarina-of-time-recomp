// From metroid-prime-4-recomp runtime/platform/fp_x86.h @ 586e91e1b6f38c9e512e39f2d47a046fdf273cf0 (MIT, Copyright (c) 2026 Jeff Aigner), copied unchanged.
// From fe3h-pc-recomp runtime/platform/fp_x86.h @ 47ef99126793dbd13e4a475feb910a13ef6639b8 (MIT, Copyright (c) 2026 Jeff Aigner), copied unchanged.
// From totk-pc-recomp runtime/platform/fp_x86.h @ c944f9f94520874f2a669add30dd08d29ee9f7b5 (MIT, Copyright (c) 2026 Valiantsin Dzerakh), copied unchanged.
// FP control on x86-64: MXCSR (rounding control bits 13-14, FTZ bit 15, DAZ bit 6, exception flags and masks).
// Inline on purpose: lifted FP code (simd.h) reads and writes it on every operation that depends on the guest FPCR.
#pragma once
#include <stdint.h>
#include <xmmintrin.h>

#define PLAT_FP_RC_MASK 0x6000u   // rounding control: 0 nearest, 0x2000 down, 0x4000 up, 0x6000 toward zero
#define PLAT_FP_FTZ 0x8000u
#define PLAT_FP_DAZ 0x0040u

static inline uint32_t plat_fp_get(void) { return _mm_getcsr(); }
static inline void plat_fp_set(uint32_t v) { _mm_setcsr(v); }
