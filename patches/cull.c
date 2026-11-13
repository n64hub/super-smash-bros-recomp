// The battle's 3D at the wide picture's edges (RT64's Aspect Ratio Expand): the game draws a fighter only while the
// camera sees it inside the 4:3 view, and past it a magnifying bubble at the view's edge, so in a wider window a fighter
// in the picture's sides vanished into a bubble at the 4:3 edge. gmCameraCheckTargetInBounds, the check the fighters'
// display makes (ftdisplaymain.c), takes the picture's width instead, so a fighter is drawn wherever the picture shows
// it and the bubble comes up only past the picture's edge, where the game puts it, at the 4:3 view's edge.
//
// The players' name tags and the arrows over items make the same check for where they draw their sprite, at the 4:3
// frame's coordinates, so while the HUD's link draws (patches/hud.c) the check stays the 4:3 view's: a tag is hidden
// past it as before, never drawn off the frame.

#include "patches.h"
#include <gm/gmcamera.h>
#include <gm/gmtypes.h>

// Set by gcCaptureTaggedGObjs (hud.c) while it draws the HUD's link.
extern sb32 cull_hud_drawing;

// The decomp's gmCameraCheckTargetInBounds (src/gm/gmcamera.c), its width widened to the picture's.
RECOMP_PATCH sb32 gmCameraCheckTargetInBounds(f32 pos_x, f32 pos_y) {
    f32 viewport_width = (gGMCameraStruct.viewport_width / 2);
    f32 viewport_height = (gGMCameraStruct.viewport_height / 2);

    if (!cull_hud_drawing) {
        viewport_width *= recomp_get_aspect_ratio(4.0F / 3.0F) / (4.0F / 3.0F);
    }
    if ((pos_x < -viewport_width) || (pos_x > viewport_width) || (pos_y < -viewport_height) ||
        (pos_y > viewport_height)) {
        return FALSE;
    }
    return TRUE;
}
