// From metroid-prime-4-recomp runtime/nvn_tex.h @ 586e91e1b6f38c9e512e39f2d47a046fdf273cf0 (MIT, Copyright (c) 2026 Jeff Aigner), copied unchanged.
// From fe3h-pc-recomp runtime/nvn_tex.h @ 47ef99126793dbd13e4a475feb910a13ef6639b8 (MIT, Copyright (c) 2026 Jeff Aigner), copied unchanged.
// From totk-pc-recomp runtime/nvn_tex.h @ c944f9f94520874f2a669add30dd08d29ee9f7b5 (MIT, Copyright (c) 2026 Valiantsin Dzerakh), copied unchanged.
// NVN texture storage layout (Tegra X1 block-linear), see nvn_tex.c.
// Ported from MacNativeGames games/totk/third_party/d2r_gfx/gfx/nvn/nvn_tex.h @ b75be8cf (zlib, deko3d-derived; see nvn_tex.c).
// nvn_texdesc and nvn_texlayout are defined in nvn_obj.h.
#pragma once
#include <stdint.h>
#include "nvn_obj.h"

void nvn_tex_layout(const nvn_texdesc *d, nvn_texlayout *o);
uint64_t nvn_tex_level_offset(const nvn_texdesc *d, unsigned level);
int nvn_fmt_bpb(int format, int *bw, int *bh);   // Bytes per block (0 = unknown format), block width and height
// Block height / block depth actually used by level `level` (log2 of the GOB count): the per-level block height shrinks with that level's row count (block rows), and deswizzling uses it level by level
void nvn_tex_level_tiles(const nvn_texdesc *d, unsigned level, unsigned *tileH, unsigned *tileD);

/* LINEAR_RENDER_TARGET (0x40) uses pitch storage even without LINEAR (0x10).
   TOTK's CPU-loaded water height/mask textures use that flag alone. */
static inline int nvn_tex_is_linear(const nvn_texdesc *d) { return (d->flags & 0x50) != 0; }
static inline uint32_t nvn_tex_linear_stride(const nvn_texdesc *d) {
    if (d->stride) return d->stride;
    int bw, bh; int bpb = nvn_fmt_bpb(d->format, &bw, &bh);
    uint32_t align = (d->flags & 0x40) ? 128 : 32;
    return (((d->width + bw - 1) / bw) * bpb + align - 1) & ~(align - 1);
}
