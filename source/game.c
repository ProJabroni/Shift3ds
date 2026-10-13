#include "game.h"
#include <string.h>

static int fdiv(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

bool game_tile_active(const Game *g, char c)
{
    switch (c) {
        case '#': return true;
        case 'W': return !g->world;
        case 'K': return g->world;
        default:  return false;
    }
}

static bool solid_tile(const Game *g, int tx, int ty)
{
    if (tx < 0 || tx >= GW) return true;    // side walls
    if (ty < 0) return true;                // ceiling
    if (ty >= GH) return false;             // bottomless pit
    return game_tile_active(g, g->tiles[ty][tx]);
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

// Does the (slightly shrunken) player box touch a tile of type c?
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

void game_sync(Game *g)
{
    g->onGround = hits(g, g->px, g->py + 1);
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
            }
            g->tiles[y][x] = c;
        }
        g->tiles[y][GW] = 0;
    }
    g->world = false;
    g->state = ST_PLAY;
    game_sync(g);
}

void game_step(Game *g, int dir, bool jump, bool shift)
{
    if (g->state != ST_PLAY) return;
    g->frames++;
    if (g->deny > 0)  g->deny--;
    if (g->flash > 0) g->flash--;

    if (shift) {
        g->world = !g->world;
        if (hits(g, g->px, g->py)) {        // would end up inside a block
            g->world = !g->world;
            g->deny = 14;
        } else {
            g->shifts++;
            g->flash = 8;
        }
    }

    // Horizontal movement, one pixel at a time.
    int vx = dir * MOVE_SPEED;
    int sx = vx < 0 ? -1 : 1;
    for (int i = 0; i < (vx 
