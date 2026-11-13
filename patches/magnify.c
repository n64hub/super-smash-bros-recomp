// The magnifying bubble at the wide picture's sides (RT64's Aspect Ratio Expand). cull.c draws a fighter wherever the
// wide picture shows it, so the bubble comes up only once the fighter is past the picture's edge; the game places the
// bubble at its 4:3 view's edge, so this puts it at the side of the picture the fighter left by, as hud.c does the HUD:
// the sides are those of the picture RecompFrontend's HUD Placement gives the HUD (recomp_get_hud_aspect_ratio), which
// are the picture's own when HUD Placement is at least as wide as the window. At 4:3, with Aspect Ratio Original or
// with HUD Placement Original, everything is where the game puts it.
//
// The bubble is drawn in three pieces, each moved its own way:
//
// - ifCommonPlayerMagnifyGetPosition places it along the view's edges, toward the fighter: here along the HUD's
//   frame's, the 4:3 view's half-width widened by how far the HUD's sides lie past the 4:3 frame's.
// - ifCommonPlayerMagnifyUpdateViewport draws its frame (ifCommonPlayerMagnifyUpdateRender: the frame's texture into
//   the z buffer as a colour image, which masks the fighter to a circle, then into the picture in the stage's fog
//   colour), then gives the fighter, drawn next, a small viewport and scissor around it. A bubble past the 4:3 view's
//   left or right edge has each of them aligned to that side (gEXSetRectAlign, gEXSetViewportAlign, gEXSetScissor
//   with G_EX_ORIGIN_LEFT or G_EX_ORIGIN_RIGHT), from the place it would have at the same distance from the 4:3
//   frame's side. A scissor aligned to a side keeps its numbers inside the 4:3 view, where one aligned to the centre
//   would reach past it: RT64 widens the stage only while its scissor covers every scissor the framebuffer has drawn
//   with (hud.c), so a centred scissor past 4:3 put the stage back in the middle 4:3 on every frame with a bubble.
//   The frame itself is drawn under the battle viewport's scissor moved to the HUD's frame's sides
//   (magnify_push_scissor, hud.c's), since the game's own, 10 to 310 in the 4:3 frame, cut it at the 4:3 frame's side.
// - ifCommonPlayerMagnifyProcDisplay draws its ring, a model under the magnifying camera's orthographic projection
//   through the battle's viewport, which RT64 widens keeping the projection's units at the 4:3 frame's pixels: the
//   ring at the bubble's own position lands at the widened place as it is, under magnify_push_scissor's scissor too.
//
// The arrows the game shows at the 4:3 view's sides while a fighter is past one (ifCommonPlayerArrows*ProcDisplay),
// drawn under their own orthographic camera the same way, move out by the same distance as the bubble.

#include "patches.h"
#include "rt64_extended_gbi.h"
#include <if/interface.h>
#include <if/ifcommon.h>
#include <ft/fighter.h>
#include <gm/gmcamera.h>
#include <sys/objdisplay.h>
#include <sys/rdp.h>

#define MAGNIFY_SCREEN_WIDTH 320

// The decomp's bubble state (src/if/ifcommon.c), declared in no header.
extern IFPlayerMagnify sIFCommonPlayerMagnifyInterface[GMCOMMON_PLAYERS_MAX];
extern u8 dIFCommonPlayerMagnifyColorsR[];
extern u8 dIFCommonPlayerMagnifyColorsG[];
extern u8 dIFCommonPlayerMagnifyColorsB[];

// How far the HUD's sides lie past the 4:3 frame's, in the 4:3 frame's pixels.
static f32 magnify_side_shift(void) {
    return (MAGNIFY_SCREEN_WIDTH / 2) * ((recomp_get_hud_aspect_ratio(4.0F / 3.0F) / (4.0F / 3.0F)) - 1.0F);
}

// The battle viewport's scissor (10 to 310 in the 4:3 frame) at the same margins from the HUD's frame's sides, as hud.c
// gives the HUD: the bubble's frame and ring are drawn under it, since the game's own cuts them at the 4:3 frame's.
static Gfx* magnify_push_scissor(Gfx* dl) {
    // RT64 reads its extended commands only in a list that turns them on (hud.c).
    gEXEnable(dl++);
    gEXPushScissor(dl++);
    gEXSetScissor(dl++, G_SC_NON_INTERLACE, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, gGMCameraStruct.viewport_ulx,
                  gGMCameraStruct.viewport_uly, gGMCameraStruct.viewport_lrx - MAGNIFY_SCREEN_WIDTH,
                  gGMCameraStruct.viewport_lry);
    return dl;
}

// The decomp's ifCommonPlayerMagnifyGetPosition (src/if/ifcommon.c), its left and right edges the HUD's frame's.
RECOMP_PATCH void ifCommonPlayerMagnifyGetPosition(f32 player_pos_x, f32 player_pos_y, Vec2f* magnify_pos) {
    f32 left;
    f32 right;
    f32 up;
    f32 down;
    f32 bak_right;
    f32 bak_up;
    f32 diag_hz;
    f32 diag_vt;
    f32 div_xy;
    f32 shift = magnify_side_shift();

    left = (-gGMCameraStruct.viewport_width / 2) - shift + (20 * gIFCommonPlayerInterface.magnify_scale) + 5;
    bak_right = right = (+gGMCameraStruct.viewport_width / 2) + shift - (20 * gIFCommonPlayerInterface.magnify_scale) - 5;
    bak_up = up = (+gGMCameraStruct.viewport_height / 2) - (20 * gIFCommonPlayerInterface.magnify_scale);
    down = (-gGMCameraStruct.viewport_height / 2) + (20 * gIFCommonPlayerInterface.magnify_scale);

    if (player_pos_x == 0.0F) {
        up = bak_up;

        if (player_pos_y > 0.0F) {
            magnify_pos->y = up;
        } else {
            magnify_pos->y = down;
        }
        magnify_pos->x = 0.0F;
    } else {
        div_xy = player_pos_y / player_pos_x;

        if (((up / right) < div_xy) || (-(up / right) > div_xy)) {
            up = bak_up;

            if (player_pos_y > 0.0F) {
                magnify_pos->y = up;
            } else {
                magnify_pos->y = down;
            }
            diag_hz = (magnify_pos->y * player_pos_x) / player_pos_y;

            right = bak_right;

            if (diag_hz < left) {
                magnify_pos->x = left;
            } else if (diag_hz > right) {
                magnify_pos->x = right;
            } else {
                magnify_pos->x = diag_hz;
            }
        } else {
            right = bak_right;

            if (player_pos_x > 0.0F) {
                magnify_pos->x = right;
            } else {
                magnify_pos->x = left;
            }
            diag_vt = (magnify_pos->x * player_pos_y) / player_pos_x;

            up = bak_up;

            if (diag_vt < down) {
                magnify_pos->y = down;
            } else if (diag_vt > up) {
                magnify_pos->y = up;
            } else {
                magnify_pos->y = diag_vt;
            }
        }
    }
}

// The decomp's ifCommonPlayerMagnifyUpdateViewport (src/if/ifcommon.c), with a bubble past the 4:3 view's left or
// right edge drawn aligned to that side of the HUD's frame.
RECOMP_PATCH void ifCommonPlayerMagnifyUpdateViewport(Gfx** dls, FTStruct* fp) {
    Gfx* dl;
    f32 magnify_x;
    f32 magnify_y;
    IFPlayerMagnify* ifmag;
    CObj* cobj;
    f32 scale;
    s32 ulx;
    s32 uly;
    s32 lrx;
    s32 lry;
    f32 edge;
    s32 origin;
    s32 origin_x;

    if (gIFCommonPlayerInterface.is_magnify_display != FALSE) {
        ifmag = &sIFCommonPlayerMagnifyInterface[fp->player];

        magnify_x = fp->magnify_pos.x;
        magnify_y = fp->magnify_pos.y;

        ifCommonPlayerMagnifyGetPosition(magnify_x, magnify_y, &ifmag->pos);

        // Past the 4:3 view's edge (the game's own right, from ifCommonPlayerMagnifyGetPosition), the bubble is drawn
        // where it would be at the same distance from the 4:3 frame's side, aligned to that side: origin_x is the
        // side's x in the 4:3 frame, which RT64 moves to the HUD's frame's side.
        edge = (gGMCameraStruct.viewport_width / 2) - (20 * gIFCommonPlayerInterface.magnify_scale) - 5;
        magnify_x = ifmag->pos.x;
        origin = G_EX_ORIGIN_NONE;
        origin_x = 0;

        if (magnify_x > edge) {
            origin = G_EX_ORIGIN_RIGHT;
            origin_x = MAGNIFY_SCREEN_WIDTH;
            magnify_x -= magnify_side_shift();
        } else if (magnify_x < -edge) {
            origin = G_EX_ORIGIN_LEFT;
            magnify_x += magnify_side_shift();
        }
        magnify_x = magnify_x + gGMCameraStruct.viewport_center_x;
        magnify_y = gGMCameraStruct.viewport_center_y - ifmag->pos.y;

        gSPMatrix(dls[0]++, &CObjGetStruct(gGCCurrentCamera)->xobjs[0]->mtx,
                  G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);

        if (gIFCommonPlayerInterface.magnify_mode != 1) {
            cobj = CObjGetStruct(gGMCameraGObj);

            gSPViewport(dls[0]++, &cobj->viewport);

            gDPSetScissor(dls[0]++, G_SC_NON_INTERLACE, gGMCameraStruct.viewport_ulx, gGMCameraStruct.viewport_uly,
                          gGMCameraStruct.viewport_lrx, gGMCameraStruct.viewport_lry);
        } else {
            gIFCommonPlayerInterface.magnify_mode = 2;
        }

        scale = (16.0F * gIFCommonPlayerInterface.magnify_scale);

        dls[0] = magnify_push_scissor(dls[0]);

        if (origin != G_EX_ORIGIN_NONE) {
            gEXSetRectAlign(dls[0]++, origin, origin, -origin_x * 4, 0, -origin_x * 4, 0);
        }
        ifCommonPlayerMagnifyUpdateRender(dls, ifmag->color_id, magnify_x - scale, magnify_y - scale);

        dl = dls[0];

        if (origin != G_EX_ORIGIN_NONE) {
            gEXSetRectAlign(dl++, G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, 0, 0, 0, 0);
        }
        gEXPopScissor(dl++);
        scale = (18.0F * gIFCommonPlayerInterface.magnify_scale);

        magnify_x -= (scale / 2);
        magnify_y -= (scale / 2);

        syRdpSetViewport(&ifmag->viewport, magnify_x, magnify_y, scale + magnify_x, scale + magnify_y);

        if (origin != G_EX_ORIGIN_NONE) {
            gEXSetViewportAlign(dl++, origin, -origin_x * 4, 0);
        }
        gSPViewport(dl++, &ifmag->viewport);

        if (origin != G_EX_ORIGIN_NONE) {
            gEXSetViewportAlign(dl++, G_EX_ORIGIN_NONE, 0, 0);
        }
        ulx = (ifmag->viewport.vp.vtrans[0] / 4) - (ifmag->viewport.vp.vscale[0] / 4);
        uly = (ifmag->viewport.vp.vtrans[1] / 4) - (ifmag->viewport.vp.vscale[1] / 4);
        lrx = (ifmag->viewport.vp.vtrans[0] / 4) + (ifmag->viewport.vp.vscale[0] / 4);
        lry = (ifmag->viewport.vp.vtrans[1] / 4) + (ifmag->viewport.vp.vscale[1] / 4);

        if (ulx < gGMCameraStruct.viewport_ulx) {
            ulx = gGMCameraStruct.viewport_ulx;
        }
        if (lrx > gGMCameraStruct.viewport_lrx) {
            lrx = gGMCameraStruct.viewport_lrx;
        }
        if (uly < gGMCameraStruct.viewport_uly) {
            uly = gGMCameraStruct.viewport_uly;
        } else if (lry > gGMCameraStruct.viewport_lry) {
            lry = gGMCameraStruct.viewport_lry;
        }
        if (origin != G_EX_ORIGIN_NONE) {
            gEXSetScissor(dl++, G_SC_NON_INTERLACE, origin, origin, ulx - origin_x, uly, lrx - origin_x, lry);
        } else {
            gDPSetScissor(dl++, G_SC_NON_INTERLACE, ulx, uly, lrx, lry);
        }
        dls[0] = dl;
    }
}

// The decomp's ifCommonPlayerMagnifyProcDisplay (src/if/ifcommon.c), its ring drawn under magnify_push_scissor's
// scissor.
RECOMP_PATCH void ifCommonPlayerMagnifyProcDisplay(FTStruct* fp) {
    GObj* interface_gobj;
    DObj* dobj;
    IFPlayerMagnify* ifmag;
    CObj* cobj;

    if (gIFCommonPlayerInterface.is_magnify_display != FALSE) {
        ifmag = &sIFCommonPlayerMagnifyInterface[fp->player];

        interface_gobj = ifmag->interface_gobj;

        dobj = DObjGetStruct(interface_gobj);

        dobj->translate.vec.f.x = ifmag->pos.x;
        dobj->translate.vec.f.y = ifmag->pos.y;

        dobj->rotate.vec.f.z = syUtilsArcTan2(fp->magnify_pos.y, fp->magnify_pos.x) - F_CST_DTOR32(90.0F);

        dobj->scale.vec.f.x = dobj->scale.vec.f.y = gIFCommonPlayerInterface.magnify_scale * 0.5F;

        cobj = CObjGetStruct(gGMCameraGObj);

        gSPViewport(gSYTaskmanDLHeads[0]++, &cobj->viewport);
        gDPSetScissor(gSYTaskmanDLHeads[0]++, G_SC_NON_INTERLACE, gGMCameraStruct.viewport_ulx,
                      gGMCameraStruct.viewport_uly, gGMCameraStruct.viewport_lrx, gGMCameraStruct.viewport_lry);
        gSYTaskmanDLHeads[0] = magnify_push_scissor(gSYTaskmanDLHeads[0]);
        gSPMatrix(gSYTaskmanDLHeads[0]++, &CObjGetStruct(gGCCurrentCamera)->xobjs[1]->mtx,
                  G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
        gSPClearGeometryMode(gSYTaskmanDLHeads[0]++, G_ZBUFFER);
        gDPPipeSync(gSYTaskmanDLHeads[0]++);
        gDPSetRenderMode(gSYTaskmanDLHeads[0]++, G_RM_AA_OPA_SURF, G_RM_AA_OPA_SURF2);
        gDPSetAlphaCompare(gSYTaskmanDLHeads[0]++, G_AC_NONE);
        gDPSetPrimColor(gSYTaskmanDLHeads[0]++, 0, 0, dIFCommonPlayerMagnifyColorsR[ifmag->color_id],
                        dIFCommonPlayerMagnifyColorsG[ifmag->color_id], dIFCommonPlayerMagnifyColorsB[ifmag->color_id],
                        0xFF);
        gcDrawDObjDLHead0(interface_gobj);
        gEXPopScissor(gSYTaskmanDLHeads[0]++);
    }
}

// An arrow drawn the bubble's distance out from where the game puts it.
static void magnify_draw_arrow(GObj* interface_gobj, f32 shift) {
    DObj* dobj = DObjGetStruct(interface_gobj);
    f32 x = dobj->translate.vec.f.x;

    dobj->translate.vec.f.x = x + shift;
    gcDrawDObjTreeForGObj(interface_gobj);
    dobj->translate.vec.f.x = x;
}

// The decomp's ifCommonPlayerArrowsLeftProcDisplay (src/if/ifcommon.c), at the HUD's frame's left side.
RECOMP_PATCH void ifCommonPlayerArrowsLeftProcDisplay(GObj* interface_gobj) {
    if (gIFCommonPlayerInterface.arrows_flags & 1) {
        magnify_draw_arrow(interface_gobj, -magnify_side_shift());
    }
}

// The decomp's ifCommonPlayerArrowsRightProcDisplay (src/if/ifcommon.c), at the HUD's frame's right side.
RECOMP_PATCH void ifCommonPlayerArrowsRightProcDisplay(GObj* interface_gobj) {
    if (gIFCommonPlayerInterface.arrows_flags & 2) {
        magnify_draw_arrow(interface_gobj, magnify_side_shift());
    }
}
