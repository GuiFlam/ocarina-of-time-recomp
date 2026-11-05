// From metroid-prime-4-recomp runtime/ui/binds_page.h @ 586e91e1b6f38c9e512e39f2d47a046fdf273cf0 (MIT, Copyright (c) 2026 Jeff Aigner), copied unchanged.
// From fe3h-pc-recomp runtime/ui/binds_page.h @ 47ef99126793dbd13e4a475feb910a13ef6639b8 (MIT, Copyright (c) 2026 Jeff Aigner), copied unchanged.
// From totk-pc-recomp runtime/ui/binds_page.h @ c944f9f94520874f2a669add30dd08d29ee9f7b5 (MIT, Copyright (c) 2026 Valiantsin Dzerakh), copied unchanged.
// The remapping rows of the menu's Controls tab (ui/binds_page.cpp). Present path only (ImGui).
#pragma once

// Inside the page's opt_begin / opt_end: every action's two keys and its controller button, remapped by selecting a
// cell and pressing the new one, and the reset of them all. binds_after_page, after opt_end: the window that waits for
// the press.
void binds_rows(float scale);
void binds_after_page(void);
// A cell waits for a press (or its press is still reaching the menu): B, Esc, L / R and the tab keys stay on the page.
bool binds_modal(void);
