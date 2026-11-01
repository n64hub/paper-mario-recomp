#ifndef __PM_H__
#define __PM_H__

#include <cstdint>
#include <string_view>

namespace pm {
    inline constexpr std::u8string_view program_id = u8"PaperMarioRecompiled";
    inline constexpr std::string_view program_name = "Paper Mario: Recompiled";

    // The game's code sections (register_overlays.cpp) and the patches over it (register_patches.cpp).
    void register_overlays();
    void register_patches();

    // The settings (config.cpp): RecompFrontend's tabs.
    void init_config();

    // Backs the effects' TLB-mapped pages with memory (recomp_api.cpp), before the game runs.
    void map_tlb_pages(uint8_t* rdram);
}

#endif
