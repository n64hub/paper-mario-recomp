# Paper Mario: Recompiled

> **Made with AI.** Almost all of the code, documentation and tests in this repo were written by AI agents
> (Claude), directed and play-tested by a person. None of it is offered to the upstream projects it builds on, which
> don't take AI-written code.

A native PC port of the N64 *Paper Mario* (USA), made by static recompilation
([N64Recomp](https://github.com/N64Recomp/N64Recomp) + [N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime) +
[RT64](https://github.com/rt64/rt64), with [RecompFrontend](https://github.com/N64Recomp/RecompFrontend)'s launcher
and menus): the game's own MIPS code translated to C and run natively, with RT64 drawing it. The symbols and the
patches' headers come from the [pmret/papermario](https://github.com/pmret/papermario) decompilation. It is not an
emulator.

The repo holds no ROM, no asset from the game and no data extracted from it: you supply your own ROM, and nothing in
git or CI is made from one except the symbol names and addresses in `syms/`.

**Status:** released for Windows and Linux (`releases/`). The desktop build boots through the logos and the intro
story to the title, plays the attract demo with its battles, and plays a new game from file select into the world,
with sound (the audio microcode recompiled), Flash saves, and keyboard and gamepad input. Every map of every area
plays (`tools/play_plan_maps.json` warps to each on the web page), and so does the ending's parade; the game hasn't
been played through from start to end yet. The Linux build has been booted on Mesa's software Vulkan only. In a
window wider than 4:3 the world and battles reach the window's edges, their backgrounds with them, and the menus and
the HUD stay in the middle 4:3 (Graphics: Aspect Ratio Expand by default, Original for the console's 4:3). RT64
draws at the window's resolution, with MSAA, and interpolates to the display's rate. The same game builds into a web
page that plays in Chrome and Firefox through WebGPU, at [n64hub.com/paper-mario](https://n64hub.com/paper-mario/)
(`docs/web.md`).

## The ROM

*Paper Mario* (USA), as a big-endian `.z64` of 40 MiB with SHA-1 `3837f44cda784b466c9a2d99df70d77c322b97a0`. Other
regions and revisions are not supported.

## Building

You need that ROM at `roms/papermario.us.z64`. On Windows: LLVM's clang-cl, CMake 3.31 and Ninja, and a clang
with a MIPS backend plus ld.lld for `patches/`. On Linux: `clang lld libsdl2-dev libgtk-3-dev libfreetype-dev`, CMake
3.31 and Ninja.

From the repo's root in a bash shell:

```sh
git submodule update --init --recursive
bash tools/recompile.sh -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl   # the game's code as C
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl \
      -DPATCHES_C_COMPILER=<MIPS clang> -DPATCHES_LD=<ld.lld>
ninja -C build PaperMarioRecompiled
```

With [ccache](https://ccache.dev) on `PATH` every compile goes through it, so a new build dir or another checkout
of the same code compiles only what changed since the last build on that computer (CMakeLists.txt says how).

On Linux the same lines take `clang` and `clang++` in place of `clang-cl`, and the system's `clang` and `ld.lld` for
`patches/`. `tools/recompile.sh` builds N64Recomp from the submodule the first time, then translates the game. Run
`build/PaperMarioRecompiled` (`.exe` on Windows) from the repo's root, pick the ROM with Load ROM, then Start Game.
`tools/release.sh`'s header says how a release is built.

`syms/` is written from the decomp's own build of your ROM: `tools/build_decomp.sh` builds it in Docker and brings
back its ELF, and `tools/gen_syms.sh` turns that into N64Recomp's symbols. Only a move of `lib/papermario` needs them
again.

## License

GPL-3.0 (`LICENSE`), as the recompilations it follows.
