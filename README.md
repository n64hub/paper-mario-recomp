# Super Smash Bros.: Recompiled

> **Made with AI.** Almost all of the code, documentation and tests in this repo were written by AI agents
> (Claude), directed and play-tested by a person. None of it is offered to the upstream projects it builds on, which
> don't take AI-written code.

A native PC port of the N64 *Super Smash Bros.* (USA), made by static recompilation
([N64Recomp](https://github.com/N64Recomp/N64Recomp) + [N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime) +
[RT64](https://github.com/rt64/rt64), with [RecompFrontend](https://github.com/N64Recomp/RecompFrontend)'s launcher
and menus): the game's own MIPS code translated to C and run natively, with RT64 drawing it. The symbols come from
[Smash64RecompSyms](https://github.com/Smash64Recomp/Smash64RecompSyms), the patches' headers from the
[ssb-decomp-re](https://github.com/VetriTheRetri/ssb-decomp-re) decompilation. It is not an emulator.

The repo holds no ROM, no asset from the game and no data extracted from it: you supply your own ROM, and nothing in
git or CI is made from one except the symbol names and addresses in `syms/`.

**Status:** the desktop build boots through the opening movie to the title and plays every mode: the 1P Game, Training
Mode, both Bonus Practices (Break the Targets, Board the Platforms) and VS battles on every stage, with sound (the audio
microcode recompiled), SRAM saves, and keyboard and gamepad input. Each pad plays a port of its own, in the order the
pads were connected, and the keyboard plays port 1, so up to four people play without setting anything up (the controls
page's Assign players still picks who plays which port), and every port has a Rumble Pak, which the game starts on a pad
as a console does. In a window wider than 4:3 the battles and the stages' backgrounds reach the window's edges, and the
menus and the HUD stay in the middle 4:3 (Graphics: Aspect Ratio Expand by default, Original for the console's 4:3).
RT64 draws at the window's resolution, with MSAA, and interpolates to the display's rate. The same game builds into a
web page that plays in Chrome and Firefox through WebGPU, at
[n64hub.com/super-smash-bros](https://n64hub.com/super-smash-bros/) (`docs/web.md`).

## The ROM

*Super Smash Bros.* (USA), as a big-endian `.z64` of 16 MiB with SHA-1 `e2929e10fccc0aa84e5776227e798abc07cedabf`.
Other regions and revisions are not supported.

## Playing

Download the newest release for Windows or Linux from
[Releases](https://github.com/n64hub/super-smash-bros-recomp/releases), unpack it and run `SmashBrosRecompiled` from
its folder; the release's notes say what each needs. Pick the ROM with Load ROM, then Start Game. Saves and settings
are kept in your user profile, so a newer release carries them over.

## Building

You need that ROM at `roms/smashbrothers.us.z64`. On Windows: LLVM's clang-cl, CMake 3.31 and Ninja, and a clang
with a MIPS backend plus ld.lld for `patches/`. On Linux: `clang lld libsdl2-dev libgtk-3-dev libfreetype-dev`, CMake
3.31 and Ninja.

From the repo's root in a bash shell:

```sh
git submodule update --init --recursive
bash tools/recompile.sh -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl   # the game's code as C
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl \
      -DPATCHES_C_COMPILER=<MIPS clang> -DPATCHES_LD=<ld.lld>
ninja -C build SmashBrosRecompiled
```

On Linux the same lines take `clang` and `clang++` in place of `clang-cl`, and the system's `clang` and `ld.lld` for
`patches/`. `tools/recompile.sh` builds N64Recomp from the submodule the first time, then translates the game. Run
`build/SmashBrosRecompiled` (`.exe` on Windows) from the repo's root, pick the ROM with Load ROM, then Start Game.
`tools/release.sh`'s header says how a release is built.

## License

GPL-3.0 (`LICENSE`), as the recompilations it follows. The symbols in `syms/` are Smash64RecompSyms', under CC0.
