// libultra functions the game links that N64Recomp leaves out (its ignored_funcs) and the runtime doesn't implement,
// so each call to one in RecompiledFuncs/ comes here.
#include <cstdint>

#include "recomp.h"

// u64 % s64 (libultra's libc/ll.c), for the game's own 64-bit arithmetic: a and b in a0:a1 and a2:a3, the result in
// v0:v1.
extern "C" void __ll_rem_recomp(uint8_t* rdram, recomp_context* ctx) {
    uint64_t a = (ctx->r4 << 32) | ((ctx->r5 << 0) & 0xFFFFFFFFu);
    uint64_t b = (ctx->r6 << 32) | ((ctx->r7 << 0) & 0xFFFFFFFFu);
    uint64_t ret = b != 0 ? a % b : 0;

    ctx->r2 = (int32_t)(ret >> 32);
    ctx->r3 = (int32_t)(ret >> 0);
}

// The CPU's watch register, which the game's boot (sys/main.c) sets to trap writes to the RDRAM registers: there are
// none here to watch.
extern "C" void __osSetWatchLo_recomp(uint8_t* rdram, recomp_context* ctx) {
}

// The TLB: the effects' pages are backed by memory of their own at their addresses (src/game/recomp_api.cpp,
// pm::map_tlb_pages), so there is nothing to map.
extern "C" void osMapTLB_recomp(uint8_t* rdram, recomp_context* ctx) {
}

extern "C" void osUnmapTLB_recomp(uint8_t* rdram, recomp_context* ctx) {
}

// A write to the cartridge bus: the game's debug output (is_debug_init and is_debug_print in the decomp's
// src/is_debug.c) writes to the IS-Viewer's registers, which a cartridge doesn't have.
extern "C" void osEPiWriteIo_recomp(uint8_t* rdram, recomp_context* ctx) {
    ctx->r2 = 0;
}
