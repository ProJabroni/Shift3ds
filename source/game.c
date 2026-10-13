// Port of the Atari 8-bit "Shift" game rules. See game.h.
#include "game.h"
#include <string.h>

// ---- hinged bars (the swinging 4-tile platforms the buttons move) ----------

static int bar_step(int b0)
{
    switch (b0 & ~1) {
    case 0:  return -1;
    case 2:  return 1;
    case 4:  return -40;
    default: return 40;
    }
}

static int bar_flip(int v)
{
    if ((v & 3) == 1 || (v & 3) == 2) v ^= 3;
    if ((v & 0xc) == 4 || (v & 0xc) == 8) v ^= 0xc;
    return v;
}

static void bar_erase(Game *g, int k)
{
    Bar *b = &g->bar[k];
    if (!b->b0) return;
    int pos = b->cell, st = bar_step(b->b0);
    for (int i = 0; i < 4; i++) {
        pos += st;
        if (pos >= 0 && pos < CELLS) g->tile[pos] = b->back[i];
    }
}

static void vertical(Game *g);

static void bar_draw(Game *g, int k, bool physics)
{
    Bar *b = &g->bar[k];
    if (!b->b0) return;
    int code = (b->b0 & 3) ? 0x4d : 0x51, dec = 0;
    if ((b->b0 & 0x0a) == 0) { dec = 1; code += 3; }
    if (g->world) {
        if ((b->b0 & 0x0a) == 0) { code -= 3; dec = 0; }
        else { code += 3; dec = 1; }
    }
    int pos = b->cell, st = bar_step(b->b0);
    for (int i = 0; i < 4; i++) {
        pos += st;
        if (pos >= 0 && pos < CELLS) {
            b->back[i] = g->tile[pos];
            g->tile[pos] = (unsigned char)code;
        }
        code += dec ? -1 : 1;
        if (physics) { vertical(g); vertical(g); }
    }
}

// ---- tile probing ----------------------------------------------------------

static int tile_at(const Game *g, int row, int col)
{
    int i = row * COLS + col;
    if (row < 0 || i < 0 || i >= CELLS) return 0x20;     // outside the room: empty
    return g->tile[i];
}

static void load_level(Game *g, int level);

// 1 = the player may enter this tile, 0 = it blocks. Has the original's side
// effects (spikes, exit, key, buttons, secret).
static int probe_tile(Game *g, int code)
{
    int t = g->types[code] & 0xf0;
    if (t == g->cls || t == 0xd0 || t == 0x30) return 0;
    switch (t) {
    case 0xf0:                                   // spikes
        if (g->fall || g->probe) return 1;
        if (g->state == ST_PLAY) { g->state = ST_DEAD; g->timer = 70; g->events |= EV_DIE; }
        return 0;
    case 0xe0:                                   // exit door
        g->exitFlag = 1;
        return 1;
    case 0xc0:                                   // key: opens the force-field gate
        if (g->hasKey) return 1;
        g->hasKey = 1;
        g->types[0x64] = 0x00; g->types[0xe4] = 0x10;
        g->events |= EV_KEY;
        return g->jump < 0;
    case 0x80: case 0x90: case 0xa0: case 0xb0: { // button k swings bar k
        if (g->cool12) return 1;
        int k = (t >> 4) - 8;
        g->cool12 = 0x19;
        g->events |= EV_BUTTON;
        if (k < g->numBars) {
            bar_erase(g, k);
            g->bar[k].b0 ^= g->bar[k].b1;
            bar_draw(g, k, true);
        }
        return g->jump < 0;
    }
    case 0x40:                                   // secret tile
        g->gotoLevel = 36;
        return 1;
    default:
        return 1;
    }
}

static int row_of(int y) { return (((y - 0x20) & 0xff) & 0xf8) >> 3; }

static int probe_cell(Game *g, int row, int col)
{
    return probe_tile(g, tile_at(g, row, col));
}

// Two tiles side by side starting under pixel (ax + xoff), at scanline y.
// 1 only if both are passable.
static int probe_pair(Game *g, int xoff, int y)
{
    int row = row_of(y), col = (g->ax + xoff) >> 3;
    if (!probe_cell(g, row, col)) return 0;
    return probe_cell(g, row, col + 1);
}

// The tiles beside the player at column c.
static int probe_side(Game *g, int c)
{
    int row = row_of(g->y);
    if ((g->y & 7) < 4)
        if (!probe_cell(g, row, c)) return 0;
    return probe_cell(g, row + 1, c);
}

// ---- movement --------------------------------------------------------------

static void fall_check(Game *g)
{
    if (!g->fall && g->jump < 0) {
        g->probe = 1;
        if (probe_pair(g, 3, g->y + 0x10)) { g->fall = 1; g->jump = -1; }
    }
    g->probe = 0;
}

static void align_and_face(Game *g)
{
    g->y = (((g->y - 0x20) & ~7) + 0x20);
    g->facing = (g->facing == 0 || g->facing == 2) ? 0 : 1;
}

static void move_h(Game *g, int newax, int right)
{
    int c = (newax + (right ? 15 : 0)) >> 3;
    int saveax = g->ax;
    (void)saveax;
    if (probe_side(g, c)) {
        g->ax = newax;
        fall_check(g);
    }
}

static const int RISE[3]  = {0, 1, 2};      // pixels per frame for phases 0..2
static const int COUNT[3] = {4, 8, 6};      // frames in each phase

static void vertical(Game *g)
{
    if (g->jump >= 0) {                     // rising
        int ph = g->jump;
        int newy = g->y - RISE[ph];
        int ok;
        if (ph == 0) ok = 1;
        else if (newy < 0x20) ok = 0;
        else ok = probe_pair(g, 3, newy);
        if (!ok) { g->jump = 0; g->jcnt = 4; return; }
        g->y = newy;
        if (--g->jcnt != 0) return;
        ph--;
        g->jump = ph;
        if (ph < 0) {
            fall_check(g);
            if (!g->fall) align_and_face(g);
            return;
        }
        g->jcnt = COUNT[ph];
        return;
    }
    if (!g->fall) return;
    int x = 1;
    g->jcnt >>= 1;
    if (g->jcnt == 0) x = 2;
    g->y += x;
    if (g->y >= 0x20 + 200) {                        // fell out of the room
        if (g->state == ST_PLAY) { g->state = ST_DEAD; g->timer = 40; g->events |= EV_DIE; }
        return;
    }
    if (probe_pair(g, 3, g->y - 1 + 0x10)) return;   // still airborne
    g->fall = 0;                                     // landed
    align_and_face(g);
    g->cool11 = 0x0c;
    g->events |= EV_LAND;
    probe_pair(g, 3, g->y + 0x0f);
}

static void do_shift(Game *g)
{
    if (g->cool11 || g->fall || g->jump >= 0) return;
    if (g->level < 2 || g->level == 36) return;
    int row = row_of(g->y + 0x10), col = (g->ax + 3) >> 3;
    if ((g->types[tile_at(g, row, col)] & 0xf0) != g->cls ||
        (g->types[tile_at(g, row, col + 1)] & 0xf0) != g->cls) {
        g->events |= EV_DENY;
        return;
    }
    g->cool11 = 0x0c;
    for (int k = 0; k < g->numBars; k++) bar_erase(g, k);
    for (int i = 0; i < CELLS / 2; i++) {
        unsigned char t = g->tile[i];
        g->tile[i] = g->tile[CELLS - 1 - i];
        g->tile[CELLS - 1 - i] = t;
    }
    g->cls ^= 0x10;
    g->world ^= 1;
    g->y = (0xe8 - g->y) & 0xff;
    g->ax = 304 - g->ax;
    for (int k = 0; k < g->numBars; k++) {
        Bar *b = &g->bar[k];
        if (!b->b0) continue;
        b->cell = CELLS - 1 - b->cell;
        b->b0 = bar_flip(b->b0);
        b->b1 = bar_flip(b->b1);
    }
    for (int k = 0; k < g->numBars; k++) bar_draw(g, k, false);
    g->shifts++;
    g->events |= EV_SHIFT;
}

static void process_input(Game *g, int in)
{
    if (!in) return;
    // up
    if ((in & IN_JUMP) && g->jump < 0 && !g->fall && g->level != 0) {
        int ny = g->y - 2, ok = 0;
        if (ny >= 0x20) ok = probe_pair(g, 3, ny);
        if (ok) { g->jump = 2; g->jcnt = 6; g->events |= EV_JUMP; }
    }
    if (in & IN_LEFT) {
        g->facing = 2;
        if (g->ax - 2 >= 0) move_h(g, g->ax - 2, 0);
    } else if (in & IN_RIGHT) {
        g->facing = 3;
        if (g->ax + 2 < 304) move_h(g, g->ax + 2, 1);
    }
    if (in & IN_SHIFT) do_shift(g);
}

// ---- level loading ---------------------------------------------------------

static void load_level(Game *g, int level)
{
    const AtariLevel *L = &ATARI_LEVELS[level];
    memset(g, 0, sizeof *g);
    g->level = level;
    int pos = 0;
    for (const unsigned char *p = L->rle; !(p[0] == 0xff && p[1] == 0xff); p += 2)
        for (int i = 0; i < p[1] && pos < CELLS; i++) g->tile[pos++] = p[0];
    memcpy(g->types, ATARI_TYPES, 256);
    g->ax = L->startX;
    g->y = L->startY + 0x20;
    g->cls = 0x10;
    g->jump = -1;
    g->gotoLevel = -1;
    g->numBars = L->numObjs;
    for (int k = 0; k < g->numBars; k++) {
        g->bar[k].b0 = L->objs[k].b0;
        g->bar[k].b1 = L->objs[k].b1;
        g->bar[k].cell = L->objs[k].cell;
    }
    for (int k = 0; k < g->numBars; k++) bar_draw(g, k, false);
    g->state = ST_PLAY;
}

void game_load(Game *g, int level) { load_level(g, level); }

int game_next_level(int level)
{
    if (level == 36) return 30;
    if (level >= 35) return -1;          // finished the game
    return level + 1;
}

void game_step(Game *g, int in)
{
    g->events = 0;
    g->frames++;
    if (g->state == ST_DEAD) {
        if (--g->timer <= 0) { int lv = g->level, f = g->frames, s = g->shifts; load_level(g, lv); g->frames = f; g->shifts = s; }
        return;
    }
    if (g->state == ST_WIN) { if (g->timer > 0) g->timer--; return; }

    process_input(g, in);
    vertical(g);
    if (g->cool11) g->cool11--;
    if (g->cool12) g->cool12--;
    if (g->gotoLevel >= 0) { load_level(g, g->gotoLevel); return; }
    if (g->exitFlag && g->jump < 0 && !g->fall && g->state == ST_PLAY) {
        g->state = ST_WIN; g->timer = 45; g->events |= EV_WIN;
    }
}

// ---- compact state (used by the host-side solver) --------------------------

unsigned long long game_pack(const Game *g)
{
    unsigned long long v = 0;
    v = (v << 9) | (unsigned)g->ax;
    v = (v << 8) | (unsigned)g->y;
    v = (v << 2) | (unsigned)(g->jump + 1);
    v = (v << 4) | (unsigned)g->jcnt;
    v = (v << 1) | (unsigned)g->fall;
    v = (v << 4) | (unsigned)g->cool11;
    v = (v << 5) | (unsigned)g->cool12;
    v = (v << 1) | (unsigned)g->hasKey;
    v = (v << 1) | (unsigned)g->world;
    v = (v << 1) | (unsigned)g->exitFlag;
    for (int k = 0; k < 4; k++) v = (v << 4) | (unsigned)(k < g->numBars ? g->bar[k].b0 : 0);
    return v;
}

void game_unpack(Game *g, int level, unsigned long long v)
{
    int b0[4];
    for (int k = 3; k >= 0; k--) { b0[k] = (int)(v & 15); v >>= 4; }
    int exitf = (int)(v & 1); v >>= 1;
    int world = (int)(v & 1); v >>= 1;
    int key = (int)(v & 1); v >>= 1;
    int c12 = (int)(v & 31); v >>= 5;
    int c11 = (int)(v & 15); v >>= 4;
    int fall = (int)(v & 1); v >>= 1;
    int jcnt = (int)(v & 15); v >>= 4;
    int jump = (int)(v & 3) - 1; v >>= 2;
    int y = (int)(v & 255); v >>= 8;
    int ax = (int)(v & 511);
    load_level(g, level);
    for (int k = 0; k < g->numBars; k++) bar_erase(g, k);
    if (world) {
        for (int i = 0; i < CELLS / 2; i++) {
            unsigned char t = g->tile[i];
            g->tile[i] = g->tile[CELLS - 1 - i];
            g->tile[CELLS - 1 - i] = t;
        }
        for (int k = 0; k < g->numBars; k++) {
            g->bar[k].cell = CELLS - 1 - g->bar[k].cell;
            g->bar[k].b1 = bar_flip(g->bar[k].b1);
        }
        g->cls = 0; g->world = 1;
    }
    for (int k = 0; k < g->numBars; k++) g->bar[k].b0 = b0[k];
    for (int k = 0; k < g->numBars; k++) bar_draw(g, k, false);
    if (key) { g->hasKey = 1; g->types[0x64] = 0x00; g->types[0xe4] = 0x10; }
    g->ax = ax; g->y = y; g->jump = jump; g->jcnt = jcnt; g->fall = fall;
    g->cool11 = c11; g->cool12 = c12; g->exitFlag = exitf;
}
