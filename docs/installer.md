<!-- From metroid-prime-4-recomp docs/installer.md @ 586e91e1b6f38c9e512e39f2d47a046fdf273cf0 (MIT, Copyright (c) 2026 Jeff Aigner), copied with mp4 renamed to oot. -->
<!-- From fe3h-pc-recomp docs/installer.md @ 47ef99126793dbd13e4a475feb910a13ef6639b8 (MIT, Copyright (c) 2026 Jeff Aigner), renamed to mp4; MP4 changes are noted where made. -->
<!-- From totk-pc-recomp docs/installer.md @ c944f9f94520874f2a669add30dd08d29ee9f7b5 (MIT, Copyright (c) 2026 Valiantsin Dzerakh), adapted to MP4. -->
# Installer

A public release that turns the player's own game files into a playable OOT folder. The release carries no game
data and no code derived from the game: the installer builds the game on the player's machine. The installed folder
holds only the game; the installer and its tools stay outside it.

The installer is totk-pc-recomp's (`installer/`, MIT), renamed and changed where this game differs: one supported
update line, our extraction tools, no RomFS patches, and a shader list of our own (see "Shader list").

## What the player does

1. Downloads `OOT-Setup-<version>-windows.zip` from GitHub Releases, unpacks it anywhere and starts `OOT-Setup.exe`.
2. Picks:
   - the base game (XCI or NSP) and the 1.1.0 update (NSP), the last Switch 1 update;
   - their `prod.keys`;
   - the game language and the install folder.
3. Waits for the setup, one time. The window shows each step and its progress.
4. Plays `oot.exe` from the install folder. The installer and the source files can be deleted.

The install folder (`main.c` `portable_paths()` reads this layout; it stays portable):

```
<install folder>/
  oot.exe, SDL3.dll, libopus-0.dll, oot_xlate.exe
  openh264-2.7.0-win64.dll       Cisco's OpenH264 binary, downloaded from Cisco by the setup (unless the player says no),
  openh264-BINARY_LICENSE.txt    with Cisco's licence for it
  oot.ini                       language; the other keys as commented defaults (oot.ini.example)
  exefs/, romfs/                 extracted from the player's files
  profile/                       saves
  logs/                          oot.log of the last run, oot.1.log to oot.4.log of the runs before it
  shader-cache/                  the prepared translations (shader-cache.pack) and those made in play since
  shader-prepared.bin            the variants the setup prepared, which every start rebuilds into the driver's cache
  shader-answers.bin             the answers met in play
  oot-install.json              what built this folder: release version, input hashes, finished steps
```

## Updating the port

The player runs a newer installer on the same folder. It reads `oot-install.json`, keeps `romfs/`, `exefs/`,
`profile/` and `oot.ini` after checking their hashes, rebuilds `oot.exe` and reinstalls the translator. A newer
release that changes nothing game-derived skips the rebuild.

## Setup steps

1. **Check** before writing anything: the files open with the keys, the base is 1.0.0 and the update 1.1.0 (by the
   hashes of their code modules, `tools/data/expected-v*.json`), the keys are new enough for the update, enough space
   is free on the target disk (about 61 GiB at the peak of a first install, 35 GiB after it, most of it the 26 GiB RomFS), the CPU has AVX2
   (`-march=x86-64-v3`), a Vulkan 1.3 GPU exists. Each failure is one clear message.
2. **Extract** the ExeFS of the update and the RomFS (the base image with the update's BKTR patch, merged by hactool,
   which verifies every IVFC level) into the install folder and verify the hashes.
3. **Lift** the ExeFS to C (`recomp/gen.py`, `recomp/patches-v<version>.txt`) into a temp folder next to the installer.
4. **Compile** the generated C, `modules.c`, `romfs.c` and `patches.c` (they bake the per-install layout, RomFS hashes
   and the game version's patch page) and link them with the prebuilt runtime library into `oot.exe`
   (`build_game.py --runtime-lib`).
5. **Prepare**: install the shader translator (`oot_xlate.exe`) next to the game, then prepare the shaders the
   release's list names (`tools/prepare_shaders.py`: find each in the player's RomFS, `oot.exe --prepare-shaders`
   translates them and builds them into the driver's cache, about 2 minutes for 35,000 variants on 12 cores) and pack
   the cache into one file (`tools/pack_shader_cache.py`). Shaders the list lacks are translated as they are met in
   play. Download Cisco's OpenH264 binary for the cinematics, unless the player said no (see "Release layout").
6. **Clean up** the temp folder (generated C and objects).

A cancelled or failed setup resumes from the last finished step (`oot-install.json`).

## Command line

`installer/` (stdlib only; `installer/layout.py` maps the release folder and, in a repository, the dev toolchain):

```
python -m installer --base FILE --update FILE --keys FILE --lang CODE --dest DIR [--root DIR] [--temp DIR] [--verify]
                    [--openh264 yes|no]
```

- On a folder with `oot-install.json` only `--dest` is needed; the game files only when the extraction must run again.
- stdout: one JSON event per line, types `start`, `plan`, `step` (started, done, skipped), `progress` (step, phase,
  done/total/unit), `space` (bytes needed and free on the install disk and the temp disk), `warning`, `error`, `end`
  (`installer/events.py`). Messages are ids with parameters; `installer/text/en.json` holds their English text. The
  human-readable log goes to `<temp>/logs/`.
- Exit codes: 0 done, 1 failed, 2 bad command line, 3 cancelled. The window cancels with Ctrl+Break to the
  installer's process group.
- Each step records a key over its inputs (runner.py). A step with the recorded key and intact output is skipped, so
  resume and update are one rule; output kept from another release is re-hashed first.
- Peak space on the install disk: the extraction copies both Program NCAs (about 26.3 GiB) next to the merged RomFS
  (26.2 GiB) before it deletes them; the shader and pipeline caches reach about 5.5 GiB after the first area. The check computes the exact figure.
- Window (`python -m installer.gui`, `installer/gui.py` draws, `installer/guilogic.py` decides): form, step list with
  progress, space line, collapsible log, Cancel, finish screen with Open folder. English or Russian, chosen from the
  Windows display language, switchable in the window; every sentence is an id in `installer/text/<en|ru>.json`. A
  folder with `oot-install.json` needs only the folder. `--selftest [--shots DIR]` builds every screen in both
  languages and closes itself.
- `tests/installer_test.py`: bookkeeping with fake steps.
- Dev mode (a repository checkout, no `python/python.exe`): the Python that runs the module runs the tools, so use
  one with numpy, lz4 and cryptography (`.venv-win`); `toolchain/llvm-mingw-*` and `toolchain/deps-mingw` when they
  exist, else MSYS2 UCRT64's clang, ninja, headers, libraries and DLLs (the usual build's); the runtime library from
  `build/install-rtlib` (`tools/build_game.py --make-runtime-lib --out build/install-rtlib`); `toolchain/hactool/hactool.exe`
  (`tools/build_hactool.py` on Windows); `build/xlate/oot_xlate.exe`; the temp folder `build/install-temp`.

## Release layout

The unpacked release; `tools/make_release.py` assembles it and `--zip` packs it:

```
OOT-Setup.exe        starts python/pythonw.exe -m installer.gui (installer/launcher/launcher.c, no console)
installer/            the installer: steps, CLI, window, en/ru text catalog
python/               Python 3.14 embeddable + numpy, lz4, cryptography, cffi, pycparser (+ tkinter for the window)
recomp/, tools/       the lifter, the build, extraction and shader scripts the steps run (copied from the repo), tools/data/
data/shader-list.bin  the shader list (see "Shader list"); not in the repository
llvm/                 llvm-mingw, x86_64-only subset (Apache-2.0 with LLVM exception; mingw-w64 parts permissive)
runtime/              libootrt.a and the headers the generated code and modules.c/romfs.c/patches.c include
bin/                  oot_xlate.exe, hactool.exe, SDL3.dll, libopus-0.dll, ninja.exe (no openh264: see below)
THIRD_PARTY_NOTICES.md
oot.ini.example, VERSION  the default ini; the release string (the repository's VERSION plus the commit)
```

- SDL3 comes from the upstream release, not MSYS2: MSYS2's DLL imports the LGPL libiconv.
- openh264, the cinematics' H.264 decoder: Cisco's H.264 patent licence covers Cisco's own binary only when it is
  downloaded separately to the player's machine and the player controls its use. So the release carries only
  openh264's headers; the Prepare step downloads Cisco's `openh264-2.7.0-win64.dll` from ciscobinary.openh264.org,
  checks it against Cisco's published SHA-256 and puts it, with `openh264-BINARY_LICENSE.txt`, next to `oot.exe`. The
  window's checkbox (shown with "OpenH264 Video Codec provided by Cisco Systems, Inc.") and `--openh264 yes|no` decide
  whether it does; `openh264 = 0` in oot.ini turns it off later. Without it the game runs and the cinematics show no
  picture (`runtime/ov_movie.c` h264_lib). A failed download is a warning, and the next run of the setup retries.
- Every bundled third-party piece gets a row in `THIRD_PARTY_NOTICES.md`.
- The release contains nothing game-derived; `make_release.py` runs `tools/check_content.py` over the folder before it
  finishes.

## Constraints

- The installer steps contain no Win32 calls: Linux follows with its own compiler and the same steps.
- The keys file path is passed to the tools; key values never appear in the UI, logs or files.
- The window is a front end over the CLI, which prints one JSON progress event per line. Tests and CI use the CLI.
- Paths may hold any Unicode characters. Every exe the setup runs or builds reads its arguments and paths as UTF-8
  through an `activeCodePage` manifest: hactool (our patch), oot.exe (`runtime/oot.rc`), ninja (upstream).
- A game file whose data fails its hash checks or ends early stops the setup with `check.input_damaged`, which names
  the file and asks to copy it again (`tools/extract.py` `DamagedFile`).
- Unsigned exe: SmartScreen warns on first start.

## Later

- An icon for `oot.exe` and the setup (`runtime/oot.ico`; the launcher builds without one for now).
- A GitHub release workflow: a release is `make_release.py --zip` on a machine with uv and Python 3.14, a tag at the
  commit the zip names in its `VERSION`, and `gh release create` with the zip as `OOT-Setup-<version>-windows.zip`.
  NSZ and XCZ input. Linux release.

## Shader list

The game's shaders are Maxwell code in `SNVN` chunks: `MaterialArchive.arc` and every material (`MTRL`) and vector
graphic (`SVGM`) asset of the paks and the update's patch pak, about 260,000 programs (`tools/shader_inventory.py`). Each
is translated per variant (the texture types and vertex formats it is bound with), so translating every program up
front would cost hours and many gigabytes and still miss the variants the game asks for. The list names the variants
play met instead: a file in the layout of `shader-answers.bin` (stage, the runtime's code hash, the binding answers)
with the input ids zeroed. It holds no game code; `tools/prepare_shaders.py` finds each program in the player's own
RomFS by its hash. The list ships in the release (`data/shader-list.bin`) and stays out of the repository.

To make or extend it, play a build (the runtime records every variant in `shader-answers.bin` next to `oot.exe`) and
merge the files:

```
python tools/prepare_shaders.py --make-list build/shader-list.bin build/v131072/shader-answers.bin [more files]
```

`tools/make_release.py --shader-list` takes it (default `build/shader-list.bin`). On a fresh copy of the build, the list
of one play of the first area (34,812 variants of 2,826 programs) took the translations in play from 1,883 to 14 and the
draws that waited for a shader from 24,549 to 30, and shortened the longest frame of the save load from 2.4 to 1.6 s.
