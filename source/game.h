// Shift 3DS - game logic.  A port of the Atari 8-bit "Shift" rules (screen of
// 40x25 tiles of 8x8 pixels, 16x16 player, walking 2 px/frame, 180-degree room
// rotation) so the original 37 level layouts play exactly as designed.
// Platform independent and integer-only; shared with the host-side tools.
#pragma once
#include <stdbool.h>
#include "atari_data.h"

#define COLS        40
#define ROWS        24            // visible rows (a 25th hidden row exists)
#define CELLS       1000          // 40 * 25
#define NUM_LEVELS  ATARI_NUM_LEVELS

enum { ST_PLAY = 0, ST_DEAD, ST_WIN };
enum { EV_JUMP = 1, EV_SHIFT = 2, EV_DENY = 4, EV_KEY = 8, EV_DIE = 16,
       EV_WIN = 32, EV_LAND = 64, EV_BUTTON = 128 };
enum { IN_LEFT = 1, IN_RIGHT = 2, IN_JUMP = 4, IN_SHIFT = 8 };

typedef struct { int b0, b1, cell; unsigned char back[4]; } Bar;

typedef struct {
    int  level;
    unsigned char tile[CELLS];   // the screen as currently shown (rotated in world 1)
    unsigned char types[256];    // tile behaviour table (the key opens the gate)
    int  ax, y;                  // player left x (0..) and Atari scanline (row*8 + 32)
    int  jump, jcnt, fall, probe;
    int  cls;                    // 0x10 or 0x00: tile class that is solid right now
    int  cool11, cool12;         // shift cooldown, button cooldown
    int  hasKey, exitFlag;
    int  world;                  // 0 normal, 1 rotated 180 degrees
    int  facing;                 // 0/2 left, 1/3 right (sprite)
    int  state, timer;
    int  shifts, frames;
    int  events;
    int  numBars;
    Bar  bar[4];
    int  gotoLevel;              // >=0: secret tile asked to jump to that level
} Game;

void game_load(Game *g, int level);
void game_step(Game *g, int input);          // IN_* bits, call once per 1/60 s
int  game_next_level(int level);              // -1 after the last level

// Compact packed state for search tools (not used by the game itself).
unsigned long long game_pack(const Game *g);
void game_unpack(Game *g, int level, unsigned long long packed);
