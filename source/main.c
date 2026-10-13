// Shift 3DS - rendering, input and screen flow (citro2d). Game rules live in game.c.
#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "music.h"

#define TOP_W 400
#define BOT_W 320
#define SCR_H 240
#define CELL  10.0f          // one 8x8 Atari tile = 10x10 pixels on the 3DS top screen
#define PIX   1.25f          // one Atari pixel

enum { SCR_TITLE, SCR_PLAY, SCR_END };

static C2D_TextBuf g_textBuf;
static float g_shift;        // horizontal parallax offset applied to everything drawn

static inline u32 rgb(int r, int g, int b) { return C2D_Color32(r, g, b, 255); }
static inline u32 rgba(int r, int g, int b, int a) { return C2D_Color32(r, g, b, a); }

static void rect(float x, float y, float w, float h, float z, u32 c)
{
    C2D_DrawRectSolid(x + g_shift, y, z, w, h, c);
}

// Draws text; align: -1 left, 0 centered on x, 1 right-aligned to x.
static void text(const char *s, float x, float y, float scale, u32 color, int align)
{
    C2D_Text t;
    float w, h;
    C2D_TextParse(&t, g_textBuf, s);
    C2D_TextOptimize(&t);
    C2D_TextGetDimensions(&t, scale, scale, &w, &h);
    if (align == 0) x -= w / 2;
    else if (align > 0) x -= w;
    C2D_DrawText(&t, C2D_WithColor, x + g_shift, y, 0.9f, scale, scale, color);
}

static u32 mix(u32 a, u32 b, int pct)   // pct% of b
{
    int ar = a & 255, ag = (a >> 8) & 255, ab = (a >> 16) & 255;
    int br = b & 255, bg = (b >> 8) & 255, bb = (b >> 16) & 255;
    return rgb(ar + (br - ar) * pct / 100, ag + (bg - ag) * pct / 100, ab + (bb - ab) * pct / 100);
}

// eye: +1 = left eye, -1 = right eye. slider: 0..1 from the 3D slider.
// Flat UI layers are shifted by eye*slider*d pixels: positive d pops out of the screen.
#define LAYER(d) (g_shift = eye * slider * (d))

// ------------------------------------------------------------- 3D room view --
// The room is a shallow box seen through the screen, like a diorama:
//   z = 0  is the screen itself (the front faces of the ground blocks),
//   z = -1 is the back wall.
// Perspective pulls deeper points toward the middle of the screen, so you see the
// sides of every block even with the 3D slider off. The slider then adds the matching
// left/right eye shift. Nothing sticks out of the screen, so nothing gets cut off at
// the screen edges, and the black front faces have no eye shift at all (no ghosting).

#define VX      200.0f     // vanishing point = screen centre
#define VY      120.0f
#define PERSP   0.04f      // the back wall is 4% smaller than the screen
#define STEREO  6.0f       // per-eye shift of the back wall at full slider, in pixels
#define Z_OBJ   (-0.35f)   // spikes, doors, text... stand a third of the way into the room
#define Z_PFRONT (-0.2f)   // the player is a small cube between these depths
#define Z_PBACK  (-0.6f)
#define DEPTH   0.5f       // citro2d depth value for the room (painter's order)

static float g_eye, g_slider;

static inline float pscale(float z) { return 1.0f + PERSP * z; }
static inline float pxp(float x, float z) { return VX + (x - VX) * pscale(z) + g_eye * g_slider * STEREO * z; }
static inline float pyp(float y, float z) { return VY + (y - VY) * pscale(z); }

static void prect(float x, float y, float w, float h, float z, u32 c)
{
    float s = pscale(z);
    C2D_DrawRectSolid(pxp(x, z), pyp(y, z), DEPTH, w * s, h * s, c);
}

// A face joining the edge (x0,y0)-(x1,y1) at depth zf to the same edge at depth zb,
// shaded from colour cf (front) to cb (back).
static void pface(float x0, float y0, float x1, float y1, float zf, float zb, u32 cf, u32 cb)
{
    float ax = pxp(x0, zf), ay = pyp(y0, zf), bx = pxp(x1, zf), by = pyp(y1, zf);
    float cx = pxp(x1, zb), cy = pyp(y1, zb), dx = pxp(x0, zb), dy = pyp(y0, zb);
    C2D_DrawTriangle(ax, ay, cf, bx, by, cf, cx, cy, cb, DEPTH);
    C2D_DrawTriangle(ax, ay, cf, cx, cy, cb, dx, dy, cb, DEPTH);
}

// Shading of the four kinds of side face (light comes from the top left).
typedef struct { u32 top, bottom, left, right; } FaceColors;

// Each eye has its own vanishing point: the 3D slider moves them apart sideways.
// A side face is visible to an eye only when it faces that eye's vanishing point.
static inline float eye_vx(void) { return VX - g_eye * g_slider * STEREO / PERSP; }

// Draws the visible sides of the box x0..x1, y0..y1 between depths zf and zb.
static void pbox_sides(float x0, float y0, float x1, float y1, float zf, float zb, const FaceColors *f, u32 fog, int fogPct)
{
    float vx = eye_vx();
    if (y0 > VY) pface(x0, y0, x1, y0, zf, zb, f->top,    mix(f->top, fog, fogPct));
    if (y1 < VY) pface(x0, y1, x1, y1, zf, zb, f->bottom, mix(f->bottom, fog, fogPct));
    if (x0 > vx) pface(x0, y0, x0, y1, zf, zb, f->left,   mix(f->left, fog, fogPct));
    if (x1 < vx) pface(x1, y0, x1, y1, zf, zb, f->right,  mix(f->right, fog, fogPct));
}

static const u32 BLACK = 0xFF1C1614;   // ABGR: front faces of the ground
static const u32 INK   = 0xFF1E1A18;   // objects drawn in the room
static const u32 DECAL = 0xFFEAE6E4;   // white shapes printed on the ground

// A cell is drawn as ground when its "paper" is black: in world 0 that is every
// code with bit 7 set, in world 1 every code without it. Outside the room is ground.
static bool solid_at(const Game *g, int col, int row)
{
    if (col < 0 || col >= COLS || row < 0 || row >= ROWS) return true;
    int code = g->tile[row * COLS + col];
    return ((code & 0x80) != 0) != (g->world != 0);
}

// The 8 rows of ink bits of a tile, already turned for the current world.
static void glyph_bits(const Game *g, int code, int tick, unsigned out[8])
{
    bool w = g->world;
    bool inv = (code & 0x80) != 0;
    int gl = code & 0x7f;
    if (g->hasKey && gl == 0x64) gl = 0x61;               // the opened force field looks empty
    for (int gy = 0; gy < 8; gy++) {
        unsigned bits;
        if (gl >= 0x73 && gl <= 0x76) {                      // animated button
            bits = ATARI_BUTTON[(tick / 8) & 3][gy];
            if (w) bits ^= 0xff;
        } else {
            bits = ATARI_FONT[gl][w ? 7 - gy : gy];
            if (w) { unsigned r = 0; for (int b = 0; b < 8; b++) if (bits & (1u << b)) r |= 1u << (7 - b); bits = r; }
        }
        if (inv) bits ^= 0xff;
        // bits now mark "ink" pixels (the colour opposite to the paper)
        out[gy] = bits & 0xff;
    }
}

static void draw_glyph(const Game *g, int col, int row, int code, int tick, float z, u32 color)
{
    unsigned bits[8];
    glyph_bits(g, code, tick, bits);
    float x = col * CELL, y = row * CELL;
    for (int gy = 0; gy < 8; gy++) {
        int gx = 0;
        while (gx < 8) {
            if (!(bits[gy] & (0x80u >> gx))) { gx++; continue; }
            int g0 = gx;
            while (gx < 8 && (bits[gy] & (0x80u >> gx))) gx++;
            prect(x + g0 * PIX, y + gy * PIX, (gx - g0) * PIX, PIX, z, color);
        }
    }
}

static void draw_level(const Game *g, float eye, float slider, int tick)
{
    g_eye = eye; g_slider = slider; g_shift = 0;
    bool w = g->world;
    u32 air  = w ? rgb(226, 232, 246) : rgb(240, 240, 234);
    u32 grid = mix(air, rgb(100, 110, 150), 9);

    // ground sides: dark near the screen, fading toward the wall colour at the back
    static const FaceColors ground = {
        0xFF8C7C78,   // top    (lit)           ABGR
        0xFF2E2826,   // bottom (in shadow)
        0xFF625654,   // left
        0xFF463C3A,   // right
    };

    // 1. back wall with a faint tile grid (gives the eyes something to lock onto)
    C2D_DrawRectSolid(0, 0, DEPTH, TOP_W, SCR_H, air);
    for (int c = 0; c <= COLS; c++) prect(c * CELL - 0.5f, 0, 1, SCR_H, -1, grid);
    for (int r = 0; r <= ROWS; r++) prect(0, r * CELL - 0.5f, TOP_W, 1, -1, grid);

    // 2. the inside walls of the room box (the outside counts as ground)
    pface(0, 0, TOP_W, 0, 0, -1, ground.bottom, mix(ground.bottom, air, 65));
    pface(0, SCR_H, TOP_W, SCR_H, 0, -1, ground.top, mix(ground.top, air, 65));
    pface(0, 0, 0, SCR_H, 0, -1, ground.right, mix(ground.right, air, 65));
    pface(TOP_W, 0, TOP_W, SCR_H, 0, -1, ground.left, mix(ground.left, air, 65));

    // 3. sides of the ground blocks, merged into long runs
    for (int r = 0; r < ROWS; r++) {
        float yt = r * CELL, yb = yt + CELL;
        for (int pass = 0; pass < 2; pass++) {          // 0: top faces, 1: bottom faces
            float y = pass ? yb : yt;
            if (pass ? !(y < VY) : !(y > VY)) continue;
            int c = 0;
            while (c < COLS) {
                bool on = solid_at(g, c, r) && !solid_at(g, c, pass ? r + 1 : r - 1);
                if (!on) { c++; continue; }
                int c0 = c;
                while (c < COLS && solid_at(g, c, r) && !solid_at(g, c, pass ? r + 1 : r - 1)) c++;
                u32 f = pass ? ground.bottom : ground.top;
                pface(c0 * CELL, y, c * CELL, y, 0, -1, f, mix(f, air, 65));
            }
        }
    }
    float vx = eye_vx();
    for (int c = 0; c < COLS; c++) {
        float xl = c * CELL, xr = xl + CELL;
        for (int pass = 0; pass < 2; pass++) {          // 0: left faces, 1: right faces
            float x = pass ? xr : xl;
            if (pass ? !(x < vx) : !(x > vx)) continue;
            int r = 0;
            while (r < ROWS) {
                bool on = solid_at(g, c, r) && !solid_at(g, pass ? c + 1 : c - 1, r);
                if (!on) { r++; continue; }
                int r0 = r;
                while (r < ROWS && solid_at(g, c, r) && !solid_at(g, pass ? c + 1 : c - 1, r)) r++;
                u32 f = pass ? ground.right : ground.left;
                pface(x, r0 * CELL, x, r * CELL, 0, -1, f, mix(f, air, 65));
            }
        }
    }

    // 4. things standing in the room: spikes, doors, keys, buttons, text
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) {
            int code = g->tile[r * COLS + c];
            if (code == 0x00 || code == 0x80 || solid_at(g, c, r)) continue;
            draw_glyph(g, c, r, code, tick, Z_OBJ, INK);
        }

    // 5. the player: an orange cube inside the room
    float px = g->ax * PIX, py = (g->y - 0x20) * PIX, ps = 16 * PIX;
    bool dead = g->state == ST_DEAD;
    FaceColors cube = dead
        ? (FaceColors){ rgb(240, 110, 110), rgb(120, 20, 20), rgb(200, 50, 50), rgb(160, 30, 30) }
        : (FaceColors){ rgb(255, 200, 120), rgb(170, 70, 10), rgb(235, 125, 25), rgb(200, 95, 15) };
    u32 body = dead ? rgb(210, 45, 45) : rgb(255, 145, 25);
    pbox_sides(px, py, px + ps, py + ps, Z_PFRONT, Z_PBACK, &cube, air, 20);
    prect(px, py, ps, ps, Z_PFRONT, body);
    prect(px, py, ps, 2, Z_PFRONT, dead ? rgb(240, 110, 110) : rgb(255, 200, 130));
    bool right = g->facing == 1 || g->facing == 3;
    float ex = right ? px + ps - 11 : px + 4;
    prect(ex, py + 6, 3, 5, Z_PFRONT, rgb(20, 20, 24));
    prect(ex + 5, py + 6, 3, 5, Z_PFRONT, rgb(20, 20, 24));
    if (g->events & EV_DENY) {
        prect(px - 3, py - 3, ps + 6, 2, Z_PFRONT, rgb(230, 40, 40));
        prect(px - 3, py + ps + 1, ps + 6, 2, Z_PFRONT, rgb(230, 40, 40));
    }

    // 6. front faces of the ground, on the screen plane, with their printed shapes
    for (int r = 0; r < ROWS; r++) {
        int c = 0;
        while (c < COLS) {
            if (!solid_at(g, c, r)) { c++; continue; }
            int c0 = c;
            while (c < COLS && solid_at(g, c, r)) c++;
            C2D_DrawRectSolid(c0 * CELL, r * CELL, DEPTH, (c - c0) * CELL, CELL, BLACK);
        }
    }
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) {
            int code = g->tile[r * COLS + c];
            if (code == 0x00 || code == 0x80 || !solid_at(g, c, r)) continue;
            draw_glyph(g, c, r, code, tick, 0, DECAL);
        }
}

static void draw_banner(const char *title, const char *sub)
{
    rect(60, 80, 280, 80, 0.7f, rgba(0, 0, 0, 215));
    text(title, TOP_W / 2, 88, 0.8f, rgb(255, 255, 255), 0);
    text(sub, TOP_W / 2, 126, 0.5f, rgb(215, 215, 215), 0);
}

static void draw_bottom(const Game *g, int screen, int selLevel)
{
    g_shift = 0;
    rect(0, 0, BOT_W, SCR_H, 0.0f, rgb(28, 28, 34));
    char buf[96];

    if (screen == SCR_PLAY) {
        snprintf(buf, sizeof buf, "Level %d / %d", g->level + 1, NUM_LEVELS - 1);
        text(buf, BOT_W / 2, 8, 0.6f, rgb(255, 255, 255), 0);
        int t = g->frames * 10 / 60;
        snprintf(buf, sizeof buf, "Shifts %d    Time %d.%ds", g->shifts, t / 10, t % 10);
        text(buf, BOT_W / 2, 36, 0.45f, rgb(190, 190, 200), 0);

        bool w = g->world;
        rect(20, 80, 280, 80, 0.1f, w ? rgb(235, 235, 230) : rgb(70, 70, 80));
        text("TAP TO SHIFT", BOT_W / 2, 98, 0.8f, w ? rgb(20, 20, 24) : rgb(240, 240, 240), 0);
        text("stand on the ground first", BOT_W / 2, 132, 0.42f, w ? rgb(70, 70, 80) : rgb(180, 180, 190), 0);

        rect(20, 172, 135, 52, 0.1f, rgb(60, 60, 72));
        text("RESTART", 87, 186, 0.55f, rgb(240, 240, 240), 0);
        rect(165, 172, 135, 52, 0.1f, rgb(60, 60, 72));
        text("MENU", 232, 186, 0.55f, rgb(240, 240, 240), 0);
    } else {
        text("CONTROLS", BOT_W / 2, 10, 0.7f, rgb(255, 255, 255), 0);
        const char *lines[] = {
            "Move      Circle Pad / D-Pad",
            "Jump      A  (or Up)",
            "Shift     B / X / L / R / tap screen",
            "Restart   Y",
            "Menu      Start",
        };
        for (int i = 0; i < 5; i++) text(lines[i], 22, 52 + i * 26, 0.5f, rgb(205, 205, 215), -1);
        text("SHIFT turns the whole room upside-down.", BOT_W / 2, 196, 0.42f, rgb(255, 170, 60), 0);
        text("You can only shift while standing on the ground.", BOT_W / 2, 214, 0.42f, rgb(255, 170, 60), 0);
        (void)selLevel;
    }
}

static void draw_title(const Game *preview, int sel, int tick, float eye, float slider)
{
    draw_level(preview, eye, slider, tick);
    // the panels float just in front of the screen; the room stays behind it
    LAYER(1);
    rect(40, 20, 320, 66, 0.6f, rgba(0, 0, 0, 185));
    rect(40, 176, 320, 52, 0.6f, rgba(0, 0, 0, 185));
    LAYER(2);
    text("SHIFT", TOP_W / 2, 20, 1.5f, rgb(255, 255, 255), 0);
    char buf[64];
    snprintf(buf, sizeof buf, "<  Level %d / %d  >", sel + 1, NUM_LEVELS - 1);
    text(buf, TOP_W / 2, 182, 0.6f, rgb(255, 255, 255), 0);
    text("Press A to play      START to quit", TOP_W / 2, 206, 0.45f, rgb(220, 220, 225), 0);
    g_shift = 0;
}

// ------------------------------------------------------------------ main --

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    gfxInitDefault();
    gfxSet3D(true);                 // stereoscopic 3D on the top screen
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(8192);
    C2D_Prepare();
    C3D_RenderTarget *topL = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget *topR = C2D_CreateScreenTarget(GFX_TOP, GFX_RIGHT);
    C3D_RenderTarget *bot  = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    g_textBuf = C2D_TextBufNew(4096);

    static Game g, preview;
    int screen = SCR_TITLE, sel = 0, tick = 0;
    int totalShifts = 0, totalFrames = 0, shiftBuf = 0;
    game_load(&g, 0);
    game_load(&preview, 0);
    audio_init();

    while (aptMainLoop()) {
        hidScanInput();
        u32 down = hidKeysDown(), held = hidKeysHeld();
        touchPosition tp = {0, 0};
        bool touched = (down & KEY_TOUCH) != 0;
        if (touched) hidTouchRead(&tp);
        tick++;

        if (screen == SCR_TITLE) {
            if (down & KEY_START) break;
            int old = sel;
            if (down & (KEY_LEFT | KEY_CPAD_LEFT))   sel = (sel + NUM_LEVELS - 2) % (NUM_LEVELS - 1);
            if (down & (KEY_RIGHT | KEY_CPAD_RIGHT)) sel = (sel + 1) % (NUM_LEVELS - 1);
            if (sel != old) game_load(&preview, sel);
            game_step(&preview, 0);
            if (down & (KEY_A | KEY_TOUCH)) {
                game_load(&g, sel);
                totalShifts = totalFrames = 0;
                shiftBuf = 0;
                screen = SCR_PLAY;
            }
        } else if (screen == SCR_PLAY) {
            bool restart = (down & KEY_Y) != 0;
            bool menu    = (down & KEY_START) != 0;
            bool shift   = (down & (KEY_B | KEY_X | KEY_L | KEY_R | KEY_ZL | KEY_ZR)) != 0;
            if (touched) {
                if (tp.py >= 172 && tp.py < 224 && tp.px >= 20 && tp.px < 155)       restart = true;
                else if (tp.py >= 172 && tp.py < 224 && tp.px >= 165 && tp.px < 300) menu = true;
                else shift = true;
            }
            if (shift) shiftBuf = 16;           // remember the press for a few frames

            if (menu) { screen = SCR_TITLE; game_load(&preview, sel); }
            else if (restart) { game_load(&g, g.level); shiftBuf = 0; }
            else {
                int in = 0;
                if (held & (KEY_LEFT | KEY_CPAD_LEFT))   in |= IN_LEFT;
                if (held & (KEY_RIGHT | KEY_CPAD_RIGHT)) in |= IN_RIGHT;
                if (held & (KEY_A | KEY_DUP | KEY_CPAD_UP)) in |= IN_JUMP;
                if (shiftBuf > 0) { in |= IN_SHIFT; shiftBuf--; }
                int prev = g.state;
                game_step(&g, in);
                if (g.events & (EV_SHIFT | EV_DENY)) shiftBuf = 0;
                audio_events(g.events);
                if (g.state == ST_WIN && prev != ST_WIN) { totalShifts += g.shifts; totalFrames += g.frames; }
                if (g.state == ST_WIN && g.timer == 0) {
                    int nx = game_next_level(g.level);
                    if (nx < 0) screen = SCR_END;
                    else { game_load(&g, nx); shiftBuf = 0; }
                }
            }
        } else { // SCR_END
            if (down & (KEY_A | KEY_START | KEY_TOUCH)) screen = SCR_TITLE;
        }

        // ---- draw ----
        float slider = osGet3DSliderState();   // 0.0 (off) .. 1.0 (max depth)

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TextBufClear(g_textBuf);

        for (int eye = 0; eye < 2; eye++) {
            if (eye == 1 && slider <= 0.0f) break;
            C3D_RenderTarget *t = eye ? topR : topL;
            C2D_TargetClear(t, rgb(0, 0, 0));
            C2D_SceneBegin(t);
            float e = eye ? -1.0f : 1.0f;
            if (screen == SCR_TITLE) {
                draw_title(&preview, sel, tick, e, slider);
            } else {
                draw_level(&g, e, slider, tick);
                g_shift = 0;
                if (screen == SCR_END) {
                    char buf[96];
                    int tt = totalFrames * 10 / 60;
                    snprintf(buf, sizeof buf, "%d shifts  -  %d.%d seconds", totalShifts, tt / 10, tt % 10);
                    draw_banner("ALL LEVELS CLEARED!", buf);
                } else if (g.state == ST_WIN) {
                    draw_banner("LEVEL CLEAR", "");
                } else if (g.state == ST_DEAD) {
                    draw_banner("OUCH", "");
                }
            }
        }

        C2D_TargetClear(bot, rgb(0, 0, 0));
        C2D_SceneBegin(bot);
        draw_bottom(&g, screen == SCR_PLAY ? SCR_PLAY : SCR_TITLE, sel);

        C3D_FrameEnd(0);
    }

    audio_exit();
    C2D_TextBufDelete(g_textBuf);
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}
