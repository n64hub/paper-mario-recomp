// The memory behind the game's TLB-mapped effects, and the natives the patches call (patches/syms.ld gives each a dummy
// address that N64Recomp turns into a call by name). (pm_load_overlays, which us.toml's hooks call, is in
// src/main/register_overlays.cpp, beside N64Recomp's section table.)

#include "recomp.h"
#include "librecomp/overlays.hpp"
#include "librecomp/game.hpp"
#include "librecomp/helpers.hpp"
#include "ultramodern/config.hpp"
#include "recompui/recompui.h"

#include <algorithm>
#include <cstdio>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#include "pm.h"

// The game's effects run at 0xE0000000 and up behind the TLB (the decomp's src/effects.c: load_effect maps a 4 KB page
// pair of gEffectDataBuffer at each effect's address before it copies the effect there, and clear_effect_data maps
// gEffectGlobals at 0xE0200000). N64Recomp's memory macros index RDRAM by address - 0x80000000, so the recompiled
// effects read and write RDRAM + 0x60000000 and up, and the TLB calls are left out (us.toml's ignored functions). Each
// effect has its own address, and only effects use those pages, so backing the whole stretch with memory of its own is
// the TLB's mapping without the aliasing. Natively that is inside the runtime's 4 GB reservation, of which the runtime
// makes only the console's 512 MB usable, so the stretch is made usable here: on Windows committed (VirtualAlloc), as
// the runtime reserves the rest without committing it and VirtualProtect fails on such pages; on Linux mprotect, as
// the rest is mapped without access. On a page RDRAM is an allocation in the wasm heap, so the heap's break is moved
// past it, as Conker's page does, and the page's memory grows to about 1.6 GB of address space, most of it never
// touched.
static constexpr uint32_t effects_vram = 0xE0000000;
static constexpr uint32_t effects_end = 0xE0210000;

void pm::map_tlb_pages(uint8_t* rdram) {
    uint8_t* begin = rdram + (effects_vram - 0x80000000u);
    uint8_t* end = rdram + (effects_end - 0x80000000u);
    bool ok;
#if defined(__EMSCRIPTEN__)
    ok = false;
    uintptr_t brk = (uintptr_t)sbrk(0);
    if ((uintptr_t)begin >= brk) {
        ok = true;
        while (ok && brk < (uintptr_t)end) {
            uintptr_t step = std::min<uintptr_t>((uintptr_t)end - brk, 0x40000000); // sbrk takes an int
            // Another thread's malloc moving the break in between would own part of the range.
            ok = (uintptr_t)sbrk((intptr_t)step) == brk;
            brk += step;
        }
    }
#elif defined(_WIN32)
    ok = VirtualAlloc(begin, end - begin, MEM_COMMIT, PAGE_READWRITE) != nullptr;
#else
    ok = mprotect(begin, end - begin, PROT_READ | PROT_WRITE) == 0;
#endif
    if (!ok) {
        fprintf(stderr, "could not back the effects' TLB pages at 0x%08X\n", effects_vram);
    }
}

// recomp_get_aspect_ratio(original): the picture's aspect ratio, the window's with Aspect Ratio Expand (never narrower
// than original), else original (patches/widescreen.c).
extern "C" void recomp_get_aspect_ratio(uint8_t* rdram, recomp_context* ctx) {
    float original = _arg<0, float>(rdram, ctx);
    int width, height;
    recompui::get_window_size(width, height);
    if (ultramodern::renderer::get_graphics_config().ar_option != ultramodern::renderer::AspectRatio::Expand ||
        height <= 0) {
        _return(ctx, original);
        return;
    }
    _return(ctx, std::max(static_cast<float>(width) / height, original));
}
