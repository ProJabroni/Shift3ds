// Shift 3DS - platform independent game logic.
// Integer-only physics so the host-side solver (test/solve.c) and the 3DS
// build behave identically.
//
// The mechanic: pressing SHIFT rotates the whole room 180 degrees (and inverts
// the colours). In level coordinates that simply reverses gravity, so you fall
// toward what used to be the ceiling. Controls stay relative to the screen.
#pragma once
#include <stdbool.h>

#define TILE        16
#define GW          25            // 25 * 16 = 400 px (top screen width)
#define GH          15            // 15 * 16 = 240 px (top screen height)
#define NUM_LEVELS  8
#define MAX_KEYS    3

#define PW          12            // player hitbox
#define PH          12
#define MOVE_SPEED  3
#define JUMP_V      9             // jump impulse (magnitude)
#define GRAVITY     1
#define MAX_FALL    8

// Tile characters in level data:
//  '#' solid     '^' spikes (deadly)     'k' key     'L' lock (solid until all
//  keys are collected)     'E' exit door     'S' player start
enum { ST_PLAY = 0, ST_DEAD, ST_WIN };

typedef struct {
    int  level;
    char tiles[GH][GW + 1];
    int  px, py;          // top-left of the player in level coordinates, pixels
    int  vy;              // velocity along level Y (positive = toward bottom row)
    bool flip;            // false: gravity toward bottom row; true: toward top row
    bool onGround;
    int  state;
    int  shifts;
    int  frames;
    int  flash;           // >0: a shift just happened (visual feedback)
    int  numKeys, keysLeft;
    unsigned keyMask;     // bit i set = key i collected
    int  keyX[MAX_KEYS], keyY[MAX_KEYS];
} Game;

extern const char *const LEVEL_NAMES[NUM_LEVELS];
extern const char *const LEVEL_HINTS[NUM_LEVELS];
extern const char *const LEVELS[NUM_LEVELS][GH];

void game_load(Game *g, int level);
// dir: -1/0/1 relative to the SCREEN, jump: jump if grounded, shift: flip room
void game_step(Game *g, int dir, bool jump, bool shift);
void game_set_keymask(Game *g, unsigned mask);   // restore collected keys (solver)
void game_sync(Game *g);   // recompute onGround after editing px/py/flip
