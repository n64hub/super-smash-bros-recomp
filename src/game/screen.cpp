// The title and the menus at the picture's full height (RT64's Aspect Ratio Expand): a 2D screen fills the window's
// height, or its width where the window is no wider than 4:3, its shape kept, centred, the rest black.
//
// The game draws every screen inside a border of its own, 10 pixels on each side: the frame's reset list
// (syRdpResetSettings) sets a scissor of 10, 10 to 310, 230, the menus' cameras a viewport of the same, and the
// sprites are clipped to it (spScissor). RT64 keeps a 2D screen at 4:3 in the middle of a wider picture and stretches a
// rectangle as wide as its scissor across the window, so in a 1280x720 window the title and the menus stood in a box
// with black above and below as well as beside it, their backdrops stretched to its width.
//
// Here each such screen is scaled about the screen's middle until its 220 lines fill the picture's 240, by 240/220, or
// in a window no wider than 4:3 until its 300 columns fill the 320, by 320/300, whichever is less. The game sends each
// frame's lists to the RSP in func_80005240 (the decomp's src/sys/taskman.c), and us.toml hooks its start:
// ssb_screen_fill reads the list it is given, in the scenes that are 2D screens (kScreens: the title, every menu and
// the screens between them, not the battles, the opening movie or How to Play). The scene is the one whose task loop
// runs (syTaskmanStartTask, hooked too), as a 1P Game battle runs with scene_curr left at its intro's. A frame's
// commands lie in the four buffers the game writes anew each frame (sSYTaskmanDLBuffers), which branch into each
// other, and its graphics heap: the walk follows a branch or a call into them and leaves the lists the game's files
// hold alone. In them:
// - each texture rectangle is scaled, its edges on whole pixels (scale_rect says why), and clipped to the 4:3 frame,
//   its texture start moved to match, and drawn through a buffer that keeps it at its shape
//   (gEXSetRectAspect(G_EX_ASPECT_ADJUST)), since a scaled backdrop is as wide as the frame, which RT64 would stretch
//   across the window. They are point sampled: scaled, a bilinear filter blurs a strip of a picture into the next and
//   stops at its edges, so a backdrop drawn in strips showed as bands. A rectangle that reads one of the game's
//   framebuffers (a picture RT64 drew, already scaled) is left as it is;
// - each fill rectangle smaller than the frame (a panel, a fade) is scaled the same way, through a buffer that keeps it
//   at its shape, and so is a fade over the whole frame. A camera's clear (the screen inside the border, in fill mode)
//   has the whole window cleared to black first, or the sides keep what a wide picture before left there. The frame's
//   own clear (in fill mode) is left to RT64, which widens it to the window;
// - each scissor is scaled the same way and clipped to the frame, so the border's 10, 10 to 310, 230 becomes 0, 0 to
//   319.75, 240: a quarter pixel inside the frame's right side, or RT64 widens what is drawn under it (a camera's
//   clear) to the window (scale_scissor);
// - each viewport is a copy scaled the same way, so a model drawn on a menu (a fighter, a stage's preview) grows with
//   it, loaded with its origin at the window's centre, or RT64 widens a model whose scissor spans the frame to the
//   window as it does the battle's (scale_viewport).
// Only what is drawn to the screen: what the game draws into another image is left as it drew it. And a scaled screen
// draws with a depth buffer of its own, cleared over the whole screen: the game's starts 10 lines before the memory
// it has, so that the lines its border keeps clear hold other data (the scene data, the battle state), and a model
// drawn into those lines, which RT64 writes back to RAM, wrote over them (a VS battle's results, its fighters
// scaled, crashed the 1P Game's next battle).
// The buffers are this file's in RDRAM, above the 4 MB the game uses. With Aspect Ratio Original nothing changes.

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "ultramodern/config.hpp"
#include "recompui/recompui.h"

#define F3DEX_GBI_2
#include "rt64_extended_gbi.h"

// gbi.h's, for gEXSetScissor.
#ifndef G_SC_NON_INTERLACE
#define G_SC_NON_INTERLACE 0
#endif

namespace {

// F3DEX2's commands this reads (the decomp's include/PR/gbi.h).
constexpr uint32_t G_DL = 0xDE, G_ENDDL = 0xDF, G_TEXRECT = 0xE4, G_FILLRECT = 0xF6, G_RDPHALF_1 = 0xE1,
                   G_RDPHALF_2 = 0xF1;
constexpr uint32_t G_SETFILLCOLOR = 0xF7, G_SETSCISSOR = 0xED, G_SETTIMG = 0xFD, G_SETCIMG = 0xFF, G_SETZIMG = 0xFE;
constexpr uint32_t G_VTX = 0x01, G_TRI1 = 0x05, G_TRI2 = 0x06, G_QUAD = 0x07;
constexpr uint32_t G_DL_NOPUSH = 1;
// G_MOVEMEM of a viewport (G_MV_VIEWPORT, 16 bytes).
constexpr uint32_t MOVEMEM_VIEWPORT_W0 = 0xDC080008;
// G_SETOTHERMODE_H's texture filter (G_MDSFT_TEXTFILT) and its point value, and its cycle type (G_MDSFT_CYCLETYPE)
// and its copy and fill values.
constexpr uint32_t SET_TEXTURE_FILTER = 0xE3001201, G_TF_POINT = 0;
constexpr uint32_t SET_CYCLE_TYPE = 0xE3000A01, G_CYC_COPY = 0x00200000, G_CYC_FILL = 0x00300000;
// G_SETOTHERMODE_H's whole word (gDPSetOtherMode), and its cycle type's and texture filter's bits.
constexpr uint32_t SET_OTHER_MODE_H = 0xE3000817, CYCLE_TYPE_BITS = 0x00300000, TEXTURE_FILTER_BITS = 0x00003000;

// gSCManagerSceneData.scene_curr, the scene the game is in (the decomp's src/sc/scdef.h, SCKind).
constexpr uint32_t SCENE_CURR = 0x800A4AD0;
// The scenes that are 2D screens: No Controller, the title, Mode Select, 1P Mode, VS Mode, VS Options, Item Switch,
// an unlock's message, Challenger Approaching, the 1P game's "VS" screen, Screen Adjust, the four character selects
// and Bonus 2's, stage select, VS results, VS Record, the character data, Data Clear, Continue?, the 1P game's stage
// clear and its scores, Congratulations, Options, Data and Sound Test.
constexpr uint8_t kScreens[] = {0,  1,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
                                21, 24, 25, 26, 47, 49, 50, 51, 55, 57, 58, 59};

// sSYTaskmanDLBuffers, the frame's four list buffers for each of the two frames in flight (start, length), and
// gSYTaskmanGraphicsHeap (id, start, end, pointer).
constexpr uint32_t DL_BUFFERS = 0x80046570, GRAPHICS_HEAP = 0x800465D8;
// The game's framebuffers (gSYFramebufferSets, three of 320x240 at 16 bits, up to the 4 MB's end).
constexpr uint32_t FRAMEBUFFERS = 0x00392A00, FRAMEBUFFERS_END = 0x00400000;
// The game's RAM: 4 MB.
constexpr uint32_t GAME_RAM = 0x00400000;

constexpr float SCREEN_WIDTH = 320.0f, SCREEN_HEIGHT = 240.0f, CENTRE_X = 160.0f, CENTRE_Y = 120.0f;
// The 2D screen inside the game's border: 300x220.
constexpr float INNER_WIDTH = 300.0f, INNER_HEIGHT = 220.0f;

// 512 KB of 128-byte buffers, used in turn: the character selects draw several hundred rectangles a frame, and the
// renderer may read a list a frame or more after the game sends it.
constexpr uint32_t BUFFER_BASE = 0x80700000;
constexpr uint32_t BUFFER_SIZE = 0x80;
constexpr uint32_t BUFFER_COUNT = 4096;
// The depth buffer a scaled screen draws with: 320x240 at 16 bits, past the buffers.
constexpr uint32_t DEPTH = 0x80780000;
// gSYVideoZBuffer, the game's depth buffer. It points 10 lines (or 20) before the memory it has, so that lines the
// border keeps clear hold other data (the decomp's src/sys/video.h): the scene data, the battle state.
constexpr uint32_t VIDEO_ZBUFFER = 0x80046670;
// The commands read from a task's lists, at most, and the calls deep they nest (F3DEX2's own limit).
constexpr uint32_t MAX_COMMANDS = 0x20000;
constexpr int MAX_DEPTH = 18;

uint8_t* g_rdram;
uint32_t g_next_buffer = 0;
float g_fill = 1.0f;

uint32_t rd_w(uint32_t addr) {
    return *reinterpret_cast<uint32_t*>(g_rdram + ((addr & 0x7FFFFF) & ~3u));
}

void wr_w(uint32_t addr, uint32_t value) {
    *reinterpret_cast<uint32_t*>(g_rdram + ((addr & 0x7FFFFF) & ~3u)) = value;
}

uint8_t rd_b(uint32_t addr) {
    return g_rdram[(addr & 0x7FFFFF) ^ 3];
}

uint32_t next_buffer() {
    uint32_t buffer = BUFFER_BASE + BUFFER_SIZE * g_next_buffer;
    g_next_buffer = (g_next_buffer + 1) % BUFFER_COUNT;
    return buffer;
}

bool in_buffers(uint32_t addr) {
    addr &= 0x1FFFFFFF;
    return addr >= (BUFFER_BASE & 0x1FFFFFFF) && addr < ((BUFFER_BASE + BUFFER_SIZE * BUFFER_COUNT) & 0x1FFFFFFF);
}

// Whether the list at addr (physical) is one the game wrote this frame: in its list buffers or its graphics heap.
bool frame_list(uint32_t addr) {
    for (uint32_t i = 0; i < 8; i++) {
        uint32_t start = rd_w(DL_BUFFERS + i * 8) & 0x1FFFFFFF, length = rd_w(DL_BUFFERS + i * 8 + 4);
        if (length != 0 && addr >= start && addr < start + length) {
            return true;
        }
    }
    uint32_t start = rd_w(GRAPHICS_HEAP + 4) & 0x1FFFFFFF, end = rd_w(GRAPHICS_HEAP + 8) & 0x1FFFFFFF;
    return addr >= start && addr < end;
}

// A display list's address as the RSP reads it, or 0 for one in another segment than 0 (the game's files' own).
uint32_t list_address(uint32_t w1) {
    if (((w1 >> 24) & 0x0F) != 0) {
        return 0;
    }
    return w1 & 0x00FFFFFF;
}

float scale_x(float x) {
    return CENTRE_X + (x - CENTRE_X) * g_fill;
}

float scale_y(float y) {
    return CENTRE_Y + (y - CENTRE_Y) * g_fill;
}

// The first whole pixel at or past v, inside 0 to limit.
int edge(float v, float limit) {
    return int(std::ceil(std::fmin(std::fmax(v, 0.0f), limit) - 0.001f));
}

// The rectangle at addr (its three commands) scaled and clipped to the 4:3 frame, its commands written at out, false
// if nothing of it is left. A rectangle's texture starts at the first whole pixel it covers, so one starting between
// two would draw its rows up to a line off: each edge is the first whole pixel at or past it, the same for two
// rectangles that meet there, so they still meet, and every pixel one covers reads its own rows of the texture. In
// copy mode its lower right corner is the last pixel it covers, not the one past it.
bool scale_rect(uint32_t addr, uint32_t out, bool copy) {
    uint32_t w0 = rd_w(addr), w1 = rd_w(addr + 4), st = rd_w(addr + 12), d = rd_w(addr + 20);
    float ulx = ((w1 >> 12) & 0xFFF) / 4.0f, uly = (w1 & 0xFFF) / 4.0f;
    float lrx = ((w0 >> 12) & 0xFFF) / 4.0f, lry = (w0 & 0xFFF) / 4.0f;
    if (copy) {
        lrx += 1.0f;
        lry += 1.0f;
    }
    float s = int16_t(st >> 16) / 32.0f, t = int16_t(st & 0xFFFF) / 32.0f;
    float dsdx = int16_t(d >> 16) / 1024.0f, dtdy = int16_t(d & 0xFFFF) / 1024.0f;

    float x0 = scale_x(ulx), y0 = scale_y(uly);
    int new_ulx = edge(x0, SCREEN_WIDTH), new_uly = edge(y0, SCREEN_HEIGHT);
    int new_lrx = edge(scale_x(lrx), SCREEN_WIDTH), new_lry = edge(scale_y(lry), SCREEN_HEIGHT);
    if (new_lrx <= new_ulx || new_lry <= new_uly) {
        return false;
    }
    float new_dsdx = dsdx / g_fill, new_dtdy = dtdy / g_fill;
    float new_s = s + (new_ulx - x0) * new_dsdx;
    float new_t = t + (new_uly - y0) * new_dtdy;
    if (copy) {
        new_lrx -= 1;
        new_lry -= 1;
    }

    wr_w(out, (w0 & 0xFF000000) | (uint32_t(new_lrx * 4) << 12) | uint32_t(new_lry * 4));
    wr_w(out + 4, (w1 & 0xFF000000) | (uint32_t(new_ulx * 4) << 12) | uint32_t(new_uly * 4));
    wr_w(out + 8, rd_w(addr + 8));
    wr_w(out + 12, (uint32_t(int(std::lround(new_s * 32))) << 16) | (uint32_t(int(std::lround(new_t * 32))) & 0xFFFF));
    wr_w(out + 16, rd_w(addr + 16));
    wr_w(out + 20, (uint32_t(int(std::lround(new_dsdx * 1024))) << 16) |
                       (uint32_t(int(std::lround(new_dtdy * 1024))) & 0xFFFF));
    return true;
}

// The fill rectangle at addr scaled and clipped to the frame, written at out, false if nothing of it is left. In fill
// and copy modes its lower right corner is the last pixel it covers.
bool scale_fill(uint32_t addr, uint32_t out, bool inclusive) {
    uint32_t w0 = rd_w(addr), w1 = rd_w(addr + 4);
    float ulx = ((w1 >> 12) & 0xFFF) / 4.0f, uly = (w1 & 0xFFF) / 4.0f;
    float lrx = ((w0 >> 12) & 0xFFF) / 4.0f, lry = (w0 & 0xFFF) / 4.0f;
    if (inclusive) {
        lrx += 1.0f;
        lry += 1.0f;
    }
    int new_ulx = edge(scale_x(ulx), SCREEN_WIDTH), new_uly = edge(scale_y(uly), SCREEN_HEIGHT);
    int new_lrx = edge(scale_x(lrx), SCREEN_WIDTH), new_lry = edge(scale_y(lry), SCREEN_HEIGHT);
    if (new_lrx <= new_ulx || new_lry <= new_uly) {
        return false;
    }
    if (inclusive) {
        new_lrx -= 1;
        new_lry -= 1;
    }
    wr_w(out, (w0 & 0xFF000000) | (uint32_t(new_lrx * 4) << 12) | uint32_t(new_lry * 4));
    wr_w(out + 4, (w1 & 0xFF000000) | (uint32_t(new_ulx * 4) << 12) | uint32_t(new_uly * 4));
    return true;
}

// Whether the fill rectangle at addr covers the screen inside the game's border (a camera's clear).
bool fills_inner(uint32_t addr) {
    uint32_t w0 = rd_w(addr), w1 = rd_w(addr + 4);
    return ((w1 >> 12) & 0xFFF) <= 10 * 4 && (w1 & 0xFFF) <= 10 * 4 && ((w0 >> 12) & 0xFFF) >= 309 * 4 &&
           (w0 & 0xFFF) >= 229 * 4;
}

// Whether the fill rectangle at addr covers the whole frame (the frame's clear).
bool fills_frame(uint32_t addr, bool inclusive) {
    uint32_t w0 = rd_w(addr), w1 = rd_w(addr + 4);
    float extra = inclusive ? 1.0f : 0.0f;
    return ((w1 >> 12) & 0xFFF) == 0 && (w1 & 0xFFF) == 0 && ((w0 >> 12) & 0xFFF) / 4.0f + extra >= SCREEN_WIDTH &&
           (w0 & 0xFFF) / 4.0f + extra >= SCREEN_HEIGHT;
}

// The rectangle at addr, size bytes of commands, replaced by a branch to a buffer that draws it scaled (by scale, which
// writes it at the buffer's next commands and says how many bytes it wrote, 0 for nothing left) at its shape, and
// branches back after it.
// fill_color, for a camera's clear, has the window cleared to black first (window_clear).
template <typename Scale>
void replace(uint32_t addr, uint32_t size, Scale scale, const uint32_t* fill_color = nullptr) {
    uint32_t buffer = next_buffer();
    uint32_t pos = buffer;
    auto put = [&pos](uint32_t a, uint32_t b) {
        wr_w(pos, a);
        wr_w(pos + 4, b);
        pos += 8;
    };
    GfxCommand c[2];
    auto put_ex = [&](int n) {
        for (int i = 0; i < n; i++) {
            put(c[i].values.word0, c[i].values.word1);
        }
    };
    gEXEnable(c);
    put_ex(1);
    if (fill_color != nullptr) {
        // The window black, past the scaled frame too: the frame no longer covers the border the camera leaves, and
        // nothing else draws beside it, so the sides kept what a wide picture before (the opening's) left there.
        gEXPushScissor(c);
        put_ex(1);
        gEXSetScissor(c, G_SC_NON_INTERLACE, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, 0, 0, 0, 240);
        put_ex(2);
        put(G_SETFILLCOLOR << 24, 0);
        gEXSetRectAlign(c, G_EX_ORIGIN_LEFT, G_EX_ORIGIN_RIGHT, 0, 0, -int(SCREEN_WIDTH) * 4, 0);
        put_ex(2);
        put((G_FILLRECT << 24) | (319u * 4 << 12) | (239u * 4), 0);
        gEXSetRectAlign(c, G_EX_ORIGIN_NONE, G_EX_ORIGIN_NONE, 0, 0, 0, 0);
        put_ex(2);
        gEXPopScissor(c);
        put_ex(1);
        put(G_SETFILLCOLOR << 24, *fill_color);
    }
    gEXSetRectAspect(c, G_EX_ASPECT_ADJUST);
    put_ex(1);
    pos += scale(pos);
    gEXSetRectAspect(c, G_EX_ASPECT_AUTO);
    put_ex(1);
    put((G_DL << 24) | (G_DL_NOPUSH << 16), (addr + size) & 0x1FFFFFFF);
    wr_w(addr, (G_DL << 24) | (G_DL_NOPUSH << 16));
    wr_w(addr + 4, buffer & 0x1FFFFFFF);
}

// The scissor at addr scaled and clipped to the frame, in place, and a quarter pixel inside the frame's right side at
// least: RT64 widens a scissor that spans the whole width the frame drew in to the window, and a fill rectangle drawn
// under it (a camera's clear, which the title's is in orange) with it. The right one, as RT64 draws the last column
// under a scissor that ends a quarter pixel before it, and not the first under one that starts a quarter pixel past
// it. Scaling a scissor again (the reset list each task calls) gives it back as it is.
void scale_scissor(uint32_t addr) {
    uint32_t w0 = rd_w(addr), w1 = rd_w(addr + 4);
    float ulx = ((w0 >> 12) & 0xFFF) / 4.0f, uly = (w0 & 0xFFF) / 4.0f;
    float lrx = ((w1 >> 12) & 0xFFF) / 4.0f, lry = (w1 & 0xFFF) / 4.0f;
    uint32_t new_ulx = uint32_t(edge(scale_x(ulx), SCREEN_WIDTH)) * 4;
    uint32_t new_uly = uint32_t(edge(scale_y(uly), SCREEN_HEIGHT)) * 4;
    uint32_t new_lrx = uint32_t(edge(scale_x(lrx), SCREEN_WIDTH)) * 4;
    uint32_t new_lry = uint32_t(edge(scale_y(lry), SCREEN_HEIGHT)) * 4;
    if (new_lrx > SCREEN_WIDTH * 4 - 1) {
        new_lrx = uint32_t(SCREEN_WIDTH * 4 - 1);
    }
    wr_w(addr, (w0 & 0xFF000000) | (new_ulx << 12) | new_uly);
    wr_w(addr + 4, (w1 & 0xFF000000) | (new_lrx << 12) | new_lry);
}

// The viewport the G_MOVEMEM at addr loads, copied to a buffer, scaled about the screen's middle, and loaded from
// there with its origin at the window's centre (gEXSetViewportAlign), through the same buffer. A viewport is four
// shorts of scale and four of translation, in quarter pixels. RT64 widens a model to the window when its viewport and
// scissor span the width of every scissor the frame drew under, unless the viewport has an origin of its own: so a
// menu's model (a fighter on character select, the Fighting Polygon Team on its 1P Game intro) is kept at the screen's
// shape and clipped to its scissor. The origin is the viewport's as it loads, so the battles' own are left as they
// are.
void scale_viewport(uint32_t addr) {
    uint32_t vp = rd_w(addr + 4);
    if ((vp & 0x1FFFFFFF) >= GAME_RAM) {
        return;
    }
    vp |= 0x80000000;
    uint32_t sx_sy = rd_w(vp), sz = rd_w(vp + 4), tx_ty = rd_w(vp + 8), tz = rd_w(vp + 12);
    auto sh = [](uint32_t w, int hi) { return float(int16_t(hi ? (w >> 16) : (w & 0xFFFF))); };
    auto pack = [](float a, float b) {
        return (uint32_t(int(std::lround(a))) << 16) | (uint32_t(int(std::lround(b))) & 0xFFFF);
    };
    float sx = sh(sx_sy, 1) * g_fill, sy = sh(sx_sy, 0) * g_fill;
    float tx = CENTRE_X * 4 + (sh(tx_ty, 1) - CENTRE_X * 4) * g_fill;
    float ty = CENTRE_Y * 4 + (sh(tx_ty, 0) - CENTRE_Y * 4) * g_fill;
    uint32_t buffer = next_buffer();
    uint32_t viewport = buffer + BUFFER_SIZE - 16;
    wr_w(viewport, pack(sx, sy));
    wr_w(viewport + 4, sz);
    wr_w(viewport + 8, pack(tx, ty));
    wr_w(viewport + 12, tz);
    uint32_t pos = buffer;
    auto put = [&pos](uint32_t a, uint32_t b) {
        wr_w(pos, a);
        wr_w(pos + 4, b);
        pos += 8;
    };
    GfxCommand c[2];
    auto put_ex = [&](int n) {
        for (int i = 0; i < n; i++) {
            put(c[i].values.word0, c[i].values.word1);
        }
    };
    gEXEnable(c);
    put_ex(1);
    // A viewport at the centre's origin is moved by half the frame's width: moved back, it stays where the game put it.
    gEXSetViewportAlign(c, G_EX_ORIGIN_CENTER, -int(CENTRE_X) * 4, 0);
    put_ex(2);
    put(MOVEMEM_VIEWPORT_W0, viewport & 0x1FFFFFFF);
    gEXSetViewportAlign(c, G_EX_ORIGIN_NONE, 0, 0);
    put_ex(2);
    put((G_DL << 24) | (G_DL_NOPUSH << 16), (addr + 8) & 0x1FFFFFFF);
    wr_w(addr, (G_DL << 24) | (G_DL_NOPUSH << 16));
    wr_w(addr + 4, buffer & 0x1FFFFFFF);
}

// Whether the colour image the game draws into is the screen: one of its framebuffers, or segment 15, which the game
// points at the framebuffer it draws (syTaskmanInitSegmentF, the decomp's src/sys/taskman.c), or none set yet.
bool on_screen(uint32_t image) {
    return image == 0 || image == 0x0F000000 || (image >= FRAMEBUFFERS && image < FRAMEBUFFERS_END);
}

// Whether w1 is the game's depth buffer.
bool game_depth(uint32_t w1) {
    return (w1 & 0x1FFFFFFF) == (rd_w(VIDEO_ZBUFFER) & 0x1FFFFFFF);
}

bool is_rect(uint32_t addr) {
    return (rd_w(addr) >> 24) == G_TEXRECT && (rd_w(addr + 8) >> 24) == G_RDPHALF_1 &&
           (rd_w(addr + 16) >> 24) == G_RDPHALF_2;
}

// Whether the next thing drawn after addr, in the same list, is a texture rectangle, not a model.
bool rect_next(uint32_t addr) {
    for (int i = 0; i < 64; i++) {
        addr += 8;
        uint32_t op = rd_w(addr) >> 24;
        if (op == G_TEXRECT) {
            return true;
        }
        if (op == G_VTX || op == G_TRI1 || op == G_TRI2 || op == G_QUAD || op == G_DL || op == G_ENDDL) {
            return false;
        }
    }
    return false;
}

// How much a screen is scaled, false if not at all: in a window no wider than 4:3 the frame fills its width; in a wider
// one, its height, and as much of the width. With Aspect Ratio Original, not at all.
bool set_fill() {
    if (ultramodern::renderer::get_graphics_config().ar_option != ultramodern::renderer::AspectRatio::Expand) {
        return false;
    }
    int width = 0, height = 0;
    recompui::get_window_size(width, height);
    float wide = 1.0f;
    if (width > 0 && height > 0) {
        wide = std::max(1.0f, (float(width) / float(height)) / (4.0f / 3.0f));
    }
    g_fill = std::fmin(SCREEN_HEIGHT / INNER_HEIGHT, SCREEN_WIDTH * wide / INNER_WIDTH);
    return g_fill > 1.0f;
}

// The scene whose task loop runs (set as each starts, below): scene_curr then, but for a 1P Game battle, which the 1P
// Game starts with scene_curr left at its intro's (sc/sc1pmode/sc1pmanager.c).
constexpr uint8_t SCENE_1P_GAME = 52;
uint8_t g_scene = 0xFF;
bool g_1p_battle_next = false;

bool screen_scene() {
    return std::find(std::begin(kScreens), std::end(kScreens), g_scene) != std::end(kScreens);
}

// The task's lists from list on, scaled: rectangles, scissors and viewports, rectangles point sampled. A branch or a
// call into the frame's own lists is followed, each list once.
void fill(uint32_t list) {
    uint32_t stack[MAX_DEPTH];
    int depth = 0;
    uint32_t called[64];
    int called_count = 0;
    uint32_t texture = 0;
    uint32_t cycle = 0;
    uint32_t image = 0;
    uint32_t fill_color = 0;
    bool fill_color_set = false;
    uint32_t addr = list & 0x1FFFFFFF;
    for (uint32_t n = 0; n < MAX_COMMANDS; n++) {
        uint32_t w0 = rd_w(addr), w1 = rd_w(addr + 4);
        uint32_t op = w0 >> 24;
        if (op == G_DL) {
            uint32_t target = list_address(w1);
            bool branch = ((w0 >> 16) & 0xFF) == G_DL_NOPUSH;
            bool follow = target != 0 && !in_buffers(w1) && frame_list(target);
            if (follow && !branch) {
                for (int i = 0; i < called_count; i++) {
                    follow = follow && called[i] != target;
                }
                follow = follow && depth < MAX_DEPTH && called_count < 64;
            }
            if (follow) {
                if (!branch) {
                    called[called_count++] = target;
                    stack[depth++] = addr + 8;
                }
                addr = target;
                continue;
            }
            if (branch && !in_buffers(w1)) {
                // A branch out of the frame's lists ends this one.
                if (depth == 0) {
                    return;
                }
                addr = stack[--depth];
                continue;
            }
            addr += 8;
            continue;
        }
        if (op == G_ENDDL) {
            if (depth == 0) {
                return;
            }
            addr = stack[--depth];
            continue;
        }
        if (op == G_SETTIMG) {
            texture = w1 & 0x1FFFFFFF;
        }
        else if (w0 == SET_CYCLE_TYPE) {
            cycle = w1;
        }
        else if (w0 == SET_OTHER_MODE_H) {
            cycle = w1 & CYCLE_TYPE_BITS;
            if ((w1 & TEXTURE_FILTER_BITS) != G_TF_POINT && rect_next(addr)) {
                wr_w(addr + 4, w1 & ~TEXTURE_FILTER_BITS);
            }
        }
        else if (w0 == SET_TEXTURE_FILTER) {
            if (w1 != G_TF_POINT && rect_next(addr)) {
                wr_w(addr + 4, G_TF_POINT);
            }
        }
        else if ((op == G_SETZIMG || op == G_SETCIMG) && game_depth(w1)) {
            // The game's depth buffer, as the depth image or as the image its clear fills: this file's own.
            wr_w(addr + 4, DEPTH);
            if (op == G_SETCIMG) {
                image = DEPTH & 0x1FFFFFFF;
            }
        }
        else if (image == (DEPTH & 0x1FFFFFFF) && op == G_FILLRECT) {
            // The depth buffer's clear, over the whole scaled screen.
            scale_fill(addr, addr, cycle == G_CYC_COPY || cycle == G_CYC_FILL);
        }
        else if (!on_screen(image) && (op == G_SETSCISSOR || w0 == MOVEMEM_VIEWPORT_W0 || op == G_FILLRECT ||
                                       op == G_TEXRECT)) {
            // Drawn into another image than the screen (a picture drawn to use as a texture): left as the game drew
            // it.
            if (op == G_TEXRECT && is_rect(addr)) {
                addr += 16;
            }
        }
        else if (op == G_SETSCISSOR) {
            scale_scissor(addr);
        }
        else if (w0 == MOVEMEM_VIEWPORT_W0) {
            scale_viewport(addr);
        }
        else if (op == G_SETCIMG) {
            image = w1 & 0x1FFFFFFF;
        }
        else if (op == G_SETFILLCOLOR) {
            fill_color = w1;
            fill_color_set = true;
        }
        else if (op == G_FILLRECT) {
            bool inclusive = cycle == G_CYC_COPY || cycle == G_CYC_FILL;
            if (cycle != G_CYC_FILL || !fills_frame(addr, inclusive)) {
                // A rectangle over the whole frame in another mode than fill (a fade) is scaled as a panel is: RT64
                // would widen it over the black beside the frame.
                bool clear = cycle == G_CYC_FILL && fill_color_set && fills_inner(addr);
                replace(addr, 8, [&](uint32_t out) { return scale_fill(addr, out, inclusive) ? 8u : 0u; },
                        clear ? &fill_color : nullptr);
            }
        }
        else if (op == G_TEXRECT && is_rect(addr)) {
            bool framebuffer = texture >= FRAMEBUFFERS && texture < FRAMEBUFFERS_END;
            if (!framebuffer) {
                bool copy = cycle == G_CYC_COPY;
                replace(addr, 24, [&](uint32_t out) { return scale_rect(addr, out, copy) ? 24u : 0u; });
            }
            addr += 16;
        }
        addr += 8;
    }
}

} // namespace

// The hook at syTaskmanStartTask's start (us.toml): a scene's task loop is about to run.
extern "C" void ssb_screen_scene_start(uint8_t* rdram) {
    g_rdram = rdram;
    g_scene = g_1p_battle_next ? SCENE_1P_GAME : rd_b(SCENE_CURR);
    g_1p_battle_next = false;
}

// The hook at sc1PGameStartScene's and sc1PBonusStageStartScene's start (us.toml): the task loop that starts next is
// a 1P Game battle or a Bonus stage, whatever scene_curr says.
extern "C" void ssb_screen_1p_battle(uint8_t* rdram) {
    g_1p_battle_next = true;
}

// The hook at func_80005240's start (us.toml): list is the start of the task the game is about to send to the RSP.
extern "C" void ssb_screen_fill(uint8_t* rdram, uint32_t list) {
    g_rdram = rdram;
    if ((list & 7) == 0 && screen_scene() && set_fill()) {
        fill(list);
    }
}
