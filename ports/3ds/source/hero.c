#include "game.h"

#include <math.h>

static int hero_cx(Hero* h)
{
	return (int)lroundf(h->px / GRID);
}

static int hero_cy(Hero* h)
{
	return (int)lroundf(h->py / GRID);
}

void hero_init(Hero* h, Game* g, int slot, int hero_id, bool human, int layout,
               int spawn_x, int spawn_y, int lives)
{
	h->game = g;
	h->slot = slot;
	h->hero_id = hero_id;
	h->human = human;
	h->layout = layout;
	h->spawn_x = spawn_x;
	h->spawn_y = spawn_y;
	h->lives = lives;

	h->x = spawn_x;
	h->y = spawn_y;
	h->px = (float)(h->x * GRID);
	h->py = (float)(h->y * GRID);
	h->dir = 3;
	h->moving = false;
	h->move.t = 0;

	h->v_level = 6;
	h->slow_v_level = 20;
	h->slow_time = -1;
	h->stop_time = 0;

	h->bomb_level = 1;
	h->total_bomb = 1;
	h->bombs_out = 0;
	h->push_bomb_able = false;
	h->shield = false;
	h->var_shield = 60 + rng_int(0, 3);
	h->special.kind = TOOL_NONE;
	h->special.count = 0;

	h->score = 0;
	h->brick_score = 0;
	h->kill_score = 0;
	h->tool_score = 0;
	h->time_score = 0;
	h->bonus = 0;

	h->alive = true;
	h->age = rng_int(0, 30);
	h->prev_fire = false;
	h->last_fire_tick = -99;
}

void hero_add_score(Hero* h, int kind, int v)
{
	if (!h)
		return;
	h->score += v;
	switch (kind) {
	case SCORE_BRICK: h->brick_score += v; break;
	case SCORE_KILL: h->kill_score += v; break;
	case SCORE_TOOL: h->tool_score += v; break;
	case SCORE_TIME: h->time_score += v; break;
	case SCORE_BONUS: h->bonus += v; break;
	}
	h->game->hud_dirty = true;
}

bool hero_invulnerable(Hero* h)
{
	return h->var_shield > 0;
}

static void hero_pickup(Hero* h);

static void hero_advance_move(Hero* h)
{
	Move* m = &h->move;
	m->t++;
	float k = (float)m->t / (float)m->ticks;
	if (k > 1.0f)
		k = 1.0f;
	h->px = (float)(m->sx + m->dx * k) * GRID;
	h->py = (float)(m->sy + m->dy * k) * GRID;
	if (m->t >= m->ticks) {
		h->x = m->tx;
		h->y = m->ty;
		h->px = (float)(h->x * GRID);
		h->py = (float)(h->y * GRID);
		h->moving = false;
		hero_pickup(h);
	}
}

void hero_try_move(Hero* h, int dir)
{
	const int* v = DIR_VEC[dir];
	int tx = h->x + v[0];
	int ty = h->y + v[1];
	GameMap* map = &h->game->map;
	Cell* c = map_cell(map, tx, ty);

	h->dir = dir;
	if (!c || c->brick || c->stone || c->tool == -1)
		return;
	if (c->bomb >= 0) {
		if (!h->push_bomb_able)
			return;
		Bomb* b = &h->game->fx.bombs[c->bomb];
		if (!b->active || b->owner != h->slot)
			return;
		if (!bomb_push(b, v[0], v[1]))
			return;
	}
	int ticks = h->slow_time > 0 ? h->slow_v_level : h->v_level;
	h->move.sx = h->x;
	h->move.sy = h->y;
	h->move.tx = tx;
	h->move.ty = ty;
	h->move.dx = v[0];
	h->move.dy = v[1];
	h->move.t = 0;
	h->move.ticks = ticks;
	h->moving = true;
}

static void hero_apply_tool(Hero* h, const ToolDef* def, int tool_id)
{
	switch (def->kind) {
	case TOOL_LIFE:
		h->lives++;
		break;
	case TOOL_GLOVE:
		h->push_bomb_able = true;
		break;
	case TOOL_BOMB:
		h->total_bomb++;
		break;
	case TOOL_SHIELD:
		h->shield = true;
		break;
	case TOOL_POWER:
		h->bomb_level = h->bomb_level < 10 ? h->bomb_level + 1 : 10;
		break;
	case TOOL_SPEED:
		h->v_level = h->v_level > 3 ? h->v_level - 1 : 3;
		break;
	default:
		h->special.kind = tool_id;
		h->special.count = (tool_id == TOOL_ICE) ? 1 : 2;
		break;
	}
	hero_add_score(h, SCORE_TOOL, 100);
	game_burst(h->game, h->px + GRID / 2.0f, h->py + GRID / 2.0f,
	           def->kind == TOOL_SHIELD ? 1 : 0, 8,
	           0.4f, 1.8f, 18.0f, 1.5f, 4.0f, -0.02f);
}

static void hero_pickup(Hero* h)
{
	Game* g = h->game;
	Cell* c = map_cell(&g->map, h->x, h->y);
	if (!c || c->tool == 0 || c->tool == -1)
		return;
	if (!h->human && g->diff == 0)
		return;
	int tool_id = c->tool;
	const ToolDef* def = tool_by_id(tool_id);
	if (!h->human && g->diff != 2 && (def->kind == TOOL_SPEED || tool_is_special(def)))
		return;
	if (map_take_tool(&g->map, h->x, h->y) == 0)
		return;
	hero_apply_tool(h, def, tool_id);
}

static bool hero_fire_special(Hero* h)
{
	FX* fx = &h->game->fx;
	switch (h->special.kind) {
	case TOOL_ROCKET:
		fx_rocket(fx, h);
		return true;
	case TOOL_MINE:
		fx_mine(fx, h);
		return true;
	case TOOL_GRENADE:
		return fx_grenade(fx, h);
	case TOOL_GAS:
		fx_gas(fx, h);
		return true;
	case TOOL_ICE:
		return fx_ice_arrow(fx, h);
	case TOOL_FIRE:
		return fx_fire_arrow(fx, h);
	default:
		return false;
	}
}

bool hero_put_bomb(Hero* h)
{
	if (h->bombs_out >= h->total_bomb)
		return false;
	Cell* c = map_cell(&h->game->map, h->x, h->y);
	if (!c || c->bomb >= 0)
		return false;
	if (!fx_plant(&h->game->fx, h))
		return false;
	h->bombs_out++;
	h->game->hud_dirty = true;
	return true;
}

void hero_try_fire(Hero* h)
{
	Game* g = h->game;
	if (g->tick - h->last_fire_tick < 5)
		return;
	h->last_fire_tick = g->tick;
	Cell* c = map_cell(&g->map, h->x, h->y);
	if (!c || c->bomb >= 0)
		return;
	if (h->special.kind != TOOL_NONE && h->special.count > 0) {
		if (hero_fire_special(h)) {
			h->special.count--;
			if (h->special.count <= 0) {
				h->special.kind = TOOL_NONE;
				h->special.count = 0;
			}
		}
		return;
	}
	hero_put_bomb(h);
}

static void hero_human_think(Hero* h)
{
	bool fire = input_fire_held(h->layout);
	for (int d = 0; d < 4; d++) {
		if (input_dir_held(h->layout, d)) {
			hero_try_move(h, d);
			break;
		}
	}
	bool firing = fire || h->prev_fire;
	if (firing && !h->moving) {
		h->prev_fire = false;
		hero_try_fire(h);
	}
	if (h->moving && fire)
		h->prev_fire = true;
}

bool hero_is_save_cell(Hero* h, int x, int y)
{
	GameMap* map = &h->game->map;
	Cell* c = map_cell(map, x, y);
	if (!c || c->brick || c->stone || c->bomb >= 0 || c->tool == -1)
		return false;
	if (map_fire_at(map, x, y))
		return false;
	for (int d = 0; d < 4; d++) {
		const int* v = DIR_VEC[d];
		int cx = x;
		int cy = y;
		int dist = 0;
		Cell* cell = c;
		while (cell && !cell->brick && !cell->stone) {
			if (map_fire_at(map, cx, cy))
				return false;
			if (cell->bomb >= 0) {
				Bomb* b = &h->game->fx.bombs[cell->bomb];
				if (b->active && b->power >= dist)
					return false;
			}
			cx += v[0];
			cy += v[1];
			dist++;
			cell = map_cell(map, cx, cy);
		}
	}
	return true;
}

static int hero_cell_score(Hero* h, int x, int y)
{
	if (!hero_is_save_cell(h, x, y))
		return -1;
	int n = 0;
	for (int d = 0; d < 4; d++)
		if (hero_is_save_cell(h, x + DIR_VEC[d][0], y + DIR_VEC[d][1]))
			n++;
	return n;
}

static bool hero_is_save_here(Hero* h)
{
	return hero_is_save_cell(h, h->x, h->y);
}

static void hero_ai_seek(Hero* h)
{
	int best = -1;
	int best_dir = -1;
	int start = rng_int(0, 3);
	for (int i = 0; i < 4; i++) {
		int d = (start + i) % 4;
		const int* v = DIR_VEC[d];
		int s = hero_cell_score(h, h->x + v[0], h->y + v[1]);
		if (s > 0) {
			Cell* c = map_cell(&h->game->map, h->x + v[0], h->y + v[1]);
			if (c && c->tool != 0)
				s += 5;
			s += rng_int(0, 2);
			if (s >= best) {
				best = s;
				best_dir = d;
			}
		}
	}
	if (best_dir >= 0)
		hero_try_move(h, best_dir);
}

static void hero_ai_bomb_and_flee(Hero* h)
{
	int best = 0;
	int best_dir = -1;
	int start = rng_int(0, 3);
	for (int i = 0; i < 4; i++) {
		int d = (start + i) % 4;
		const int* v = DIR_VEC[d];
		int s = hero_cell_score(h, h->x + v[0], h->y + v[1]);
		if (s > 0 && s >= best) {
			best = s;
			best_dir = d;
		}
	}
	if (best_dir >= 0 && best >= 2) {
		hero_try_fire(h);
		hero_try_move(h, best_dir);
	}
}

static void hero_ai_escape(Hero* h)
{
	int best = 0;
	int best_dir = -1;
	int start = rng_int(0, 3);
	for (int i = 0; i < 4; i++) {
		int d = (start + i) % 4;
		const int* v = DIR_VEC[d];
		int s = hero_cell_score(h, h->x + v[0], h->y + v[1]);
		if (s >= best) {
			best = s;
			best_dir = d;
		}
	}
	if (best_dir < 0)
		best_dir = rng_int(0, 3);
	hero_try_move(h, best_dir);
}

static void hero_ai_think(Hero* h)
{
	if (h->stop_time > 0) {
		h->stop_time--;
		return;
	}
	int escape_rate = game_diff_escape(h->game->diff);
	if (hero_is_save_here(h)) {
		if (rng_float() * 100.0f < 20.0f)
			hero_ai_seek(h);
		else if (rng_float() * 100.0f < 5.0f)
			hero_ai_bomb_and_flee(h);
	} else if (rng_float() * 100.0f < (float)escape_rate) {
		hero_ai_escape(h);
	}
}

bool hero_hurt(Hero* h, Hero* owner)
{
	if (!h->alive || h->var_shield > 0)
		return false;
	if (h->shield) {
		h->shield = false;
		h->var_shield = 60;
		game_burst(h->game, h->px + GRID / 2.0f, h->py + GRID / 2.0f,
		           1, 14, 0.8f, 2.4f, 22.0f, 1.5f, 4.0f, 0.08f);
		return true;
	}
	if (owner && owner != h)
		hero_add_score(owner, SCORE_KILL, 1000);
	h->lives--;
	fx_anim(&h->game->fx, SET_DIE_SMOKE, h->x, h->y, 36, 1.1f, 0.9f);
	game_burst(h->game, h->px + GRID / 2.0f, h->py + GRID / 2.0f,
	           2, 18, 1.0f, 3.2f, 28.0f, 1.5f, 4.0f, 0.08f);
	if (!h->human && h->lives > 0) {
		h->var_shield = 60;
		return true;
	}
	h->alive = false;
	fx_anim(&h->game->fx, SET_HERO_EXP, h->x, h->y, 36, 1.2f, 1.0f);
	h->game->hud_dirty = true;
	return true;
}

void hero_step(Hero* h)
{
	if (!h->alive)
		return;
	h->age++;
	if (h->var_shield > 0)
		h->var_shield--;
	if (h->slow_time > 0)
		h->slow_time--;

	int cx = hero_cx(h);
	int cy = hero_cy(h);
	Cell* c = map_cell(&h->game->map, cx, cy);
	if (c && c->tool == -1)
		h->stop_time = 300;

	if (h->moving) {
		hero_advance_move(h);
		return;
	}

	if (map_fire_at(&h->game->map, cx, cy)) {
		int owner_slot = map_fire_owner(&h->game->map, cx, cy);
		hero_hurt(h, game_hero(h->game, owner_slot));
		if (!h->alive)
			return;
	}

	if (h->stop_time > 0) {
		h->stop_time--;
		return;
	}

	if (h->human)
		hero_human_think(h);
	else
		hero_ai_think(h);
}
