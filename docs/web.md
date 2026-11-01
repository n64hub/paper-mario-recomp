# The web page

The game plays in a browser: built with Emscripten on N64Recomp-browser's `frontend/` into `PaperMarioRecompiled.html`
(the launcher, the recomp's menus, SDL2's audio and keyboard, the ROM from Load ROM, and the saves kept in IndexedDB;
`lib/N64Recomp-browser/frontend/CMakeLists.txt`'s header says what each part does), drawn through WebGPU. The page is
built from your ROM like the desktop program, so it is never built in CI and never published with the game in it.

What is the game's own stays here: the `EMSCRIPTEN` branches of `CMakeLists.txt` that make the game a page,
`tools/build_web.sh`, the prebuilt shader list (`src/rt64_shaders.txt`, recorded by `tools/shader_plan.json`; the
list's header says how to record it again), the play plans (`tools/play_plan.json`, `tools/play_plan_extended.json`)
and the survey they share (`tools/shader_survey.js`). The rest is N64Recomp-browser's: `docs/porting.md` (WebGPU and
the shaders, prebuilt shader lists, Emscripten), `docs/playing.md` (playing and measuring a page), `docs/forks.md` and
`docs/findings.md`.

## Build it

On Linux with Docker, with the ROM at `roms/papermario.us.z64`:

```bash
tools/build_web.sh                  # [cpus] [cmake arguments]
```

It builds `build/web/PaperMarioRecompiled.html` with its `.js`, `.wasm` (about 21 MB) and `.data` (about 9 MB: the
recomp's fonts and menus, nothing of the game) in `tools/docker/Dockerfile.web`, the image every N64Recomp-browser
page is built in, with ccache in `build/ccache-web`, so a rebuild after a patch takes a minute or two. The script's
header says each step. Three things differ from the desktop build:

- The patches library (`patches/`, built for MIPS) is recompiled by the native N64Recomp that `tools/recompile.sh`
  builds, not the runtime's, which Emscripten would build for the browser.
- RT64 builds its pipelines from `src/rt64_shaders.txt` (`RT64_PREBUILT_SHADER_LIST`), in sections: the boot's before
  the game starts, and an area's the first time it draws.
- The heap grows (`-sALLOW_MEMORY_GROWTH`, up to 4 GB): the effects' TLB pages lie about 1.5 GB past RDRAM in it
  (`src/game/recomp_api.cpp`), past the 1536 MB every page starts with, and the page aborts at boot without it.

Serve `build/web` over HTTP from localhost (any static server; the page brings its own `coi_serviceworker.js` for the
headers threads need) and open `PaperMarioRecompiled.html` in Chrome or Firefox with WebGPU, then Load ROM.

## The plans

A plan reaches the title the way a player does, by the keyboard's defaults (Enter is Start, Space A, WASD the stick):
Start through the logos and the intro story. From there `tools/shader_survey.js` takes the game where the plan needs
it as the game's own code does, with no patch: the attract demo or the intro story, as the title's wait does when it
runs out, and any map at any entrance once a game is under way, as an exit does (the map change mode written into
game mode slot 0, the map found by its name in the game's own table of areas), at a point in the story it sets
first. So a plan reaches any area with a new file, without a save, and goes as far whatever the browser. The survey's
header has each function and address; the waits in the world are on the game's own frame count
(`gGameStatus.frameCounter`, 30 a second).

`tools/play_plan.json` (about 3 minutes) boots, shoots the title, watches the attract demo's first scenes and its
first battle, starts a new file from file select, and walks Toad Town, its pause menu, and Koopa Village.
`tools/play_plan_extended.json` (about 5 minutes) goes on to a map of each later chapter's area. Play them with
N64Recomp-browser's `tools/play_page.mjs` (its `docs/playing.md`).

## The shader list

`src/rt64_shaders.txt` holds about 1670 shaders in 25 sections: the boot's, then the intro story, the attract demo, a
new file's start, the pause menu, and one an area, each built the first time the area draws. It is recorded by code
with `tools/shader_plan.json` (the list's header says what each section plays and how to record it again), and four
lines at the top of `new_game`, which file select and a new file's first map draw in a window wider than 4:3, were
added by hand from a play that missed them. A play that misses a shader prints it (`play_page.mjs` counts the
misses); add it to the section of the area it was missed in.

## How it plays

In Chrome, uncapped, the regular plan plays with no shader missed and no WebGPU error: a median of about 240 presents
a second in a 1256x645 window on a desktop GPU, and about 110 at 3840x2000 (the stand-in for a laptop's integrated
graphics), both on a busy host, RT64 interpolating the game's frames to the display's rate. Its gaps of up to about
half a second fall at map loads, where the game loads too. The page presents with Skip Buffering as the desktop build
does (`src/main/main.cpp` says why: Present Early showed the background alone between the world's frames).

Widescreen is the desktop's (`patches/widescreen.c`): the world, battles, a dark room's light and the pause menu's
blurred world reach the window's sides.

In Firefox the regular plan plays clean, no exception and no WebGPU error, at about 60 presents a second, in a
1256x645 window and at 3840x2000 both. Firefox builds a section of the shader list in line with the device, and
RT64's thread can wait 4-13 s for it in one list. The game steps its logic every other VI whatever its lists do, so
it went on to the demo's next map in such a wait, and the page trapped or stopped drawing in 5 plays of 5. The web
support sends no VI while the renderer is stalled like that (N64ModernRuntime's `ULTRAMODERN_VIS_WAIT_FOR_RENDERER`),
and the game waits with it.
