// The natives us.toml's hooks call, each from the generated C of the game function it hooks.

#include "recomp.h"
#include "librecomp/overlays.hpp"
#include "librecomp/game.hpp"

#include <chrono>

// The hook at syDmaLoadOverlay's start (us.toml): the code sections in the overlay's stretch of ROM are now at its
// ram_load_start. ovl is the game's SYOverlay: rom_start, rom_end, ram_load_start, then the sections' bounds.
extern "C" void ssb_load_overlay(uint8_t* rdram, gpr ovl) {
    u32 rom_start = MEM_W(0, ovl);
    u32 rom_end = MEM_W(4, ovl);
    u32 ram = MEM_W(8, ovl);
    if (rom_end > rom_start) {
        load_overlays(rom_start, ram, rom_end - rom_start);
    }
}

extern "C" void yield_self_1ms(uint8_t* rdram);

// The hook at the start of each getter the game's busy-waits call (us.toml): called again within 100 us means a loop is
// spinning on it, so the other game threads (and the messages the VI and audio send them) get to run first, at most a
// millisecond. A getter called once a frame never waits.
extern "C" void ssb_spin_yield(uint8_t* rdram, int getter) {
    using clock = std::chrono::steady_clock;
    thread_local clock::time_point last_call[4];
    if (clock::now() - last_call[getter] < std::chrono::microseconds(100)) {
        yield_self_1ms(rdram);
    }
    last_call[getter] = clock::now();
}
