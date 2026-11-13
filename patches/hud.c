// The battle HUD at the wide picture's sides (RT64's Aspect Ratio Expand), where the game draws it in its 4:3 frame:
// each player's damage and stocks spread across the picture's width as they are across the 4:3 one, so port 1's sits
// near its left edge and port 4's near its right, and the rest of the HUD (the timer, the Bonus Practices' targets or
// platforms and their timer, the 1P Game's enemy stocks, Training Mode's displays) at the side it is drawn on. The
// sides are those of the picture RecompFrontend's HUD Placement gives the HUD (16:9 by default); at 4:3, or with Aspect
// Ratio Original, RT64 places everything as the game does.
//
// The game draws its HUD as interface GObjs on display link 23, each a few sprites (SObjs) drawn as texture rectangles,
// and gcCaptureTaggedGObjs is the one place that calls a link's displays. So rather than a copy of each display, the
// patch of gcCaptureTaggedGObjs puts RT64's rectangle alignment (gEXSetRectAlign) before each of link 23's GObjs and
// takes it off after: an origin is a fraction of the picture's width (G_EX_ORIGIN_LEFT 0, G_EX_ORIGIN_RIGHT 0x400),
// and a rectangle's offset of minus that fraction of the 4:3 width keeps it where the game drew it at 4:3.
//
// - A player's damage and stocks, and a stock flying from one player's panel to another's in a team battle: the origin
//   at the panel's (or the flying stock's) own x, as a fraction of 320, so each panel keeps its share of the width and
//   its digits and stocks stay together.
// - The players' name tags and the arrows over items above the screen: none, since they follow the 3D.
// - Anything else on the link: by the middle of its visible sprites, left of 140 at the left side, right of 180 at the
//   right, and in between where the game draws it (GO!, GAME SET, the countdown, the announcements).
//
// The pause menu (link 24) and the other menus stay in the middle 4:3.

#include "patches.h"
#include "rt64_extended_gbi.h"
#include <sys/objdisplay.h>
#include <sys/objman.h>
#include <sys/taskman.h>
#include <if/interface.h>
#include <if/ifcommon.h>
#include <gm/gmcamera.h>
#include <sc/scene.h>

#define HUD_DL_LINK 23
#define HUD_SCREEN_WIDTH 320

// The proportional origin of x, in the 4:3 frame's pixels.
#define HUD_ORIGIN_AT(x) (((x) * G_EX_ORIGIN_RIGHT) / HUD_SCREEN_WIDTH)
#define HUD_NO_ALIGN -1

// The middle of a GObj's visible sprites, or -1 if it shows none.
static s32 hud_sobjs_middle(GObj* gobj) {
    SObj* sobj;
    f32 left = 100000.0F;
    f32 right = -100000.0F;

    if (gobj->obj_kind != nGCCommonAppendSObj) {
        return -1;
    }
    for (sobj = SObjGetStruct(gobj); sobj != NULL; sobj = sobj->next) {
        if (sobj->sprite.attr & SP_HIDDEN) {
            continue;
        }
        if (sobj->pos.x < left) {
            left = sobj->pos.x;
        }
        if (sobj->pos.x + sobj->sprite.width * sobj->sprite.scalex > right) {
            right = sobj->pos.x + sobj->sprite.width * sobj->sprite.scalex;
        }
    }
    if (right < left) {
        return -1;
    }
    return (s32)((left + right) * 0.5F);
}

static sb32 hud_has_process(GObj* gobj, void (*func)(GObj*)) {
    GObjProcess* gobjproc;

    for (gobjproc = gobj->gobjproc_head; gobjproc != NULL; gobjproc = gobjproc->link_next) {
        if ((gobjproc->kind == nGCProcessKindFunc) && (gobjproc->exec.func == func)) {
            return TRUE;
        }
    }
    return FALSE;
}

// Whether the scene is a battle, the only scenes whose link 23 is the HUD.
static sb32 hud_in_battle(void) {
    switch (gSCManagerSceneData.scene_curr) {
    case nSCKindVSBattle:
    case nSCKind1PGame:
    case nSCKind1PBonusStage:
    case nSCKind1PTrainingMode:
    case nSCKindAutoDemo:
        return TRUE;
    default:
        return FALSE;
    }
}

// Where a GObj on the HUD's link is anchored: an origin, or HUD_NO_ALIGN.
static s32 hud_origin(GObj* gobj) {
    s32 middle;

    if ((gobj->proc_display == ifCommonPlayerDamageProcDisplay) ||
        (gobj->proc_display == ifCommonPlayerStockMultiProcDisplay) ||
        (gobj->proc_display == ifCommonPlayerStockSingleProcDisplay)) {
        return HUD_ORIGIN_AT(gIFCommonPlayerInterface.player_pos_x[ifGetPlayer(gobj)]);
    }
    if ((gobj->proc_display == ifCommonPlayerTagProcDisplay) || (gobj->proc_display == ifCommonItemArrowProcDisplay)) {
        return HUD_NO_ALIGN;
    }
    middle = hud_sobjs_middle(gobj);

    if (middle < 0) {
        return HUD_NO_ALIGN;
    }
    if (hud_has_process(gobj, ifCommonPlayerStockStealProcUpdate)) {
        return HUD_ORIGIN_AT(middle);
    }
    if (middle < 140) {
        return G_EX_ORIGIN_LEFT;
    }
    if (middle > 180) {
        return G_EX_ORIGIN_RIGHT;
    }
    return HUD_NO_ALIGN;
}

static void hud_align(s32 origin) {
    s32 offset = -((origin * HUD_SCREEN_WIDTH * 4) / G_EX_ORIGIN_RIGHT);

    // RT64 reads its extended commands only in a list that turns them on, and turns them off at each list's start.
    gEXEnable(gSYTaskmanDLHeads[0]++);
    gEXSetRectAlign(gSYTaskmanDLHeads[0]++, origin, origin, offset, 0, offset, 0);
    // The interface camera's scissor is the battle viewport's (10 to 310 in the 4:3 frame), which would cut what lies
    // past the 4:3 frame: the same margins from the wide picture's sides, until hud_unalign gives the camera's back.
    // Not the whole picture's: RT64 widens the 3D only while its scissor covers every scissor the framebuffer has
    // drawn with, so a HUD scissor from 0 to 320 would leave the stage in the middle 4:3.
    gEXPushScissor(gSYTaskmanDLHeads[0]++);
    gEXSetScissor(gSYTaskmanDLHeads[0]++, G_SC_NON_INTERLACE, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT,
                  gGMCameraStruct.viewport_ulx, gGMCameraStruct.viewport_uly,
                  gGMCameraStruct.viewport_lrx - HUD_SCREEN_WIDTH, gGMCameraStruct.viewport_lry);
}

static void hud_unalign(void) {
    gEXSetRectAlign(gSYTaskmanDLHeads[0]++, G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, 0, 0, 0, 0);
    gEXPopScissor(gSYTaskmanDLHeads[0]++);
}

// The decomp's gcCaptureTaggedGObjs (src/sys/objdisplay.c), with the HUD's link drawn at the picture's sides.
RECOMP_PATCH void gcCaptureTaggedGObjs(GObj* camera_gobj, s32 link_id, sb32 is_tag_mask_or_id) {
    GObj* current_gobj = gGCCommonDLLinks[link_id];
    s32 origin;

    while (current_gobj != NULL) {
        if (!(current_gobj->flags & GOBJ_FLAG_HIDDEN)) {
            if (((is_tag_mask_or_id == 0) && (camera_gobj->camera_tag & current_gobj->camera_tag)) ||
                ((is_tag_mask_or_id == 1) && (camera_gobj->camera_tag == current_gobj->camera_tag))) {
                origin = ((link_id == HUD_DL_LINK) && hud_in_battle()) ? hud_origin(current_gobj) : HUD_NO_ALIGN;

                if (origin != HUD_NO_ALIGN) {
                    hud_align(origin);
                }
                dGCCurrentStatus = nGCStatusDisplaying;
                gGCCurrentDisplay = current_gobj;

                current_gobj->proc_display(current_gobj);
                dGCCurrentStatus = nGCStatusCapturing;

                current_gobj->frame_draw_last = dSYTaskmanFrameCount;

                if (origin != HUD_NO_ALIGN) {
                    hud_unalign();
                }
            }
        }
        current_gobj = current_gobj->dl_link_next;
    }
}
