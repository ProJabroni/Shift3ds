#include <3ds.h>
#include "citro2d.h"
void gfxInitDefault(void){} void gfxExit(void){} bool aptMainLoop(void){return false;}
void hidScanInput(void){} u32 hidKeysDown(void){return 0;} u32 hidKeysHeld(void){return 0;}
void hidTouchRead(touchPosition*t){(void)t;} void gfxSet3D(bool b){(void)b;} float osGet3DSliderState(void){return 1;}
C3D_RenderTarget* C2D_CreateScreenTarget(int a,int b){(void)a;(void)b;return 0;}
bool C3D_FrameBegin(u8 a){(void)a;return 1;} void C3D_FrameEnd(u8 a){(void)a;}
void C2D_TargetClear(C3D_RenderTarget*t,u32 c){(void)t;(void)c;} void C2D_SceneBegin(C3D_RenderTarget*t){(void)t;}
void C2D_Fini(void){} void C3D_Fini(void){}
