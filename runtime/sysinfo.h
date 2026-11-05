// From metroid-prime-4-recomp runtime/sysinfo.h @ 586e91e1b6f38c9e512e39f2d47a046fdf273cf0 (MIT, Copyright (c) 2026 Jeff Aigner), copied with mp4 renamed to oot.
// From fe3h-pc-recomp runtime/sysinfo.h @ 47ef99126793dbd13e4a475feb910a13ef6639b8 (MIT, Copyright (c) 2026 Jeff Aigner), copied with fe3h renamed to mp4 only.
// From totk-pc-recomp runtime/sysinfo.h @ c944f9f94520874f2a669add30dd08d29ee9f7b5 (MIT, Copyright (c) 2026 Valiantsin Dzerakh), copied with totk renamed to fe3h only.
// What a bug report needs about the machine (runtime/sysinfo.c), for the menu's "Copy system info" and "Open log folder".
#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Plain text, one item per line: OS and build, CPU, RAM, the GPU and its driver as the Vulkan device reports them, and
// every setting with a value (oot.ini, the menu, the environment). Technical text for the developers, in English like
// the log. The length written (truncated to n - 1), or 0 on failure.
int sysinfo_text(char *buf, size_t n);
// The CPU's brand string ("AMD Ryzen 7 7800X3D 8-Core Processor"); "" when the CPU has none.
void sysinfo_cpu_name(char *out, size_t n);
// The folder with the log: the folder of logs\oot.log when this run writes it, else of the file stderr goes to, else
// of oot.exe. 0 on failure.
int sysinfo_log_folder(char *out, size_t n);

#ifdef __cplusplus
}
#endif
