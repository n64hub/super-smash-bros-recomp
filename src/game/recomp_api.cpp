// The natives us.toml's hooks call, each from the generated C of the game function it hooks, and those the patches call
// (patches/syms.ld gives each a dummy address that N64Recomp turns into a call by name).

#include "recomp.h"
#include "librecomp/overlays.hpp"
#include "librecomp/game.hpp"
#include "librecomp/helpers.hpp"
#include "ultramodern/config.hpp"
#include "recompui/recompui.h"

#include <algorithm>
#include <chrono>

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

extern "C" void yield_self_1ms(uint8_t* rdram);

// The hook at the start of each getter the game's busy-waits call (us.toml): called again within 100 us means a loop is
// spinning on it, so the other game threads (and the messages the VI and audio send them) get to run first, at most a
// millisecond. A getter called once a frame never waits.
extern "C" void ssb_spin_yield(uint8_t* rdram, int getter) {
    using clock = std::chrono::steady_clock;
    thread_local clock::time_point last_call[4];
    if (clock::now() - last_call[getter] < std::chrono::microseconds(100)) {
        yield_self_1ms(rdram);
    }
    last_call[getter] = clock::now();
}

// recomp_get_aspect_ratio(original), a native the patches call (patches/syms.ld): the picture's aspect ratio, the
// window's with Aspect Ratio Expand (never narrower than original), else original (patches/cull.c).
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

// recomp_get_hud_aspect_ratio(original), a native the patches call (patches/syms.ld): the aspect ratio of the frame
// RT64 aligns the HUD to (gEXSetRectAlign's G_EX_ORIGIN_LEFT and G_EX_ORIGIN_RIGHT), the picture's narrowed to HUD
// Placement's (original for Original, 16:9 for 16:9, the picture's for Full), as RecompFrontend sets RT64's
// extended aspect ratio from it (patches/magnify.c).
extern "C" void recomp_get_hud_aspect_ratio(uint8_t* rdram, recomp_context* ctx) {
    float original = _arg<0, float>(rdram, ctx);
    int width, height;
    recompui::get_window_size(width, height);
    const ultramodern::renderer::GraphicsConfig& config = ultramodern::renderer::get_graphics_config();
    if (config.ar_option != ultramodern::renderer::AspectRatio::Expand || height <= 0) {
        _return(ctx, original);
        return;
    }
    float picture = std::max(static_cast<float>(width) / height, original);
    switch (config.hr_option) {
    case ultramodern::renderer::HUDRatioMode::Clamp16x9:
        _return(ctx, std::clamp(16.0f / 9.0f, original, picture));
        return;
    case ultramodern::renderer::HUDRatioMode::Full:
        _return(ctx, picture);
        return;
    default:
        _return(ctx, original);
        return;
    }
}
