#include "audio.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CLIPS 24
#define MAX_VOICES 16

typedef struct {
	Uint8* data; /* device-format PCM */
	Uint32 len;
} Clip;

typedef struct {
	const Clip* clip;
	Uint32 pos;
	float vol;
	bool loop;
	bool active;
} Voice;

static Clip g_clips[MAX_CLIPS];
static char g_names[MAX_CLIPS][32];
static int g_clip_count;

static Voice g_voices[MAX_VOICES];
static Voice g_music;

static SDL_AudioDeviceID g_dev;
static SDL_AudioSpec g_spec;
static bool g_ready;
static bool g_music_on = true;
static bool g_sfx_on = true;
static const char* g_error = "SDL 音频关闭";
static volatile int g_peak;

static const char* data_root(char* buf, size_t n)
{
	const char* root = getenv("POPBOMB_DATA");
	if (root)
		return root;
	const char* base = SDL_GetBasePath();
	if (!base)
		return NULL;
	snprintf(buf, n, "%s", base);
	return buf;
}

static void mix_into(Sint32* acc, int frames, const Voice* v)
{
	const Sint16* src = (const Sint16*)v->clip->data;
	Uint32 total = v->clip->len / 2; /* samples */
	Uint32 pos = v->pos;
	for (int i = 0; i < frames * 2; i++) {
		if (pos >= total) {
			if (!v->loop)
				break;
			pos = 0;
		}
		acc[i] += (Sint32)((float)src[pos] * v->vol);
		pos++;
	}
}

static void audio_callback(void* userdata, Uint8* stream, int len)
{
	(void)userdata;
	int frames = len / 4;
	memset(stream, 0, (size_t)len);
	if (frames <= 0)
		return;

	Sint32* acc = calloc((size_t)frames * 2, sizeof(Sint32));
	if (!acc)
		return;
	if (g_music.active && g_music_on)
		mix_into(acc, frames, &g_music);
	for (int i = 0; i < MAX_VOICES; i++)
		if (g_voices[i].active && g_sfx_on)
			mix_into(acc, frames, &g_voices[i]);

	/* The 3DS lets the DSP sum its channels; a software mixer has to scale the
	   whole block down when several sounds overlap, or it clips. */
	Sint32 peak = 0;
	for (int i = 0; i < frames * 2; i++) {
		Sint32 s = acc[i];
		Sint32 mag = s < 0 ? -s : s;
		if (mag > peak)
			peak = mag;
	}
	float norm = peak > 32767 ? 32767.0f / (float)peak : 1.0f;

	Sint16* out = (Sint16*)stream;
	for (int i = 0; i < frames * 2; i++)
		out[i] = (Sint16)((float)acc[i] * norm);
	g_peak = (int)((float)peak * norm);
	free(acc);

	/* Advance playback positions after mixing. */
	Uint32 step = (Uint32)frames * 2;
	if (g_music.active) {
		g_music.pos += step;
		Uint32 total = g_music.clip->len / 2;
		if (g_music.pos >= total)
			g_music.pos = g_music.loop ? g_music.pos % total : 0;
		if (!g_music.loop && g_music.pos == 0)
			g_music.active = false;
	}
	for (int i = 0; i < MAX_VOICES; i++) {
		Voice* v = &g_voices[i];
		if (!v->active)
			continue;
		v->pos += step;
		if (v->pos >= v->clip->len / 2)
			v->active = false;
	}
}

void audio_init(void)
{
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
		g_error = SDL_GetError();
		return;
	}
	SDL_AudioSpec want;
	memset(&want, 0, sizeof(want));
	want.freq = 44100;
	want.format = AUDIO_S16SYS;
	want.channels = 2;
	want.samples = 1024;
	want.callback = audio_callback;
	SDL_AudioSpec have;
	SDL_zero(have);
	g_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
	if (!g_dev) {
		g_error = SDL_GetError();
		return;
	}
	g_spec = have;
	g_ready = true;
	SDL_PauseAudioDevice(g_dev, 0);
}

void audio_exit(void)
{
	if (g_dev) {
		SDL_CloseAudioDevice(g_dev);
		g_dev = 0;
	}
	for (int i = 0; i < g_clip_count; i++)
		free(g_clips[i].data);
	g_clip_count = 0;
	g_ready = false;
}

bool audio_ok(void) { return g_ready; }

void audio_update(void) {}

const char* audio_error_text(void) { return g_ready ? NULL : g_error; }

const char* audio_status_text(void) { return g_ready ? "SDL 音频" : "音频关闭"; }

/* Test probe: highest sample the mixer produced since the last call. */
int audio_debug_peak(void)
{
	int p = g_peak;
	g_peak = 0;
	return p;
}

static Clip* load_clip(const char* name)
{
	for (int i = 0; i < g_clip_count; i++)
		if (strcmp(g_names[i], name) == 0)
			return &g_clips[i];
	if (g_clip_count >= MAX_CLIPS)
		return NULL;

	char root_buf[1024];
	const char* root = data_root(root_buf, sizeof(root_buf));
	if (!root)
		return NULL;
	char path[1200];
	SDL_AudioSpec src;
	Uint8* src_buf = NULL;
	Uint32 src_len = 0;
	const char* const layouts[] = { "%s/romfs/audio/%s.wav", "%s/audio/%s.wav", "%s/%s.wav" };
	bool ok = false;
	for (size_t i = 0; i < 3 && !ok; i++) {
		snprintf(path, sizeof(path), layouts[i], root, name);
		ok = SDL_LoadWAV(path, &src, &src_buf, &src_len) != NULL;
	}
	if (!ok)
		return NULL;

	SDL_AudioCVT cvt;
	if (SDL_BuildAudioCVT(&cvt, src.format, src.channels, src.freq,
	                      g_spec.format, g_spec.channels, g_spec.freq) < 0) {
		SDL_free(src_buf);
		return NULL;
	}
	cvt.len = (int)src_len;
	cvt.buf = malloc((size_t)cvt.len * (size_t)cvt.len_mult);
	memcpy(cvt.buf, src_buf, src_len);
	SDL_free(src_buf);
	SDL_ConvertAudio(&cvt);

	Clip* c = &g_clips[g_clip_count];
	c->data = cvt.buf;
	c->len = (Uint32)cvt.len_cvt;
	snprintf(g_names[g_clip_count], sizeof(g_names[0]), "%s", name);
	g_clip_count++;
	return c;
}

static void trigger(const char* name, float vol, bool loop, bool music)
{
	if (!g_ready || !name)
		return;
	Clip* c = load_clip(name);
	if (!c)
		return;
	Voice* slot = NULL;
	if (music) {
		slot = &g_music;
	} else {
		for (int i = 0; i < MAX_VOICES; i++) {
			if (!g_voices[i].active) {
				slot = &g_voices[i];
				break;
			}
		}
		if (!slot)
			slot = &g_voices[0];
	}
	SDL_LockAudioDevice(g_dev);
	slot->clip = c;
	slot->pos = 0;
	slot->vol = vol;
	slot->loop = loop;
	slot->active = true;
	SDL_UnlockAudioDevice(g_dev);
}

void audio_play(const char* name) { trigger(name, 0.6f, false, false); }

void audio_play_vol(const char* name, float volume) { trigger(name, volume, false, false); }

void audio_music(const char* name) { trigger(name, 0.35f, true, true); }

void audio_stop_music(void)
{
	if (!g_dev)
		return;
	SDL_LockAudioDevice(g_dev);
	g_music.active = false;
	SDL_UnlockAudioDevice(g_dev);
}

void audio_pause_music(void)
{
	if (!g_dev)
		return;
	SDL_LockAudioDevice(g_dev);
	g_music.loop = false;
	SDL_UnlockAudioDevice(g_dev);
}

void audio_resume_music(void)
{
	if (!g_dev || !g_music.clip)
		return;
	SDL_LockAudioDevice(g_dev);
	g_music.loop = true;
	g_music.active = true;
	SDL_UnlockAudioDevice(g_dev);
}

void audio_set_music_enabled(bool on) { g_music_on = on; }
void audio_set_sfx_enabled(bool on) { g_sfx_on = on; }
bool audio_music_enabled(void) { return g_music_on; }
bool audio_sfx_enabled(void) { return g_sfx_on; }
