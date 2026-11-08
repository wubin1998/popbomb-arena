#include "audio.h"

#include <3ds.h>
#include <stdio.h>
#include <string.h>

#define MUSIC_CH 0
#define SFX_FIRST_CH 1
#define SFX_CHANNELS 8
#define MUSIC_GAIN 0.28f
#define MAX_PCM_BYTES (16u * 1024u * 1024u)

typedef struct {
	const char* name;
	const char* path;
	bool music;
	float gain;
} SoundDef;

static const SoundDef SOUNDS[] = {
	{ "place",     "romfs:/audio/place.wav",     false, 0.50f },
	{ "explode",   "romfs:/audio/explode.wav",   false, 0.55f },
	{ "pickup",    "romfs:/audio/pickup.wav",    false, 0.50f },
	{ "push",      "romfs:/audio/push.wav",      false, 0.45f },
	{ "rocket",    "romfs:/audio/rocket.wav",    false, 0.60f },
	{ "grenade",   "romfs:/audio/grenade.wav",   false, 0.60f },
	{ "mine",      "romfs:/audio/mine.wav",      false, 0.35f },
	{ "stageWin",  "romfs:/audio/stageWin.wav",  false, 0.70f },
	{ "stageLose", "romfs:/audio/stageLose.wav", false, 0.35f },
	{ "sfx315",    "romfs:/audio/sfx315.wav",    false, 0.40f },
	{ "sfx319",    "romfs:/audio/sfx319.wav",    false, 0.45f },
	{ "sfx325",    "romfs:/audio/sfx325.wav",    false, 0.45f },
	{ "menu",      "romfs:/audio/menu.wav",      true,  1.00f },
	{ "bgm1",      "romfs:/audio/bgm1.wav",      true,  1.00f },
	{ "bgm2",      "romfs:/audio/bgm2.wav",      true,  1.00f },
	{ "bgm3",      "romfs:/audio/bgm3.wav",      true,  1.00f },
	{ "bgm4",      "romfs:/audio/bgm4.wav",      true,  1.00f },
};

#define SOUND_COUNT ((int)(sizeof(SOUNDS) / sizeof(SOUNDS[0])))

typedef struct {
	s16* pcm;
	u32 bytes;
	u32 nsamples;
	int channels;
	int rate;
	bool loaded;
	bool tried;
} Clip;

static Clip s_clips[SOUND_COUNT];
static ndspWaveBuf s_music_wb;
static ndspWaveBuf s_sfx_wb[SFX_CHANNELS];
static int s_music_idx = -1;
static int s_want_music = -1;
static int s_sfx_rr;
static bool s_ready;
static bool s_enabled = true;
static bool s_error;

static u16 rd16(const unsigned char* p)
{
	return (u16)(p[0] | (p[1] << 8));
}

static u32 rd32(const unsigned char* p)
{
	return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

static void clip_free(Clip* c)
{
	if (c->pcm)
		linearFree(c->pcm);
	memset(c, 0, sizeof(*c));
}

static bool clip_load(int idx)
{
	Clip* c = &s_clips[idx];
	if (c->tried)
		return c->loaded;
	c->tried = true;

	FILE* f = fopen(SOUNDS[idx].path, "rb");
	if (!f)
		return false;

	unsigned char hdr[12];
	if (fread(hdr, 1, 12, f) != 12 || memcmp(hdr, "RIFF", 4) != 0 ||
	    memcmp(hdr + 8, "WAVE", 4) != 0)
		goto fail;

	int format = 0, channels = 0, bits = 0;
	long rate = 0;
	long data_off = -1;
	u32 data_size = 0;

	for (int guard = 0; guard < 32; guard++) {
		unsigned char chunk[8];
		if (fread(chunk, 1, 8, f) != 8)
			break;
		u32 size = rd32(chunk + 4);
		if (size > MAX_PCM_BYTES)
			goto fail;
		if (memcmp(chunk, "fmt ", 4) == 0) {
			unsigned char fb[16];
			if (size < 16 || fread(fb, 1, 16, f) != 16)
				goto fail;
			format = rd16(fb);
			channels = rd16(fb + 2);
			rate = (long)rd32(fb + 4);
			bits = rd16(fb + 14);
			long skip = (long)size - 16 + (long)(size & 1);
			if (skip > 0 && fseek(f, skip, SEEK_CUR) != 0)
				goto fail;
		} else if (memcmp(chunk, "data", 4) == 0) {
			data_off = ftell(f);
			data_size = size;
			if (data_off < 0)
				goto fail;
			if (fseek(f, (long)size + (long)(size & 1), SEEK_CUR) != 0)
				goto fail;
		} else if (fseek(f, (long)size + (long)(size & 1), SEEK_CUR) != 0) {
			goto fail;
		}
		if (format && data_off >= 0)
			break;
	}

	if (format != 1 || bits != 16 || (channels != 1 && channels != 2) ||
	    rate < 8000 || rate > 48000 || data_off < 0 || data_size < 2)
		goto fail;

	u32 bytes = data_size & ~1u;
	if (bytes > MAX_PCM_BYTES)
		goto fail;
	if (fseek(f, data_off, SEEK_SET) != 0)
		goto fail;

	s16* pcm = (s16*)linearAlloc(bytes);
	if (!pcm)
		goto fail;
	if (fread(pcm, 1, bytes, f) != bytes) {
		linearFree(pcm);
		goto fail;
	}

	DSP_FlushDataCache(pcm, bytes);

	c->pcm = pcm;
	c->bytes = bytes;
	c->nsamples = bytes / sizeof(s16);
	c->channels = channels;
	c->rate = (int)rate;
	c->loaded = true;
	fclose(f);
	return true;

fail:
	fclose(f);
	s_error = true;
	return false;
}

static int find_sound(const char* name)
{
	if (!name)
		return -1;
	for (int i = 0; i < SOUND_COUNT; i++)
		if (strcmp(SOUNDS[i].name, name) == 0)
			return i;
	return -1;
}

static void channel_setup(int ch, const Clip* c, float gain)
{
	ndspChnReset(ch);
	ndspChnSetInterp(ch, NDSP_INTERP_LINEAR);
	ndspChnSetFormat(ch, c->channels == 2 ? NDSP_FORMAT_STEREO_PCM16
	                                      : NDSP_FORMAT_MONO_PCM16);
	ndspChnSetRate(ch, (float)c->rate);
	float mix[12] = { 0 };
	mix[0] = gain;
	mix[1] = gain;
	ndspChnSetMix(ch, mix);
}

void audio_init(void)
{
	if (R_FAILED(ndspInit()))
		return;
	ndspSetOutputMode(NDSP_OUTPUT_STEREO);
	ndspSetMasterVol(1.0f);
	for (int i = 0; i < SFX_CHANNELS; i++)
		ndspChnReset(SFX_FIRST_CH + i);
	ndspChnReset(MUSIC_CH);
	s_ready = true;
}

void audio_exit(void)
{
	for (int i = 0; i < SOUND_COUNT; i++)
		clip_free(&s_clips[i]);
	s_music_idx = -1;
	s_want_music = -1;
	if (s_ready) {
		ndspChnReset(MUSIC_CH);
		for (int i = 0; i < SFX_CHANNELS; i++)
			ndspChnReset(SFX_FIRST_CH + i);
		ndspExit();
	}
	s_ready = false;
}

bool audio_ok(void)
{
	return s_ready && !s_error;
}

const char* audio_error_text(void)
{
	/* ndspInit needs the DSP firmware (/3ds/dspfirm.cdc or Luma's hb:ndsp). */
	if (!s_ready)
		return "音频关闭：未找到DSP固件";
	if (s_error)
		return "音频关闭：音效加载失败";
	return NULL;
}

static void music_start(int idx)
{
	Clip* c = &s_clips[idx];
	ndspChnReset(MUSIC_CH);
	memset(&s_music_wb, 0, sizeof(s_music_wb));
	s_music_wb.data_vaddr = c->pcm;
	s_music_wb.nsamples = c->nsamples;
	s_music_wb.looping = true;
	channel_setup(MUSIC_CH, c, MUSIC_GAIN);
	ndspChnWaveBufAdd(MUSIC_CH, &s_music_wb);
	s_music_idx = idx;
}

void audio_music(const char* name)
{
	int idx = find_sound(name);
	if (idx < 0 || !SOUNDS[idx].music)
		return;
	s_want_music = idx;
	if (!s_ready || !s_enabled)
		return;
	if (s_music_idx == idx) {
		ndspChnSetPaused(MUSIC_CH, false);
		return;
	}
	if (!clip_load(idx))
		return;

	ndspChnReset(MUSIC_CH);
	if (s_music_idx >= 0)
		clip_free(&s_clips[s_music_idx]);
	s_music_idx = -1;
	music_start(idx);
}

void audio_stop_music(void)
{
	s_want_music = -1;
	if (!s_ready)
		return;
	ndspChnReset(MUSIC_CH);
	s_music_idx = -1;
}

void audio_pause_music(void)
{
	if (s_ready && s_music_idx >= 0)
		ndspChnSetPaused(MUSIC_CH, true);
}

void audio_resume_music(void)
{
	if (s_ready && s_music_idx >= 0)
		ndspChnSetPaused(MUSIC_CH, false);
}

static int pick_sfx_channel(void)
{
	for (int i = 0; i < SFX_CHANNELS; i++) {
		int ch = SFX_FIRST_CH + i;
		if (!ndspChnIsPlaying(ch))
			return ch;
	}
	s_sfx_rr = (s_sfx_rr + 1) % SFX_CHANNELS;
	return SFX_FIRST_CH + s_sfx_rr;
}

void audio_play_vol(const char* name, float volume)
{
	if (!s_ready || !s_enabled)
		return;
	int idx = find_sound(name);
	if (idx < 0 || SOUNDS[idx].music)
		return;
	if (!clip_load(idx))
		return;

	Clip* c = &s_clips[idx];
	int ch = pick_sfx_channel();
	int slot = ch - SFX_FIRST_CH;
	channel_setup(ch, c, volume);

	ndspWaveBuf* wb = &s_sfx_wb[slot];
	memset(wb, 0, sizeof(*wb));
	wb->data_vaddr = c->pcm;
	wb->nsamples = c->nsamples;
	wb->looping = false;
	ndspChnWaveBufAdd(ch, wb);
}

void audio_play(const char* name)
{
	int idx = find_sound(name);
	float volume = idx >= 0 ? SOUNDS[idx].gain : 0.5f;
	audio_play_vol(name, volume);
}

void audio_set_enabled(bool on)
{
	if (s_enabled == on)
		return;
	s_enabled = on;
	if (!s_ready)
		return;
	if (!on) {
		audio_stop_music();
		for (int i = 0; i < SFX_CHANNELS; i++)
			ndspChnReset(SFX_FIRST_CH + i);
	} else if (s_want_music >= 0) {
		int idx = s_want_music;
		s_want_music = -1;
		audio_music(SOUNDS[idx].name);
	}
}

bool audio_enabled(void)
{
	return s_enabled;
}
