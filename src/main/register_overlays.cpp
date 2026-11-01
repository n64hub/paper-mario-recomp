#include "pm.h"
#include "../../RecompiledFuncs/recomp_overlays.inl"

#include "librecomp/overlays.hpp"

// The game's code sections, from N64Recomp's section table: .main, mapped at boot with the 1 MB after ROM 0x1000, and
// the overlays, each mapped when the game copies it out of ROM (pm_load_overlays below).
void pm::register_overlays() {
    recomp::overlays::overlay_section_table_data_t sections {
        .code_sections = section_table,
        .num_code_sections = ARRLEN(section_table),
        .total_num_sections = num_sections,
    };

    recomp::overlays::overlays_by_index_t overlays {
        .table = overlay_sections_by_index,
        .len = ARRLEN(overlay_sections_by_index),
    };

    recomp::overlays::register_overlays(sections, overlays);
}

// The hooks at nuPiReadRom's and dma_copy's start (us.toml): the code sections lying wholly in the size bytes of ROM
// at rom are now at ram. The game loads its overlays (maps, battles, effects, world and battle code) with dma_copy,
// which reads in 0x2000-byte pieces through nuPiReadRom, and a few, such as a battle's partner, with nuPiReadRom alone.
// librecomp's load_overlays expects a range that holds at least one section: for a piece inside a section its bounds
// cross and it walks off the table, so a read is passed on only when a section lies in it.
extern "C" void pm_load_overlays(uint8_t* rdram, uint32_t rom, uint32_t ram, uint32_t size) {
    (void)rdram;
    for (const SectionTableEntry& section : section_table) {
        if (section.rom_addr >= rom && section.rom_addr + section.size <= rom + size && section.size > 0) {
            load_overlays(rom, (int32_t)ram, size);
            return;
        }
    }
}
