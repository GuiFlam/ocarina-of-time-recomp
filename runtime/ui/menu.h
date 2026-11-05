// From metroid-prime-4-recomp runtime/ui/menu.h @ 586e91e1b6f38c9e512e39f2d47a046fdf273cf0 (MIT, Copyright (c) 2026 Jeff Aigner), copied with mp4 renamed to oot.
// From fe3h-pc-recomp runtime/ui/menu.h @ 47ef99126793dbd13e4a475feb910a13ef6639b8 (MIT, Copyright (c) 2026 Jeff Aigner), copied with fe3h renamed to mp4 only.
// From totk-pc-recomp runtime/ui/menu.h @ c944f9f94520874f2a669add30dd08d29ee9f7b5 (MIT, Copyright (c) 2026 Valiantsin Dzerakh), copied with totk renamed to fe3h only.
// The in-game menu (ui/menu.cpp, Dear ImGui) and its seams to the platform side: the window and its input (ui/menu_win.cpp
// on Windows; an SDL3 window gets its own adapter later), the gamepad (input_pad_read, input.h) and the present path
// (gfx_window.c). The menu core calls no window system: events come in through menu_event_*, the drawing happens in the
// present path's command buffer.
//
// F1 or Esc on the keyboard and Minus and Plus held together (consumed in poll_merged, input_menu_gate in input.h) open it;
// F1, Esc, B or "Continue" close it. While it is open the game gets no input (input_menu, input.h) and keeps running; the
// cursor is released from the mouse camera. Tabs Screen, Game, Sound, Controls and Menu (L / R or Q / E switch them) hold
// option rows (ui/option_rows.h) over the cfg settings; the bottom row has "Continue" and "Quit game".
// Test hooks (ui/menu.cpp): OOT_MENU_TEST=<frame>[:<frames per page>] opens the menu at that presented frame and saves
// one swapchain shot per tab into OOT_MENU_TEST_DIR; OOT_MENU_TEST_SET=<frame>:<key>=<value>[,...][;...] sets settings
// at given frames without input (SAVE=1 writes oot.ini).
//
// Threads: the event functions are called by the window thread, menu_toggle by whichever thread polls the pad, the
// overlay functions by the present path under its lock. ImGui itself runs on the present path only.
#pragma once
#include <stdint.h>
#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---- state, any thread
int menu_is_open(void);
void menu_toggle(void);                   // F1 / Esc on the keyboard, Minus+Plus on the pad
void menu_perf_cycle(void);               // F10: the performance overlay off, fps, full

// ---- events from the window adapter, used only while the menu is open. key: an ImGuiKey; coordinates in pixels of the
// window's client area, which is the swapchain image.
void menu_event_key(int key, int down);
void menu_event_mouse_pos(float x, float y);
void menu_event_mouse_button(int button, int down);   // 0 left, 1 right, 2 middle
void menu_event_wheel(float dx, float dy);
void menu_event_focus(int focused);

// ---- the overlay, drawn into the swapchain image after the game image (gfx_window.c, present lock held)
typedef struct {
    VkInstance instance;
    VkPhysicalDevice phys;
    VkDevice device;
    uint32_t queue_family;
    VkQueue queue;                        // submissions take vk_queue_lock
    VkFormat format;                      // of the swapchain image, a colour attachment
    uint32_t width, height;
    int image_w, image_h;                 // the game image this present shows (its crop)
    int scaler;                           // how the last present scaled it to the window: 0 not, 1 bilinear blit, 2 FSR 1
} MenuTarget;
// Lays out this frame's overlay; 1 when there is something to draw. frame: the presented frame (0 unknown). *shot: a
// path when the test hook wants this frame's swapchain image saved, else NULL. The first open starts ImGui's Vulkan
// back end and uploads the font. With the menu closed it costs two loads and a compare.
int menu_overlay_frame(uint64_t frame, const MenuTarget *t, const char **shot);
// Records the frame menu_overlay_frame laid out into cb, which is inside dynamic rendering on the image.
void menu_overlay_draw(VkCommandBuffer cb);
void menu_vk_release(void);               // before the device goes away (gfx_window_release_vk)

// ---- what the menu asks of the platform side (gfx_window.c)
void gfx_window_apply_settings(void);     // OOT_FULLSCREEN or OOT_WINDOW_SIZE changed: the window follows
void gfx_window_request_close(void);      // as the window's close button: the process ends
int gfx_window_monitor_size(int *w, int *h);   // pixels of the monitor the window is on; 0 when unknown
double gfx_window_refresh_hz(void);            // its refresh rate (OOT_FPS_LIMIT=display); 0 when unknown
unsigned gfx_window_dpi(void);                 // the window's DPI (96 at 100% display scale); any thread
// The present mode the swapchain got, as a OOT_VK_PRESENT value ("fifo" when mailbox is missing); "" before the first
// swapchain. Any thread.
const char *gfx_window_present_mode(void);

#ifdef __cplusplus
}
#endif
