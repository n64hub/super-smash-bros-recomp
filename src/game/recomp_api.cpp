// The natives the patches call (patches/syms.ld gives each a dummy address that N64Recomp turns into a call by name).

#include "recomp.h"
#include "librecomp/overlays.hpp"
#include "librecomp/helpers.hpp"
#include "librecomp/game.hpp"
#include "ultramodern/config.hpp"
#include "recompui/recompui.h"

#include <algorithm>

// The hook at syDmaLoadOverlay's start (us.toml): the code sections in the overlay's stretch of ROM are now at its
// ram_load_start. ovl is the game's SYOverlay: rom_start, rom_end, ram_load_start, then the sections' bounds.
extern "C" void ssb_load_overlay(uint8_t* rdram, gpr ovl) {
    u32 rom_start = MEM_W(0, ovl);
    u32 rom_end = MEM_W(4, ovl);
    u32 ram = MEM_W(8, ovl);
    if (rom_end > rom_start) {
        load_overlays(rom_start, ram, rom_end - rom_start);
    }
}

// recomp_get_aspect_ratio(original): the picture's aspect ratio, the window's with Aspect Ratio Expand (never narrower
// than original), else original (patches/widescreen.c).
extern "C" void recomp_get_aspect_ratio(uint8_t* rdram, recomp_context* ctx) {
    float original = _arg<0, float>(rdram, ctx);
    int width, height;
    recompui::get_window_size(width, height);
    if (ultramodern::renderer::get_graphics_config().ar_option != ultramodern::renderer::AspectRatio::Expand ||
        height <= 0) {
        _return(ctx, original);
        return;
    }
    _return(ctx, std::max(static_cast<float>(width) / height, original));
}
