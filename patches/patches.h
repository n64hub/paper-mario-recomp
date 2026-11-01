#ifndef __PATCHES_H__
#define __PATCHES_H__

// What every patch includes: the sections N64Recomp reads a patch's role from, the natives (syms.ld) and libultra's
// headers from the decomp. The decomp's common.h needs headers its build makes from the ROM's assets (item_enum.h and
// others in ver/us/build/include), which stay out of git, so a patch declares the game's functions and structs it
// uses itself (pm_game.h).

#define RECOMP_EXPORT __attribute__((section(".recomp_export")))
#define RECOMP_PATCH __attribute__((section(".recomp_patch")))
#define RECOMP_FORCE_PATCH __attribute__((section(".recomp_force_patch")))
#define RECOMP_DECLARE_EVENT(func)                                                          \
    _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wunused-parameter\"") \
        __attribute__((noinline, weak, used, section(".recomp_event"))) void func {         \
    }                                                                                       \
    _Pragma("GCC diagnostic pop")

// libultra functions the runtime reimplements are called by their _recomp names (syms.ld).
#define osRecvMesg osRecvMesg_recomp
#define osEPiStartDma osEPiStartDma_recomp

#include <ultra64.h>

#endif
