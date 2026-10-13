// Shift 3DS - platform independent game logic.
// Integer-only physics so the host-side solver (test/solve.c) and the 3DS
// build behave identically.
#pragma once
#include <stdbool.h>

#define TILE        16
#define GW          25            // 25 * 16 = 400 px (top screen width)
#define GH          15            // 15 * 16 = 240 px (top screen height)
#define NUM_LEVELS  8

#define PW          12            // player hitbox
#define PH          12
#define MOVE_SPEED  3
#define JUMP_V      (-9)
#define GRAVITY     1
#define MAX_FALL    8

// Tile characters in level data:
//  '#' always solid     'W' solid only in the white world
//  'K' solid only in the black world   '^' spikes   'E' exit   'S' start
enum { ST_PLAY = 0, ST_DEAD, ST_WIN };

typedef struct {
    int  level;
    char tiles[GH][GW + 1];
    int  px, py;          // top-left of the player, in pixels
    int  vy;
    bool world;           // false = white world, true = black world
    bool onGround;
    int  state;
    int  shifts;
    int  frames;
    int  deny;            // >0: a shift was just refused (visual feedback)
    int  flash;           // >0: a shift just happened (visual feedback)
} Game;

extern const char *const LEVEL_NAMES[NUM_LEVELS];
extern const char *const LEVELS[NUM_LEVELS][GH];

void game_load(Game *g, int level);
// dir: -1/0/1, jump: jump requested (jumps if grounded), shift: toggle world
void game_step(Game *g, int dir, bool jump, bool shift);
bool game_tile_active(const Game *g, char c);   // is this tile solid right now?
void game_sync(Game *g);   // recompute derived state (onGround) after editing px/py/world
