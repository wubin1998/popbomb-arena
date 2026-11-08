#include <3ds.h>
#include <citro2d.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "assets.h"
#include "audio.h"
#include "game.h"

#define STEP_MS (1000.0 / 35.0)

enum { SCREEN_TITLE = 0, SCREEN_GAME, SCREEN_RESULT };
enum { ROW_PLAYERS = 0, ROW_DIFF, ROW_HERO, ROW_START, ROW_COUNT };

static C3D_RenderTarget* s_top;
static C3D_RenderTarget* s_bottom;

static Game g_game;
static int g_screen = SCREEN_TITLE;
static int g_selected = ROW_START;
static bool g_quit;

typedef struct {
	int players;
	int diff;
	int hero_pick[2];
	int lives[2];
	int totals[2];
	int stage;
	int total_stage;
	int order[6];
	int pair[2];
} Run;

static Run g_run;

static const char* const DIFF_NAMES[3] = { "EASY", "NORMAL", "HARD" };
static const int DIFF_STAGES[3] = { 15, 20, 25 };

/* ---------------- input ---------------- */

bool input_dir_held(int layout, int d)
{
	if (layout == 2) {
		circlePosition p;
		hidCircleRead(&p);
		switch (d) {
		case 0: return p.dx < -40;
		case 1: return p.dy > 40;
		case 2: return p.dx > 40;
		default: return p.dy < -40;
		}
	}
	static const u32 keys[4] = { KEY_DLEFT, KEY_DUP, KEY_DRIGHT, KEY_DDOWN };
	return (hidKeysHeld() & keys[d]) != 0;
}

bool input_fire_held(int layout)
{
	if (layout == 2)
		return (hidKeysHeld() & KEY_X) != 0;
	return (hidKeysHeld() & KEY_A) != 0;
}

/* ---------------- helpers ---------------- */

static void draw_icon(int set, int frame, float x, float y, float w, float h, float alpha)
{
	C2D_Image im = assets_frame(set, frame);
	if (!im.tex || !im.subtex)
		return;
	C2D_ImageTint tint;
	const C2D_ImageTint* tp = NULL;
	if (alpha < 0.999f) {
		C2D_AlphaImageTint(&tint, alpha);
		tp = &tint;
	}
	C2D_DrawImageAt(im, x, y, 0.0f, tp, w / im.subtex->width, h / im.subtex->height);
}

static void text_center(float cx, float y, float scale, uint32_t color, const char* text)
{
	font_draw(cx - font_width(scale, text) / 2.0f, y, scale, color, text);
}

static void fmt_time(char* out, size_t n, int seconds)
{
	if (seconds < 0)
		seconds = 0;
	snprintf(out, n, "%d:%02d", seconds / 60, seconds % 60);
}

static float clampf(float v, float a, float b)
{
	return v < a ? a : (v > b ? b : v);
}

static void shuffle_order(int* a, int n)
{
	for (int i = n - 1; i > 0; i--) {
		int j = rng_int(0, i);
		int t = a[i];
		a[i] = a[j];
		a[j] = t;
	}
}

/* ---------------- run / stage flow ---------------- */

static void next_special_pair(void)
{
	int cycle = (g_run.stage - 1) % 4;
	if (cycle == 0 || cycle == 3) {
		shuffle_order(g_run.order, 6);
		g_run.pair[0] = g_run.order[0];
		g_run.pair[1] = g_run.order[1];
	} else {
		g_run.pair[0] = g_run.order[cycle * 2];
		g_run.pair[1] = g_run.order[cycle * 2 + 1];
	}
}

static void start_stage(void)
{
	int scene = rng_int(0, 4);
	game_init(&g_game, g_run.players, g_run.diff, g_run.stage, g_run.total_stage,
	          scene, g_run.hero_pick, g_run.lives);
	g_game.special_pair[0] = g_run.pair[0];
	g_game.special_pair[1] = g_run.pair[1];
	g_screen = SCREEN_GAME;

	char bgm[8];
	snprintf(bgm, sizeof(bgm), "bgm%d", 1 + (g_run.stage - 1) % 4);
	audio_music(bgm);
}

static void start_run(void)
{
	int players = g_run.players;
	int diff = g_run.diff;
	int hero0 = g_run.hero_pick[0];
	int hero1 = g_run.hero_pick[1];

	memset(&g_run, 0, sizeof(g_run));
	g_run.players = players;
	g_run.diff = diff;
	g_run.hero_pick[0] = hero0;
	g_run.hero_pick[1] = hero1;
	g_run.stage = 1;
	g_run.total_stage = DIFF_STAGES[diff];
	g_run.lives[0] = 1;
	g_run.lives[1] = 1;
	for (int i = 0; i < 6; i++)
		g_run.order[i] = 7 + i;
	shuffle_order(g_run.order, 6);
	next_special_pair();
	start_stage();
}

static void handle_result(void)
{
	for (int i = 0; i < g_run.players; i++) {
		Hero* h = game_hero(&g_game, i + 1);
		g_run.lives[i] = h ? (h->lives > 0 ? h->lives : 0) : 0;
		if (g_game.stage_clear && h)
			g_run.totals[i] += h->score;
	}

	bool game_over = !g_game.stage_clear && g_run.lives[0] <= 0
	                 && (g_run.players < 2 || g_run.lives[1] <= 0);
	if (!game_over) {
		for (int i = 0; i < g_run.players; i++) {
			if (g_run.lives[i] <= 0) {
				g_run.lives[i] = 1;
				g_run.totals[i] = g_run.totals[i] > 1000 ? g_run.totals[i] - 1000 : 0;
			}
		}
	}
	g_game.can_retry = !game_over && !g_game.stage_clear;

	audio_stop_music();
	if (g_game.stage_clear)
		audio_play_vol("stageWin", 0.7f);
	else
		audio_play_vol("stageLose", 0.6f);
	g_screen = SCREEN_RESULT;
}

/* ---------------- menus and global keys ---------------- */

static void cycle_hero(int player, int delta)
{
	int id = g_run.hero_pick[player];
	id = 1 + ((id - 1 + (delta > 0 ? 1 : 3)) % 4);
	if (g_run.players == 2) {
		int other = g_run.hero_pick[1 - player];
		if (id == other)
			id = 1 + (id % 4);
	}
	g_run.hero_pick[player] = id;
}

static void toggle_players(void)
{
	g_run.players = g_run.players == 1 ? 2 : 1;
	if (g_run.players == 2 && g_run.hero_pick[1] == g_run.hero_pick[0])
		cycle_hero(1, 1);
}

static void handle_title(u32 down)
{
	if (down & (KEY_DUP | KEY_CPAD_UP))
		g_selected = (g_selected + ROW_COUNT - 1) % ROW_COUNT;
	else if (down & (KEY_DDOWN | KEY_CPAD_DOWN))
		g_selected = (g_selected + 1) % ROW_COUNT;

	int dir = 0;
	if (down & (KEY_DLEFT | KEY_CPAD_LEFT))
		dir = -1;
	else if (down & (KEY_DRIGHT | KEY_CPAD_RIGHT))
		dir = 1;

	if (dir != 0) {
		if (g_selected == ROW_PLAYERS) {
			toggle_players();
		} else if (g_selected == ROW_DIFF) {
			g_run.diff = (g_run.diff + 3 + dir) % 3;
		} else if (g_selected == ROW_HERO) {
			cycle_hero(0, dir);
		}
	}

	if (down & KEY_A) {
		if (g_selected == ROW_START) {
			start_run();
		} else if (g_selected == ROW_PLAYERS) {
			toggle_players();
		} else if (g_selected == ROW_DIFF) {
			g_run.diff = (g_run.diff + 1) % 3;
		} else if (g_selected == ROW_HERO) {
			cycle_hero(0, 1);
		}
	}
	if (down & KEY_X) {
		if (g_run.players == 2)
			cycle_hero(1, 1);
	}
}

static void handle_global(u32 down)
{
	if (g_screen == SCREEN_TITLE) {
		if (down & KEY_START) {
			g_quit = true;
			return;
		}
		handle_title(down);
		return;
	}

	if (g_screen == SCREEN_GAME) {
		if (down & KEY_START) {
			g_game.paused = !g_game.paused;
			if (g_game.paused)
				audio_pause_music();
			else
				audio_resume_music();
			g_selected = ROW_START;
		}
		if (down & KEY_SELECT) {
			audio_stop_music();
			audio_music("menu");
			g_screen = SCREEN_TITLE;
		}
		return;
	}

	if (g_screen == SCREEN_RESULT) {
		if (down & (KEY_A | KEY_START)) {
			if (g_game.stage_clear) {
				if (g_game.final_stage) {
					audio_music("menu");
					g_screen = SCREEN_TITLE;
				} else {
					g_run.stage++;
					next_special_pair();
					start_stage();
				}
			} else if (g_game.can_retry) {
				start_stage();
			} else {
				audio_music("menu");
				g_screen = SCREEN_TITLE;
			}
		}
		if (down & KEY_SELECT) {
			audio_music("menu");
			g_screen = SCREEN_TITLE;
		}
	}
}

/* ---------------- title screen ---------------- */

static void render_title_top(void)
{
	text_center(200.0f, 24.0f, 1.35f, 0xffffff, "POPBOMB ARENA");
	text_center(200.0f, 66.0f, 0.55f, 0xb8c4d8, "3DS HOMEBREW EDITION");

	float base = 200.0f - 2.0f * 74.0f;
	for (int i = 0; i < 4; i++) {
		float bob = sinf((float)osGetTime() / 420.0f + (float)i * 0.9f) * 5.0f;
		draw_icon(SET_HERO1 + i, 4, base + (float)i * 74.0f + 8.0f, 118.0f + bob,
		          58.0f, 92.0f, 1.0f);
	}
	text_center(200.0f, 214.0f, 0.45f, 0x8f9bb0, "A: SELECT   DPAD: CHANGE   START: QUIT");
	const char* audio_msg = audio_error_text();
	if (audio_msg)
		text_center(200.0f, 88.0f, 0.40f, 0xff7d7d, audio_msg);
}

/* ---------------- bottom screen ---------------- */

static void card_rect(float x, float y, float w, float h, uint32_t rgb, float a)
{
	C2D_DrawRectSolid(x, y, 0.0f, w, h,
	                  C2D_Color32((rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff,
	                              (u32)(clampf(a, 0.0f, 1.0f) * 255.0f)));
}

static void render_title_bottom(void)
{
	card_rect(0, 0, 320, 240, 0x151a26, 1.0f);
	text_center(160.0f, 8.0f, 0.75f, 0xffffff, "GAME SETUP");

	float y = 60.0f;
	for (int i = 0; i < ROW_COUNT; i++) {
		char line[96];
		if (i == ROW_PLAYERS)
			snprintf(line, sizeof(line), "PLAYERS      < %dP >", g_run.players);
		else if (i == ROW_DIFF)
			snprintf(line, sizeof(line), "DIFFICULTY   < %s >", DIFF_NAMES[g_run.diff]);
		else if (i == ROW_HERO)
			snprintf(line, sizeof(line), "HERO         < %s%s%s >",
			         HEROES[g_run.hero_pick[0] - 1].name,
			         g_run.players == 2 ? " / " : "",
			         g_run.players == 2 ? HEROES[g_run.hero_pick[1] - 1].name : "");
		else
			snprintf(line, sizeof(line), "            START GAME");

		uint32_t color = i == g_selected ? 0xffd166 : 0xd7deea;
		if (i == g_selected) {
			float w = font_width(0.72f, line);
			font_draw(164.0f - w / 2.0f - 14.0f, y, 0.7f, 0xffd166, ">");
		}
		text_center(164.0f, y, 0.72f, color, line);
		y += 38.0f;
	}

	text_center(160.0f, 198.0f, 0.48f, 0x93a0b5, "STAGES: EASY 15 / NORMAL 20 / HARD 25");
	text_center(160.0f, 216.0f, 0.44f, 0x6f7c92, "1P DPAD+A   2P DPAD+A / CPAD+X   X: P2 HERO");
}

static void render_hud(void)
{
	card_rect(0, 0, 320, 240, 0x11151f, 1.0f);

	char buf[64];
	char tbuf[16];
	fmt_time(tbuf, sizeof(tbuf), g_game.time);
	snprintf(buf, sizeof(buf), "STAGE %d/%d", g_game.stage, g_game.total_stage);
	font_draw(8.0f, 5.0f, 0.6f, 0xffffff, buf);
	text_center(160.0f, 4.0f, 0.6f, 0xffd166, SCENES[g_game.map.scene_id].name);
	float tw = font_width(0.75f, tbuf);
	font_draw(312.0f - tw, 2.0f, 0.75f, g_game.time <= 30 ? 0xff6b6b : 0xffffff, tbuf);
	C2D_DrawRectSolid(8.0f, 30.0f, 0.0f, 304.0f, 1.0f, C2D_Color32(255, 255, 255, 40));

	for (int i = 0; i < g_game.hero_count; i++) {
		Hero* h = &g_game.heroes[i];
		const HeroInfo* info = &HEROES[h->hero_id - 1];
		float x = (i % 2 == 0) ? 8.0f : 164.0f;
		float y = (i < 2) ? 38.0f : 138.0f;
		float w = 148.0f;
		float hgt = 96.0f;

		card_rect(x, y, w, hgt, h->alive ? 0x1c2333 : 0x171b24, 1.0f);
		C2D_DrawRectSolid(x, y, 0.0f, 3.0f, hgt,
		                  C2D_Color32((info->color >> 16) & 0xff, (info->color >> 8) & 0xff,
		                              info->color & 0xff, h->alive ? 255 : 90));

		draw_icon(info->set_id, 4, x + 8.0f, y + 12.0f, 34.0f, 54.0f,
		          h->alive ? 1.0f : 0.35f);

		font_draw(x + 50.0f, y + 4.0f, 0.55f, info->color, info->name);
		if (!h->alive)
			font_draw(x + 50.0f, y + 22.0f, 0.45f, 0x8a94a6, "OUT");

		snprintf(buf, sizeof(buf), "%d", h->score);
		font_draw(x + 50.0f, y + 34.0f, 0.55f, 0xffffff, buf);

		snprintf(buf, sizeof(buf), "LIFE %d", h->lives > 0 ? h->lives : 0);
		font_draw(x + 8.0f, y + 70.0f, 0.42f, 0xc7d0de, buf);

		snprintf(buf, sizeof(buf), "BOMB %d/%d", h->bombs_out, h->total_bomb);
		font_draw(x + 8.0f, y + 82.0f, 0.38f, 0xc7d0de, buf);

		snprintf(buf, sizeof(buf), "PWR %d", h->bomb_level);
		font_draw(x + 86.0f, y + 70.0f, 0.38f, 0xc7d0de, buf);

		if (h->push_bomb_able)
			draw_icon(SET_TOOL, TOOL_GLOVE, x + 118.0f, y + 12.0f, 20.0f, 20.0f, 1.0f);
		if (h->shield)
			draw_icon(SET_TOOL, TOOL_SHIELD, x + 118.0f, y + 36.0f, 20.0f, 20.0f, 1.0f);
		if (h->special.kind != TOOL_NONE) {
			draw_icon(SET_TOOL, h->special.kind, x + 86.0f, y + 84.0f, 18.0f, 18.0f, 1.0f);
			snprintf(buf, sizeof(buf), "x%d", h->special.count);
			font_draw(x + 106.0f, y + 82.0f, 0.38f, 0xffffff, buf);
		}
	}

	if (g_game.state == STATE_COUNTDOWN) {
		int n = (g_game.count_ticks + FPS - 1) / FPS;
		if (n > 4)
			n = 4;
		snprintf(buf, sizeof(buf), "%d", n > 0 ? n : 1);
		text_center(160.0f, 96.0f, 1.6f, 0xffd166, buf);
	}

	if (g_game.paused) {
		card_rect(0, 0, 320, 240, 0x000000, 0.62f);
		text_center(160.0f, 84.0f, 1.1f, 0xffffff, "PAUSED");
		text_center(160.0f, 130.0f, 0.5f, 0xc7d0de, "START: RESUME");
		text_center(160.0f, 150.0f, 0.5f, 0xc7d0de, "SELECT: QUIT TO TITLE");
	}
}

static void render_result_bottom(void)
{
	card_rect(0, 0, 320, 240, 0x11151f, 1.0f);

	const char* title;
	if (g_game.stage_clear)
		title = g_game.final_stage ? "ALL STAGES CLEAR!" : "STAGE CLEAR!";
	else
		title = g_game.can_retry ? "TRY AGAIN" : "GAME OVER";

	text_center(160.0f, 14.0f, 0.95f, g_game.stage_clear ? 0xffd166 : 0xff7d7d, title);

	char buf[64];
	snprintf(buf, sizeof(buf), "STAGE %d/%d   %s", g_game.stage, g_game.total_stage,
	         SCENES[g_game.map.scene_id].name);
	text_center(160.0f, 50.0f, 0.5f, 0xbcc6d6, buf);

	char tbuf[16];
	fmt_time(tbuf, sizeof(tbuf), g_game.time);
	snprintf(buf, sizeof(buf), "TIME LEFT %s", tbuf);
	text_center(160.0f, 68.0f, 0.5f, 0xbcc6d6, buf);

	float y = 96.0f;
	for (int i = 0; i < g_game.hero_count && i < 4; i++) {
		Hero* h = &g_game.heroes[i];
		const HeroInfo* info = &HEROES[h->hero_id - 1];
		snprintf(buf, sizeof(buf), "%-6s %6d  BRK %d  KILL %d  TOOL %d",
		         info->name, h->score, h->brick_score, h->kill_score, h->tool_score);
		font_draw(20.0f, y, 0.45f, h->human ? info->color : 0x9aa6ba, buf);
		y += 20.0f;
	}

	if (g_run.players == 1) {
		snprintf(buf, sizeof(buf), "TOTAL  %d", g_run.totals[0]);
	} else {
		snprintf(buf, sizeof(buf), "TOTAL  1P %d   2P %d", g_run.totals[0], g_run.totals[1]);
	}
	text_center(160.0f, 184.0f, 0.5f, 0xd7deea, buf);

	const char* hint;
	if (g_game.stage_clear)
		hint = g_game.final_stage ? "A: TITLE" : "A: NEXT STAGE";
	else
		hint = g_game.can_retry ? "A: RETRY (lives left)" : "A: TITLE";
	text_center(160.0f, 214.0f, 0.55f, 0xffd166, hint);
}

/* ---------------- render ---------------- */

static void render(void)
{
	C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

	C2D_TargetClear(s_top, C2D_Color32(0, 0, 0, 255));
	C2D_SceneBegin(s_top);
	if (g_screen == SCREEN_TITLE) {
		render_title_top();
	} else {
		game_render(&g_game, 240.0f / 390.0f);
		if (g_game.paused)
			text_center(200.0f, 110.0f, 1.0f, 0xffffff, "PAUSED");
	}

	C2D_TargetClear(s_bottom, C2D_Color32(0, 0, 0, 255));
	C2D_SceneBegin(s_bottom);
	if (g_screen == SCREEN_TITLE)
		render_title_bottom();
	else if (g_screen == SCREEN_GAME)
		render_hud();
	else
		render_result_bottom();

	C3D_FrameEnd(0);
}

/* ---------------- main ---------------- */

int main(void)
{
	romfsInit();
	audio_init();
	gfxInitDefault();
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
	C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
	C2D_Prepare();

	s_top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
	s_bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

	bool assets_ok = assets_load_all();
	font_init();
	rng_seed((uint32_t)osGetTime());

	g_run.players = 1;
	g_run.diff = 1;
	g_run.hero_pick[0] = 1;
	g_run.hero_pick[1] = 2;
	g_run.total_stage = DIFF_STAGES[g_run.diff];
	g_selected = ROW_START;

	audio_music("menu");

	u64 last = osGetTime();
	double acc = 0.0;

	while (aptMainLoop()) {
		hidScanInput();
		u32 down = hidKeysDown();

		if (!assets_ok) {
			if (down & KEY_START)
				break;
		} else {
			handle_global(down);
		}
		if (g_quit)
			break;

		u64 now = osGetTime();
		double dt = (double)(now - last);
		last = now;
		if (dt > 240.0)
			dt = 240.0;
		acc += dt;

		int steps = 0;
		while (acc >= STEP_MS && steps < 5) {
			acc -= STEP_MS;
			steps++;
			if (g_screen == SCREEN_GAME && !g_game.paused)
				game_step(&g_game);
		}

		if (g_screen == SCREEN_GAME && g_game.state == STATE_OVER)
			handle_result();

		if (!assets_ok) {
			C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
			C2D_TargetClear(s_top, C2D_Color32(120, 0, 0, 255));
			C2D_SceneBegin(s_top);
			C2D_TargetClear(s_bottom, C2D_Color32(80, 0, 0, 255));
			C2D_SceneBegin(s_bottom);
			text_center(160.0f, 100.0f, 0.5f, 0xffffff, "ASSET LOAD FAILED");
			text_center(160.0f, 120.0f, 0.5f, 0xffffff, "PRESS START TO EXIT");
			C3D_FrameEnd(0);
		} else {
			render();
		}
	}

	audio_exit();
	assets_free_all();
	font_shutdown();
	C2D_Fini();
	C3D_Fini();
	gfxExit();
	romfsExit();
	return 0;
}
