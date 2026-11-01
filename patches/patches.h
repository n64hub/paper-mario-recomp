#ifndef __PATCHES_H__
#define __PATCHES_H__

// What every patch includes: the sections N64Recomp reads a patch's role from, the natives (syms.ld) and libultra's
// headers from the decomp. A patch includes the decomp's own headers for the rest (common.h): the few its build makes
// from the ROM's assets (item_enum.h and others in ver/us/build/include) stay out of git, and dummy_headers/ stands in
// for them with what the patches need.

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

#include "pm_stdarg.h"
#include <ultra64.h>

// The natives (src/game/recomp_api.cpp).
f32 recomp_get_aspect_ratio(f32 original);

#endif
