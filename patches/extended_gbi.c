// RT64's extended commands (rt64_extended_gbi.h), which the other patches add to the game's lists, are read as such
// only in a list that turns them on first: without it RT64 read the first one as the RDP command its opcode is, and
// the picture stopped changing. RT64 turns them off at each list's start, and the game has two lists each frame: the
// background's (gfx_task_background), which starts with gfx_init_state, and the rest's (gfx_draw_frame), whose second
// command is spr_render_init's call, which adds nothing to the list otherwise.

#include "patches.h"
#include "common.h"
#include "rt64_extended_gbi.h"

extern Gfx D_80074210[];
extern Gfx D_80074230[];

// The decomp's gfx_init_state (src/background_gfx.c), with RT64's extended commands turned on first.
RECOMP_PATCH void gfx_init_state(void) {
    gEXEnable(gMainGfxPos++);
    gSPSegment(gMainGfxPos++, 0x00, 0x0);
    gSPDisplayList(gMainGfxPos++, OS_K0_TO_PHYSICAL(D_80074230));
    gSPDisplayList(gMainGfxPos++, OS_K0_TO_PHYSICAL(D_80074210));
}

void spr_update_player_raster_cache(void);
void spr_clear_quad_cache(void);

// The decomp's spr_render_init (src/sprite.c), with RT64's extended commands turned on for the frame's main list.
RECOMP_PATCH void spr_render_init(void) {
    gEXEnable(gMainGfxPos++);
    spr_update_player_raster_cache();
    spr_clear_quad_cache();
}
