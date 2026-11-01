# CLAUDE.md

A static recompilation of the N64 *Paper Mario* (USA) with N64Recomp, N64ModernRuntime, RecompFrontend and RT64, from
the pmret/papermario decompilation. Almost all of it is written by AI agents, so this file is what the next one needs:
the rules, where things are, and the facts about the game the code can't say on its own.

## What lives where

This repo is the game: its recompilation, the patches over it, the desktop build and its web page.

- `syms/`: the symbols N64Recomp reads, written from the decomp's own build of the ROM (`tools/build_decomp.sh`, then
  `tools/gen_syms.sh` and `tools/gen_syms.py`, whose header says how), never edited by hand: every code section of the
  game with its ROM offset and VRAM, the functions named as the decomp names them.
- `us.toml`: N64Recomp's configuration, with the game's hooks; `tools/recompile.sh` writes `RecompiledFuncs/` with it,
  and the audio microcode with `n_aspMain.us.toml`.
- `patches/` and `patches.toml`: MIPS C against `lib/papermario`'s headers, recompiled into PatchesLib, where a patch to
  the game's C goes (`patches/Makefile`'s header says how they build; `patches/dummy_headers/` stands in for the headers
  the decomp generates in its build). `ui_funcs.h`, `patch_helpers.h` and `recompui_event_structs.h` are
  RecompFrontend's, which includes the first from the game's `patches/`.
- `src/`: RecompFrontend's main (`src/main/main.cpp`, with the GameEntry), the code sections and patches it gives the
  runtime (`src/main/register_overlays.cpp`, `src/main/register_patches.cpp`), the natives `us.toml`'s hooks and the
  patches call (`src/game/recomp_api.cpp`), the libultra functions the runtime lacks (`src/game/libultra.cpp`), the
  settings, and on Windows a stack trace on a crash (`src/main/crash_trace.cpp`).
- `lib/`: `papermario` (the decomp, for the patches' headers and `syms/`), upstream `rt64`, `N64ModernRuntime` and
  `RecompFrontend` at the commits N64Recomp-browser's web forks are built on, and `N64Recomp-browser` at a release tag.
  The web support every recomp's page shares is developed and documented there; `tools/build_n64recomp.sh` and
  `CMakeLists.txt` apply its `patches/` to the other three, and `CMakeLists.txt` then this game's own changes to them
  (`runtime-patches/`, its README lists them).
- `docs/web.md`: the web page: `tools/build_web.sh`, its prebuilt shader list (`src/rt64_shaders.txt`,
  `tools/shader_plan.json`, `tools/record_shaders.sh`), its play plans (`tools/play_plan*.json`) and the survey they
  share (`tools/shader_survey.js`), which takes the game to any map or the attract demo as its own code does.
- Comments beside the code, for how and why: each file's header. Read the header of the file you change and update
  it in the same commit.

## Rules

- No ROMs, ROM-derived assets or extracted game data in git, CI or PRs (`tools/ci/check_repo.py` checks). Nothing
  from the decomp's assets or its build is committed but `syms/`. The launcher's art is text only (`icons/`): no logo,
  character or picture from the game.
- The desktop releases are built where the ROM is, by `tools/release.sh` (its header says how), and published to
  GitHub Releases with their notes in `releases/`.
- The scope is the game as it plays, plus what a web page needs (RT64 interpolation to the display's rate, resolution
  Auto), plus modern enhancements: each an option with an Original value that plays as the console does, none
  changing how the game plays. USA only.
- Code from any other Paper Mario project may be read for what it changes, but nothing that isn't the decomp's or CC0
  is copied.
- Write for a stranger, in the present tense: nothing about a contributor's machine, servers, paths or accounts, and
  nothing about where anyone keeps ROMs.
- GPL-3.0, as the recompilations it follows.

## How it fits together

- **The code.** `.main` (ROM 0x1000, VRAM 0x80025C00, the entry point) is stored raw in the ROM, and the runtime maps
  what lies in ROM 0x1000 plus 1 MB at start. Everything else is an overlay: the engine's second half, each area's maps,
  battles, partners, the effects, the file and pause menus. The game copies code out of ROM three ways (nuPiReadRom,
  dma_copy in 0x2000-byte pieces through it, and nuPiReadRomOverlay), and a hook at each (`us.toml`) tells the runtime
  what is about to be copied (`pm_load_overlays`, `src/main/register_overlays.cpp`). Many overlays share a VRAM.
- **The effects** run at 0xE0000000 and up behind the TLB: `src/game/recomp_api.cpp` backs that stretch with memory of
  its own and the TLB calls do nothing (its header says why that is the same mapping). On a page that stretch is about
  1.5 GB past RDRAM in the wasm heap, so the page's heap grows (`CMakeLists.txt`).
- **The anti-piracy.** Four functions are reached only through pointers computed from words on the cartridge's bus,
  which the runtime doesn't map; a hook at each (`us.toml`) calls the function a cartridge leads to.
- **Spins.** The runtime runs one game thread at a time and switches only when the running one calls into it, where a
  console's interrupts preempt any thread. NuSystem leaves two threads spinning once the game runs (boot_main's
  endless loop and boot_idle's wait for an idle function the game never sets), and nuGfxTaskAllEndWait spins on its
  task count: each pass yields (`us.toml`). Without them the game hangs on a black screen at boot. A new hang with
  every other game thread waiting in `osRecvMesg` is another such loop.
- **Microcodes.** Graphics: F3DEX2, which RT64 knows. Audio: `n_aspMain`, recompiled by RSPRecomp from
  `n_aspMain.us.toml`. librecomp's vector unit uses SSE4.1, so `RecompiledFuncs` builds with `-march=nehalem` on
  x86-64.
- **Saves.** 1 Mbit of Flash. `main.cpp` declares `SaveType::Flashram`; the runtime keeps it in
  `saves/papermario.n64.us.1.0.bin`. The file menu reads a file with no save yet at slot -1, page -128, which a
  console's Flash wraps to slot 7, as the runtime's does.
- **Threads** (`main.cpp` names them): 0 the PI manager or the VI manager (by priority), 1 idle, 2 the crash screen's,
  3 the game's main at priority 10 or else audio, 4 NuSystem's graphics thread, where the game runs on each retrace, 5
  its task manager, 6 the controllers, 17-19 the audio's scheduler threads.
- **Input.** RecompFrontend's default bindings. Keyboard: WASD the stick, Space A, Left Shift B, Q Z, R R, E L, Enter
  Start, the arrows the C buttons. The game reads port 1 only.
- **Widescreen** (`patches/widescreen.c`, its header says how). RecompFrontend's Graphics tab defaults to Aspect Ratio
  Expand, HUD Placement 16:9, Resolution Auto and Framerate Display. RT64 widens the 3D, but the game keeps the world
  inside its own frame, by its cameras' scissors and a background drawn only in the 4:3 picture: the patches open a
  camera whose view is the whole width to the window's sides and draw the background across, and a smaller view (the
  intro story's, the demo's framed picture) keeps the game's frame. A dark room's light and the pause menu's blurred
  world are widened there too. `patches/extended_gbi.c` turns on RT64's extended
  commands at the start of each of the game's display lists, since RT64 turns them off at each. Original (Aspect
  Ratio Original) is the console's 4:3.
- **Testing.** `--boot` starts the game from the launcher's first frame once the ROM is stored, `--window <w>x<h>`
  sizes the window, and `--input <file>` plays port 1 from a script of timed button states (`main.cpp` says the
  format), for unattended runs.
