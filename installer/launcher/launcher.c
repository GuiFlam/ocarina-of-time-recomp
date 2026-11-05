// From metroid-prime-4-recomp installer/launcher/launcher.c @ 586e91e1b6f38c9e512e39f2d47a046fdf273cf0 (MIT, Copyright (c) 2026 Jeff Aigner), copied with mp4 renamed to oot.
// From fe3h-pc-recomp installer/launcher/launcher.c @ 47ef99126793dbd13e4a475feb910a13ef6639b8 (MIT, Copyright (c) 2026 Jeff Aigner), renamed to mp4; MP4 changes are noted where made.
/* From totk-pc-recomp installer/launcher/launcher.c @ c944f9f94520874f2a669add30dd08d29ee9f7b5 (MIT, Copyright (c) 2026 Valiantsin Dzerakh), renamed to mp4. */
/* OOT-Setup.exe: starts python\pythonw.exe -m installer.gui with this exe's folder as the working directory.

   No console window. The two message strings come from the catalog (installer/text/<lang>.json, ids launcher.title and
   launcher.python_missing); tools/make_release.py writes them into launcher_text.h before it compiles this file.
   Build: llvm/bin/llvm-windres -I runtime -O coff launcher.rc -o launcher_rc.o, then
   llvm/bin/clang -O2 -mwindows launcher.c launcher_rc.o -o OOT-Setup.exe */
#include <windows.h>
#include <wchar.h>

#include "launcher_text.h"

#define LANGID_RUSSIAN 0x19
#define PATH_CHARS (MAX_PATH * 2)

static void fail(void)
{
    int russian = (GetUserDefaultUILanguage() & 0x3ff) == LANGID_RUSSIAN;
    MessageBoxW(NULL, russian ? LAUNCHER_PYTHON_MISSING_RU : LAUNCHER_PYTHON_MISSING_EN,
                russian ? LAUNCHER_TITLE_RU : LAUNCHER_TITLE_EN, MB_OK | MB_ICONERROR);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command_line, int show)
{
    wchar_t folder[PATH_CHARS], python[PATH_CHARS], command[PATH_CHARS + 64];
    DWORD n = GetModuleFileNameW(NULL, folder, PATH_CHARS);
    STARTUPINFOW si = { sizeof si };
    PROCESS_INFORMATION pi;
    (void)instance; (void)previous; (void)command_line; (void)show;
    if (n == 0 || n >= PATH_CHARS) { fail(); return 1; }
    while (n > 0 && folder[n - 1] != L'\\') n--;
    folder[n > 0 ? n - 1 : 0] = 0;
    if (swprintf(python, PATH_CHARS, L"%ls\\python\\pythonw.exe", folder) < 0 ||
        GetFileAttributesW(python) == INVALID_FILE_ATTRIBUTES) { fail(); return 1; }
    if (swprintf(command, PATH_CHARS + 64, L"\"%ls\" -m installer.gui", python) < 0) { fail(); return 1; }
    if (!CreateProcessW(python, command, NULL, NULL, FALSE, 0, NULL, folder, &si, &pi)) { fail(); return 1; }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}
