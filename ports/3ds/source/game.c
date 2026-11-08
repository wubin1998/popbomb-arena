#include "game.h"

#include <limits.h>
#include <math.h>
#include <string.h>

static const int BRICK_SCORE[3] = { 5, 10, 15 };
static const int ESCAPE_RATE[3] = { 20, 60, 100 };

int game_diff_brick_score(int diff)
{
	return BRICK_SCORE[diff < 0 || diff > 2 ? 1 : diff];
}

int game_diff_escape(int diff)
{
	return ESCAPE_RATE[diff < 0 || diff > 2 ? 1 : diff];
}

Hero* game_hero(Game* g, int slot)
{
	if (slot <= 0)
		return NULL;
	for (int i = 0; i < g->hero_count; i++)
		if (g->heroes[i].slot == slot)
			return &g->heroes[i];
	return NULL;
}

Hero* game_hero_by_id(Game* g, int hero_id)
{
	for (int i = 0; i < g->hero_count; i++)
		if (g->heroes[i].hero_id == hero_id)
			return &g->heroes[i];
	return NULL;
}

static void spawn_heroes(Game* g, const int* human_ids, const int* lives)
{
	bool used[5] = { false, false, false, false, false };
	g->hero_count = 0;

	for (int i = 0; i < g->players; i++) {
		int hero_id = human_ids ? human_ids[i] : (i + 1);
		if (hero_id < 1 || hero_id > 4)
			hero_id = 1;
		used[hero_id] = true;
		int layout = g->players == 1 ? 0 : (i == 0 ? 1 : 2);
		Hero* h = &g->heroes[g->hero_count++];
		hero_init(h, g, i + 1, hero_id, true, layout,
		          g->map.spawn_x[hero_id - 1], g->map.spawn_y[hero_id - 1],
		          lives ? lives[i] : 1);
		if (h->lives <= 0)
			h->alive = false;
		if (g->diff == 0)
			h->shield = true;
	}

	int ai_needed = 4 - g->players;
	int ai_index = 0;
	for (int id = 1; id <= 4 && ai_needed > 0; id++) {
		if (used[id])
			continue;
		Hero* h = &g->heroes[g->hero_count++];
		hero_init(h, g, g->players + ai_index + 1, id, false, -1,
		          g->map.spawn_x[id - 1], g->map.spawn_y[id - 1], 1);
		ai_index++;
		ai_needed--;
	}
}

void game_init(Game* g, int players, int diff, int stage, int total_stage, int scene_id,
               const int* human_ids, const int* lives)
{
	memset(g, 0, sizeof(*g));
	g->players = players < 1 ? 1 : players;
	g->diff = diff < 0 || diff > 2 ? 1 : diff;
	g->stage = stage;
	g->total_stage = total_stage;
	g->scene_id = scene_id;
	g->brick_score = game_diff_brick_score(g->diff);
	g->escape_rate = game_diff_escape(g->diff);

	g->time = TOTAL_TIME;
	g->state = STATE_COUNTDOWN;
	g->count_ticks = 116;
	g->tick = 0;
	g->hud_dirty = true;
	g->paused = false;

	map_init(&g->map, scene_id);
	fx_init(&g->fx, g);
	spawn_heroes(g, human_ids, lives);
}

static void begin_end(Game* g, int kind)
{
	g->state = STATE_ENDING;
	g->end_kind = kind;
	g->end_tick = 40;
}

static void finish_end(Game* g)
{
	int kind = g->end_kind;
	bool timeup = kind == END_TIMEUP;

	if (timeup) {
		int best_human = INT_MIN;
		int best_ai = 0;
		for (int i = 0; i < g->hero_count; i++) {
			Hero* h = &g->heroes[i];
			if (!h->alive)
				continue;
			if (h->human) {
				if (h->score > best_human)
					best_human = h->score;
			} else if (h->score > best_ai) {
				best_ai = h->score;
			}
		}
		kind = best_human > best_ai ? END_WIN : END_LOSE;
	}

	if (kind == END_WIN) {
		for (int i = 0; i < g->hero_count; i++) {
			Hero* h = &g->heroes[i];
			if (!h->human || !h->alive)
				continue;
			hero_add_score(h, SCORE_BONUS, 1000);
			if (!timeup)
				hero_add_score(h, SCORE_TIME, g->time * 10);
		}
	}

	g->end_kind = kind;
	g->stage_clear = kind == END_WIN;
	g->final_stage = kind == END_WIN && g->stage >= g->total_stage;
	g->can_retry = kind == END_LOSE;
	if (g->can_retry) {
		g->can_retry = false;
		for (int i = 0; i < g->hero_count; i++)
			if (g->heroes[i].human && g->heroes[i].lives > 0)
				g->can_retry = true;
	}
	g->state = STATE_OVER;
	g->hud_dirty = true;
}

static void check_end(Game* g)
{
	if (g->state != STATE_PLAYING)
		return;
	bool humans_alive = false;
	bool ai_alive = false;
	for (int i = 0; i < g->hero_count; i++) {
		Hero* h = &g->heroes[i];
		if (!h->alive)
			continue;
		if (h->human)
			humans_alive = true;
		else
			ai_alive = true;
	}
	if (!humans_alive)
		begin_end(g, END_LOSE);
	else if (!ai_alive)
		begin_end(g, END_WIN);
}

void game_step(Game* g)
{
	if (g->paused || g->state == STATE_OVER)
		return;
	g->tick++;

	if (g->state == STATE_COUNTDOWN) {
		g->count_ticks--;
		if (g->count_ticks <= 0)
			g->state = STATE_PLAYING;
		return;
	}

	if (g->state == STATE_ENDING) {
		fx_step(&g->fx);
		map_update_fire(&g->map);
		map_update_ice(&g->map);
		game_update_particles(g);
		if (--g->end_tick <= 0)
			finish_end(g);
		return;
	}

	g->time_acc++;
	if (g->time_acc >= FPS) {
		g->time_acc -= FPS;
		g->time--;
		g->hud_dirty = true;
		if (g->time <= 0) {
			g->time = 0;
			begin_end(g, END_TIMEUP);
			return;
		}
	}

	map_update_fire(&g->map);
	map_update_ice(&g->map);
	map_update_tools(&g->map);
	for (int i = 0; i < g->hero_count; i++)
		hero_step(&g->heroes[i]);
	fx_step(&g->fx);
	game_update_particles(g);
	check_end(g);
}

bool game_is_over(Game* g)
{
	return g->state == STATE_OVER;
}

/* ---------------- particles ---------------- */

static const uint32_t PALETTE_TOOL[2] = { 0xffd166, 0xffffff };
static const uint32_t PALETTE_SHIELD[2] = { 0x9fe8ff, 0xffffff };
static const uint32_t PALETTE_HERO[3] = { 0xff5b9d, 0xffffff, 0xffd166 };
static const uint32_t PALETTE_BOOM[4] = { 0xffd166, 0xff8a3d, 0xff4d6d, 0xfff3c4 };

void game_burst(Game* g, float x, float y, int palette, int count,
                float speed_min, float speed_max, float life_max,
                float size_min, float size_max, float gravity)
{
	const uint32_t* colors;
	int ncolors;
	switch (palette) {
	case 1: colors = PALETTE_SHIELD; ncolors = 2; break;
	case 2: colors = PALETTE_HERO; ncolors = 3; break;
	case 3: colors = PALETTE_BOOM; ncolors = 4; break;
	default: colors = PALETTE_TOOL; ncolors = 2; break;
	}
	for (int i = 0; i < count; i++) {
		Particle* p = NULL;
		for (int k = 0; k < MAX_PARTICLES; k++) {
			if (!g->particles[k].active) {
				p = &g->particles[k];
				break;
			}
		}
		if (!p)
			return;
		float angle = rng_float() * 6.2831853f;
		float sp = speed_min + rng_float() * (speed_max - speed_min);
		p->active = true;
		p->x = x;
		p->y = y;
		p->vx = cosf(angle) * sp;
		p->vy = sinf(angle) * sp;
		p->life = 0;
		p->max_life = rng_int(10, life_max > 10.0f ? (int)life_max : 22);
		p->size = size_min + rng_float() * (size_max - size_min);
		p->color = colors[rng_int(0, ncolors - 1)];
		p->gravity = gravity;
		p->square = false;
	}
}

void game_update_particles(Game* g)
{
	for (int i = 0; i < MAX_PARTICLES; i++) {
		Particle* p = &g->particles[i];
		if (!p->active)
			continue;
		p->x += p->vx;
		p->y += p->vy;
		p->vy += p->gravity;
		p->vx *= 0.97f;
		p->life++;
		if (p->life >= p->max_life)
			p->active = false;
	}
}

/* ---------------- shared callbacks ---------------- */

void game_break_brick_at(Game* g, int x, int y, Hero* owner)
{
	Cell* c = map_cell(&g->map, x, y);
	if (!c || !c->brick)
		return;
	map_break_brick(&g->map, x, y);
	if (owner)
		hero_add_score(owner, SCORE_BRICK, g->brick_score);
	fx_anim(&g->fx, SET_BRICK_EXP, x, y, 11, 1.06f, 1.0f);
	const ToolDef* tool = roll_tool(g->special_pair, 2);
	if (tool)
		map_place_tool(&g->map, x, y, tool->id, rng_int(0, 8));
	g->hud_dirty = true;
}

bool game_break_ice_at(Game* g, int x, int y, Hero* owner)
{
	if (!map_melt_ice(&g->map, x, y))
		return false;
	if (owner)
		hero_add_score(owner, SCORE_BRICK, g->brick_score);
	fx_anim(&g->fx, SET_BRICK_EXP, x, y, 11, 1.06f, 1.0f);
	const ToolDef* tool = roll_tool(g->special_pair, 2);
	if (tool)
		map_place_tool(&g->map, x, y, tool->id, rng_int(0, 8));
	g->hud_dirty = true;
	return true;
}
