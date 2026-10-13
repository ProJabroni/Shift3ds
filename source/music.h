#pragma once
#include <stdint.h>
#include <stddef.h>
void audio_init(void);          // synthesises everything; silent if no DSP firmware
void audio_events(int events);  // pass Game.events (EV_* bits) to play sound effects
void audio_sfx_win(void);
void audio_exit(void);

// Pure synthesis (also used by the host test)
#define MUSIC_RATE 16000
size_t synth_music(int16_t *out, size_t max);
