// Renders the top screen for both eyes with the real drawing code from source/main.c,
// so the 3D look can be checked on a PC (writes <out>_L.ppm and <out>_R.ppm).
// Build (from the repo root):
//   gcc -O2 -w -I tools/preview -I source -o pv tools/preview/preview.c tools/preview/sys_stub.c \
//       tools/preview/music_stub.c source/game.c source/atari_data.c -lm
// Usage: ./pv LEVEL SLIDER OUTPREFIX [frames] [shift 0/1] [title 0/1]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include "citro2d.h"
#include "game.h"

#define W 400
#define H 240
static float fb[H][W][3];
static int nobj;

static void blend(int x, int y, u32 c)
{
    if (x < 0 || y < 0 || x >= W || y >= H) return;
    float a = ((c >> 24) & 255) / 255.0f;
    float r = (c & 255) / 255.0f, g = ((c >> 8) & 255) / 255.0f, b = ((c >> 16) & 255) / 255.0f;
    fb[y][x][0] = fb[y][x][0] * (1 - a) + r * a;
    fb[y][x][1] = fb[y][x][1] * (1 - a) + g * a;
    fb[y][x][2] = fb[y][x][2] * (1 - a) + b * a;
}
// pixel centres sampled like the GPU (top-left rule approximated)
bool C2D_DrawRectSolid(float x, float y, float z, float w, float h, u32 c)
{
    (void)z; nobj++;
    int x0 = (int)ceilf(x - 0.5f), x1 = (int)ceilf(x + w - 0.5f);
    int y0 = (int)ceilf(y - 0.5f), y1 = (int)ceilf(y + h - 0.5f);
    for (int yy = y0; yy < y1; yy++) for (int xx = x0; xx < x1; xx++) blend(xx, yy, c);
    return true;
}
static float edge(float ax, float ay, float bx, float by, float px, float py) { return (bx - ax) * (py - ay) - (by - ay) * (px - ax); }
bool C2D_DrawTriangle(float x0, float y0, u32 c, float x1, float y1, u32 c1, float x2, float y2, u32 c2, float z)
{
    (void)c1; (void)c2; (void)z; nobj++;
    float area = edge(x0, y0, x1, y1, x2, y2);
    if (area == 0) return true;
    int minx = (int)floorf(fminf(x0, fminf(x1, x2))), maxx = (int)ceilf(fmaxf(x0, fmaxf(x1, x2)));
    int miny = (int)floorf(fminf(y0, fminf(y1, y2))), maxy = (int)ceilf(fmaxf(y0, fmaxf(y1, y2)));
    for (int yy = miny; yy <= maxy; yy++) for (int xx = minx; xx <= maxx; xx++) {
        float px = xx + 0.5f, py = yy + 0.5f;
        float w0 = edge(x1, y1, x2, y2, px, py), w1 = edge(x2, y2, x0, y0, px, py), w2 = edge(x0, y0, x1, y1, px, py);
        if (area < 0) { w0 = -w0; w1 = -w1; w2 = -w2; }
        if (w0 >= 0 && w1 >= 0 && w2 >= 0) blend(xx, yy, c);
    }
    return true;
}
bool C2D_DrawCircleSolid(float x, float y, float z, float r, u32 c)
{
    (void)z; nobj++;
    for (int yy = (int)(y - r); yy <= (int)(y + r); yy++) for (int xx = (int)(x - r); xx <= (int)(x + r); xx++)
        if ((xx + 0.5f - x) * (xx + 0.5f - x) + (yy + 0.5f - y) * (yy + 0.5f - y) <= r * r) blend(xx, yy, c);
    return true;
}
// text: draw a faint box so layout/depth is visible
const char* C2D_TextParse(C2D_Text *t, C2D_TextBuf b, const char *s) { (void)b; t->s = s; return s + strlen(s); }
void C2D_TextOptimize(const C2D_Text *t) { (void)t; }
void C2D_TextGetDimensions(const C2D_Text *t, float sx, float sy, float *w, float *h) { *w = strlen(t->s) * 13.0f * sx; *h = 30.0f * sy; }
void C2D_DrawText(const C2D_Text *t, u32 f, float x, float y, float z, float sx, float sy, ...)
{
    (void)f; (void)z;
    va_list ap; va_start(ap, sy); u32 c = va_arg(ap, u32); va_end(ap);
    float w, h; C2D_TextGetDimensions(t, sx, sy, &w, &h);
    for (size_t i = 0; i < strlen(t->s); i++) if (t->s[i] != ' ') C2D_DrawRectSolid(x + i * 13 * sx + 2, y + 6 * sy, 9 * sx, 18 * sy, z, c);
}
int C3D_Init(unsigned long a) { (void)a; return 1; } int C2D_Init(unsigned long a) { (void)a; return 1; } void C2D_Prepare(void) {}
C2D_TextBuf C2D_TextBufNew(unsigned long a) { (void)a; return NULL; } void C2D_TextBufClear(C2D_TextBuf b) { (void)b; } void C2D_TextBufDelete(C2D_TextBuf b) { (void)b; }

#define main shift_main
#include "../../source/main.c"
#undef main

static void clear(void) { for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) fb[y][x][0] = fb[y][x][1] = fb[y][x][2] = 0; }
static void save(const char *path, float img[H][W][3])
{
    FILE *f = fopen(path, "wb"); fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) for (int k = 0; k < 3; k++) {
        float v = img[y][x][k]; if (v < 0) v = 0; if (v > 1) v = 1; fputc((int)(v * 255 + 0.5f), f);
    }
    fclose(f);
}
static float L[H][W][3], R[H][W][3];

int main(int argc, char **argv)
{
    int level = argc > 1 ? atoi(argv[1]) : 0;
    float slider = argc > 2 ? atof(argv[2]) : 1.0f;
    const char *out = argc > 3 ? argv[3] : "/tmp/pv";
    int frames = argc > 4 ? atoi(argv[4]) : 30;
    int doShift = argc > 5 ? atoi(argv[5]) : 0;
    int title = argc > 6 ? atoi(argv[6]) : 0;
    static Game g;
    game_load(&g, level);
    for (int i = 0; i < frames; i++) game_step(&g, (doShift && i == frames - 20) ? IN_SHIFT : 0);
    for (int eye = 0; eye < 2; eye++) {
        clear(); nobj = 0;
        float e = eye ? -1.0f : 1.0f;
        if (title) draw_title(&g, level, frames, e, slider); else draw_level(&g, e, slider, frames);
        memcpy(eye ? R : L, fb, sizeof fb);
        fprintf(stderr, "eye %d: %d draw calls\n", eye, nobj);
    }
    char p[512];
    snprintf(p, sizeof p, "%s_L.ppm", out); save(p, L);
    snprintf(p, sizeof p, "%s_R.ppm", out); save(p, R);
    return 0;
}
