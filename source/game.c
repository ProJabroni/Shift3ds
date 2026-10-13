#include "game.h"
#include <string.h>

static int fdiv(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

static bool solid_tile(const Game *g, int tx, int ty)
{
    if (tx < 0 || tx >= GW || ty < 0 || ty >= GH) return true;   // closed room
    char c = g->tiles[ty][tx];
    if (c == '#') return true;
    if (c == 'L') return g->keysLeft > 0;
    return false;
}

static bool hits(const Game *g, int x, int y)
{
    int x0 = fdiv(x, TILE), x1 = fdiv(x + PW - 1, TILE);
    int y0 = fdiv(y, TILE), y1 = fdiv(y + PH - 1, TILE);
    for (int ty = y0; ty <= y1; ty++)
        for (int tx = x0; tx <= x1; tx++)
            if (solid_tile(g, tx, ty)) return true;
    return false;
}

// Does the (shrunken) player box touch a tile of type c?
static bool touches(const Game *g, char c, int inset)
{
    int x0 = fdiv(g->px + inset, TILE), x1 = fdiv(g->px + PW - 1 - inset, TILE);
    int y0 = fdiv(g->py + inset, TILE), y1 = fdiv(g->py + PH - 1 - inset, TILE);
    for (int ty = y0; ty <= y1; ty++)
        for (int tx = x0; tx <= x1; tx++)
            if (tx >= 0 && tx < GW && ty >= 0 && ty < GH && g->tiles[ty][tx] == c)
                return true;
    return false;
}

static void collect_keys(Game *g)
{
    for (int i = 0; i < g->numKeys; i++) {
        if (g->keyMask & (1u << i)) continue;
        int tx = g->keyX[i], ty = g->keyY[i];
        int x0 = fdiv(g->px + 2, TILE), x1 = fdiv(g->px + PW - 3, TILE);
        int y0 = fdiv(g->py + 2, TILE), y1 = fdiv(g->py + PH - 3, TILE);
        if (tx >= x0 && tx <= x1 && ty >= y0 && ty <= y1) {
            g->keyMask |= 1u << i;
            g->tiles[ty][tx] = ' ';
            g->keysLeft--;
        }
    }
}

void game_set_keymask(Game *g, unsigned mask)
{
    g->keyMask = 0;
    g->keysLeft = g->numKeys;
    for (int i = 0; i < g->numKeys; i++) {
        g->tiles[g->keyY[i]][g->keyX[i]] = 'k';
        if (mask & (1u << i)) {
            g->keyMask |= 1u << i;
            g->tiles[g->keyY[i]][g->keyX[i]] = ' ';
            g->keysLeft--;
        }
    }
}

void game_sync(Game *g)
{
    g->onGround = hits(g, g->px, g->py + (g->flip ? -1 : 1));
}

void game_load(Game *g, int level)
{
    memset(g, 0, sizeof *g);
    g->level = level;
    for (int y = 0; y < GH; y++) {
        for (int x = 0; x < GW; x++) {
            char c = LEVELS[level][y][x];
            if (c == 'S') {
                // Stand on the bottom of the start tile, centred.
                g->px = x * TILE + (TILE - PW) / 2;
                g->py = y * TILE + (TILE - PH);
                c = ' ';
            } else if (c == 'k' && g->numKeys < MAX_KEYS) {
                g->keyX[g->numKeys] = x;
                g->keyY[g->numKeys] = y;
                g->numKeys++;
            }
            g->tiles[y][x] = c;
        }
        g->tiles[y][GW] = 0;
    }
    g->keysLeft = g->numKeys;
    g->flip = false;
    g->state = ST_PLAY;
    game_sync(g);
}

void game_step(Game *g, int dir, bool jump, bool shift)
{
    if (g->state != ST_PLAY) return;
    g->frames++;
    if (g->flash > 0) g->flash--;

    if (shift) {                 // rotate the room: gravity reverses
        g->flip = !g->flip;
        g->vy = 0;
        g->shifts++;
        g->flash = 10;
        game_sync(g);
    }

    int gs = g->flip ? -1 : 1;   // direction gravity pulls along level Y
    if (g->flip) dir = -dir;     // keep left/right relative to the screen

    // Horizontal movement, one pixel at a time.
    int vx = dir * MOVE_SPEED;
    int sx = vx < 0 ? -1 : 1;
    for (int i = 0; i < (vx < 0 ? -vx : vx); i++) {
        if (hits(g, g->px + sx, g->py)) break;
        g->px += sx;
    }

    // Vertical movement.
    if (jump && g->onGround) g->vy = -gs * JUMP_V;
    g->vy += gs * GRAVITY;
    if (g->vy >  MAX_FALL) g->vy =  MAX_FALL;
    if (g->vy < -MAX_FALL) g->vy = -MAX_FALL;
    int sy = g->vy < 0 ? -1 : 1;
    for (int i = 0; i < (g->vy < 0 ? -g->vy : g->vy); i++) {
        if (hits(g, g->px, g->py + sy)) { g->vy = 0; break; }
        g->py += sy;
    }
    game_sync(g);

    collect_keys(g);
    if (touches(g, '^', 3))      g->state = ST_DEAD;
    else if (touches(g, 'E', 4)) g->state = ST_WIN;
}
