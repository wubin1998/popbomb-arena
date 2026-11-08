#pragma once

#include <stdbool.h>

/* Names match the web build's audio keys: place, explode, pickup, push,
   rocket, grenade, mine, stageWin, stageLose, menu, bgm1..bgm4,
   sfx315, sfx319, sfx325. */
void audio_init(void);
void audio_exit(void);
bool audio_ok(void);

/* Refills the streaming music buffers; call once per frame. */
void audio_update(void);

/* NULL when audio is fine; otherwise a short line naming the failure. */
const char* audio_error_text(void);

/* Short state line for the settings page. */
const char* audio_status_text(void);

void audio_play(const char* name);
void audio_play_vol(const char* name, float volume);

void audio_music(const char* name);
void audio_stop_music(void);
void audio_pause_music(void);
void audio_resume_music(void);

void audio_set_music_enabled(bool on);
void audio_set_sfx_enabled(bool on);
bool audio_music_enabled(void);
bool audio_sfx_enabled(void);
