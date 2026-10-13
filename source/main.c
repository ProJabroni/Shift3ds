// Shift 3DS - rendering, input and screen flow (citro2d). Game rules live in game.c.
#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <string.h>
#include "game.h"

#define TOP_W 400
#define BOT_W 320
#define SCR_H 240

enum { SCR_TITLE, SCR_PLAY, SCR_END };

static C2D_TextBuf g_textBuf;
static bool g_rot;   // when true, rect()/tri() draw the room rotated 180 degrees

static inline u32 rgb(int r, int g, int b) { return C2D_Color32(r, g, b, 255); }
static inline u32 rgba(int r, int g, int b, int a) { return C2D_Color32(r, g, b, a); }

static void rect(float x, float y, float w, float h, float z, u32 c)
{
    if (g_rot) { x = TOP_W - (x + w); y = SCR_H - (y + h); }
    C2D_DrawRectSolid(x, y, z, w, h, c);
}

static void tri(float x0, float y0, float x1, float y1, float x2, float y2, float z, u32 c)
{
    if (g_rot) {
        x0 = TOP_W - x0; y0 = SCR_H - y0;
        x1 = TOP_W - x1; y1 = SCR_H - y1;
        x2 = TOP_W - x2; y2 = SCR_H - y2;
    }
    C2D_DrawTriangle(x0, y0, c, x1, y1, c, x2, y2, c, z);
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
    C2D_DrawText(&t, C2D_WithColor, x, y, 0.9f, scale, scale, color);
}

// ---------------------------------------------------------------- drawing --

// Normal room: white air, black ground. Flipped room: colours invert too.
static u32 bg_color(bool flip) { return flip ? rgb(10, 10, 12) : rgb(246, 246, 242); }
static u32 fg_color(bool flip) { return flip ? rgb(246, 246, 242) : rgb(10, 10, 12); }

static bool is_solid_char(const Game *g, int tx, int ty)
{
    if (tx < 0 || tx >= GW || ty < 0 || ty >= GH) return true;
    char c = g->tiles[ty][tx];
    return c == '#' || (c == 'L' && g->keysLeft > 0);
}

static void draw_level(const Game *g)
{
    bool f = g->flip;
    u32 bg = bg_color(f), fg = fg_color(f);
    g_rot = f;
    rect(0, 0, TOP_W, SCR_H, 0.0f, bg);

    for (int ty = 0; ty < GH; ty++) {
        for (int tx = 0; tx < GW; tx++) {
            char c = g->tiles[ty][tx];
            float x = tx * TILE, y = ty * TILE;
            switch (c) {
            case '#':
                rect(x, y, TILE, TILE, 0.1f, fg);
                break;
            case 'L':
                if (g->keysLeft > 0) {          // locked: striped block
                    rect(x, y, TILE, TILE, 0.1f, fg);
                    rect(x + 3, y + 3, TILE - 6, TILE - 6, 0.11f, bg);
                    rect(x + 6, y + 6, TILE - 12, TILE - 12, 0.12f, fg);
                }
                break;
            case '^': {
                // Point away from the neighbouring solid tile.
                bool onFloor = is_solid_char(g, tx, ty + 1);
                bool onCeil  = is_solid_char(g, tx, ty - 1);
                float x0 = x, x1 = x + TILE, xm = x + TILE / 2;
                if (onFloor || !onCeil)
                    tri(x0, y + TILE, x1, y + TILE, xm, y + 2, 0.2f, fg);
                else
                    tri(x0, y, x1, y, xm, y + TILE - 2, 0.2f, fg);
                break;
            }
            case 'k':                            // key: ring with a bar
                rect(x + 3, y + 3, 10, 10, 0.2f, fg);
                rect(x + 5, y + 5, 6, 6, 0.21f, bg);
                rect(x + 7, y + 7, 2, 2, 0.22f, fg);
                break;
            case 'E':                            // door
                rect(x + 1, y, TILE - 2, TILE, 0.1f, fg);
                rect(x + 3, y + 2, TILE - 6, TILE - 2, 0.11f, bg);
                rect(x + 9, y + 8, 2, 2, 0.12f, fg);
                break;
            default: break;
            }
        }
    }

    // Player: orange block, eyes toward the floor side.
    u32 body = rgb(255, 140, 20);
    if (g->state == ST_DEAD) body = rgb(200, 40, 40);
    rect(g->px, g->py, PW, PH, 0.3f, body);
    rect(g->px + 3, g->py + 3, 2, 3, 0.31f, rgb(20, 20, 24));
    rect(g->px + 7, g->py + 3, 2, 3, 0.31f, rgb(20, 20, 24));

    // Shift flash: fades in from the previous colours.
    if (g->flash > 0) {
        u32 old = bg_color(!f);
        int a = g->flash * 22;
        g_rot = false;
        rect(0, 0, TOP_W, SCR_H, 0.5f, (old & 0x00FFFFFF) | ((u32)a << 24));
    }
    g_rot = false;
}

static void draw_banner(const char *title, const char *sub)
{
    rect(60, 80, 280, 80, 0.7f, rgba(0, 0, 0, 215));
    text(title, TOP_W / 2, 88, 0.8f, rgb(255, 255, 255), 0);
    text(sub, TOP_W / 2, 126, 0.5f, rgb(215, 215, 215), 0);
}

static void draw_bottom(const Game *g, int screen)
{
    rect(0, 0, BOT_W, SCR_H, 0.0f, rgb(28, 28, 34));
    char buf[96];

    if (screen == SCR_PLAY) {
        snprintf(buf, sizeof buf, "Level %d/%d  -  %s", g->level + 1, NUM_LEVELS, LEVEL_NAMES[g->level]);
        text(buf, BOT_W / 2, 8, 0.55f, rgb(255, 255, 255), 0);
        int t = g->frames * 10 / 60;
        if (g->numKeys > 0)
            snprintf(buf, sizeof buf, "Shifts %d   Time %d.%ds   Keys %d/%d",
                     g->shifts, t / 10, t % 10, g->numKeys - g->keysLeft, g->numKeys);
        else
            snprintf(buf, sizeof buf, "Shifts %d   Time %d.%ds", g->shifts, t / 10, t % 10);
        text(buf, BOT_W / 2, 32, 0.45f, rgb(190, 190, 200), 0);
        text(LEVEL_HINTS[g->level], BOT_W / 2, 52, 0.42f, rgb(255, 170, 60), 0);

        // Big touch button
        rect(20, 80, 280, 80, 0.1f, g->flip ? rgb(235, 235, 230) : rgb(70, 70, 80));
        text("TAP TO SHIFT", BOT_W / 2, 98, 0.8f, g->flip ? rgb(20, 20, 24) : rgb(240, 240, 240), 0);
        text("rotate the room", BOT_W / 2, 132, 0.42f, g->flip ? rgb(70, 70, 80) : rgb(180, 180, 190), 0);

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
        text("SHIFT rotates the room: gravity flips.", BOT_W / 2, 196, 0.42f, rgb(255, 170, 60), 0);
        text("Left/Right always follow the screen.", BOT_W / 2, 214, 0.42f, rgb(255, 170, 60), 0);
    }
}

static void draw_title(int sel, int tick)
{
    bool f = (tick / 90) % 2;
    g_rot = false;
    rect(0, 0, TOP_W, SCR_H, 0.0f, bg_color(f));
    rect(0, 150, TOP_W, 90, 0.05f, bg_color(!f));
    for (int i = 0; i < 5; i++) {
        bool dark = (i + tick / 30) % 2;
        rect(80 + i * 48, 44, 44, 44, 0.1f, dark ? rgb(10, 10, 12) : rgb(246, 246, 242));
        rect(86 + i * 48, 50, 32, 32, 0.11f, rgb(255, 140, 20));
    }
    text("SHIFT", TOP_W / 2, 92, 1.3f, fg_color(f), 0);
    char buf[64];
    snprintf(buf, sizeof buf, "<  Level %d: %s  >", sel + 1, LEVEL_NAMES[sel]);
    text(buf, TOP_W / 2, 170, 0.6f, fg_color(!f), 0);
    text("Press A to play      START to quit", TOP_W / 2, 204, 0.5f, fg_color(!f), 0);
}

// ------------------------------------------------------------------ main --

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    C3D_RenderTarget *top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget *bot = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    g_textBuf = C2D_TextBufNew(4096);

    static Game g;
    int screen = SCR_TITLE, sel = 0, tick = 0;
    int totalShifts = 0, totalFrames = 0;
    game_load(&g, 0);

    while (aptMainLoop()) {
        hidScanInput();
        u32 down = hidKeysDown(), held = hidKeysHeld();
        touchPosition tp = {0, 0};
        bool touched = (down & KEY_TOUCH) != 0;
        if (touched) hidTouchRead(&tp);
        tick++;

        if (screen == SCR_TITLE) {
            if (down & KEY_START) break;
            if (down & (KEY_LEFT | KEY_CPAD_LEFT))   sel = (sel + NUM_LEVELS - 1) % NUM_LEVELS;
            if (down & (KEY_RIGHT | KEY_CPAD_RIGHT)) sel = (sel + 1) % NUM_LEVELS;
            if (down & (KEY_A | KEY_TOUCH)) {
                game_load(&g, sel);
                totalShifts = totalFrames = 0;
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

            if (menu) { screen = SCR_TITLE; }
            else if (g.state == ST_PLAY) {
                if (restart) game_load(&g, g.level);
                else {
                    int dir = 0;
                    if (held & (KEY_LEFT | KEY_CPAD_LEFT))   dir -= 1;
                    if (held & (KEY_RIGHT | KEY_CPAD_RIGHT)) dir += 1;
                    bool jump = (held & (KEY_A | KEY_DUP | KEY_CPAD_UP)) != 0;
                    game_step(&g, dir, jump, shift);
                }
            } else if (g.state == ST_DEAD) {
                if (restart || (down & KEY_A) || (touched && !menu)) game_load(&g, g.level);
            } else if (g.state == ST_WIN) {
                if ((down & KEY_A) || (touched && !menu)) {
                    totalShifts += g.shifts;
                    totalFrames += g.frames;
                    if (g.level + 1 >= NUM_LEVELS) screen = SCR_END;
                    else game_load(&g, g.level + 1);
                }
            }
        } else { // SCR_END
            if (down & (KEY_A | KEY_START | KEY_TOUCH)) screen = SCR_TITLE;
        }

        // ---- draw ----
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TextBufClear(g_textBuf);

        C2D_TargetClear(top, rgb(0, 0, 0));
        C2D_SceneBegin(top);
        if (screen == SCR_TITLE) {
            draw_title(sel, tick);
        } else {
            draw_level(&g);
            if (screen == SCR_END) {
                char buf[96];
                int t = totalFrames * 10 / 60;
                snprintf(buf, sizeof buf, "%d shifts  -  %d.%d seconds", totalShifts, t / 10, t % 10);
                draw_banner("ALL LEVELS CLEARED!", buf);
            } else if (g.state == ST_WIN) {
                draw_banner("LEVEL CLEAR", "A / tap: next level");
            } else if (g.state == ST_DEAD) {
                draw_banner("OUCH", "A / Y / tap: try again");
            }
        }

        C2D_TargetClear(bot, rgb(0, 0, 0));
        C2D_SceneBegin(bot);
        draw_bottom(&g, screen == SCR_PLAY ? SCR_PLAY : SCR_TITLE);

        C3D_FrameEnd(0);
    }

    C2D_TextBufDelete(g_textBuf);
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}
