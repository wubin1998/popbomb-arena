#include "audio.h"

#include <3ds.h>
#include <stdio.h>
#include <string.h>

#define MUSIC_CH 0
#define SFX_FIRST_CH 1
#define SFX_CHANNELS 8
#define MUSIC_GAIN 0.38f
#define MAX_PCM_BYTES (16u * 1024u * 1024u)

/* Music is streamed: a few small buffers refilled as the DSP finishes them.
   Loading a whole 44s track into the linear heap failed on hardware, which
   left the game silent while the short effects still played. */
#define MUSIC_BUFS 3
#define MUSIC_CHUNK (16u * 1024u)

/* If the channel empties without the DSP asking for more, the queue was lost;
   restart the track, but not more than a few times. */
#define MUSIC_RETRIES 3
#define MUSIC_RETRY_FRAMES 90

/* ndsp can initialize and still never consume the queue; that is as silent as
   no audio at all. A working channel finishes a 16 KB buffer in well under a
   second, so a longer gap means switching to CSND. */
#define MUSIC_STALL_FRAMES 90

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

typedef struct {
	long data_off;
	u32 data_bytes;
	int channels;
	int rate;
} WavInfo;

static Clip s_clips[SOUND_COUNT];
static ndspWaveBuf s_sfx_wb[SFX_CHANNELS];
static int s_sfx_rr;
static bool s_ready;
static bool s_music_on = true;
static bool s_sfx_on = true;
static bool s_error;
static bool s_music_error;

/* stream state */
static FILE* s_ms_f;
static WavInfo s_ms;
static u32 s_ms_pos;
static s16* s_ms_mem;
static ndspWaveBuf s_ms_wb[MUSIC_BUFS];
static int s_music_idx = -1;
static int s_want_music = -1;
static int s_retries;
static int s_retry_wait;
static int s_stall_frames;

/* Without DSP firmware ndspInit fails on hardware, which made the whole game
   silent. CSND is the older sound service: it needs no firmware, so it carries
   the audio when ndsp is unavailable. */
static bool s_csnd;
static Result s_init_result;
static int s_csnd_music = -1;
static s16* s_csnd_mem;
static u32 s_csnd_bytes;
static int s_csnd_rate;
static int s_csnd_chs[CSND_NUM_CHANNELS];
static int s_csnd_nch;
static int s_csnd_pick;
static int s_csnd_wait;
static bool s_csnd_seen;
static bool s_music_paused;
static char s_err_buf[48];
static char s_status_buf[32];

static u16 rd16(const unsigned char* p)
{
	return (u16)(p[0] | (p[1] << 8));
}

static u32 rd32(const unsigned char* p)
{
	return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

/* Reads the RIFF header and leaves the file positioned after the data chunk
   header is skipped; data_off points at the first PCM byte. */
static bool wav_parse(FILE* f, WavInfo* out)
{
	unsigned char hdr[12];
	if (fread(hdr, 1, 12, f) != 12 || memcmp(hdr, "RIFF", 4) != 0 ||
	    memcmp(hdr + 8, "WAVE", 4) != 0)
		return false;

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
			return false;
		if (memcmp(chunk, "fmt ", 4) == 0) {
			unsigned char fb[16];
			if (size < 16 || fread(fb, 1, 16, f) != 16)
				return false;
			format = rd16(fb);
			channels = rd16(fb + 2);
			rate = (long)rd32(fb + 4);
			bits = rd16(fb + 14);
			long skip = (long)size - 16 + (long)(size & 1);
			if (skip > 0 && fseek(f, skip, SEEK_CUR) != 0)
				return false;
		} else if (memcmp(chunk, "data", 4) == 0) {
			data_off = ftell(f);
			data_size = size;
			if (data_off < 0)
				return false;
			if (fseek(f, (long)size + (long)(size & 1), SEEK_CUR) != 0)
				return false;
		} else if (fseek(f, (long)size + (long)(size & 1), SEEK_CUR) != 0) {
			return false;
		}
		if (format && data_off >= 0)
			break;
	}

	if (format != 1 || bits != 16 || (channels != 1 && channels != 2) ||
	    rate < 8000 || rate > 48000 || data_off < 0 || data_size < 2)
		return false;

	out->data_off = data_off;
	out->data_bytes = data_size & ~1u;
	out->channels = channels;
	out->rate = (int)rate;
	return true;
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

	WavInfo info;
	if (!wav_parse(f, &info))
		goto fail;

	if ((u32)info.data_off + info.data_bytes > MAX_PCM_BYTES)
		goto fail;
	if (fseek(f, info.data_off, SEEK_SET) != 0)
		goto fail;

	s16* pcm = (s16*)linearAlloc(info.data_bytes);
	if (!pcm)
		goto fail;
	if (fread(pcm, 1, info.data_bytes, f) != info.data_bytes) {
		linearFree(pcm);
		goto fail;
	}

	DSP_FlushDataCache(pcm, info.data_bytes);

	c->pcm = pcm;
	c->bytes = info.data_bytes;
	c->nsamples = info.data_bytes / sizeof(s16);
	c->channels = info.channels;
	c->rate = info.rate;
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

static void channel_setup(int ch, int channels, int rate, float gain)
{
	ndspChnReset(ch);
	ndspChnSetInterp(ch, NDSP_INTERP_LINEAR);
	ndspChnSetFormat(ch, channels == 2 ? NDSP_FORMAT_STEREO_PCM16
	                                   : NDSP_FORMAT_MONO_PCM16);
	ndspChnSetRate(ch, (float)rate);
	float mix[12] = { 0 };
	mix[0] = gain;
	mix[1] = gain;
	ndspChnSetMix(ch, mix);
	/* A reset channel starts unpaused, but a channel we paused earlier would
	   swallow the buffers we are about to queue. */
	ndspChnSetPaused(ch, false);
}

/* ---------------- CSND fallback ---------------- */

/* Probing a channel costs a second or so of silence, so a few tries are enough
   to tell a dead channel from a dead service. */
#define CSND_PROBE_FRAMES 90
#define CSND_PROBE_TRIES 3

static void csnd_build_channels(void)
{
	s_csnd_nch = 0;
	for (int ch = 0; ch < CSND_NUM_CHANNELS; ch++)
		if (csndChannels & (1u << ch))
			s_csnd_chs[s_csnd_nch++] = ch;
	s_csnd_pick = 0;
	s_csnd_music = s_csnd_nch > 0 ? s_csnd_chs[0] : -1;
	s_csnd_wait = 0;
	s_csnd_seen = false;
}

static bool csnd_music_active(void)
{
	u8 active = 0;
	if (s_csnd_music < 0)
		return false;
	return R_SUCCEEDED(csndIsPlaying((u32)s_csnd_music, &active)) && active;
}

static void csnd_music_stop(void)
{
	if (s_csnd_music >= 0) {
		CSND_SetPlayState((u32)s_csnd_music, 0);
		csndExecCmds(true);
	}
}

static void csnd_music_halt(void)
{
	csnd_music_stop();
	if (s_csnd_mem) {
		linearFree(s_csnd_mem);
		s_csnd_mem = NULL;
	}
	s_csnd_bytes = 0;
	s_music_idx = -1;
}

/* CSND takes a whole buffer instead of a stream, which suits it: its counter is
   a byte count, not the 16-bit sample count that limited ndspWaveBuf. The
   buffer outlives a failed channel so a retry does not read the file again. */
static bool csnd_music_load(int idx)
{
	if (s_csnd_mem) {
		linearFree(s_csnd_mem);
		s_csnd_mem = NULL;
	}

	FILE* f = fopen(SOUNDS[idx].path, "rb");
	if (!f)
		return false;

	WavInfo info;
	if (!wav_parse(f, &info) || fseek(f, info.data_off, SEEK_SET) != 0) {
		fclose(f);
		return false;
	}

	u32 bytes = info.data_bytes & ~3u;
	s16* pcm = (s16*)linearAlloc(bytes);
	if (!pcm || fread(pcm, 1, bytes, f) != bytes) {
		if (pcm)
			linearFree(pcm);
		fclose(f);
		return false;
	}
	fclose(f);
	DSP_FlushDataCache(pcm, bytes);

	s_csnd_mem = pcm;
	s_csnd_bytes = bytes;
	s_csnd_rate = info.rate;
	return true;
}

static bool csnd_music_play(void)
{
	if (s_csnd_music < 0 || !s_csnd_mem)
		return false;

	u32 flags = SOUND_ENABLE | SOUND_FORMAT_16BIT | SOUND_REPEAT |
	            SOUND_LINEAR_INTERP;
	if (R_FAILED(csndPlaySound(s_csnd_music, flags, (u32)s_csnd_rate, MUSIC_GAIN,
	                           0.0f, s_csnd_mem, s_csnd_mem, s_csnd_bytes)))
		return false;

	/* Configuring a channel arms its registers; the play bits travel in their
	   own command. Sending both covers either ordering the hardware wants. */
	CSND_SetPlayState((u32)s_csnd_music, 1);
	csndExecCmds(true);
	s_csnd_wait = 0;
	return true;
}

static void csnd_music_start(int idx)
{
	if (s_csnd_nch <= 0)
		csnd_build_channels();
	if (!csnd_music_load(idx) || !csnd_music_play()) {
		csnd_music_halt();
		s_music_error = true;
		return;
	}
	s_music_idx = idx;
	s_music_error = false;
	s_music_paused = false;
	s_retries = 0;
	s_retry_wait = 0;
}

/* Moves the track to the next channel; false once the tries run out. */
static bool csnd_music_retry(void)
{
	if (s_csnd_seen || s_csnd_pick + 1 >= s_csnd_nch ||
	    s_csnd_pick + 1 >= CSND_PROBE_TRIES)
		return false;
	csnd_music_stop();
	s_csnd_pick++;
	s_csnd_music = s_csnd_chs[s_csnd_pick];
	return csnd_music_play();
}

/* Asks the service whether the channel is running, and moves the track when it
   never started: a channel the mixer leaves idle is as silent as a dead one. */
static void csnd_update(void)
{
	if (s_csnd_seen || s_music_paused || s_music_idx < 0 || !s_csnd_mem)
		return;
	if (++s_csnd_wait < CSND_PROBE_FRAMES)
		return;
	s_csnd_wait = 0;
	if (csnd_music_active())
		s_csnd_seen = true;
	else
		csnd_music_retry();
}

static void csnd_music_pause(bool paused)
{
	if (s_csnd_music < 0 || s_music_idx < 0)
		return;
	CSND_SetPlayState((u32)s_csnd_music, paused ? 0 : 1);
	csndExecCmds(true);
}

static void csnd_sfx_play(int idx, float volume)
{
	Clip* c = &s_clips[idx];
	int ch = -1;
	for (int i = 0; i < CSND_NUM_CHANNELS; i++) {
		if (i == s_csnd_music || !(csndChannels & (1u << i)))
			continue;
		u8 active = 1;
		if (R_SUCCEEDED(csndIsPlaying((u32)i, &active)) && !active) {
			ch = i;
			break;
		}
	}
	for (int i = 0; ch < 0 && i < CSND_NUM_CHANNELS; i++)
		if (i != s_csnd_music && (csndChannels & (1u << i)))
			ch = i;
	if (ch < 0)
		return;

	u32 flags = SOUND_ENABLE | SOUND_FORMAT_16BIT | SOUND_ONE_SHOT |
	            SOUND_LINEAR_INTERP;
	csndPlaySound(ch, flags, (u32)c->rate, volume, 0.0f, c->pcm, c->pcm,
	              c->bytes & ~3u);
}

/* ---------------- music stream ---------------- */

static void stream_close(void)
{
	if (s_ms_f) {
		fclose(s_ms_f);
		s_ms_f = NULL;
	}
	s_ms_pos = 0;
}

/* Fills dst from the current track, wrapping to the start at the end so the
   track loops seamlessly. */
static u32 stream_read(s16* dst, u32 bytes)
{
	u32 got = 0;
	while (got < bytes && s_ms_f) {
		if (s_ms_pos >= s_ms.data_bytes) {
			if (fseek(s_ms_f, s_ms.data_off, SEEK_SET) != 0)
				break;
			s_ms_pos = 0;
		}
		u32 rem = s_ms.data_bytes - s_ms_pos;
		u32 want = bytes - got < rem ? bytes - got : rem;
		want &= ~1u;
		if (!want)
			break;
		if (fread((unsigned char*)dst + got, 1, want, s_ms_f) != want)
			break;
		got += want;
		s_ms_pos += want;
	}
	if (got < bytes)
		memset((unsigned char*)dst + got, 0, bytes - got);
	return got;
}

static bool stream_fill(int slot)
{
	if (!s_ms_mem || !s_ms_f)
		return false;

	s16* buf = s_ms_mem + (size_t)slot * (MUSIC_CHUNK / sizeof(s16));
	u32 got = stream_read(buf, MUSIC_CHUNK);
	if (got < 2)
		return false;

	DSP_FlushDataCache(buf, got);

	ndspWaveBuf* wb = &s_ms_wb[slot];
	wb->data_vaddr = buf;
	wb->nsamples = got / (u32)(sizeof(s16) * s_ms.channels);
	wb->looping = false;
	ndspChnWaveBufAdd(MUSIC_CH, wb);
	return true;
}

static void music_halt(void)
{
	if (s_csnd) {
		csnd_music_halt();
		return;
	}
	ndspChnReset(MUSIC_CH);
	stream_close();
	s_music_idx = -1;
	s_music_paused = false;
}

static void music_start(int idx)
{
	if (s_csnd) {
		csnd_music_start(idx);
		return;
	}

	music_halt();

	FILE* f = fopen(SOUNDS[idx].path, "rb");
	if (!f) {
		s_music_error = true;
		return;
	}

	WavInfo info;
	if (!wav_parse(f, &info)) {
		fclose(f);
		s_music_error = true;
		return;
	}

	if (!s_ms_mem) {
		s_ms_mem = (s16*)linearAlloc(MUSIC_BUFS * MUSIC_CHUNK);
		if (!s_ms_mem) {
			fclose(f);
			s_music_error = true;
			return;
		}
	}

	s_ms = info;
	s_ms_pos = 0;
	if (fseek(f, info.data_off, SEEK_SET) != 0) {
		fclose(f);
		return;
	}
	s_ms_f = f;

	memset(s_ms_wb, 0, sizeof(s_ms_wb));
	channel_setup(MUSIC_CH, info.channels, info.rate, MUSIC_GAIN);
	for (int i = 0; i < MUSIC_BUFS; i++)
		if (!stream_fill(i))
			break;

	s_music_idx = idx;
	s_music_error = false;
	s_music_paused = false;
	s_retries = 0;
	s_retry_wait = 0;
	s_stall_frames = 0;
}

/* Gives up on ndsp when its channel never plays anything and moves the whole
   soundtrack to CSND. */
static void failover_to_csnd(void)
{
	int idx = s_music_idx;

	ndspChnReset(MUSIC_CH);
	for (int i = 0; i < SFX_CHANNELS; i++)
		ndspChnReset(SFX_FIRST_CH + i);
	stream_close();
	if (s_ms_mem) {
		linearFree(s_ms_mem);
		s_ms_mem = NULL;
	}
	ndspExit();

	s_music_idx = -1;
	s_retries = 0;
	s_retry_wait = 0;
	s_stall_frames = 0;

	if (R_FAILED(csndInit())) {
		s_ready = false;
		s_music_error = true;
		return;
	}
	s_csnd = true;
	s_ready = true;
	csnd_build_channels();
	if (idx >= 0)
		csnd_music_start(idx);
}

/* ---------------- public API ---------------- */

void audio_init(void)
{
	s_init_result = ndspInit();
	if (R_FAILED(s_init_result)) {
		s_init_result = csndInit();
		if (R_FAILED(s_init_result))
			return;
		s_csnd = true;
		s_ready = true;
		csnd_build_channels();
		return;
	}
	ndspSetOutputMode(NDSP_OUTPUT_STEREO);
	ndspSetMasterVol(1.0f);
	for (int i = 0; i < SFX_CHANNELS; i++)
		ndspChnReset(SFX_FIRST_CH + i);
	ndspChnReset(MUSIC_CH);
	s_ready = true;
}

void audio_exit(void)
{
	if (s_ready)
		music_halt();
	stream_close();
	if (s_ms_mem) {
		linearFree(s_ms_mem);
		s_ms_mem = NULL;
	}
	for (int i = 0; i < SOUND_COUNT; i++)
		clip_free(&s_clips[i]);
	s_want_music = -1;
	if (s_ready) {
		if (s_csnd) {
			csndExit();
		} else {
			ndspChnReset(MUSIC_CH);
			for (int i = 0; i < SFX_CHANNELS; i++)
				ndspChnReset(SFX_FIRST_CH + i);
			ndspExit();
		}
	}
	s_ready = false;
}

/* Refills finished music buffers; call once per frame. */
void audio_update(void)
{
	if (!s_ready || !s_music_on || s_music_idx < 0)
		return;
	if (s_csnd) {
		csnd_update();
		return;
	}
	if (!s_ms_f || ndspChnIsPaused(MUSIC_CH))
		return;

	bool active = false;
	bool progressed = false;
	for (int i = 0; i < MUSIC_BUFS; i++) {
		if (s_ms_wb[i].status == NDSP_WBUF_DONE) {
			progressed = true;
			if (stream_fill(i))
				active = true;
		} else if (s_ms_wb[i].status != NDSP_WBUF_FREE) {
			active = true;
		}
	}
	if (progressed) {
		s_retries = 0;
		s_retry_wait = 0;
		s_stall_frames = 0;
		return;
	}
	if (active) {
		s_retry_wait = 0;
		if (++s_stall_frames >= MUSIC_STALL_FRAMES)
			failover_to_csnd();
		return;
	}

	/* Nothing is queued and nothing is playing: the channel lost its buffers.
	   Re-queue the track, giving up after a few tries. */
	if (s_retry_wait > 0) {
		s_retry_wait--;
		return;
	}
	if (s_retries >= MUSIC_RETRIES) {
		s_music_error = true;
		return;
	}
	s_retries++;
	s_retry_wait = MUSIC_RETRY_FRAMES;
	int idx = s_music_idx;
	music_start(idx);
}

const char* audio_status_text(void)
{
	if (!s_ready)
		return "音频未初始化";
	if (!s_music_on)
		return "音乐已关闭";
	if (s_music_error)
		return "音乐播放失败";
	if (s_music_idx < 0)
		return "音乐未播放";

	/* The channel number matters when a report comes back from a console: it
	   says which channels the service handed out and which one ran. */
	if (s_csnd)
		snprintf(s_status_buf, sizeof s_status_buf, "%s CSND%d",
		         csnd_music_active() ? "音乐播放中" : "音乐等待中", s_csnd_music);
	else
		snprintf(s_status_buf, sizeof s_status_buf, "%s NDSP",
		         ndspChnIsPlaying(MUSIC_CH) ? "音乐播放中" : "音乐等待中");
	return s_status_buf;
}

bool audio_ok(void)
{
	return s_ready && !s_error && !s_music_error;
}

const char* audio_error_text(void)
{
	/* Both sound services failed; the code says which one and why. */
	if (!s_ready) {
		snprintf(s_err_buf, sizeof s_err_buf, "音频初始化失败 %08lX",
		         (unsigned long)(u32)s_init_result);
		return s_err_buf;
	}
	if (s_music_error)
		return "音频：音乐缓冲分配失败";
	if (s_error)
		return "音频：音效加载失败";
	return NULL;
}

void audio_music(const char* name)
{
	int idx = find_sound(name);
	if (idx < 0 || !SOUNDS[idx].music)
		return;
	s_want_music = idx;
	if (!s_ready || !s_music_on)
		return;
	if (s_music_idx == idx) {
		s_music_paused = false;
		if (s_csnd)
			csnd_music_pause(false);
		else
			ndspChnSetPaused(MUSIC_CH, false);
		return;
	}
	music_start(idx);
}

void audio_stop_music(void)
{
	s_want_music = -1;
	if (!s_ready)
		return;
	music_halt();
}

void audio_pause_music(void)
{
	if (!s_ready || s_music_idx < 0)
		return;
	s_music_paused = true;
	if (s_csnd)
		csnd_music_pause(true);
	else
		ndspChnSetPaused(MUSIC_CH, true);
}

void audio_resume_music(void)
{
	if (!s_ready || s_music_idx < 0)
		return;
	s_music_paused = false;
	if (s_csnd)
		csnd_music_pause(false);
	else
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
	if (!s_ready || !s_sfx_on)
		return;
	int idx = find_sound(name);
	if (idx < 0 || SOUNDS[idx].music)
		return;
	if (!clip_load(idx))
		return;

	if (s_csnd) {
		csnd_sfx_play(idx, volume);
		return;
	}

	Clip* c = &s_clips[idx];
	int ch = pick_sfx_channel();
	int slot = ch - SFX_FIRST_CH;
	channel_setup(ch, c->channels, c->rate, volume);

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

void audio_set_music_enabled(bool on)
{
	if (s_music_on == on)
		return;
	s_music_on = on;
	if (!s_ready)
		return;
	if (!on) {
		if (s_music_idx >= 0)
			s_want_music = s_music_idx;
		music_halt();
	} else if (s_want_music >= 0) {
		int idx = s_want_music;
		s_want_music = -1;
		audio_music(SOUNDS[idx].name);
	}
}

void audio_set_sfx_enabled(bool on)
{
	if (s_sfx_on == on)
		return;
	s_sfx_on = on;
	if (!s_ready)
		return;
	if (!on) {
		if (s_csnd) {
			for (int ch = 0; ch < CSND_NUM_CHANNELS; ch++)
				if (ch != s_csnd_music && (csndChannels & (1u << ch)))
					CSND_SetPlayState((u32)ch, 0);
			csndExecCmds(true);
		} else {
			for (int i = 0; i < SFX_CHANNELS; i++)
				ndspChnReset(SFX_FIRST_CH + i);
		}
	}
}

bool audio_music_enabled(void)
{
	return s_music_on;
}

bool audio_sfx_enabled(void)
{
	return s_sfx_on;
}
