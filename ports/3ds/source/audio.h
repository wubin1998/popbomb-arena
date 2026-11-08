#pragma once

#include <stdbool.h>

/* Names match the web build's audio keys: place, explode, pickup, push,
   rocket, grenade, mine, stageWin, stageLose, menu, bgm1..bgm4,
   sfx315, sfx319, sfx325. */
void audio_init(void);
void audio_exit(void);
bool audio_ok(void);

void audio_play(const char* name);
void audio_play_vol(const char* name, float volume);

void audio_music(const char* name);
void audio_stop_music(void);
void audio_pause_music(void);
void audio_resume_music(void);

void audio_set_enabled(bool on);
bool audio_enabled(void);
