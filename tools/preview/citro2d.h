#pragma once
// Host stand-in for citro2d: a tiny software rasterizer so the real drawing code
// in source/main.c can be previewed as images (tools/preview/preview.c).
#include <3ds.h>
typedef struct C3D_RenderTarget_ C3D_RenderTarget;
typedef struct { const char *s; } C2D_Text;
typedef struct C2D_TextBuf_ *C2D_TextBuf;
#define C3D_DEFAULT_CMDBUF_SIZE 0x40000
#define C3D_FRAME_SYNCDRAW 1
#define C2D_WithColor 2
#define C2D_Color32(r,g,b,a) ((u32)(r)|((u32)(g)<<8)|((u32)(b)<<16)|((u32)(a)<<24))
int C3D_Init(unsigned long); int C2D_Init(unsigned long); void C2D_Prepare(void);
C3D_RenderTarget* C2D_CreateScreenTarget(int,int);
C2D_TextBuf C2D_TextBufNew(unsigned long); void C2D_TextBufClear(C2D_TextBuf); void C2D_TextBufDelete(C2D_TextBuf);
const char* C2D_TextParse(C2D_Text*, C2D_TextBuf, const char*); void C2D_TextOptimize(const C2D_Text*);
void C2D_TextGetDimensions(const C2D_Text*, float, float, float*, float*);
void C2D_DrawText(const C2D_Text*, u32, float, float, float, float, float, ...);
bool C2D_DrawRectSolid(float,float,float,float,float,u32);
bool C2D_DrawTriangle(float,float,u32,float,float,u32,float,float,u32,float);
bool C2D_DrawCircleSolid(float,float,float,float,u32);
bool C3D_FrameBegin(u8); void C3D_FrameEnd(u8); void C2D_TargetClear(C3D_RenderTarget*, u32); void C2D_SceneBegin(C3D_RenderTarget*);
void C2D_Fini(void); void C3D_Fini(void);
