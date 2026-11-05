// From metroid-prime-4-recomp runtime/ov.h @ 586e91e1b6f38c9e512e39f2d47a046fdf273cf0 (MIT, Copyright (c) 2026 Jeff Aigner), copied with mp4 renamed to oot.
// From fe3h-pc-recomp runtime/ov.h @ 47ef99126793dbd13e4a475feb910a13ef6639b8 (MIT, Copyright (c) 2026 Jeff Aigner), copied with fe3h renamed to mp4; declares ov_oe_perf_config.
// From totk-pc-recomp runtime/ov.h @ c944f9f94520874f2a669add30dd08d29ee9f7b5 (MIT, Copyright (c) 2026 Valiantsin Dzerakh), copied with totk renamed to fe3h only.
// Shared by the system overrides: runtime/ov_system.c, ov_input.c, ov_media.c and ov_gfx.c, and their probe.
// oe_push_message, oe_set_focus and ov_nv_state are in gfx.h (the window posts focus messages, the NVN layer
// allocates through the recorded callbacks).
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "gfx.h"

// ---- ov_system.c
extern uint32_t ov_oe_perf_config[2];     // SetPerformanceConfiguration's last request per PerformanceMode (ov_system.c)
int ov_handheld(void);                     // OOT_OPERATION_MODE=handheld (default docked); fatal on another value
const char *ov_language(void);             // the game's language, a Switch language code ("en-US", "zh-Hans", ...)
const char *ov_shared_font_dir(char *buf, size_t n);   // OOT_SHARED_FONTS, else <romfs>/../../firmware/shared-fonts
uint64_t ov_bfttf_size(const uint8_t *p, uint64_t len); // decoded font size of a BFTTF image, 0 if it is not one
