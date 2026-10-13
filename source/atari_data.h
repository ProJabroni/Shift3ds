#pragma once
// Level data decoded from the Atari 8-bit port of Shift.
#define ATARI_NUM_LEVELS 37
typedef struct { unsigned char b0, b1; unsigned short cell; } AtariObj;   // hinged 4-tile bar
typedef struct {
    unsigned char startX, startY;      // player start, Atari pixels (screen 320x192)
    unsigned char numObjs;
    AtariObj objs[4];
    const unsigned char *rle;          // (tile, count) pairs, 1000 cells, ends 0xFF 0xFF
    int unused;
} AtariLevel;
extern const AtariLevel ATARI_LEVELS[ATARI_NUM_LEVELS];
extern const unsigned char ATARI_TYPES[256];       // tile behaviour (high nibble)
extern const unsigned char ATARI_FONT[128][8];     // 8x8 glyphs (bit7 of tile = inverse)
extern const unsigned char ATARI_BUTTON[4][8];    // animation frames of the button glyph (tiles 0x73-0x76)
