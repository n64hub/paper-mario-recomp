// The HUD at the wide picture's sides (RT64's Aspect Ratio Expand): the status bar's left box (HP, FP, Star Power) at
// the left side and its right box (Star Points, coins) at the right, and the coin counter the shops show at the left,
// where the game draws them in its 4:3 frame. The sides are those of the picture RecompFrontend's HUD Placement gives
// the HUD (16:9 by default); at 4:3, or with Aspect Ratio Original, nothing here changes the game's list.
//
// The game draws them with texture rectangles and scissors only (draw_box without a rotation, hud_element_draw_next,
// draw_number), plus the Star Power's shimmer, a few particles in a 320x240 viewport of their own. So rather than a copy
// of update_status_bar, 600 lines of state and blinking, the commands the game adds to the main list while it draws them
// are put back with RT64's alignments between them (hud_anchor): each rectangle placed from the side it starts on,
// each scissor's sides from the sides they are on, the shimmer's viewport from the left, and the game's own placement
// and its last scissor back after them. The menus (pause, battle, the popups) stay in the middle 4:3.

#include "patches.h"
#include "common.h"
#include "rt64_extended_gbi.h"

void update_status_bar(void);
void update_coin_counter(void);
void render_windows(s32* windowsArray, s32 parent, s32 flags, s32 baseX, s32 baseY, s32 opacity, s32 darkening,
                    f32 (*rotScaleMtx)[4]);

// The status bar's left box is 174 wide, from its drawPosX (update_status_bar): what starts left of its end is its.
#define STATUS_BAR_LEFT_WIDTH 174

// Room for the status bar's commands, a few thousand at most (each HUD element a load and a rectangle); a range larger
// than this is left as the game drew it.
#define HUD_COPY_MAX 8192
static Gfx sHudCopy[HUD_COPY_MAX];

static s32 hud_wide(void) {
    return recomp_get_aspect_ratio(4.0f / 3.0f) > 4.0f / 3.0f + 0.001f;
}

static void hud_rect_align(s32 origin) {
    if (origin == G_EX_ORIGIN_RIGHT) {
        gEXSetRectAlign(gMainGfxPos++, G_EX_ORIGIN_RIGHT, G_EX_ORIGIN_RIGHT, -SCREEN_WIDTH * 4, 0, -SCREEN_WIDTH * 4, 0);
    } else {
        gEXSetRectAlign(gMainGfxPos++, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_LEFT, 0, 0, 0, 0);
    }
}

// Puts back the commands the game added to the main list from start, each texture rectangle placed from the left side
// if it starts left of splitX (the game's pixels) and from the right otherwise, each scissor's sides by the same rule,
// and each viewport from the left; then RT64's own placement of rectangles and viewports, and the game's last scissor
// as it set it, for what the game draws next.
static void hud_anchor(Gfx* start, s32 splitX) {
    s32 count = gMainGfxPos - start;
    s32 origin = G_EX_ORIGIN_NONE;
    s32 viewports = false;
    Gfx* lastScissor = nullptr;
    s32 i;

    if (count <= 0 || count > HUD_COPY_MAX) {
        return;
    }
    for (i = 0; i < count; i++) {
        sHudCopy[i] = start[i];
    }
    gMainGfxPos = start;

    for (i = 0; i < count; i++) {
        Gfx* cmd = &sHudCopy[i];
        u32 op = cmd->words.w0 >> 24;

        if (op == G_TEXRECT || op == G_TEXRECTFLIP) {
            // w1 holds the upper left corner, in quarter pixels.
            s32 ulx = (cmd->words.w1 >> 12) & 0xFFF;
            s32 side = ulx < splitX * 4 ? G_EX_ORIGIN_LEFT : G_EX_ORIGIN_RIGHT;
            if (side != origin) {
                hud_rect_align(side);
                origin = side;
            }
        } else if (op == G_SETSCISSOR) {
            s32 ulx = ((cmd->words.w0 >> 12) & 0xFFF) / 4;
            s32 uly = (cmd->words.w0 & 0xFFF) / 4;
            s32 lrx = ((cmd->words.w1 >> 12) & 0xFFF) / 4;
            s32 lry = (cmd->words.w1 & 0xFFF) / 4;
            s32 mode = (cmd->words.w1 >> 24) & 0x3;
            s32 lorigin = ulx < splitX ? G_EX_ORIGIN_LEFT : G_EX_ORIGIN_RIGHT;
            s32 rorigin = lrx <= splitX ? G_EX_ORIGIN_LEFT : G_EX_ORIGIN_RIGHT;
            gEXSetScissor(gMainGfxPos++, mode, lorigin, rorigin, lorigin == G_EX_ORIGIN_RIGHT ? ulx - SCREEN_WIDTH : ulx,
                          uly, rorigin == G_EX_ORIGIN_RIGHT ? lrx - SCREEN_WIDTH : lrx, lry);
            lastScissor = cmd;
            continue;
        } else if (op == G_MOVEMEM && (cmd->words.w0 & 0xFF) == G_MV_VIEWPORT && !viewports) {
            gEXSetViewportAlign(gMainGfxPos++, G_EX_ORIGIN_LEFT, 0, 0);
            viewports = true;
        }
        *gMainGfxPos++ = *cmd;
    }

    gEXSetRectAlign(gMainGfxPos++, G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, 0, 0, 0, 0);
    if (viewports) {
        gEXSetViewportAlign(gMainGfxPos++, G_EX_ORIGIN_NONE, 0, 0);
    }
    if (lastScissor != nullptr) {
        *gMainGfxPos++ = *lastScissor;
    }
}

// The decomp's draw_status_ui (src/inventory.c), with the status bar at the sides.
RECOMP_PATCH void draw_status_ui(void) {
    Gfx* start = gMainGfxPos;

    update_status_bar();
    if (hud_wide()) {
        hud_anchor(start, gStatusBar.drawPosX + STATUS_BAR_LEFT_WIDTH);
    }
    update_coin_counter();
}

// The decomp's render_window_root (src/windows.c), with the coin counter at the left side. render_windows draws the
// root windows in priority order, each with its children after it; here it is called for one root window at a time
// (the others left out of its array, their children kept), which draws the same in the same order, so that the coin
// counter's commands can be told from the rest.
RECOMP_PATCH void render_window_root(void) {
    s32 priorityArray[ARRAY_COUNT(gWindows)];
    s32 rootArray[ARRAY_COUNT(gWindows)];
    s32* ptr = priorityArray;
    s32 i;
    s32 j;
    Window* window;

    for (i = 0; i < ARRAY_COUNT(gWindows); i++) {
        *ptr++ = -1;
    }
    for (i = 0, window = gWindows; i < ARRAY_COUNT(gWindows); window++, i++) {
        if (window->flags != 0) {
            priorityArray[window->priority] = i;
        }
    }

    gSPLoadGeometryMode(gMainGfxPos++, 0);
    gSPSetGeometryMode(gMainGfxPos++, G_ZBUFFER | G_SHADE | G_CULL_BOTH | G_SHADING_SMOOTH);
    gDPPipelineMode(gMainGfxPos++, G_PM_NPRIMITIVE);
    gDPSetCombineMode(gMainGfxPos++, G_CC_SHADE, G_CC_SHADE);
    gDPSetAlphaCompare(gMainGfxPos++, G_AC_NONE);
    gSPSetOtherMode(gMainGfxPos++, G_SETOTHERMODE_H, G_MDSFT_ALPHADITHER, 16, G_AD_DISABLE | G_CD_DISABLE | G_CK_NONE | G_TC_FILT | G_TF_BILERP | G_TT_NONE | G_TL_TILE | G_TD_CLAMP | G_TP_NONE);
    gSPClipRatio(gMainGfxPos++, FRUSTRATIO_2);
    gDPSetColorImage(gMainGfxPos++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH, osVirtualToPhysical(nuGfxCfb_ptr));
    gDPPipeSync(gMainGfxPos++);

    if (!hud_wide()) {
        render_windows(priorityArray, WIN_NONE, 0, 0, 0, 255, 0, nullptr);
        return;
    }

    // @recomp One root window at a time, in the array's order.
    for (i = 0; i < ARRAY_COUNT(gWindows); i++) {
        s32 id = priorityArray[i];
        Gfx* start;

        if (id < 0 || gWindows[id].parent != WIN_NONE) {
            continue;
        }
        for (j = 0; j < ARRAY_COUNT(gWindows); j++) {
            s32 other = priorityArray[j];
            rootArray[j] = (other >= 0 && gWindows[other].parent == WIN_NONE && other != id) ? -1 : other;
        }
        start = gMainGfxPos;
        render_windows(rootArray, WIN_NONE, 0, 0, 0, 255, 0, nullptr);
        if (id == WIN_CURRENCY_COUNTER) {
            hud_anchor(start, SCREEN_WIDTH);
        }
    }
}
