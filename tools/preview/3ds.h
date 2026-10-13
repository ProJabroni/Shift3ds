#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef uint32_t u32; typedef uint8_t u8; typedef uint16_t u16;
enum { KEY_A=1,KEY_B=2,KEY_START=8,KEY_DRIGHT=16,KEY_DLEFT=32,KEY_DUP=64,KEY_DDOWN=128,KEY_R=256,KEY_L=512,KEY_X=1024,KEY_Y=2048,
 KEY_ZL=1<<14,KEY_ZR=1<<15,KEY_TOUCH=1<<20,KEY_CPAD_RIGHT=1<<28,KEY_CPAD_LEFT=1<<29,KEY_CPAD_UP=1<<30,
 KEY_LEFT=KEY_DLEFT|KEY_CPAD_LEFT, KEY_RIGHT=KEY_DRIGHT|KEY_CPAD_RIGHT };
typedef struct { uint16_t px,py; } touchPosition;
#define GFX_TOP 0
#define GFX_BOTTOM 1
#define GFX_LEFT 0
void gfxInitDefault(void); void gfxExit(void); bool aptMainLoop(void);
void hidScanInput(void); u32 hidKeysDown(void); u32 hidKeysHeld(void);
void hidTouchRead(touchPosition*);
#define GFX_RIGHT 1
void gfxSet3D(bool); float osGet3DSliderState(void);
typedef int32_t s32; typedef int16_t s16; typedef uint64_t u64; typedef int Result;
