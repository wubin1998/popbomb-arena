#include <3ds.h>
#include <citro2d.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "assets.h"
#include "audio.h"
#include "game.h"
#include "net.h"

#define STEP_MS (1000.0 / 35.0)

enum { SCREEN_TITLE = 0, SCREEN_GAME, SCREEN_RESULT, SCREEN_LOBBY };
enum { PAGE_HOME = 0, PAGE_MODE, PAGE_DIFF, PAGE_HERO, PAGE_INFO };
enum { MODE_SOLO = 0, MODE_LOCAL, MODE_HOST, MODE_JOIN };

static C3D_RenderTarget* s_top;
static C3D_RenderTarget* s_bottom;

static Game g_game;
static int g_screen = SCREEN_TITLE;
static int g_page = PAGE_HOME;
static int g_cursor;
static bool g_quit;

typedef struct {
	int mode;
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
static const char* g_notice;
static bool g_stage_seen;
static uint8_t g_stage_seq;

static const char* const MODE_NAMES[4] = { "单人", "双人", "联机主机", "联机加入" };
static const char* const DIFF_NAMES[3] = { "简单", "普通", "困难" };
static const int DIFF_STAGES[3] = { 15, 20, 25 };

/* ---------------- input ---------------- */

bool input_dir_held(int layout, int d)
{
	if (layout == 1 || layout == 2) {
		if (net_role() != NET_OFF)
			return (net_input_bits(layout) >> d) & 1;
	}
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
	if (layout == 1 || layout == 2) {
		if (net_role() != NET_OFF)
			return (net_input_bits(layout) >> 4) & 1;
	}
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

static void card_rect(float x, float y, float w, float h, uint32_t rgb, float a)
{
	C2D_DrawRectSolid(x, y, 0.0f, w, h,
	                  C2D_Color32((rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff,
	                              (u32)(clampf(a, 0.0f, 1.0f) * 255.0f)));
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
	static uint8_t stage_seq;

	uint32_t seed = (uint32_t)(osGetTime() ^ ((uint64_t)(g_run.stage + 1) * 2654435761u));
	rng_seed(seed);
	int scene = rng_int(0, 4);

	if (net_role() == NET_HOST) {
		NetStage st;
		memset(&st, 0, sizeof(st));
		st.seed = rng_next();
		st.seq = ++stage_seq;
		st.scene = (uint8_t)scene;
		st.stage = (uint8_t)g_run.stage;
		st.total_stage = (uint8_t)g_run.total_stage;
		st.diff = (uint8_t)g_run.diff;
		st.players = (uint8_t)g_run.players;
		st.hero[0] = (uint8_t)g_run.hero_pick[0];
		st.hero[1] = (uint8_t)g_run.hero_pick[1];
		st.lives[0] = (uint8_t)g_run.lives[0];
		st.lives[1] = (uint8_t)g_run.lives[1];
		st.pair[0] = (uint8_t)g_run.pair[0];
		st.pair[1] = (uint8_t)g_run.pair[1];
		net_send_stage(&st);
		rng_seed(st.seed);
	}

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
	int mode = g_run.mode;
	int players = g_run.players;
	int diff = g_run.diff;
	int hero0 = g_run.hero_pick[0];
	int hero1 = g_run.hero_pick[1];

	memset(&g_run, 0, sizeof(g_run));
	g_run.mode = mode;
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

static void leave_net(void)
{
	if (net_role() != NET_OFF) {
		net_send_bye();
		net_close();
	}
}

static void start_net_lobby(void)
{
	int role = g_run.mode == MODE_HOST ? NET_HOST : NET_CLIENT;
	if (role == NET_CLIENT)
		net_set_hero(g_run.hero_pick[0]);
	if (!net_open(role)) {
		g_notice = net_error();
		return;
	}
	g_notice = NULL;
	g_stage_seen = false;
	g_screen = SCREEN_LOBBY;
}

static void apply_net_stage(const NetStage* st)
{
	int heroes[2] = { st->hero[0], st->hero[1] };
	int lives[2] = { st->lives[0], st->lives[1] };

	g_run.players = st->players;
	g_run.diff = st->diff;
	g_run.stage = st->stage;
	g_run.total_stage = st->total_stage;
	g_run.hero_pick[0] = st->hero[1];
	g_run.hero_pick[1] = st->hero[0];
	g_run.lives[0] = lives[0];
	g_run.lives[1] = lives[1];
	g_run.pair[0] = st->pair[0];
	g_run.pair[1] = st->pair[1];
	if (st->stage == 1)
		g_run.totals[0] = g_run.totals[1] = 0;

	net_reset_ticks();
	rng_seed(st->seed);
	game_init(&g_game, st->players, st->diff, st->stage, st->total_stage,
	          st->scene, heroes, lives);
	g_game.special_pair[0] = st->pair[0];
	g_game.special_pair[1] = st->pair[1];

	char bgm[8];
	snprintf(bgm, sizeof(bgm), "bgm%d", 1 + (st->stage - 1) % 4);
	audio_music(bgm);
	g_screen = SCREEN_GAME;
}

static void start_net_match(void)
{
	int peer = net_peer_hero();
	if (peer < 1 || peer > 4 || peer == g_run.hero_pick[0]) {
		peer = 0;
		for (int id = 1; id <= 4 && !peer; id++)
			if (id != g_run.hero_pick[0])
				peer = id;
	}
	g_run.players = 2;
	g_run.hero_pick[1] = peer;
	start_run();
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

static int page_count(void)
{
	switch (g_page) {
	case PAGE_HOME: return 2;
	case PAGE_MODE: return 4;
	case PAGE_DIFF: return 3;
	case PAGE_HERO: return 4;
	default: return 1;
	}
}

static void enter_page(int page, int cursor)
{
	g_page = page;
	g_cursor = cursor;
}

static void return_to_title(void)
{
	enter_page(PAGE_HOME, 0);
	g_screen = SCREEN_TITLE;
	audio_music("menu");
}

static void handle_title(u32 down)
{
	if (down & (KEY_DUP | KEY_DLEFT | KEY_CPAD_UP | KEY_CPAD_LEFT))
		g_cursor = (g_cursor + page_count() - 1) % page_count();
	else if (down & (KEY_DDOWN | KEY_DRIGHT | KEY_CPAD_DOWN | KEY_CPAD_RIGHT))
		g_cursor = (g_cursor + 1) % page_count();

	if (down & KEY_B) {
		if (g_page == PAGE_MODE)
			enter_page(PAGE_HOME, 0);
		else if (g_page == PAGE_DIFF)
			enter_page(PAGE_MODE, g_run.mode);
		else if (g_page == PAGE_HERO) {
			if (g_run.mode >= MODE_HOST)
				enter_page(PAGE_MODE, g_run.mode);
			else
				enter_page(PAGE_DIFF, g_run.diff);
		}
		else if (g_page == PAGE_INFO)
			enter_page(PAGE_HOME, 1);
		return;
	}

	if ((down & KEY_X) && g_page == PAGE_HERO && g_run.mode == MODE_LOCAL)
		cycle_hero(1, 1);

	if (!(down & KEY_A))
		return;

	switch (g_page) {
	case PAGE_HOME:
		if (g_cursor == 0)
			enter_page(PAGE_MODE, g_run.mode);
		else
			enter_page(PAGE_INFO, 0);
		break;
	case PAGE_MODE:
		g_run.mode = g_cursor;
		g_run.players = g_cursor == MODE_SOLO ? 1 : 2;
		if (g_run.players == 2 && g_run.hero_pick[1] == g_run.hero_pick[0])
			cycle_hero(1, 1);
		if (g_cursor >= MODE_HOST)
			enter_page(PAGE_HERO, g_run.hero_pick[0] - 1);
		else
			enter_page(PAGE_DIFF, g_run.diff);
		break;
	case PAGE_DIFF:
		g_run.diff = g_cursor;
		enter_page(PAGE_HERO, g_run.hero_pick[0] - 1);
		break;
	case PAGE_HERO:
		g_run.hero_pick[0] = g_cursor + 1;
		if (g_run.mode == MODE_LOCAL && g_run.hero_pick[1] == g_run.hero_pick[0])
			cycle_hero(1, 1);
		if (g_run.mode >= MODE_HOST)
			start_net_lobby();
		else
			start_run();
		break;
	case PAGE_INFO:
		enter_page(PAGE_HOME, 1);
		break;
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

	if (g_screen == SCREEN_LOBBY) {
		if (down & (KEY_B | KEY_SELECT)) {
			leave_net();
			return_to_title();
		} else if (down & KEY_A) {
			if (g_run.mode == MODE_HOST && net_peer_joined())
				start_net_match();
		}
		return;
	}

	if (g_screen == SCREEN_GAME) {
		if (down & KEY_START && net_role() == NET_OFF) {
			g_game.paused = !g_game.paused;
			if (g_game.paused)
				audio_pause_music();
			else
				audio_resume_music();
		}
		if (down & KEY_SELECT) {
			leave_net();
			audio_stop_music();
			return_to_title();
		}
		return;
	}

	if (g_screen == SCREEN_RESULT) {
		bool client = net_role() == NET_CLIENT;
		if ((down & (KEY_A | KEY_START)) && !client) {
			if (g_game.stage_clear) {
				if (g_game.final_stage) {
					leave_net();
					return_to_title();
				} else {
					g_run.stage++;
					next_special_pair();
					start_stage();
				}
			} else if (g_game.can_retry) {
				start_stage();
			} else {
				leave_net();
				return_to_title();
			}
		}
		if (down & KEY_SELECT) {
			leave_net();
			return_to_title();
		}
	}
}

/* ---------------- title screen ---------------- */

static void draw_title_bg(void)
{
	draw_icon(SET_TITLE, 1, 0.0f, 0.0f, 400.0f, 240.0f, 1.0f);
}

/* Corner brackets used as the selection marker. */
static void marker_frame(float x, float y, float w, float h, uint32_t rgb)
{
	const float t = 2.0f, l = 11.0f;
	card_rect(x, y, l, t, rgb, 1.0f);
	card_rect(x, y, t, l, rgb, 1.0f);
	card_rect(x + w - l, y, l, t, rgb, 1.0f);
	card_rect(x + w - t, y, t, l, rgb, 1.0f);
	card_rect(x, y + h - t, l, t, rgb, 1.0f);
	card_rect(x, y + h - l, t, l, rgb, 1.0f);
	card_rect(x + w - l, y + h - t, l, t, rgb, 1.0f);
	card_rect(x + w - t, y + h - l, t, l, rgb, 1.0f);
}

static void draw_menu_button(float x, float y, const char* label, bool selected)
{
	draw_icon(selected ? SET_BTN_SEL : SET_BTN, 1, x, y, 96.0f, 30.0f, 1.0f);
	text_center(x + 48.0f, y + 6.0f, 0.58f, 0xffffff, label);
}

static void dim_poster(void)
{
	draw_icon(SET_TITLE, 1, 0.0f, 0.0f, 400.0f, 240.0f, 0.45f);
	card_rect(0.0f, 0.0f, 400.0f, 240.0f, 0x120e18, 0.45f);
}

/* Item legend, laid out like the original help page: 12 tools in two
   columns with icon + short description. */
static void render_info_top(void)
{
	draw_icon(SET_TITLE, 1, 0.0f, 0.0f, 400.0f, 240.0f, 0.30f);
	/* Solid face: the poster behind it made the small glyphs hard to read. */
	card_rect(0.0f, 0.0f, 400.0f, 240.0f, 0xfff2f7, 1.0f);
	card_rect(0.0f, 0.0f, 400.0f, 32.0f, 0xd94f86, 1.0f);
	text_center(200.0f, 5.0f, 0.82f, 0xffffff, "道具");

	for (int i = 0; i < 12; i++) {
		int col = i / 6;
		int row = i % 6;
		float x = 14.0f + col * 192.0f;
		float y = 42.0f + row * 29.0f;
		if (row % 2 == 0)
			card_rect(x, y - 2.0f, 186.0f, 28.0f, 0xffffff, 0.85f);
		draw_icon(SET_TOOL, i + 1, x + 4.0f, y, 24.0f, 24.0f, 1.0f);
		font_draw(x + 34.0f, y + 4.0f, 0.55f, 0x4a2338, TOOLS[i].desc);
	}
	text_center(200.0f, 218.0f, 0.40f, 0x7d5266, "A / B 返回");
}

static void render_title_top(void)
{
	switch (g_page) {
	case PAGE_HOME:
		draw_title_bg();
		marker_frame(262.0f, g_cursor == 0 ? 145.0f : 189.0f, 116.0f, 34.0f,
		             0xffd166);
		break;
	case PAGE_MODE:
		dim_poster();
		text_center(200.0f, 20.0f, 0.85f, 0xffffff, "选择模式");
		for (int i = 0; i < 4; i++)
			draw_menu_button(152.0f, 66.0f + i * 38.0f, MODE_NAMES[i], i == g_cursor);
		text_center(200.0f, 222.0f, 0.4f, 0x93a0b5, "A 确认   B 返回");
		break;
	case PAGE_DIFF: {
		dim_poster();
		text_center(200.0f, 34.0f, 0.85f, 0xffffff, "选择难度");
		for (int i = 0; i < 3; i++)
			draw_menu_button(152.0f, 84.0f + i * 44.0f, DIFF_NAMES[i], i == g_cursor);
		char buf[32];
		snprintf(buf, sizeof(buf), "关卡数 %d", DIFF_STAGES[g_cursor]);
		text_center(200.0f, 210.0f, 0.5f, 0xffd166, buf);
		break;
	}
	case PAGE_HERO:
		draw_icon(SET_HERO_BG, 1, 0.0f, 0.0f, 400.0f, 240.0f, 1.0f);
		text_center(200.0f, 12.0f, 0.9f, 0x1d5f86, "选择角色");
		for (int i = 0; i < 4; i++) {
			float x = 46.0f + i * 84.0f;
			draw_icon(HEROES[i].set_id, 4, x, 54.0f, 56.0f, 72.0f, 1.0f);
			text_center(x + 28.0f, 136.0f, 0.6f, 0x1d4a66, HEROES[i].name);
			if (i == g_cursor)
				marker_frame(x - 8.0f, 46.0f, 72.0f, 114.0f, 0xffc93c);
			if (g_run.mode == MODE_LOCAL && g_run.hero_pick[1] == i + 1) {
				card_rect(x + 20.0f, 40.0f, 20.0f, 14.0f, 0x1d5f86, 1.0f);
				text_center(x + 30.0f, 43.0f, 0.3f, 0xffffff, "2P");
			}
		}
		text_center(200.0f, 176.0f, 0.62f, 0x1d4a66,
		            HEROES[g_cursor].name);
		text_center(200.0f, 200.0f, 0.4f, 0x2b6f9e,
		            g_run.mode == MODE_LOCAL ? "A 确认   X 换 2P 角色   B 返回"
		                                     : "A 确认   B 返回");
		break;
	case PAGE_INFO:
		render_info_top();
		break;
	}
}

/* ---------------- bottom screen ---------------- */

static const char* page_title(void)
{
	switch (g_page) {
	case PAGE_MODE: return "选择模式";
	case PAGE_DIFF: return "选择难度";
	case PAGE_HERO: return "选择角色";
	case PAGE_INFO: return "道具";
	default: return "游戏";
	}
}

static void render_title_bottom(void)
{
	/* Pink card that matches the poster instead of a black panel; dark text on
	   the cream face keeps the menu readable. */
	card_rect(0, 0, 320, 240, 0xf3aec7, 1.0f);
	card_rect(5, 5, 310, 230, 0xfff3f8, 1.0f);
	card_rect(5, 5, 310, 34, 0xd94f86, 1.0f);
	card_rect(5, 39, 310, 2, 0xe9c2d2, 1.0f);
	text_center(160.0f, 7.0f, 0.8f, 0xffffff, page_title());

	const uint32_t c_active = 0xc4376f;
	const uint32_t c_normal = 0x5a2a44;
	const uint32_t c_muted = 0x94677c;

	char line[96];
	float y = 56.0f;

	bool act = g_page == PAGE_MODE;
	card_rect(24.0f, y - 8.0f, 272.0f, 30.0f, act ? 0xffd9e8 : 0xffffff, 1.0f);
	if (act)
		card_rect(24.0f, y - 8.0f, 4.0f, 30.0f, c_active, 1.0f);
	snprintf(line, sizeof(line), "模式  < %s >", MODE_NAMES[g_run.mode]);
	text_center(160.0f, y, 0.62f, act ? c_active : c_normal, line);
	y += 34.0f;

	act = g_page == PAGE_DIFF;
	card_rect(24.0f, y - 8.0f, 272.0f, 30.0f, act ? 0xffd9e8 : 0xffffff, 1.0f);
	if (act)
		card_rect(24.0f, y - 8.0f, 4.0f, 30.0f, c_active, 1.0f);
	snprintf(line, sizeof(line), "难度  < %s >", DIFF_NAMES[g_run.diff]);
	text_center(160.0f, y, 0.62f, act ? c_active : c_normal, line);
	y += 34.0f;

	act = g_page == PAGE_HERO;
	card_rect(24.0f, y - 8.0f, 272.0f, 30.0f, act ? 0xffd9e8 : 0xffffff, 1.0f);
	if (act)
		card_rect(24.0f, y - 8.0f, 4.0f, 30.0f, c_active, 1.0f);
	if (g_run.mode == MODE_LOCAL)
		snprintf(line, sizeof(line), "角色  < %s >  < %s >",
		         HEROES[g_run.hero_pick[0] - 1].name,
		         HEROES[g_run.hero_pick[1] - 1].name);
	else
		snprintf(line, sizeof(line), "角色  < %s >",
		         HEROES[g_run.hero_pick[0] - 1].name);
	text_center(160.0f, y, 0.62f, act ? c_active : c_normal, line);

	y += 34.0f;
	snprintf(line, sizeof(line), "关卡数  %d", DIFF_STAGES[g_run.diff]);
	text_center(160.0f, y, 0.55f, c_muted, line);

	const char* hint1 = "十字键 选择   A 确认";
	const char* hint2 = "START 退出";
	if (g_page == PAGE_HERO) {
		hint1 = g_run.mode == MODE_LOCAL ? "A 确认   X 换 2P 角色" : "A 确认";
		hint2 = "B 返回";
	} else if (g_page == PAGE_INFO) {
		hint1 = "A / B 返回";
		hint2 = "";
	} else if (g_page != PAGE_HOME) {
		hint1 = "十字键 选择   A 确认";
		hint2 = "B 返回";
	}
	text_center(160.0f, 186.0f, 0.44f, 0x7a4b62, hint1);
	text_center(160.0f, 204.0f, 0.44f, 0x9c7186, hint2);

	const char* audio_msg = audio_error_text();
	if (audio_msg)
		text_center(160.0f, 222.0f, 0.40f, 0xc0392b, audio_msg);
	else if (g_notice)
		text_center(160.0f, 222.0f, 0.40f, 0xc0392b, g_notice);
	else
		text_center(160.0f, 222.0f, 0.40f, 0x8f6276,
		            "1P 十字键+A   2P 摇杆+X");
}

static void render_hud(void)
{
	card_rect(0, 0, 320, 240, 0x11151f, 1.0f);

	char buf[64];
	char tbuf[16];
	fmt_time(tbuf, sizeof(tbuf), g_game.time);
	snprintf(buf, sizeof(buf), "第 %d/%d 关", g_game.stage, g_game.total_stage);
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
			font_draw(x + 50.0f, y + 22.0f, 0.45f, 0x8a94a6, "出局");

		snprintf(buf, sizeof(buf), "%d", h->score);
		font_draw(x + 50.0f, y + 34.0f, 0.55f, 0xffffff, buf);

		snprintf(buf, sizeof(buf), "生命 %d", h->lives > 0 ? h->lives : 0);
		font_draw(x + 8.0f, y + 66.0f, 0.42f, 0xc7d0de, buf);

		snprintf(buf, sizeof(buf), "炸弹 %d/%d", h->bombs_out, h->total_bomb);
		font_draw(x + 8.0f, y + 80.0f, 0.40f, 0xc7d0de, buf);

		snprintf(buf, sizeof(buf), "威力 %d", h->bomb_level);
		font_draw(x + 92.0f, y + 66.0f, 0.40f, 0xc7d0de, buf);

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
		text_center(160.0f, 84.0f, 1.1f, 0xffffff, "暂停");
		text_center(160.0f, 130.0f, 0.5f, 0xc7d0de, "START 继续");
		text_center(160.0f, 150.0f, 0.5f, 0xc7d0de, "SELECT 返回标题");
	}
}

static void render_result_bottom(void)
{
	card_rect(0, 0, 320, 240, 0x11151f, 1.0f);

	const char* title;
	if (g_game.stage_clear)
		title = g_game.final_stage ? "全关卡通关！" : "过关！";
	else
		title = g_game.can_retry ? "再试一次" : "游戏结束";

	text_center(160.0f, 14.0f, 0.95f, g_game.stage_clear ? 0xffd166 : 0xff7d7d, title);

	char buf[64];
	snprintf(buf, sizeof(buf), "第 %d/%d 关  %s", g_game.stage, g_game.total_stage,
	         SCENES[g_game.map.scene_id].name);
	text_center(160.0f, 50.0f, 0.5f, 0xbcc6d6, buf);

	char tbuf[16];
	fmt_time(tbuf, sizeof(tbuf), g_game.time);
	snprintf(buf, sizeof(buf), "剩余时间 %s", tbuf);
	text_center(160.0f, 68.0f, 0.5f, 0xbcc6d6, buf);

	float y = 96.0f;
	for (int i = 0; i < g_game.hero_count && i < 4; i++) {
		Hero* h = &g_game.heroes[i];
		const HeroInfo* info = &HEROES[h->hero_id - 1];
		snprintf(buf, sizeof(buf), "%s   %d分  砖%d  击倒%d  道具%d",
		         info->name, h->score, h->brick_score, h->kill_score, h->tool_score);
		font_draw(20.0f, y, 0.45f, h->human ? info->color : 0x9aa6ba, buf);
		y += 20.0f;
	}

	if (g_run.players == 1) {
		snprintf(buf, sizeof(buf), "总分 %d", g_run.totals[0]);
	} else {
		snprintf(buf, sizeof(buf), "总分 1P %d   2P %d", g_run.totals[0], g_run.totals[1]);
	}
	text_center(160.0f, 184.0f, 0.5f, 0xd7deea, buf);

	const char* hint;
	if (net_role() == NET_CLIENT)
		hint = "等待主机选择…";
	else if (g_game.stage_clear)
		hint = g_game.final_stage ? "A 返回标题" : "A 下一关";
	else
		hint = g_game.can_retry ? "A 重试（还有生命）" : "A 返回标题";
	text_center(160.0f, 214.0f, 0.55f, 0xffd166, hint);
}

static void render_lobby_top(void)
{
	text_center(200.0f, 24.0f, 1.35f, 0xffffff, "泡泡堂 大乱斗");
	text_center(200.0f, 84.0f, 0.85f, 0xffd166, "联机对战");
	text_center(200.0f, 126.0f, 0.62f, 0xffffff, net_status());

	if (g_run.mode == MODE_HOST) {
		char buf[64];
		if (strcmp(net_local_ip(), "0.0.0.0") == 0)
			snprintf(buf, sizeof(buf), "未连接 WiFi");
		else
			snprintf(buf, sizeof(buf), "主机 IP %s", net_local_ip());
		text_center(200.0f, 160.0f, 0.48f, 0xb8c4d8, buf);
	}

	const char* hint = "B 返回";
	if (g_run.mode == MODE_HOST && net_peer_joined())
		hint = "A 开始对战    B 返回";
	text_center(200.0f, 208.0f, 0.45f, 0x8f9bb0, hint);
}

static void render_lobby_bottom(void)
{
	card_rect(0, 0, 320, 240, 0x151a26, 1.0f);
	text_center(160.0f, 8.0f, 0.75f, 0xffffff, "联机对战");

	if (g_run.mode == MODE_HOST) {
		bool joined = net_peer_joined();
		text_center(160.0f, 66.0f, 0.62f, joined ? 0xffd166 : 0xd7deea,
		            joined ? "玩家已连接" : "等待玩家加入…");
		text_center(160.0f, 104.0f, 0.46f, 0x93a0b5, "两台 3DS 连接同一 WiFi");
		text_center(160.0f, 126.0f, 0.46f, 0x93a0b5, "或 ZeroTier 虚拟局域网");
		char buf[64];
		snprintf(buf, sizeof(buf), "端口 %d", NET_PORT);
		text_center(160.0f, 152.0f, 0.46f, 0x6f7c92, buf);
		text_center(160.0f, 208.0f, 0.46f, 0x93a0b5,
		            joined ? "按 A 开始，2 人 + 2 AI" : "让另一台选择「联机加入」");
	} else {
		bool joined = net_peer_joined();
		text_center(160.0f, 66.0f, 0.62f, joined ? 0xffd166 : 0xd7deea,
		            joined ? "已连接，等待主机开始" : "正在搜索主机…");
		text_center(160.0f, 104.0f, 0.46f, 0x93a0b5, "请确认主机已开启房间");
		text_center(160.0f, 126.0f, 0.46f, 0x93a0b5, "且两台在同一局域网");
		text_center(160.0f, 208.0f, 0.46f, 0x93a0b5, "B 取消");
	}
}

/* ---------------- render ---------------- */

static void render(void)
{
	C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

	C2D_TargetClear(s_top, C2D_Color32(0, 0, 0, 255));
	C2D_SceneBegin(s_top);
	if (g_screen == SCREEN_TITLE) {
		render_title_top();
	} else if (g_screen == SCREEN_LOBBY) {
		render_lobby_top();
	} else {
		game_render(&g_game, 240.0f / 390.0f);
		if (g_game.paused)
			text_center(200.0f, 110.0f, 1.0f, 0xffffff, "暂停");
	}

	C2D_TargetClear(s_bottom, C2D_Color32(0, 0, 0, 255));
	C2D_SceneBegin(s_bottom);
	if (g_screen == SCREEN_TITLE)
		render_title_bottom();
	else if (g_screen == SCREEN_LOBBY)
		render_lobby_bottom();
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
	g_page = PAGE_HOME;
	g_cursor = 0;

	audio_music("menu");

	u64 last = osGetTime();
	double acc = 0.0;
	char notice_buf[64];

	while (aptMainLoop()) {
		hidScanInput();
		u32 down = hidKeysDown();

		net_poll();

		if (net_role() != NET_OFF && net_take_disconnect()) {
			snprintf(notice_buf, sizeof(notice_buf), "%s", net_status());
			leave_net();
			g_notice = notice_buf;
			return_to_title();
		}

		if (net_role() == NET_CLIENT
		    && (g_screen == SCREEN_LOBBY || g_screen == SCREEN_RESULT)) {
			NetStage st;
			if (net_recv_stage(&st) && (!g_stage_seen || st.seq != g_stage_seq)) {
				g_stage_seen = true;
				g_stage_seq = st.seq;
				g_notice = NULL;
				apply_net_stage(&st);
			}
		}

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
			if (g_screen != SCREEN_GAME || g_game.paused)
				continue;
			if (net_role() != NET_OFF) {
				int tick = g_game.tick + 1;
				net_prepare_tick(tick);
				int need = tick - NET_INPUT_DELAY;
				if (need > 0 && !net_tick_ready(need)) {
					if (acc < 5 * STEP_MS)
						acc += STEP_MS;
					break;
				}
				net_set_sim_tick(tick);
			}
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
			text_center(160.0f, 100.0f, 0.5f, 0xffffff, "素材加载失败");
			text_center(160.0f, 120.0f, 0.5f, 0xffffff, "按 START 退出");
			C3D_FrameEnd(0);
		} else {
			render();
		}
	}

	leave_net();
	audio_exit();
	assets_free_all();
	font_shutdown();
	C2D_Fini();
	C3D_Fini();
	gfxExit();
	romfsExit();
	return 0;
}
