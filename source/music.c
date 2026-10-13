// Procedural chiptune soundtrack + sound effects, synthesised at startup.
#include "music.h"
#include "game.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifdef __3DS__
#include <3ds.h>
#endif

#define STEP_SAMPLES (MUSIC_RATE / 4)   /* one eighth note at 120 bpm */
#define STEPS        64                 /* 8 bars */

static float midi_hz(int n) { return 440.0f * powf(2.0f, (n - 69) / 12.0f); }
static float sq(float ph, float duty) { ph -= floorf(ph); return ph < duty ? 1.f : -1.f; }
static float tri(float ph) { ph -= floorf(ph); return ph < .5f ? 4*ph - 1 : 3 - 4*ph; }

size_t synth_music(int16_t *out, size_t max)
{
    size_t n = (size_t)STEPS * STEP_SAMPLES;
    if (n > max) n = max;
    // chord roots (bass) and chord tones per bar: Am F C G Am F G E
    static const int root[8] = {45, 41, 48, 43, 45, 41, 43, 40};
    static const int third[8] = {3, 4, 4, 4, 3, 4, 4, 4};   // minor/major third
    static const int lead[64] = {
        81,0,0,84, 0,83,0,81,  77,0,0,81, 0,79,0,77,
        79,0,0,84, 0,83,0,79,  74,0,0,79, 0,78,0,74,
        81,0,84,0, 88,0,86,84, 84,0,81,0, 77,0,81,84,
        83,0,79,0, 74,0,79,83, 80,0,83,0, 88,0,0,0 };
    unsigned noise = 1;
    for (size_t i = 0; i < n; i++) {
        int step = (int)(i / STEP_SAMPLES), bar = step / 8, sub = step % 8;
        float t = (float)(i % STEP_SAMPLES) / STEP_SAMPLES;       // 0..1 inside step
        float ts = (float)(i % STEP_SAMPLES) / MUSIC_RATE;        // seconds in step
        float s = 0;
        // bass: triangle on the root, octave bounce
        int b = root[bar] - 12 + ((sub & 1) ? 12 : 0);
        s += 0.38f * tri(midi_hz(b) * (float)i / MUSIC_RATE) * (1.f - 0.4f * t);
        // arpeggio: pulse, root-third-fifth-octave pattern
        int pat[4] = {0, third[bar], 7, 12};
        int an = root[bar] + 12 + pat[(sub * 3 / 2 + (sub & 1)) & 3];
        s += 0.12f * sq(midi_hz(an) * (float)i / MUSIC_RATE, 0.25f) * (1.f - 0.7f * t);
        // lead melody
        int ln = lead[step];
        if (ln) {
            float env = t < 0.8f ? 1.f - 0.5f * t : (1.f - t) * 3.f;
            float vib = 1.f + 0.006f * sinf(6.28f * 5.f * (float)i / MUSIC_RATE);
            s += 0.14f * sq(midi_hz(ln) * vib * (float)i / MUSIC_RATE, 0.5f) * env;
        }
        // drums: kick on beats, hat on offbeats
        noise = noise * 1664525u + 1013904223u;
        float nz = ((noise >> 16) & 0x7fff) / 16384.f - 1.f;
        if (sub % 2 == 0) {
            float k = expf(-ts * 28.f);
            s += 0.35f * sinf(6.28f * (50.f + 110.f * expf(-ts * 40.f)) * ts) * k;
        } else {
            s += 0.07f * nz * expf(-ts * 60.f);
        }
        if (sub == 4) s += 0.10f * nz * expf(-ts * 22.f);          // snare
        if (s > 0.95f) s = 0.95f;
        if (s < -0.95f) s = -0.95f;
        out[i] = (int16_t)(s * 12000.f);
    }
    return n;
}

#ifdef __3DS__

enum { SFX_JUMP, SFX_SHIFT, SFX_DENY, SFX_KEY, SFX_DIE, SFX_WIN, SFX_LAND, NUM_SFX };
#define SFX_RATE 16000

static bool g_on;
static int16_t *g_music;
static size_t g_musicN;
static ndspWaveBuf g_musicWb;
static int16_t *g_sfx[NUM_SFX];
static size_t g_sfxN[NUM_SFX];
static ndspWaveBuf g_sfxWb[NUM_SFX];

static size_t make_sfx(int id, int16_t *out)
{
    float dur[NUM_SFX] = {0.18f, 0.30f, 0.20f, 0.28f, 0.55f, 0.80f, 0.08f};
    size_t n = (size_t)(dur[id] * SFX_RATE);
    unsigned noise = 7;
    float ph = 0;
    for (size_t i = 0; i < n; i++) {
        float t = (float)i / n, ts = (float)i / SFX_RATE, f = 0, a = 0.5f, s;
        noise = noise * 1664525u + 1013904223u;
        float nz = ((noise >> 16) & 0x7fff) / 16384.f - 1.f;
        switch (id) {
        case SFX_JUMP:  f = 300 + 500 * t; a *= 1 - t; break;
        case SFX_SHIFT: f = 900 - 750 * t; a *= (1 - t); break;
        case SFX_DENY:  f = 110; a *= (1 - t) * 0.9f; break;
        case SFX_KEY:   f = t < 0.4f ? 880 : 1320; a *= 1 - t; break;
        case SFX_DIE:   f = 500 - 420 * t; a *= 1 - t; break;
        case SFX_WIN: { static const int nt[4] = {72, 76, 79, 84};
                        f = midi_hz(nt[(int)(t * 4)]); a *= 1 - 0.6f * t; break; }
        case SFX_LAND:  f = 90; a *= 1 - t; break;
        }
        ph += f / SFX_RATE;
        s = sq(ph, 0.5f) * a;
        if (id == SFX_SHIFT) s = s * 0.6f + nz * a * 0.5f * (1 - t);
        (void)ts;
        out[i] = (int16_t)(s * 14000.f);
    }
    return n;
}

static void play_sfx(int id)
{
    if (!g_on) return;
    ndspChnWaveBufClear(1 + id % 6);
    memset(&g_sfxWb[id], 0, sizeof g_sfxWb[id]);
    g_sfxWb[id].data_vaddr = g_sfx[id];
    g_sfxWb[id].nsamples = g_sfxN[id];
    ndspChnWaveBufAdd(1 + id % 6, &g_sfxWb[id]);
}

static void setup_chan(int ch, float vol)
{
    ndspChnReset(ch);
    ndspChnSetInterp(ch, NDSP_INTERP_LINEAR);
    ndspChnSetRate(ch, MUSIC_RATE);
    ndspChnSetFormat(ch, NDSP_FORMAT_MONO_PCM16);
    float mix[12] = {0};
    mix[0] = mix[1] = vol;
    ndspChnSetMix(ch, mix);
}

void audio_init(void)
{
    if (R_FAILED(ndspInit())) return;       // no dspfirm.cdc dumped: stay silent
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    size_t cap = (size_t)STEPS * STEP_SAMPLES;
    g_music = linearAlloc(cap * sizeof(int16_t));
    if (!g_music) return;
    g_musicN = synth_music(g_music, cap);
    DSP_FlushDataCache(g_music, g_musicN * sizeof(int16_t));
    setup_chan(0, 0.45f);
    memset(&g_musicWb, 0, sizeof g_musicWb);
    g_musicWb.data_vaddr = g_music;
    g_musicWb.nsamples = g_musicN;
    g_musicWb.looping = true;
    ndspChnWaveBufAdd(0, &g_musicWb);
    for (int i = 0; i < NUM_SFX; i++) {
        g_sfx[i] = linearAlloc(SFX_RATE * sizeof(int16_t));
        if (!g_sfx[i]) return;
        g_sfxN[i] = make_sfx(i, g_sfx[i]);
        DSP_FlushDataCache(g_sfx[i], g_sfxN[i] * sizeof(int16_t));
    }
    for (int c = 1; c <= 6; c++) setup_chan(c, 0.8f);
    g_on = true;
}

void audio_events(int ev)
{
    if (ev & EV_SHIFT) play_sfx(SFX_SHIFT);
    if (ev & EV_JUMP)  play_sfx(SFX_JUMP);
    if (ev & EV_DENY)  play_sfx(SFX_DENY);
    if (ev & (EV_KEY | EV_BUTTON)) play_sfx(SFX_KEY);
    if (ev & EV_DIE)   play_sfx(SFX_DIE);
    if (ev & EV_WIN)   play_sfx(SFX_WIN);
    if ((ev & EV_LAND) && !(ev & (EV_JUMP | EV_SHIFT))) play_sfx(SFX_LAND);
}

void audio_exit(void)
{
    if (!g_on && !g_music) return;
    ndspExit();
    linearFree(g_music);
    for (int i = 0; i < NUM_SFX; i++) if (g_sfx[i]) linearFree(g_sfx[i]);
}

#endif
