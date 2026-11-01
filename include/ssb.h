#ifndef __SSB_H__
#define __SSB_H__

#include <string_view>

namespace ssb {
    inline constexpr std::u8string_view program_id = u8"SmashBrosRecompiled";
    inline constexpr std::string_view program_name = "Super Smash Bros.: Recompiled";

    // The game's code sections (register_overlays.cpp) and the patches over it (register_patches.cpp).
    void register_overlays();
    void register_patches();

    // The settings (config.cpp): RecompFrontend's tabs.
    void init_config();
}

#endif
