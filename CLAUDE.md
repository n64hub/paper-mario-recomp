# CLAUDE.md

A static recompilation of the N64 *Super Smash Bros.* (USA) with N64Recomp, N64ModernRuntime, RecompFrontend and
RT64, from the Smash64RecompSyms symbols and the VetriTheRetri/ssb-decomp-re decompilation. Almost all of it is written
by AI agents, so this file is what the next one needs: the rules, where things are, and the facts about the game the
code can't say on its own.

## What lives where

This repo is the game: its recompilation, the patches over it, the desktop build and its web page.

- `syms/`: the symbols N64Recomp reads, Smash64RecompSyms' (CC0), never edited by hand: every section of the game
  with its ROM offset and VRAM, the functions named as the decomp names them.
- `us.toml`: N64Recomp's configuration, with the game's instruction patches and hooks; `tools/recompile.sh` writes
  `RecompiledFuncs/` with it, and the audio microcode with `n_aspMain.us.toml`.
- `patches/` and `patches.toml`: MIPS C against `lib/ssb-decomp-re`'s headers, recompiled into PatchesLib, where a
  patch to the game's C goes (none is needed yet). `ui_funcs.h`, `patch_helpers.h` and `recompui_event_structs.h` are
  RecompFrontend's, which includes the first from the game's `patches/`.
- `src/`: RecompFrontend's main (`src/main/main.cpp`, with the GameEntry), the code sections and patches it gives the
  runtime (`src/main/register_overlays.cpp`, `src/main/register_patches.cpp`), the natives `us.toml`'s hooks and the
  patches call (`src/game/recomp_api.cpp`), the libultra functions the runtime lacks (`src/game/libultra.cpp`), the
  settings, and on Windows a stack trace on a crash (`src/main/crash_trace.cpp`).
- `lib/`: `ssb-decomp-re` (the decomp, for the patches' headers), upstream `rt64`, `N64ModernRuntime` and
  `RecompFrontend` at the commits N64Recomp-browser's web forks are built on, and `N64Recomp-browser` at a release tag.
  The web support every recomp's page shares is developed and documented there; `tools/build_n64recomp.sh` and
  `CMakeLists.txt` apply its `patches/` to the other three, and `CMakeLists.txt` then this game's own changes to them
  (`runtime-patches/`, its README lists them).
- `docs/web.md`: the web page: `tools/build_web.sh`, its prebuilt shader list (`src/rt64_shaders.txt`,
  `tools/shader_plan.json`, `tools/record_shaders.sh`), its play plans (`tools/play_plan*.json`) and the survey they
  share (`tools/shader_survey.js`), which starts any stage as the menus do.
- Comments beside the code, for how and why: each file's header. Read the header of the file you change and update
  it in the same commit.

## Rules

- No ROMs, ROM-derived assets or extracted game data in git, CI or PRs (`tools/ci/check_repo.py` checks). Nothing
  from the decomp's assets or its build is committed. The launcher's art is text only (`icons/`): no logo, character
  or picture from the game.
- The desktop releases are built where the ROM is, by `tools/release.sh` (its header says how), and published to
  GitHub Releases with their notes in `releases/`.
- The scope is the game as it plays, plus what a web page needs (RT64 interpolation to the display's rate, resolution
  Auto), plus modern enhancements: each an option with an Original value that plays as the console does, none
  changing how the game plays. USA only.
- Code from any other Super Smash Bros. project may be read for what it changes, but nothing that isn't the decomp's
  or CC0 is copied.
- Write for a stranger, in the present tense: nothing about a contributor's machine, servers, paths or accounts, and
  nothing about where anyone keeps ROMs.
- GPL-3.0, as the recompilations it follows.

## How it fits together

- **The code.** `main` (`.main`, ROM 0x1060, VRAM 0x80000460) is stored raw in the ROM and the runtime maps what lies
  in ROM 0x1000 plus 1 MB at start. Every other code section is an overlay the game loads through `syDmaLoadOverlay`
  (the decomp's src/sys/dma.c), from an `SYOverlay` (ROM start and end, then where it loads). A hook at that
  function's start (`us.toml`) tells the runtime what is about to be copied (`ssb_load_overlay`,
  `src/game/recomp_api.cpp`): the scene manager and the overlays the scenes share at boot (`.scmanager`, `.ovl0`,
  `.ovl1`, `.ovl2`), then each scene's own as the scene manager starts it (src/sc/scmanager.c), many of them at the
  same VRAM.
- **The entry point** is 0x80000400 (`.entry`).
- **The game's anti-piracy checks** (the decomp's lbdef.h, `LBBACKUP_ERROR_*`) are answered as a console with the
  cartridge answers them, by instruction patches in `us.toml`: two read the RSP's IMEM and DMEM at boot, which the
  runtime doesn't map, and two run code they load from a data file, which is in no overlay. Each penalty they set is
  saved in SRAM, so a check left failing would follow the save.
- **The game's own bugs** that stop it are fixed by hooks in `us.toml` too: the character data screen's demo, which
  How to Play ends in, reads past a table and out of memory when the title never picked its two demo fighters (How
  to Play from the Data menu); a hook gives it two.
- **Hardware registers.** The runtime maps none, so a read of one crashes. The audio thread reads the AI's length
  register itself (`IO_READ(AI_LEN_REG)`, twice in `syAudioThreadMain`); `us.toml` turns each into osAiGetLength's
  answer. Any other read of an 0xA4xxxxxx address in `RecompiledFuncs/` would need the same.
- **Busy-waits.** The runtime runs one game thread at a time and switches only when the running one calls into it,
  where a console's interrupts preempt any thread. The game waits for its other threads in loops that only read a
  variable they set: the boot's wait for the audio thread (src/sc/scmanager.c), the opening movie's and title's waits
  for the scheduler's tic count, and the 1P game's and VS battles' waits for the music to stop. Each getter those
  loops call has a hook (`us.toml`) that lets the other threads run when it is called again within 100 us, which only
  a loop does (`ssb_spin_yield`). Without them the game hangs on a black screen at boot. A new hang with every other
  game thread waiting in `osRecvMesg` is another such loop.
- **Microcodes.** Graphics: F3DEX2, which RT64 knows. Audio: `n_aspMain`, recompiled by RSPRecomp from
  `n_aspMain.us.toml` (its text is at ROM 0x3A2C0; its jumps through registers are listed there). librecomp's vector
  unit uses SSE4.1, so `RecompiledFuncs` builds with `-march=nehalem` on x86-64.
- **Saves.** SRAM. `main.cpp` declares `SaveType::Sram`; the runtime keeps it in `saves/smashbrothers.n64.us.1.0.bin`
  and writes it a moment after the game saves.
- **Threads** (`main.cpp` names them): 1 idle, 3 the scheduler, 4 audio, 5 the game (the scene manager), 6 the
  controllers, 8 the debug thread, which shows the game's crash screen if no frame finishes for 300 retraces.
- **Input.** RecompFrontend's default bindings. Keyboard: WASD the stick, Space A, Left Shift B, Q Z, R R, E L, Enter
  Start, the arrows the C buttons. Until players are assigned on the controls page, each pad plays a port of its own
  in the order the pads were connected and the keyboard plays port 1 (`RECOMPINPUT_PADS_IN_ORDER`,
  RecompFrontend-web's option, on in `CMakeLists.txt`), and the game is told a controller is in port 1 and in each port with a pad
  (`get_connected_device_info`, `main.cpp`), each with a Rumble Pak, which the game starts a motor on only when one is
  there. VS mode's character select shows HMN on all four ports, as on a console:
  a port without a pad can't move its hand, and the panel's HMN button sets it to CP or NA.
- **Widescreen.** RecompFrontend's Graphics tab defaults to Aspect Ratio Expand, HUD Placement 16:9, Resolution Auto
  and Framerate Display. RT64 widens the 3D, and the stages' backgrounds, which the game draws as part of the 3D, reach
  the window's edges with it, so no patch is needed: the menus and the HUD stay in the middle 4:3.
- **Testing.** `--boot` starts the game from the launcher's first frame once the ROM is stored. A test that posts
  `WM_KEYDOWN`/`WM_KEYUP` to the window drives port 1; another port needs a pad, real or virtual (SDL's
  `SDL_JoystickAttachVirtual` makes one recompinput takes as a pad).
