/* Minimal SDL2 frontend for the R36S port: boots straight into a solo match
   and reuses the 3DS game/render code. The full menu/HUD flow can be ported
   once this skeleton is accepted on hardware. */

#include <SDL.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assets.h"
#include "audio.h"
#include "citro2d.h"
#include "game.h"

#define WIN_W 640
#define WIN_H 480
#define HUD_H 40.0f
#define STEP_MS (1000.0 / 35.0)
#define SCENE_COUNT 5

static Game g_game;
static C3D_RenderTarget* s_top;
static bool g_keys[4];
static bool g_fire;
static bool g_paused;
static bool g_quit;
static SDL_GameController* g_pad;
static double g_acc;
static int g_w = WIN_W, g_h = WIN_H;

bool input_dir_held(int layout, int d)
{
	(void)layout;
	return d >= 0 && d < 4 && g_keys[d];
}

bool input_fire_held(int layout)
{
	(void)layout;
	return g_fire;
}

static void start_match(void)
{
	int scene = rng_int(0, SCENE_COUNT - 1);
	const int hero_ids[1] = { 1 };
	const int lives[1] = { 1 };
	game_init(&g_game, 1, 1, 1, 20, scene, hero_ids, lives);
	g_paused = false;
}

static void open_pad(void)
{
	for (int i = 0; i < SDL_NumJoysticks(); i++) {
		if (!SDL_IsGameController(i))
			continue;
		g_pad = SDL_GameControllerOpen(i);
		if (g_pad)
			break;
	}
}

/* keyboard first, controller overrides */
static void read_input(void)
{
	const Uint8* k = SDL_GetKeyboardState(NULL);
	g_keys[0] = k[SDL_SCANCODE_LEFT] || k[SDL_SCANCODE_A];
	g_keys[1] = k[SDL_SCANCODE_UP] || k[SDL_SCANCODE_W];
	g_keys[2] = k[SDL_SCANCODE_RIGHT] || k[SDL_SCANCODE_D];
	g_keys[3] = k[SDL_SCANCODE_DOWN] || k[SDL_SCANCODE_S];
	g_fire = k[SDL_SCANCODE_SPACE] || k[SDL_SCANCODE_Z] || k[SDL_SCANCODE_J];

	if (!g_pad)
		return;
	if (SDL_GameControllerGetButton(g_pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT))
		g_keys[0] = true;
	if (SDL_GameControllerGetButton(g_pad, SDL_CONTROLLER_BUTTON_DPAD_UP))
		g_keys[1] = true;
	if (SDL_GameControllerGetButton(g_pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT))
		g_keys[2] = true;
	if (SDL_GameControllerGetButton(g_pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN))
		g_keys[3] = true;

	Sint16 ax = SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_LEFTX);
	Sint16 ay = SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_LEFTY);
	if (ax < -12000) g_keys[0] = true;
	if (ax > 12000) g_keys[2] = true;
	if (ay < -12000) g_keys[1] = true;
	if (ay > 12000) g_keys[3] = true;

	if (SDL_GameControllerGetButton(g_pad, SDL_CONTROLLER_BUTTON_A))
		g_fire = true;
}

static void draw_hud(void)
{
	C2D_DrawRectSolid(0.0f, 0.0f, 0.0f, (float)g_w, HUD_H, C2D_Color32(10, 14, 24, 217));

	char buf[128];
	snprintf(buf, sizeof(buf), "第 %d/%d 关  %s  %d:%02d",
	         g_game.stage, g_game.total_stage, SCENES[g_game.scene_id].name,
	         g_game.time / 60, g_game.time % 60);
	font_draw(10.0f, 4.0f, 0.55f, 0xffffff, buf);

	char lives[128];
	int n = snprintf(lives, sizeof(lives), "生命");
	for (int i = 0; i < g_game.hero_count; i++) {
		Hero* h = &g_game.heroes[i];
		const char* name = HEROES[h->hero_id - 1].name;
		n += snprintf(lives + n, sizeof(lives) - (size_t)n, "  %s %d", name,
		              h->lives > 0 ? h->lives : 0);
	}
	font_draw(10.0f, 22.0f, 0.42f, 0xc7d0de, lives);

	const char* audio = audio_status_text();
	font_draw((float)g_w - font_width(0.42f, audio) - 10.0f, 22.0f, 0.42f,
	          audio_ok() ? 0x9fd6a0 : 0xff8a8a, audio);
}

static void text_center(float cx, float y, float scale, uint32_t color, const char* text)
{
	font_draw(cx - font_width(scale, text) / 2.0f, y, scale, color, text);
}

static void panel(float y, float h)
{
	C2D_DrawRectSolid((float)g_w / 2.0f - 180.0f, y, 0.0f, 360.0f, h,
	                  C2D_Color32(0, 0, 0, 178));
}

static void render(void)
{
	float s = (float)(g_h - HUD_H) / 240.0f;
	float ox = ((float)g_w - 400.0f * s) / 2.0f;

	C2D_TargetClear(s_top, C2D_Color32(0, 0, 0, 255));
	C2D_SceneBegin(s_top);

	shim_set_transform(s, ox, HUD_H);
	game_render(&g_game, 240.0f / 390.0f);

	shim_set_transform(1.0f, 0.0f, 0.0f);
	draw_hud();

	if (g_paused) {
		panel((float)g_h / 2.0f - 34.0f, 68.0f);
		text_center((float)g_w / 2.0f, (float)g_h / 2.0f - 20.0f, 1.0f,
		            0xffffff, "暂停");
	}
	if (g_game.state == STATE_OVER) {
		panel((float)g_h / 2.0f - 56.0f, 112.0f);
		text_center((float)g_w / 2.0f, (float)g_h / 2.0f - 30.0f, 0.9f,
		            0xffffff, "关卡结束");
		text_center((float)g_w / 2.0f, (float)g_h / 2.0f + 10.0f, 0.5f,
		            0xc7d0de, "按 R 重新开始");
	}

	C3D_FrameEnd(0);
}

static void run_step(void)
{
	if (g_paused || g_game.state == STATE_OVER)
		return;
	read_input();
	game_step(&g_game);
}

int audio_debug_peak(void); /* audio_sdl.c: peak mixed sample since last call */

static int self_test(int ticks, const char* out_path)
{
	start_match();
	audio_music("bgm1");
	for (int i = 0; i < ticks; i++) {
		int phase = (i / 35) % 4;
		memset(g_keys, 0, sizeof(g_keys));
		g_keys[phase] = true;
		g_fire = (i % 45) == 40;
		game_step(&g_game);
		if (g_game.state == STATE_OVER)
			break;
	}
	render();
	bool saved = shim_save_bmp(out_path);
	printf("selftest: scene=%s stage=%d state=%d lives=%d score=%d tick=%d saved=%d\n",
	       SCENES[g_game.scene_id].name, g_game.stage, g_game.state,
	       g_game.heroes[0].lives, g_game.heroes[0].score, g_game.tick, (int)saved);
	for (int i = 0; i < g_game.hero_count; i++) {
		Hero* h = &g_game.heroes[i];
		printf("  hero %s cell=(%d,%d) human=%d alive=%d lives=%d score=%d\n",
		       HEROES[h->hero_id - 1].name, h->x, h->y, (int)h->human,
		       (int)h->alive, h->lives, h->score);
	}
	if (audio_ok()) {
		SDL_Delay(400);
		printf("  audio: %s peak=%d\n", audio_status_text(), audio_debug_peak());
	} else {
		printf("  audio: %s\n", audio_status_text());
	}
	return saved ? 0 : 1;
}

int main(int argc, char** argv)
{
	(void)argc;
	(void)argv;

	const char* selftest = getenv("POPBOMB_SELFTEST");
	bool hidden = selftest != NULL;
	if (!shim_gfx_init(WIN_W, WIN_H, "PopBomb Arena", hidden)) {
		fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
		return 1;
	}
	s_top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
	shim_window_size(&g_w, &g_h);

	if (!getenv("POPBOMB_NOAUDIO"))
		audio_init();

	if (!assets_load_all()) {
		fprintf(stderr, "asset load failed (set POPBOMB_DATA to ports/3ds)\n");
		return 1;
	}
	font_init();
	rng_seed((uint32_t)SDL_GetTicks());

	if (selftest) {
		int rc = self_test(atoi(selftest), "/tmp/popbomb_selftest.bmp");
		assets_free_all();
		font_shutdown();
		audio_exit();
		shim_gfx_shutdown();
		return rc;
	}

	start_match();
	audio_music("bgm1");
	open_pad();

	Uint32 last = SDL_GetTicks();
	while (!g_quit) {
		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {
			if (ev.type == SDL_QUIT)
				g_quit = true;
			else if (ev.type == SDL_KEYDOWN && ev.key.keysym.scancode == SDL_SCANCODE_ESCAPE)
				g_quit = true;
			else if (ev.type == SDL_KEYDOWN && ev.key.keysym.scancode == SDL_SCANCODE_RETURN)
				g_paused = !g_paused;
			else if (ev.type == SDL_KEYDOWN && ev.key.keysym.scancode == SDL_SCANCODE_R)
				start_match();
			else if (ev.type == SDL_CONTROLLERDEVICEADDED && !g_pad)
				open_pad();
			else if (ev.type == SDL_CONTROLLERBUTTONDOWN &&
			         ev.cbutton.button == SDL_CONTROLLER_BUTTON_START)
				g_paused = !g_paused;
			else if (ev.type == SDL_CONTROLLERBUTTONDOWN &&
			         ev.cbutton.button == SDL_CONTROLLER_BUTTON_BACK)
				start_match();
		}

		Uint32 now = SDL_GetTicks();
		g_acc += (double)(now - last);
		last = now;
		if (g_acc > 240.0)
			g_acc = 240.0;
		int steps = 0;
		while (g_acc >= STEP_MS && steps < 5) {
			g_acc -= STEP_MS;
			steps++;
			run_step();
		}

		render();
		SDL_Delay(1);
	}

	if (g_pad)
		SDL_GameControllerClose(g_pad);
	audio_exit();
	assets_free_all();
	font_shutdown();
	shim_gfx_shutdown();
	return 0;
}
