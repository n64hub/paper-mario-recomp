// What a picture wider than 4:3 needs (RT64's Aspect Ratio Expand, the window's shape): the world drawn to the window's
// edges.
//
// RT64 widens the game's 3D to the window, but the game keeps it inside its own frame twice over. Each camera's scissor
// is its viewport (render_frame), which for the world and battles is 12, 28, 308, 212, the border a console shows too,
// and RT64 keeps it as far in from the window's edges. And the frame's background (gfx_draw_background) is a picture or
// a colour drawn only inside that viewport, with black borders around it, and the depth cleared only over the 4:3
// picture: beside it, the colour buffer kept whatever was drawn there last (the title's pillars, an earlier room). So a
// camera whose view is the whole width opens to the window's sides, its background is drawn to them, and the borders
// stay above and below. A camera with a smaller view (a picture in a corner) keeps the game's, as does a disabled one
// (the title's and the file menu's, whose picture is a framed painting) and a 4:3 picture.
//
// Two of the game's screen effects are drawn over the 4:3 picture too. A dark room's light (appendGfx_darkness_stencil)
// is a 64x64 stencil the RDP draws into a texture, which RT64 widens and centres like any square viewport and so
// breaks; wide, the stencil is worked out on the CPU from the same texture and quads, and drawn stretched to the
// window's sides, its edges' darkness carried on. And the pause menu's blurred picture of the world
// (gfx_transfer_frame_to_depth) leaves the depth buffer's two outer columns as they were, which with a camera that
// opens hold the world's depth and show as coloured columns; wide, they are set to the cleared value. The fades to or
// from a colour (a map's, a cutscene's) fill the game's frame or its view, and wide fill them to the window's sides.

#include "patches.h"
#include "common.h"
#include "camera.h"
#include "model.h"
#include "nu/nusys.h"
#include "overlay.h"
#include "rt64_extended_gbi.h"

void render_models(void);
void execute_render_tasks(void);
void render_item_entities(void);
void render_transformed_hud_elements(void);
u16 blend_background_channel(u16 arg0, s32 arg1, s32 alpha);
void gfx_transfer_frame_to_depth(u16* frameBuffer0, u16* frameBuffer1, u16* zBuffer);
extern s32 gPauseBackgroundFade;
extern u16 gBackgroundPalette[256];
extern s8 gBackroundWaveEnabled;
extern s16 gBackroundTextureYOffset;
extern f32 gBackroundWavePhase;
extern f32 gBackroundLastScrollValue;

// RT64's G_EX_FILLRECT_V1, which rt64_extended_gbi.h has no macro for: a fill rectangle placed from the origins given
// (its src/gbi/rt64_gbi_extended.cpp, fillrectV1). A fill's right side is inclusive, so one to the right edge ends at 0
// from it, and the scissor cuts it there.
#define gEXFillRectangle(cmd, lorigin, rorigin, ulx, uly, lrx, lry)                                                  \
    G_EX_COMMAND2(cmd, PARAM(RT64_EXTENDED_OPCODE, 8, 24) | PARAM(G_EX_FILLRECT_V1, 24, 0),                         \
                  PARAM(lorigin, 12, 0) | PARAM(rorigin, 12, 12), PARAM((ulx) * 4, 16, 16) | PARAM((uly) * 4, 16, 0), \
                  PARAM((lrx) * 4, 16, 16) | PARAM((lry) * 4, 16, 0))

// The game's frame: its world and battle cameras' viewport, 12 in from either side.
#define FRAME_INSET_X 12

// Whether the picture is wider than 4:3, and how far past its sides the window reaches, in the game's pixels.
static f32 widescreen_aspect(void) {
    return recomp_get_aspect_ratio(4.0f / 3.0f);
}

static s32 widescreen_margin(void) {
    f32 aspect = widescreen_aspect();
    if (aspect <= 4.0f / 3.0f + 0.001f) {
        return 0;
    }
    // Rounded up, and two pixels more, so that the background reaches past the window's edge rather than short of it:
    // RT64 moves the side of a rectangle placed from the window's centre or a side left onto its grid of the game's
    // pixels, and left again by the framebuffer's misalignment, up to two pixels in all (convertViewportRect's
    // correctMisalignment), and rectangles to the window's right edge stopped a column short of it. No wider than
    // about 32:9, so that its copies fit their list (sWideBackgroundGfx).
    f32 margin = (aspect * SCREEN_HEIGHT - SCREEN_WIDTH) * 0.5f + 3.0f;
    return margin > 280.0f ? 280 : (s32)margin;
}

// How far past its origin a rectangle placed from the window's left or right side reaches out, so that it ends at the
// window's edge: RT64 places those origins at the edges of the picture HUD Placement gives the HUD (16:9 by default),
// short of a wider window's, and clips a rectangle at its framebuffer.
#define WIDE_REACH widescreen_margin()

// Whether a camera's view spans the game's whole frame, so that it opens to the window's sides. A disabled camera
// draws no world, and the background behind it is a picture of its own, not a panorama: the title's, behind the title
// and the file menu, is one framed painting with a white border, which the stage's pillars and floor cover at 4:3. So
// it keeps the game's frame, black beside it, and the stage widens over it.
static s32 widescreen_camera_opens(Camera* camera) {
    return widescreen_margin() > 0 && !(camera->flags & CAMERA_FLAG_DISABLED) &&
           camera->viewportStartX <= FRAME_INSET_X &&
           camera->viewportStartX + camera->viewportW >= SCREEN_WIDTH - FRAME_INSET_X;
}

// The decomp's render_frame (src/cam_main.c), with the scissor of a camera that opens (above) at the window's sides.
RECOMP_PATCH void render_frame(s32 isSecondPass) {
    s32 camID;

    if (!isSecondPass) {
        gCurrentCamID = CAM_DEFAULT;
        mdl_update_transform_matrices();
    }

    if (isSecondPass) {
        camID = CAM_HUD;
    } else {
        camID = CAM_DEFAULT;
    }

    // first pass:  loop uses camIDs from CAM_DEFAULT to CAM_HUD - 1
    // second pass: loop only uses CAM_HUD
    isSecondPass = 1 - isSecondPass;

    for (; camID < ARRAY_COUNT(gCameras) - isSecondPass; camID++) {
        Camera* camera = &gCameras[camID];
        u16 matrixListPos;

        if (camera->flags == 0 || (camera->flags & (CAMERA_FLAG_NO_DRAW | CAMERA_FLAG_DISABLED))) {
            continue;
        }

        gCurrentCamID = camID;

        if (camera->fpDoPreRender != nullptr) {
            camera->fpDoPreRender(camera);
        } else {
            s32 ulx;
            s32 uly;
            s32 lrx;
            s32 lry;

            gSPViewport(gMainGfxPos++, &camera->vp);
            gSPClearGeometryMode(gMainGfxPos++, G_SHADE | G_CULL_BOTH | G_FOG | G_LIGHTING | G_TEXTURE_GEN |
                                    G_TEXTURE_GEN_LINEAR | G_LOD | G_SHADING_SMOOTH);
            gSPTexture(gMainGfxPos++, 0, 0, 0, G_TX_RENDERTILE, G_OFF);
            gDPSetCycleType(gMainGfxPos++, G_CYC_1CYCLE);
            gDPPipelineMode(gMainGfxPos++, G_PM_NPRIMITIVE);

            ulx = camera->viewportStartX;
            uly = camera->viewportStartY;
            lrx = ulx + camera->viewportW;
            lry = uly + camera->viewportH;

            if (ulx < 0) {
                ulx = 0;
            }
            if (uly < 0) {
                uly = 0;
            }
            if (lrx < 1) {
                lrx = 1;
            }
            if (lry < 1) {
                lry = 1;
            }

            if (ulx > SCREEN_WIDTH - 1) {
                ulx = SCREEN_WIDTH - 1;
            }
            if (uly > SCREEN_HEIGHT - 1) {
                uly = SCREEN_HEIGHT - 1;
            }
            if (lrx > SCREEN_WIDTH) {
                lrx = SCREEN_WIDTH;
            }
            if (lry > SCREEN_HEIGHT) {
                lry = SCREEN_HEIGHT;
            }

            // @recomp A view of the whole frame opens to the window's sides.
            if (widescreen_camera_opens(camera)) {
                gEXSetScissor(gMainGfxPos++, G_SC_NON_INTERLACE, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, -WIDE_REACH, uly,
                              WIDE_REACH, lry);
            } else {
                gDPSetScissor(gMainGfxPos++, G_SC_NON_INTERLACE, ulx, uly, lrx, lry);
            }
            gDPSetTextureLOD(gMainGfxPos++, G_TL_TILE);
            gDPSetTextureLUT(gMainGfxPos++, G_TT_NONE);
            gDPSetTextureDetail(gMainGfxPos++, G_TD_CLAMP);
            gDPSetTexturePersp(gMainGfxPos++, G_TP_PERSP);
            gDPSetTextureFilter(gMainGfxPos++, G_TF_BILERP);
            gDPSetTextureConvert(gMainGfxPos++, G_TC_FILT);
            gDPSetCombineMode(gMainGfxPos++, G_CC_SHADE, G_CC_SHADE);
            gDPSetCombineKey(gMainGfxPos++, G_CK_NONE);
            gDPSetAlphaCompare(gMainGfxPos++, G_AC_NONE);
            gDPSetRenderMode(gMainGfxPos++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
            gDPSetColorDither(gMainGfxPos++, G_CD_DISABLE);
            gSPClipRatio(gMainGfxPos++, FRUSTRATIO_2);
            gDPSetColorImage(gMainGfxPos++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH,
                                osVirtualToPhysical(nuGfxCfb_ptr));
            gDPPipeSync(gMainGfxPos++);

            if (!(camera->flags & CAMERA_FLAG_ORTHO)) {
                gSPPerspNormalize(gMainGfxPos++, camera->perspNorm);
            }

            guMtxF2L(camera->mtxPerspective, &gDisplayContext->camPerspMatrix[gCurrentCamID]);
            gSPMatrix(gMainGfxPos++, &gDisplayContext->camPerspMatrix[gCurrentCamID], G_MTX_NOPUSH | G_MTX_LOAD |
                        G_MTX_PROJECTION);
        }

        camera->mtxBillboard = &gDisplayContext->matrixStack[gMatrixListPos];
        matrixListPos = gMatrixListPos++;
        guRotate(&gDisplayContext->matrixStack[matrixListPos], -camera->curBoomYaw, 0.0f, 1.0f, 0.0f);
        camera->vpAlt.vp.vtrans[0] = camera->vp.vp.vtrans[0] + gGameStatusPtr->altViewportOffset.x;
        camera->vpAlt.vp.vtrans[1] = camera->vp.vp.vtrans[1] + gGameStatusPtr->altViewportOffset.y;

        if (!(camera->flags & CAMERA_FLAG_ORTHO)) {
            if (gCurrentCamID != CAM_HUD) {
                if (!(camera->flags & CAMERA_FLAG_RENDER_ENTITIES)) {
                    render_entities();
                }
                if (!(camera->flags & CAMERA_FLAG_RENDER_MODELS)) {
                    render_models();
                }
                render_player();
                render_npcs();
                render_workers_scene();
                render_effects_scene();
                execute_render_tasks();
                render_transformed_hud_elements();
            } else {
                guOrthoF(camera->mtxPerspective, 0.0f, SCREEN_WIDTH, -SCREEN_HEIGHT, 0.0f, -1000.0f, 1000.0f,
                            1.0f);
                guMtxF2L(camera->mtxPerspective, &gDisplayContext->camPerspMatrix[gCurrentCamID]);
                gSPMatrix(gMainGfxPos++, &gDisplayContext->camPerspMatrix[gCurrentCamID], G_MTX_NOPUSH |
                            G_MTX_LOAD | G_MTX_PROJECTION);
                render_transformed_hud_elements();
                render_item_entities();
            }
        } else {
            render_workers_scene();
            execute_render_tasks();
        }

        if (camera->fpDoPostRender != nullptr) {
            camera->fpDoPostRender(camera);
        }

        gDPPipeSync(gMainGfxPos++);
        gDPSetColorImage(gMainGfxPos++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH,
                            osVirtualToPhysical(nuGfxCfb_ptr));
        gDPPipeSync(gMainGfxPos++);
    }
}

// The background picture's strips, as appendGfx_background_texture draws them, from the window's left edge to its
// right: the picture repeats every bgMaxX pixels, its first column offsetQ quarter pixels right of bgMinX (the wave's
// offsets are in half pixels, so all of it is in quarters), and each copy is a texture rectangle placed from the
// window's centre, which the game's 4:3 rectangles can't be, since they start at 0.
static void widescreen_background_rows(s32 uly, s32 lry, s32 offsetQ, s32 bgMinX, s32 bgMaxX, s32 margin) {
    s32 leftQ = -(SCREEN_WIDTH / 2 + margin) * 4;
    s32 rightQ = (SCREEN_WIDTH / 2 + margin) * 4;
    s32 periodQ = bgMaxX * 4;
    s32 startQ = (bgMinX - SCREEN_WIDTH / 2) * 4 + offsetQ;

    while (startQ > leftQ) {
        startQ -= periodQ;
    }
    for (; startQ < rightQ; startQ += periodQ) {
        s32 ulxQ = startQ < leftQ ? leftQ : startQ;
        s32 lrxQ = startQ + periodQ > rightQ ? rightQ : startQ + periodQ;
        if (lrxQ <= ulxQ) {
            continue;
        }
        // A copy rectangle's right side is inclusive: it ends a pixel short of the next copy.
        gEXTextureRectangle(gMainGfxPos++, G_EX_ORIGIN_CENTER, G_EX_ORIGIN_CENTER, ulxQ, uly * 4, lrxQ - 4, lry * 4,
                            G_TX_RENDERTILE, (ulxQ - startQ) * 8, 0, 4096, 1024);
    }
}

// The decomp's appendGfx_background_texture (src/background.c), drawing the picture from the window's left edge to its
// right in a picture wider than 4:3 (widescreen_background_rows), where the game draws it once across its frame.
RECOMP_PATCH void appendGfx_background_texture(void) {
    Camera* cam = &gCameras[gCurrentCameraID];
    u16 flags = 0;
    s32 fogR, fogG, fogB, fogA;
    u8 r1, g1, b1, a1;
    u8 r2, g2, b2;
    u16 blendedR, blendedG, blendedB;
    s32 i;

    f32 theta, sinTheta, cosTheta, scrollValue, f5, waveOffset;

    s32 bgMinX;
    s32 bgMinY;
    s32 bgMaxX;
    s32 bgMaxY;
    s32 lineHeight;
    s32 numLines;
    s32 extraHeight;

    s32 bgXOffset;
    s16 texOffsetY;
    // @recomp How far past the frame's sides the window reaches, or 0 to draw as the game does.
    s32 margin = widescreen_camera_opens(cam) ? widescreen_margin() : 0;

    enum {
        BG_BLEND_NONE           = 0,
        BG_BLEND_HAS_FOG        = 1,
        BG_BLEND_SHOULD_LERP    = 2,
        BG_BLEND_SHOULD_BLEND   = 4,
    };

    if (is_world_fog_enabled()) {
        get_world_fog_color(&fogR, &fogG, &fogB, &fogA);
        flags = BG_BLEND_HAS_FOG;
        fogA = gGameStatusPtr->backgroundDarkness;
    }

    switch (*gBackgroundTintModePtr) {
        case ENV_TINT_NONE:
        case ENV_TINT_SHROUD:
            mdl_get_shroud_tint_params(&r1, &g1, &b1, &a1);
            if (a1 != 0) {
                flags |= BG_BLEND_SHOULD_LERP;
            }
            break;
        case ENV_TINT_DEPTH:
        case ENV_TINT_REMAP:
        default:
            mdl_get_remap_tint_params(&r1, &g1, &b1, &r2, &g2, &b2);
            if (!(r1 == 255 && g1 == 255 && b1 == 255 && r2 == 0 && g2 == 0 && b2 == 0)) {
                flags |= BG_BLEND_SHOULD_BLEND;
            }
            break;
    }

    switch (flags) {
        case BG_BLEND_NONE:
            gGameStatusPtr->backgroundFlags &= ~BACKGROUND_FLAG_FOG;
            break;
        case BG_BLEND_HAS_FOG:
            gGameStatusPtr->backgroundFlags |= BACKGROUND_FLAG_FOG;
            break;
        case BG_BLEND_SHOULD_LERP:
            gGameStatusPtr->backgroundFlags |= BACKGROUND_FLAG_FOG;
            fogR = r1;
            fogG = g1;
            fogB = b1;
            fogA = a1;
            break;
        case BG_BLEND_HAS_FOG | BG_BLEND_SHOULD_LERP:
            gGameStatusPtr->backgroundFlags |= BACKGROUND_FLAG_FOG;
            fogR = (fogR * (255 - a1) + r1 * a1) / 255;
            fogG = (fogG * (255 - a1) + g1 * a1) / 255;
            fogB = (fogB * (255 - a1) + b1 * a1) / 255;
            fogA = (fogA * (255 - a1) + a1 * a1) / 255;
            break;
        case BG_BLEND_SHOULD_BLEND:
            gGameStatusPtr->backgroundFlags |= BACKGROUND_FLAG_FOG;
            break;
    }

    if (gGameStatusPtr->backgroundFlags & BACKGROUND_FLAG_FOG) {
        switch (*gBackgroundTintModePtr) {
            case ENV_TINT_NONE:
            case ENV_TINT_SHROUD:
                if (fogA == 255) {
                    for (i = 0; i < ARRAY_COUNT(gBackgroundPalette); i++) {
                        gBackgroundPalette[i] = 1;
                    }
                } else {
                    // lerp from background palette color to fog color based on fog alpha
                    for (i = 0; i < ARRAY_COUNT(gBackgroundPalette); i++) {
                        // NOTE: values after UNPACK range from [0,31], so we need to shift fog color into that range
                        u16 palColor = gGameStatusPtr->backgroundPalette[i];
                        blendedB = blend_background_channel(UNPACK_PAL_B(palColor), fogB >> 3, fogA);
                        blendedG = blend_background_channel(UNPACK_PAL_G(palColor), fogG >> 3, fogA);
                        blendedR = blend_background_channel(UNPACK_PAL_R(palColor), fogR >> 3, fogA);
                        gBackgroundPalette[i] = blendedB << 1 | blendedG << 6 | blendedR << 11 | 1;
                    }
                }
                break;
            case ENV_TINT_DEPTH:
            case ENV_TINT_REMAP:
            default:
                // the background color channels are remapped from [0,255] -> [min,max]
                for (i = 0; i < ARRAY_COUNT(gBackgroundPalette); i++) {
                    // NOTE: values after UNPACK range from [0,31], so we need to shift other colors into that range
                    u16 palColor = gGameStatusPtr->backgroundPalette[i];
                    blendedB = (b2 >> 3) + ((UNPACK_PAL_B(palColor) * b1 >> 3) >> 5);
                    blendedG = (g2 >> 3) + ((UNPACK_PAL_G(palColor) * g1 >> 3) >> 5);
                    blendedR = (r2 >> 3) + ((UNPACK_PAL_R(palColor) * r1 >> 3) >> 5);

                    if (blendedB > 0x1F) {
                        blendedB = 0x1F;
                    }
                    if (blendedG > 0x1F) {
                        blendedG = 0x1F;
                    }
                    if (blendedR > 0x1F) {
                        blendedR = 0x1F;
                    }
                    gBackgroundPalette[i] = blendedB << 1 | blendedG << 6 | blendedR << 11 | 1;
                }
                break;
        }
    }

    theta = clamp_angle(-cam->curBoomYaw);
    sinTheta = sin_deg(theta);
    cosTheta = cos_deg(theta);
    f5 = cosTheta * cam->lookAt_obj.x - sinTheta * cam->lookAt_obj.z + cam->leadAmount;
    scrollValue = -f5 * 0.25f;
    scrollValue += gGameStatusPtr->backgroundMaxX * theta * (1 / 90.0f);

    if (fabsf(scrollValue - gBackroundLastScrollValue) < 0.3f) {
        scrollValue = gBackroundLastScrollValue;
    } else {
        gBackroundLastScrollValue = scrollValue;
    }

    while (scrollValue < 0.0f) {
        scrollValue += gGameStatusPtr->backgroundMaxX * 32;
    }

    bgXOffset = gGameStatusPtr->backgroundXOffset = ((s32)scrollValue) % gGameStatusPtr->backgroundMaxX;
    bgMaxX = gGameStatusPtr->backgroundMaxX;
    bgMaxY = gGameStatusPtr->backgroundMaxY;
    bgMinX = gGameStatusPtr->backgroundMinX;
    bgMinY = gGameStatusPtr->backgroundMinY;

    gDPPipeSync(gMainGfxPos++);
    gDPSetCycleType(gMainGfxPos++, G_CYC_COPY);
    gDPSetTexturePersp(gMainGfxPos++, G_TP_NONE);
    gDPSetTextureLUT(gMainGfxPos++, G_TT_RGBA16);
    gDPSetCombineMode(gMainGfxPos++, G_CC_DECALRGB, G_CC_DECALRGB);
    gDPSetRenderMode(gMainGfxPos++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetTextureFilter(gMainGfxPos++, G_TF_POINT);
    gDPPipeSync(gMainGfxPos++);

    if (!(gGameStatusPtr->backgroundFlags & BACKGROUND_FLAG_FOG)) {
        gDPLoadTLUT_pal256(gMainGfxPos++, gGameStatusPtr->backgroundPalette);
    } else {
        gDPLoadTLUT_pal256(gMainGfxPos++, gBackgroundPalette);
    }

    if (!gBackroundWaveEnabled) {
        lineHeight = 2048 / gGameStatusPtr->backgroundMaxX;
        numLines = gGameStatusPtr->backgroundMaxY / lineHeight;
        extraHeight = gGameStatusPtr->backgroundMaxY % lineHeight;
        for (i = 0; i < numLines; i++) {
            texOffsetY = gBackroundTextureYOffset + lineHeight * i;
            if (texOffsetY > gGameStatusPtr->backgroundMaxY) {
                texOffsetY -= gGameStatusPtr->backgroundMaxY;
            }
            gDPLoadTextureTile(gMainGfxPos++, gGameStatusPtr->backgroundRaster + bgMaxX * texOffsetY,
                               G_IM_FMT_CI, G_IM_SIZ_8b, bgMaxX, 6,
                               0, 0, 295, 5, 0,
                               G_TX_WRAP, G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);

            if (margin > 0) {
                widescreen_background_rows(lineHeight * i + bgMinY, lineHeight * i + lineHeight - 1 + bgMinY,
                                           bgXOffset * 4, bgMinX, bgMaxX, margin);
                continue;
            }
            gSPTextureRectangle(gMainGfxPos++, bgMinX * 4, (lineHeight * i + bgMinY) * 4,
                                                 (bgXOffset + bgMinX - 1) * 4, (lineHeight * i + lineHeight - 1 + bgMinY) * 4,
                                                 G_TX_RENDERTILE, (bgMaxX - bgXOffset) * 32, 0, 4096, 1024);
            gSPTextureRectangle(gMainGfxPos++, (bgXOffset + bgMinX) * 4, (lineHeight * i + bgMinY) * 4,
                                                 (bgMaxX + bgMinX - 1) * 4, (lineHeight * i + lineHeight - 1 + bgMinY) * 4,
                                                 G_TX_RENDERTILE, 0, 0, 4096, 1024);
        }
        if (extraHeight != 0) {
            texOffsetY = gBackroundTextureYOffset + lineHeight * i;
            if (texOffsetY > gGameStatusPtr->backgroundMaxY) {
                texOffsetY -= gGameStatusPtr->backgroundMaxY;
            }
            gDPLoadTextureTile(gMainGfxPos++, gGameStatusPtr->backgroundRaster + bgMaxX * texOffsetY,
                               G_IM_FMT_CI, G_IM_SIZ_8b, bgMaxX, extraHeight,
                               0, 0, 295, extraHeight - 1, 0,
                               G_TX_WRAP, G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
            if (margin > 0) {
                widescreen_background_rows(lineHeight * i + bgMinY, bgMaxY - 1 + bgMinY, bgXOffset * 4, bgMinX,
                                           bgMaxX, margin);
            } else {
                gSPTextureRectangle(gMainGfxPos++, bgMinX * 4, (lineHeight * i + bgMinY) * 4,
                                                     (bgXOffset + bgMinX - 1) * 4, (bgMaxY - 1 + bgMinY) * 4,
                                                     G_TX_RENDERTILE, (bgMaxX - bgXOffset) * 32, 0, 4096, 1024);
                gSPTextureRectangle(gMainGfxPos++, (bgXOffset + bgMinX) * 4, (lineHeight * i + bgMinY) * 4,
                                                     (bgMaxX + bgMinX - 1) * 4, (bgMaxY - 1 + bgMinY) * 4,
                                                     G_TX_RENDERTILE, 0, 0, 4096, 1024);
            }
        }
    } else {
        lineHeight = 6;
        numLines = gGameStatusPtr->backgroundMaxY / lineHeight;
        extraHeight = gGameStatusPtr->backgroundMaxY % lineHeight;
        gBackroundWavePhase += TAU / 60; // 60 frames period
        for (i = 0; i < numLines; i++) {
            waveOffset = sin_rad(gBackroundWavePhase + i * (TAU / 15)) * 3.0f;
            bgXOffset = 2.0f * (gGameStatusPtr->backgroundXOffset + waveOffset);
            texOffsetY = gBackroundTextureYOffset + lineHeight * i;
            if (texOffsetY > gGameStatusPtr->backgroundMaxY) {
                texOffsetY -= gGameStatusPtr->backgroundMaxY;
            }
            gDPLoadTextureTile(gMainGfxPos++, gGameStatusPtr->backgroundRaster + bgMaxX * texOffsetY,
                               G_IM_FMT_CI, G_IM_SIZ_8b, bgMaxX, 6,
                               0, 0, 295, 5, 0,
                               G_TX_WRAP, G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);

            if (margin > 0) {
                widescreen_background_rows(lineHeight * i + bgMinY, lineHeight * i + lineHeight - 1 + bgMinY,
                                           bgXOffset * 2, bgMinX, bgMaxX, margin);
                continue;
            }
            gSPTextureRectangle(gMainGfxPos++, bgMinX * 4, (lineHeight * i + bgMinY) * 4,
                                                 (2 * bgXOffset + (bgMinX - 1)) * 4, (lineHeight * i + lineHeight - 1 + bgMinY) * 4,
                                                 G_TX_RENDERTILE, bgMaxX * 32 - bgXOffset * 16, 0, 4096, 1024);
            gSPTextureRectangle(gMainGfxPos++, bgXOffset * 2 + bgMinX * 4, (lineHeight * i + bgMinY) * 4,
                                                 (bgMaxX + bgMinX - 1) * 4, (lineHeight * i + lineHeight - 1 + bgMinY) * 4,
                                                 G_TX_RENDERTILE, 0, 0, 4096, 1024);
        }
        if (extraHeight != 0) {
            waveOffset = sin_rad(gBackroundWavePhase + i * (TAU / 15)) * 3.0f;
            bgXOffset = 2.0f * (gGameStatusPtr->backgroundXOffset + waveOffset);
            texOffsetY = gBackroundTextureYOffset + lineHeight * i;
            if (texOffsetY > gGameStatusPtr->backgroundMaxY) {
                texOffsetY -= gGameStatusPtr->backgroundMaxY;
            }
            gDPLoadTextureTile(gMainGfxPos++, gGameStatusPtr->backgroundRaster + bgMaxX * texOffsetY,
                               G_IM_FMT_CI, G_IM_SIZ_8b, bgMaxX, extraHeight,
                               0, 0, 295, extraHeight - 1, 0,
                               G_TX_WRAP, G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
            if (margin > 0) {
                widescreen_background_rows(lineHeight * i + bgMinY, bgMaxY - 1 + bgMinY, bgXOffset * 2, bgMinX,
                                           bgMaxX, margin);
            } else {
                gSPTextureRectangle(gMainGfxPos++, bgMinX * 4, (lineHeight * i + bgMinY) * 4,
                                                     (2 * bgXOffset + (bgMinX - 1)) * 4, (bgMaxY - 1 + bgMinY) * 4, /// @bug xh = 2 * bgXOffset + (bgMinX - 1) * 4
                                                     G_TX_RENDERTILE, bgMaxX * 32 - bgXOffset * 16, 0, 4096, 1024);
                gSPTextureRectangle(gMainGfxPos++, bgXOffset * 2  + bgMinX * 4, (lineHeight * i + bgMinY) * 4,
                                                     (bgMaxX + bgMinX - 1) * 4, (bgMaxY - 1 + bgMinY) * 4, /// @bug xh = 2 * bgXOffset + (bgMinX - 1) * 4
                                                     G_TX_RENDERTILE, 0, 0, 4096, 1024);
            }
        }
    }
}

// The list the background's picture is drawn in when it opens to the window's sides, one for each of the game's two
// display contexts, since RT64 may still read the last frame's while the game builds the next. A row of the picture is
// its load (7 commands) and a copy rectangle (3) for each copy across the window, at most 5 (widescreen_margin's
// limit), and a picture is at most 40 rows and a part row, after the palette's load.
#define WIDE_BACKGROUND_GFX_COUNT 1024
static Gfx sWideBackgroundGfx[2][WIDE_BACKGROUND_GFX_COUNT];

// The decomp's gfx_draw_background (src/background_gfx.c), the frame's clears and background, with the depth cleared
// across the whole window and, behind a camera that opens (above), the background drawn to the window's sides and the
// black borders only above and below it. The pause screen's background, a copy of the 4:3 frame the game took before
// it paused, stays 4:3, black beside it.
RECOMP_PATCH void gfx_draw_background(void) {
    Camera* camera;
    s32 bgRenderState;
    s32 backgroundMinX;
    s32 backgroundMaxX;
    s32 backgroundMinY;
    s32 backgroundMaxY;
    s32 viewportStartX;
    s32 i;
    s32 a = SCREEN_COPY_TILE_HEIGHT << 2;
    s32 opens;

    gDPSetScissor(gMainGfxPos++, G_SC_NON_INTERLACE, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    camera = &gCameras[gCurrentCameraID];
    bgRenderState = gGameStatusPtr->backgroundFlags & BACKGROUND_RENDER_STATE_MASK;

    switch (bgRenderState) {
        case BACKGROUND_RENDER_STATE_BEGIN_PAUSED:
            // Save coverage to nunGfxCfb[1] using the VISCVG render mode
            gDPPipeSync(gMainGfxPos++);
            gDPSetColorImage(gMainGfxPos++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH, nuGfxCfb[1]);
            gDPSetCycleType(gMainGfxPos++, G_CYC_1CYCLE);
            gDPSetBlendColor(gMainGfxPos++, 0x80, 0x80, 0x80, 0xFF);
            gDPSetPrimDepth(gMainGfxPos++, 0xFFFF, 0xFFFF);
            gDPSetDepthSource(gMainGfxPos++, G_ZS_PRIM);
            gDPSetRenderMode(gMainGfxPos++, G_RM_VISCVG, G_RM_VISCVG2);
            gDPFillRectangle(gMainGfxPos++, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
            gDPPipeSync(gMainGfxPos++);
            gDPSetDepthSource(gMainGfxPos++, G_ZS_PIXEL);
            gGameStatusPtr->backgroundFlags &= ~BACKGROUND_RENDER_STATE_MASK;
            gGameStatusPtr->backgroundFlags |= BACKGROUND_RENDER_STATE_FILTER_PAUSED;
            break;
        case BACKGROUND_RENDER_STATE_FILTER_PAUSED:
            // Save the framebuffer into the depth buffer and run a filter on it based on the saved coverage values
            gfx_transfer_frame_to_depth(nuGfxCfb[0], nuGfxCfb[1], nuGfxZBuffer); // applies filters to the framebuffer
            // @recomp The filter leaves the frame's two columns at either side as they were in the depth buffer, which
            // at 4:3 is its cleared value there, the world drawn only inside the frame: a grey line around the paused
            // picture. A camera that opens draws its depth to the sides too, which showed as coloured columns there.
            if (widescreen_margin() > 0) {
                for (i = 0; i < SCREEN_HEIGHT; i++) {
                    u16* row = &nuGfxZBuffer[i * SCREEN_WIDTH];
                    row[0] = row[1] = row[SCREEN_WIDTH - 2] = row[SCREEN_WIDTH - 1] = GPACK_ZDZ(G_MAXFBZ, 0);
                }
            }
            gPauseBackgroundFade = 0;
            gGameStatusPtr->backgroundFlags &= ~BACKGROUND_RENDER_STATE_MASK;
            gGameStatusPtr->backgroundFlags |= BACKGROUND_RENDER_STATE_SHOW_PAUSED;
            // fallthrough
        case BACKGROUND_RENDER_STATE_SHOW_PAUSED:
            // Draw the saved framebuffer to the background, fading in at a rate of 16 opacity per frame until reaching 128 opacity
            gPauseBackgroundFade += 16;
            if (gPauseBackgroundFade > 128) {
                gPauseBackgroundFade = 128;
            }

            gDPPipeSync(gMainGfxPos++);
            gDPSetScissor(gMainGfxPos++, G_SC_NON_INTERLACE, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
            gDPSetCycleType(gMainGfxPos++, G_CYC_FILL);
            gDPSetRenderMode(gMainGfxPos++, G_RM_NOOP, G_RM_NOOP2);
            gDPSetColorImage(gMainGfxPos++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH, nuGfxCfb_ptr);
            gDPSetFillColor(gMainGfxPos++, PACK_FILL_COLOR(0, 0, 0, 1));
            // @recomp Black to the window's sides.
            if (widescreen_margin() > 0) {
                gEXSetScissor(gMainGfxPos++, G_SC_NON_INTERLACE, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, -WIDE_REACH, 0,
                              WIDE_REACH, SCREEN_HEIGHT);
                gEXFillRectangle(gMainGfxPos++, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, -WIDE_REACH, 0, WIDE_REACH,
                                 SCREEN_HEIGHT - 1);
                gDPSetScissor(gMainGfxPos++, G_SC_NON_INTERLACE, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
            } else {
                gDPFillRectangle(gMainGfxPos++, 0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);
            }
            gDPSetCycleType(gMainGfxPos++, G_CYC_1CYCLE);
            gDPSetTexturePersp(gMainGfxPos++, G_TP_NONE);
            gDPSetTextureLUT(gMainGfxPos++, G_TT_NONE);
            gDPSetRenderMode(gMainGfxPos++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
            /// @bug In 1-cycle mode, the two combiner cycles should be identical. Using Texel1 here in the second cycle,
            /// which is the actual cycle of the combiner used on hardware in 1-cycle mode, actually samples the next
            /// pixel's texel value instead of the current pixel's. This results in a one-pixel offset.
            gDPSetCombineMode(gMainGfxPos++, PM_CC_43, PM_CC_44);
            gDPSetPrimColor(gMainGfxPos++, 0, 0, 40, 40, 40, gPauseBackgroundFade);
            gDPSetTextureFilter(gMainGfxPos++, G_TF_POINT);

            for (i = 0; i < 40; i++) {
                gDPLoadTextureTile(gMainGfxPos++, nuGfxZBuffer + (i * SCREEN_WIDTH * SCREEN_COPY_TILE_HEIGHT), G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH,
                                   SCREEN_HEIGHT, 0, 0, SCREEN_WIDTH - 1, SCREEN_COPY_TILE_HEIGHT - 1, 0, G_TX_NOMIRROR | G_TX_WRAP,
                                   G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
                /// @bug Due to the previous issue with the incorrect second cycle combiner, the devs added a 1-pixel offset to texture coordinates
                /// in this texrect to compensate for the combiner error.
                gSPTextureRectangle(gMainGfxPos++,
                                    // ulx, uly, lrx, lry
                                    0 << 2, i * a, SCREEN_WIDTH << 2, a + (i * (SCREEN_COPY_TILE_HEIGHT << 2)),
                                    // tile
                                    G_TX_RENDERTILE,
                                    // s, t, dsdx, dtdy
                                    -1 << 5, 0 << 5, 1 << 10, 1 << 10);
                gDPPipeSync(gMainGfxPos++);
            }
            break;
        default:
            // Draw the scene's background as normal
            if (gOverrideFlags & GLOBAL_OVERRIDES_DISABLE_DRAW_FRAME) {
                gDPSetColorImage(gMainGfxPos++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH, osVirtualToPhysical(nuGfxCfb_ptr));
                return;
            }

            // @recomp Everything from here to the window's sides, when it reaches past the frame's.
            opens = widescreen_camera_opens(camera);
            if (widescreen_margin() > 0) {
                gEXSetScissor(gMainGfxPos++, G_SC_NON_INTERLACE, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, -WIDE_REACH, 0,
                              WIDE_REACH, SCREEN_HEIGHT);
            }

            gDPSetDepthImage(gMainGfxPos++, OS_K0_TO_PHYSICAL(nuGfxZBuffer));
            gDPSetCycleType(gMainGfxPos++, G_CYC_FILL);
            gDPSetRenderMode(gMainGfxPos++, G_RM_NOOP, G_RM_NOOP2);
            gDPSetColorImage(gMainGfxPos++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH, OS_K0_TO_PHYSICAL(nuGfxZBuffer));
            gDPSetFillColor(gMainGfxPos++, PACK_FILL_DEPTH(G_MAXFBZ, 0));
            if (widescreen_margin() > 0) {
                gEXFillRectangle(gMainGfxPos++, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, -WIDE_REACH, 0, WIDE_REACH,
                                 SCREEN_HEIGHT - 1);
            } else {
                gDPFillRectangle(gMainGfxPos++, 0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);
            }
            gDPPipeSync(gMainGfxPos++);
            gDPSetColorImage(gMainGfxPos++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH, osVirtualToPhysical(nuGfxCfb_ptr));
            gDPSetFillColor(gMainGfxPos++, PACK_FILL_COLOR(camera->bgColor[0], camera->bgColor[1], camera->bgColor[2], 1));

            backgroundMinX = gGameStatusPtr->backgroundMinX;
            backgroundMaxX = backgroundMinX + gGameStatusPtr->backgroundMaxX;
            backgroundMinY = gGameStatusPtr->backgroundMinY;
            backgroundMaxY = backgroundMinY + gGameStatusPtr->backgroundMaxY;
            viewportStartX = camera->viewportStartX;

            if (backgroundMinX < viewportStartX) {
                backgroundMinX = viewportStartX;
            }

            if (backgroundMinY < camera->viewportStartY) {
                backgroundMinY = camera->viewportStartY;
            }

            if (backgroundMaxX > viewportStartX + camera->viewportW) {
                backgroundMaxX = viewportStartX + camera->viewportW;
            }

            if (backgroundMaxY > camera->viewportStartY + camera->viewportH) {
                backgroundMaxY = camera->viewportStartY + camera->viewportH;
            }

            if (backgroundMinX < 0) {
                backgroundMinX = 0;
            }

            if (backgroundMinY < 0) {
                backgroundMinY = 0;
            }

            if (backgroundMaxX < 1) {
                backgroundMaxX = 1;
            }

            if (backgroundMaxY < 1) {
                backgroundMaxY = 1;
            }

            if (backgroundMinX > SCREEN_WIDTH - 1) {
                backgroundMinX = SCREEN_WIDTH - 1;
            }

            if (backgroundMinY > SCREEN_HEIGHT - 1) {
                backgroundMinY = SCREEN_HEIGHT - 1;
            }

            if (backgroundMaxX > SCREEN_WIDTH) {
                backgroundMaxX = SCREEN_WIDTH;
            }

            if (backgroundMaxY > SCREEN_HEIGHT) {
                backgroundMaxY = SCREEN_HEIGHT;
            }

            if (!(gGameStatusPtr->backgroundFlags & BACKGROUND_FLAG_TEXTURE)) {
                if (opens) {
                    gEXFillRectangle(gMainGfxPos++, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, -WIDE_REACH, backgroundMinY,
                                     WIDE_REACH, backgroundMaxY - 1);
                } else {
                    gDPFillRectangle(gMainGfxPos++, backgroundMinX, backgroundMinY, backgroundMaxX - 1, backgroundMaxY - 1);
                }
            } else if (opens) {
                // @recomp The picture's copies across the window take more commands than the background's list holds
                // (backgroundGfx, 512 of them, nearly all of which the game's own copy fills): in a list of their own.
                Gfx* list = sWideBackgroundGfx[gCurrentDisplayContextIndex & 1];
                Gfx* backgroundPos = gMainGfxPos;
                gSPDisplayList(backgroundPos++, list);
                gMainGfxPos = list;
                appendGfx_background_texture();
                gSPEndDisplayList(gMainGfxPos++);
                gMainGfxPos = backgroundPos;
            } else {
                appendGfx_background_texture();
            }

            gDPPipeSync(gMainGfxPos++);
            gDPSetCycleType(gMainGfxPos++, G_CYC_FILL);
            gDPSetRenderMode(gMainGfxPos++, G_RM_NOOP, G_RM_NOOP2);
            gDPSetFillColor(gMainGfxPos++, PACK_FILL_COLOR(0, 0, 0, 1));
            gDPPipeSync(gMainGfxPos++);

            if (widescreen_margin() > 0) {
                // @recomp The borders above and below to the window's sides, and beside only a view that keeps the
                // game's frame, from the window's sides. Those two are placed from the window's centre at both ends:
                // RT64 drops a fill whose right side, before it is moved from its origin, is left of its left side
                // (RDP::fillRect), which one from the window's left side to a point left of its centre is.
                if (backgroundMinY > 0) {
                    gEXFillRectangle(gMainGfxPos++, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, -WIDE_REACH, 0, WIDE_REACH,
                                     backgroundMinY - 1);
                }
                if (!opens) {
                    gEXFillRectangle(gMainGfxPos++, G_EX_ORIGIN_CENTER, G_EX_ORIGIN_CENTER,
                                     -SCREEN_WIDTH / 2 - WIDE_REACH, backgroundMinY,
                                     backgroundMinX - 1 - SCREEN_WIDTH / 2, backgroundMaxY - 1);
                    gEXFillRectangle(gMainGfxPos++, G_EX_ORIGIN_CENTER, G_EX_ORIGIN_CENTER,
                                     backgroundMaxX - SCREEN_WIDTH / 2, backgroundMinY, SCREEN_WIDTH / 2 + WIDE_REACH,
                                     backgroundMaxY - 1);
                }
                if (backgroundMaxY < SCREEN_HEIGHT) {
                    gEXFillRectangle(gMainGfxPos++, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, -WIDE_REACH, backgroundMaxY,
                                     WIDE_REACH, SCREEN_HEIGHT - 1);
                }
                gDPSetScissor(gMainGfxPos++, G_SC_NON_INTERLACE, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
                break;
            }

            if (backgroundMinY > 0) {
                gDPFillRectangle(gMainGfxPos++, 0, 0, SCREEN_WIDTH - 1, backgroundMinY - 1);
                gDPNoOp(gMainGfxPos++);
            }

            if (backgroundMinX > 0) {
                gDPFillRectangle(gMainGfxPos++, 0, backgroundMinY, backgroundMinX - 1, backgroundMaxY - 1);
                gDPNoOp(gMainGfxPos++);
            }

            if (backgroundMaxX < SCREEN_WIDTH) {
                gDPFillRectangle(gMainGfxPos++, backgroundMaxX, backgroundMinY, SCREEN_WIDTH - 1, backgroundMaxY - 1);
                gDPNoOp(gMainGfxPos++);
            }

            if (backgroundMaxY < SCREEN_HEIGHT) {
                gDPFillRectangle(gMainGfxPos++, 0, backgroundMaxY, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);
                gDPNoOp(gMainGfxPos++);
            }
            break;
    }
    gDPPipeSync(gMainGfxPos++);
}

// The darkness around Mario in a dark room or battle (OVERLAY_WORLD_DARKNESS, OVERLAY_BATTLE_DARKNESS): the game draws
// a 64x64 stencil of two turning circles into the depth buffer, copies it to an I8 texture there and draws that over
// the camera's view, black where the stencil is. RT64 widens every framebuffer by the picture's shape, the stencil's
// too, and draws its square viewport at the game's size in the middle of it, so in a picture wider than 4:3 the
// texture the game read back held the circles squeezed into its middle and whatever was beside them: the room lit at
// both sides. So there the stencil is made on the CPU as the RDP draws it (darkness_stencil_cpu), into a texture of
// its own, and the black reaches the window's sides when the camera opens (darkness_rect_wide). At 4:3 it is the
// game's own.
extern u8 ui_stencil_blurry_circle_png[];
extern Vp DarknessStencilViewport;
extern Gfx Gfx_DarknessStencilQuad[];

// Two, so that the frame being built never writes the one the frame before reads.
static u8 sDarknessStencil[2][64 * 64] __attribute__((aligned(8)));
static s32 sDarknessStencilIndex;

// The stencil: white, then each circle (the blurry circle texture's 64 texels mirrored to 128 across a quad 32 units
// wide, dark in its middle, turned by i * 70 degrees and scaled by its pulse) blended over it in black by one less its
// texel (PM_CC_56's alpha), so that what is left is its texel, under the game's projection, whose origin is the
// stencil's bottom right corner and which shows 16 units each way. Point sampled, as the game sets.
static u8* darkness_stencil_cpu(void) {
    u8* out = sDarknessStencil[sDarknessStencilIndex];
    f32 scale[2], c[2], s[2];
    s32 i, x, y;

    sDarknessStencilIndex ^= 1;
    for (i = 0; i < 2; i++) {
        if (i == 0) {
            scale[i] = 0.9 - sin_deg(gGameStatusPtr->frameCounter) * 0.05;
        } else {
            scale[i] = 0.9 - sin_deg(gGameStatusPtr->frameCounter * 2 + i * 40) * 0.07;
        }
        c[i] = cos_deg(i * 70);
        s[i] = sin_deg(i * 70);
    }
    for (y = 0; y < 64; y++) {
        f32 wy = 16.0f - (y + 0.5f) * 0.25f;
        for (x = 0; x < 64; x++) {
            f32 wx = (x + 0.5f) * 0.25f - 16.0f;
            f32 value = 1.0f;
            for (i = 0; i < 2; i++) {
                f32 ox = (wx * c[i] + wy * s[i]) / scale[i];
                f32 oy = (wy * c[i] - wx * s[i]) / scale[i];
                s32 si, ti;
                if (ox < -16.0f || ox >= 16.0f || oy < -16.0f || oy >= 16.0f) {
                    continue;
                }
                si = (s32)((ox + 16.0f) * 4.0f);
                ti = (s32)((oy + 16.0f) * 4.0f);
                si = si >= 64 ? 127 - si : si;
                ti = ti >= 64 ? 127 - ti : ti;
                value *= ui_stencil_blurry_circle_png[ti * 64 + si] / 255.0f;
            }
            out[y * 64 + x] = (u8)(value * 255.0f + 0.5f);
        }
    }
    return out;
}

// The black from the window's left edge to its right, in up to three rectangles placed from its centre: where the
// texture's s (S10.5, s0 at the view's left side, 32 / texScale more a pixel) is below 0 or past its last texel (63.5,
// which the load's shift doubles to 127) it clamps, so those two repeat their edge's texel and the one between steps
// through the texture as the game's does. Each one's s fits its 16 bits however far the window reaches.
static void darkness_rect_wide(Camera* camera, f32 s0, s32 t0, f32 texScale) {
    f32 perPixel = 32.0f / texScale;
    s32 step = 1024.0f / texScale;
    s32 leftQ = -(SCREEN_WIDTH / 2 + widescreen_margin()) * 4, rightQ = -leftQ;
    s32 uly = camera->viewportStartY * 4, lry = (camera->viewportStartY + camera->viewportH) * 4;
    f32 leftX = camera->viewportStartX - SCREEN_WIDTH / 2;
    f32 lastS = 63.5f * 32.0f;
    s32 aQ = (leftX + (0.0f - s0) / perPixel) * 4.0f;
    s32 bQ = (leftX + (lastS - s0) / perPixel) * 4.0f;

    aQ = aQ < leftQ ? leftQ : aQ > rightQ ? rightQ : aQ;
    bQ = bQ < aQ ? aQ : bQ > rightQ ? rightQ : bQ;
    gEXSetScissor(gMainGfxPos++, G_SC_NON_INTERLACE, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, -WIDE_REACH, 0, WIDE_REACH,
                  SCREEN_HEIGHT);
    if (aQ > leftQ) {
        gEXTextureRectangle(gMainGfxPos++, G_EX_ORIGIN_CENTER, G_EX_ORIGIN_CENTER, leftQ, uly, aQ, lry,
                            G_TX_RENDERTILE, 0, t0, 0, step);
    }
    if (bQ > aQ) {
        s32 s = s0 + (aQ / 4.0f - leftX) * perPixel;
        gEXTextureRectangle(gMainGfxPos++, G_EX_ORIGIN_CENTER, G_EX_ORIGIN_CENTER, aQ, uly, bQ, lry, G_TX_RENDERTILE,
                            s, t0, step, step);
    }
    if (rightQ > bQ) {
        gEXTextureRectangle(gMainGfxPos++, G_EX_ORIGIN_CENTER, G_EX_ORIGIN_CENTER, bQ, uly, rightQ, lry,
                            G_TX_RENDERTILE, (s32)lastS, t0, 0, step);
    }
    gDPSetScissor(gMainGfxPos++, G_SC_NON_INTERLACE, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
}

// The decomp's appendGfx_darkness_stencil (src/screen_render_util.c), with the stencil and the black above in a
// picture wider than 4:3.
RECOMP_PATCH void appendGfx_darkness_stencil(s32 isWorld, s32 posX, s32 posY, f32 alpha, f32 progress) {
    Camera* camera = &gCameras[gCurrentCameraID];
    f32 texScale, s0, t0;
    s32 i;

    if (alpha == 0.0f) {
        return;
    }

    if (!isWorld) {
        texScale = (255.0f - alpha) * 10.0f / 255.0f + 0.14;
    } else {
        texScale = 0.5f;
        texScale = (255.0f - alpha) * 9.5f / 255.0f + texScale;
        posX += (camera->viewportW / 2 - posX) * (255.0f - alpha) / 255.0f;
        posY += (camera->viewportH / 2 - posY) * (255.0f - alpha) / 255.0f;
    }

    if (widescreen_margin() > 0) {
        gDPPipeSync(gMainGfxPos++);
        gDPLoadTextureTile(gMainGfxPos++, osVirtualToPhysical(darkness_stencil_cpu()), G_IM_FMT_I, G_IM_SIZ_8b, 64, 64,
                           0, 0, 63, 63, 0, G_TX_MIRROR | G_TX_CLAMP, G_TX_MIRROR | G_TX_CLAMP, 6, 6, 15, 15);
        gDPSetTileSize(gMainGfxPos++, G_TX_RENDERTILE, 0, 0, 0x01FC, 0x01FC);
        gDPPipeSync(gMainGfxPos++);
    } else {
        guOrtho(&gDisplayContext->matrixStack[gMatrixListPos], -16.0f, 0.0f, 0.0f, 16.0f, -1000.0f, 1000.0f, 1.0f);
        gSPMatrix(gMainGfxPos++, &gDisplayContext->matrixStack[gMatrixListPos++], G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
        gDPSetAlphaDither(gMainGfxPos++, G_AD_PATTERN);
        gDPSetTextureFilter(gMainGfxPos++, G_TF_POINT);
        gDPSetTexturePersp(gMainGfxPos++, G_TP_PERSP);
        gDPSetColorImage(gMainGfxPos++, G_IM_FMT_RGBA, G_IM_SIZ_32b, 64, osVirtualToPhysical(nuGfxZBuffer));
        gDPSetScissor(gMainGfxPos++, G_SC_NON_INTERLACE, 0, 0, 64, 64);
        gSPViewport(gMainGfxPos++, &DarknessStencilViewport);
        gDPSetCombineMode(gMainGfxPos++, PM_CC_CONST_1, PM_CC_CONST_1);
        guRotate(&gDisplayContext->matrixStack[gMatrixListPos], 0.0f, 0.0f, 0.0f, 1.0f);
        gSPMatrix(gMainGfxPos++, &gDisplayContext->matrixStack[gMatrixListPos++], G_MTX_PUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(gMainGfxPos++, Gfx_DarknessStencilQuad);
        gSPPopMatrix(gMainGfxPos++, G_MTX_MODELVIEW);
        gDPPipeSync(gMainGfxPos++);
        gDPSetCombineMode(gMainGfxPos++, PM_CC_56, PM_CC_56);

        for (i = 0; i < 2; i++) {
            f32 f20;

            if (i == 0) {
                f20 = 0.9 - sin_deg(gGameStatusPtr->frameCounter) * 0.05;
            } else {
                f20 = 0.9 - sin_deg(gGameStatusPtr->frameCounter * 2 + i * 40) * 0.07;
            }

            gDPSetPrimColor(gMainGfxPos++, 0, 0, 255, 255, 255, 255);
            guRotate(&gDisplayContext->matrixStack[gMatrixListPos], i * 70, 0.0f, 0.0f, 1.0f);
            gSPMatrix(gMainGfxPos++, &gDisplayContext->matrixStack[gMatrixListPos++], G_MTX_PUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            guScale(&gDisplayContext->matrixStack[gMatrixListPos], f20, f20, 1.0f);
            gSPMatrix(gMainGfxPos++, &gDisplayContext->matrixStack[gMatrixListPos++], G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
            gSPDisplayList(gMainGfxPos++, Gfx_DarknessStencilQuad);
            gSPPopMatrix(gMainGfxPos++, G_MTX_MODELVIEW);
        }

        gDPPipeSync(gMainGfxPos++);
        gDPSetTexturePersp(gMainGfxPos++, G_TP_NONE);
        gDPSetColorImage(gMainGfxPos++, G_IM_FMT_I, G_IM_SIZ_8b, 64, osVirtualToPhysical(nuGfxZBuffer));
        gDPSetScissor(gMainGfxPos++, G_SC_NON_INTERLACE, 0, 0, 64, 64);
        gDPSetCombineMode(gMainGfxPos++, PM_CC_0F, PM_CC_0F);

        for (i = 0; i < 4; i++) {
            gDPLoadTextureTile(gMainGfxPos++, osVirtualToPhysical(&nuGfxZBuffer[i * 2048]), G_IM_FMT_RGBA, G_IM_SIZ_32b, 64, 16, 0, 0, 63, 15, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
            gDPPipeSync(gMainGfxPos++);
            gSPTextureRectangle(gMainGfxPos++, 0, (i * 16) * 4, 64 * 4, (i * 16 + 16) * 4, G_TX_RENDERTILE, 0, 0, 0x400, 0x400);
            gDPPipeSync(gMainGfxPos++);
        }

        gDPLoadTextureTile(gMainGfxPos++, osVirtualToPhysical(nuGfxZBuffer), G_IM_FMT_I, G_IM_SIZ_8b, 64, 64, 0, 0, 63, 63,
                           0, G_TX_MIRROR | G_TX_CLAMP, G_TX_MIRROR | G_TX_CLAMP, 6, 6, 15, 15);
        gDPSetTileSize(gMainGfxPos++, G_TX_RENDERTILE, 0, 0, 0x01FC, 0x01FC);
        gDPPipeSync(gMainGfxPos++);

        gDPSetCycleType(gMainGfxPos++, G_CYC_FILL);
        gDPSetRenderMode(gMainGfxPos++, G_RM_NOOP, G_RM_NOOP2);
        gDPSetColorImage(gMainGfxPos++, G_IM_FMT_RGBA, G_IM_SIZ_32b, 64, osVirtualToPhysical(nuGfxZBuffer));
        gDPSetFillColor(gMainGfxPos++, GPACK_ZDZ(G_MAXFBZ, 0)<<16 | GPACK_ZDZ(G_MAXFBZ, 0));
        gDPFillRectangle(gMainGfxPos++, 0, 0, 63, 63);
        gDPPipeSync(gMainGfxPos++);
    }

    gDPSetColorImage(gMainGfxPos++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH, osVirtualToPhysical(nuGfxCfb_ptr));
    gDPSetScissor(gMainGfxPos++, G_SC_NON_INTERLACE, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    gDPSetCycleType(gMainGfxPos++, G_CYC_1CYCLE);
    gDPSetColorDither(gMainGfxPos++, G_CD_MAGICSQ);
    gDPSetAlphaDither(gMainGfxPos++, G_AD_PATTERN);
    gDPSetTextureFilter(gMainGfxPos++, G_TF_AVERAGE);
    gDPSetRenderMode(gMainGfxPos++, G_RM_CLD_SURF, G_RM_CLD_SURF2);

    if (alpha != 255.0f || isWorld == 1) {
        gDPSetCombineMode(gMainGfxPos++, PM_CC_57, PM_CC_57);
    } else {
        gDPSetCombineMode(gMainGfxPos++, PM_CC_58, PM_CC_58);
    }
    alpha *= 0.5f;
    gDPSetPrimColor(gMainGfxPos++, 0, 0, 0, 0, 0, alpha * 0.5f * progress / 255.0f);
    gDPSetEnvColor(gMainGfxPos++, 255, 255, 255, (255.0f - alpha * 0.5f) * progress / 255.0f);

    if (!isWorld) {
        s0 = (12 - posX) * 32.0f / texScale + 16.0f + 1024.0f;
        t0 = (19 - posY) * 32.0f / texScale + 16.0f + 1024.0f;
    } else {
        s0 = (9 - posX) * 32.0f / texScale + 1024.0f;
        t0 = (32 - posY) * 32.0f / texScale + 1024.0f;
    }
    if (widescreen_camera_opens(camera)) {
        darkness_rect_wide(camera, s0, t0, texScale);
    } else {
        gSPTextureRectangle(gMainGfxPos++,
                            camera->viewportStartX * 4, camera->viewportStartY * 4,
                            (camera->viewportStartX + camera->viewportW) * 4, (camera->viewportStartY + camera->viewportH) * 4,
                            G_TX_RENDERTILE, s0, t0, 1024.0f / texScale, 1024.0f / texScale);
    }
}

extern s32 screen_overlay_frontType;
extern f32 screen_overlay_frontProgress;
extern s32 screen_overlay_backType;
extern f32 screen_overlay_backProgress;
extern ScreenOverlay ScreenOverlays[2];

// The decomp's _render_transition_stencil (src/screen_overlays.c) for its two plain fades, the screen's colour and the
// view's, to the window's sides in a picture wider than 4:3 (the view's only where it opens), where the game fills its
// 4:3 frame. Returns false for the other overlays (the wipes, the stencils), which the game's own function draws.
static s32 widescreen_overlay_color(s32 type, f32 progress, ScreenOverlay* overlay) {
    Camera* camera = &gCameras[gCurrentCameraID];
    s32 uly, lry;

    if (progress == 0.0f) {
        return false;
    }
    if (type == OVERLAY_SCREEN_COLOR && widescreen_margin() > 0) {
        uly = 0;
        lry = SCREEN_HEIGHT - 1;
    } else if (type == OVERLAY_VIEWPORT_COLOR && widescreen_camera_opens(camera)) {
        uly = camera->viewportStartY;
        lry = camera->viewportStartY + camera->viewportH;
    } else {
        return false;
    }

    gDPPipeSync(gMainGfxPos++);
    gDPSetColorDither(gMainGfxPos++, G_CD_MAGICSQ);
    gDPSetAlphaDither(gMainGfxPos++, G_AD_PATTERN);
    gDPSetCycleType(gMainGfxPos++, G_CYC_1CYCLE);
    if (progress == 255.0f) {
        gDPSetRenderMode(gMainGfxPos++, CVG_DST_SAVE | G_RM_OPA_SURF, CVG_DST_SAVE | G_RM_OPA_SURF2);
    } else {
        gDPSetRenderMode(gMainGfxPos++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
    }
    gDPSetCombineMode(gMainGfxPos++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(gMainGfxPos++, 0, 0, overlay->color.r, overlay->color.g, overlay->color.b, progress);
    gEXSetScissor(gMainGfxPos++, G_SC_NON_INTERLACE, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, -WIDE_REACH, 0, WIDE_REACH,
                  SCREEN_HEIGHT);
    gEXFillRectangle(gMainGfxPos++, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, -WIDE_REACH, uly, WIDE_REACH, lry);
    gDPSetScissor(gMainGfxPos++, G_SC_NON_INTERLACE, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    gDPSetColorDither(gMainGfxPos++, G_CD_DISABLE);
    return true;
}

// The decomp's render_screen_overlay_frontUI and render_screen_overlay_backUI (src/screen_overlays.c), with the fades
// above.
RECOMP_PATCH void render_screen_overlay_frontUI(void) {
    if (screen_overlay_frontType != OVERLAY_NONE && screen_overlay_frontProgress != -1.0f &&
        gGameStatusPtr->context != CONTEXT_PAUSE &&
        !widescreen_overlay_color(screen_overlay_frontType, screen_overlay_frontProgress, &ScreenOverlays[0])) {
        _render_transition_stencil(screen_overlay_frontType, screen_overlay_frontProgress, &ScreenOverlays[0]);
    }
}

RECOMP_PATCH void render_screen_overlay_backUI(void) {
    if (screen_overlay_backType != OVERLAY_NONE && screen_overlay_backProgress != -1.0f &&
        gGameStatusPtr->context != CONTEXT_PAUSE &&
        !widescreen_overlay_color(screen_overlay_backType, screen_overlay_backProgress, &ScreenOverlays[1])) {
        _render_transition_stencil(screen_overlay_backType, screen_overlay_backProgress, &ScreenOverlays[1]);
    }
}
