// From metroid-prime-4-recomp runtime/vk_vma.cpp @ 586e91e1b6f38c9e512e39f2d47a046fdf273cf0 (MIT, Copyright (c) 2026 Jeff Aigner), copied unchanged.
// From fe3h-pc-recomp runtime/vk_vma.cpp @ 47ef99126793dbd13e4a475feb910a13ef6639b8 (MIT, Copyright (c) 2026 Jeff Aigner), copied unchanged.
// From totk-pc-recomp runtime/vk_vma.cpp @ c944f9f94520874f2a669add30dd08d29ee9f7b5 (MIT, Copyright (c) 2026 Valiantsin Dzerakh), copied unchanged.
// Vulkan Memory Allocator implementation (AMD, MIT; from the MSYS2 package vulkan-memory-allocator, not copied).
// VMA is C++ only, so this is the one C++ file of the runtime; the rest uses the C interface of vk_mem_alloc.h.
#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 1
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include <vulkan/vulkan.h>
#include "vk_mem_alloc.h"
