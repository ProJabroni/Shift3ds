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

static void tri(float x0, float y0, float x1, float y1, float x2, float y2, float z, u32 c)
{
    C2D_DrawTriangle(x0 + g_shift, y0, c, x1 + g_shift, y1, c, x2 + g_shift, y2, c, z);
}

static void disc(float x, float y, float z, float r, u32 c)
{
    C2D_DrawCircleSolid(x + g_shift, y, z, r, c);
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
// Each layer is shifted by eye*slider*depth pixels: positive depth pops out of the
// screen, negative sinks behind it.
#define LAYER(d) (g_shift = eye * slider * (d))

// ---------------------------------------------------------------- level view --

static const u32 BLACK = 0xFF0C0C10;   // ABGR
static const u32 WHITE = 0xFFF4F4F0;

// What is drawn for cell `i` (visible rows only).
static bool is_fill(const Game *g, int i)
{
    int c = g->tile[i];
    return g->world ? c == 0x00 : c == 0x80;
}

static bool fill_at(const Game *g, int col, int row)
{
    if (col < 0 || col >= COLS || row < 0 || row >= ROWS) return true;   // room edge acts as wall
    return is_fill(g, row * COLS + col);
}

// Draws horizontal runs of cells selected by `sel`.
typedef bool (*CellSel)(const Game *, int col, int row);

static bool sel_fill(const Game *g, int c, int r)  { return fill_at(g, c, r) && c >= 0 && c < COLS && r >= 0 && r < ROWS && is_fill(g, r * COLS + c); }
static bool sel_top(const Game *g, int c, int r)   { return sel_fill(g, c, r) && !fill_at(g, c, r - 1); }
static bool sel_bot(const Game *g, int c, int r)   { return sel_fill(g, c, r) && !fill_at(g, c, r + 1); }

static void runs(const Game *g, CellSel sel, float inx, float iny, float w, float h, float z, u32 col, int mode)
{
    // mode 0: whole cell inset by (inx,iny); 1: top strip h; 2: bottom strip h
    for (int r = 0; r < ROWS; r++) {
        int c = 0;
        while (c < COLS) {
            if (!sel(g, c, r)) { c++; continue; }
            int c0 = c;
            while (c < COLS && sel(g, c, r)) c++;
            float x = c0 * CELL + inx, wd = (c - c0) * CELL + w;
            float y = r * CELL + iny;
            if (mode == 2) y = r * CELL + CELL - h;
            rect(x, y, wd, mode ? h : CELL + h, z, col);
        }
    }
}

static void edge_bars(const Game *g, float z, u32 hi, u32 lo)
{
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) {
            if (!sel_fill(g, c, r)) continue;
            if (!fill_at(g, c - 1, r)) rect(c * CELL, r * CELL, 2, CELL, z, hi);
            if (!fill_at(g, c + 1, r)) rect(c * CELL + CELL - 2, r * CELL, 2, CELL, z, lo);
        }
}

static void draw_glyph_cell(const Game *g, int col, int row, int code, float eye, float slider, int tick)
{
    bool w = g->world;
    bool inv = (code & 0x80) != 0;
    int gl = code & 0x7f;
    if (g->hasKey && gl == 0x64) gl = 0x61;               // the opened force field looks empty
    u32 ink = w ? WHITE : BLACK, paper = w ? BLACK : WHITE;
    if (inv) { u32 t = ink; ink = paper; paper = t; }
    float x = col * CELL, y = row * CELL;
    bool paperBlack = paper == BLACK;
    LAYER(4);
    if (paperBlack) rect(x, y, CELL, CELL, 0.12f, BLACK);
    LAYER(8);
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
        int gx = 0;
        while (gx < 8) {
            if (!(bits & (0x80u >> gx))) { gx++; continue; }
            int g0 = gx;
            while (gx < 8 && (bits & (0x80u >> gx))) gx++;
            rect(x + g0 * PIX, y + gy * PIX, (gx - g0) * PIX, PIX, 0.2f, ink);
        }
    }
}

static void draw_level(const Game *g, float eye, float slider, int tick)
{
    bool w = g->world;
    u32 air = w ? rgb(226, 232, 246) : rgb(244, 244, 238);
    u32 dim1 = mix(air, rgb(120, 130, 160), 7), dim2 = mix(air, rgb(120, 130, 160), 12);

    // far background: dim shapes sink behind the screen
    LAYER(-9);
    rect(-60, -10, TOP_W + 120, SCR_H + 20, 0.0f, air);
    for (int i = 0; i < 6; i++)
        disc(30 + i * 85 - (tick / 6) % 85, 60 + (i * 53) % 130, 0.01f, 40 + (i * 17) % 25, dim1);
    LAYER(-5);
    for (int i = 0; i < 9; i++)
        rect(i * 56 - 20 - (tick / 3) % 56, 24 + (i * 37) % 160, 30, 30, 0.02f, dim2);
    LAYER(-2);
    for (int i = 0; i < 14; i++)
        rect(i * 32 + 6, (i % 2) ? 12 : 220, 14, 8, 0.03f, dim2);

    // extruded black ground: shadow, side slices, raised top face, bevel
    u32 shadow = mix(air, rgb(30, 30, 50), 30);
    LAYER(0);  runs(g, sel_fill, 3, 3, 0, 0, 0.05f, shadow, 0);
    LAYER(1.5f); runs(g, sel_fill, 0, 0, 0, 0, 0.06f, rgb(70, 70, 82), 0);
    LAYER(3);    runs(g, sel_fill, 0, 0, 0, 0, 0.07f, rgb(46, 46, 56), 0);
    LAYER(4.5f); runs(g, sel_fill, 0, 0, 0, 0, 0.08f, rgb(28, 28, 36), 0);
    LAYER(6);    runs(g, sel_fill, 0, 0, 0, 0, 0.12f, BLACK, 0);
    runs(g, sel_top, 0, 0, 0, 2, 0.13f, rgb(86, 86, 100), 1);
    runs(g, sel_bot, 0, 0, 0, 2, 0.13f, rgb(36, 36, 44), 2);
    edge_bars(g, 0.13f, rgb(86, 86, 100), rgb(30, 30, 38));

    // everything that is not plain air or ground: drawn from the original glyphs
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) {
            int code = g->tile[r * COLS + c];
            if (code == 0x00 || code == 0x80) continue;      // plain air / ground
            draw_glyph_cell(g, c, r, code, eye, slider, tick);
        }

    // the player: orange block, pops out the most, with a shadow on the ground plane
    float px = g->ax * PIX, py = (g->y - 0x20) * PIX, ps = 16 * PIX;
    u32 body = rgb(255, 140, 20);
    if (g->state == ST_DEAD) body = rgb(200, 40, 40);
    LAYER(3);
    rect(px + 3, py + 3, ps, ps, 0.25f, shadow);
    LAYER(11);
    rect(px, py, ps, ps, 0.3f, body);
    rect(px, py, ps, 2, 0.31f, rgb(255, 196, 120));
    rect(px, py + ps - 3, ps, 3, 0.31f, rgb(200, 90, 10));
    bool right = g->facing == 1 || g->facing == 3;
    float ex = right ? px + ps - 11 : px + 4;
    rect(ex, py + 6, 3, 5, 0.32f, rgb(20, 20, 24));
    rect(ex + 5, py + 6, 3, 5, 0.32f, rgb(20, 20, 24));
    if (g->events & EV_DENY) {
        LAYER(13);
        rect(px - 3, py - 3, ps + 6, 2, 0.4f, rgba(230, 40, 40, 220));
        rect(px - 3, py + ps + 1, ps + 6, 2, 0.4f, rgba(230, 40, 40, 220));
    }
    g_shift = 0;
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
    LAYER(14);
    rect(40, 20, 320, 66, 0.6f, rgba(0, 0, 0, 170));
    LAYER(15);
    text("SHIFT", TOP_W / 2, 20, 1.5f, rgb(255, 255, 255), 0);
    LAYER(14);
    rect(40, 176, 320, 52, 0.6f, rgba(0, 0, 0, 170));
    char buf[64];
    snprintf(buf, sizeof buf, "<  Level %d / %d  >", sel + 1, NUM_LEVELS - 1);
    LAYER(15);
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
