#include "game.h"

#include <math.h>
#include <string.h>

/* ---------------- bombs ---------------- */

Bomb* fx_plant(FX* fx, Hero* hero)
{
	for (int i = 0; i < MAX_BOMBS; i++) {
		Bomb* b = &fx->bombs[i];
		if (b->active)
			continue;
		memset(b, 0, sizeof(*b));
		b->game = fx->game;
		b->active = true;
		b->x = hero->x;
		b->y = hero->y;
		b->owner = hero->slot;
		b->power = hero->bomb_level > 1 ? hero->bomb_level : 1;
		b->fuse = 60;
		b->t = 0;
		b->px = (float)(b->x * GRID);
		b->py = (float)(b->y * GRID);
		b->dead = false;
		b->sliding = false;
		Cell* c = map_cell(&fx->game->map, b->x, b->y);
		if (c)
			c->bomb = (int16_t)i;
		return b;
	}
	return NULL;
}

static void bomb_explode(Bomb* b)
{
	if (b->dead)
		return;
	Game* g = b->game;
	b->dead = true;
	b->active = false;
	Cell* c = map_cell(&g->map, b->x, b->y);
	if (c && c->bomb == (int16_t)(b - g->fx.bombs))
		c->bomb = -1;
	Hero* owner = game_hero(g, b->owner);
	if (owner && owner->bombs_out > 0)
		owner->bombs_out--;
	fx_boom_at(&g->fx, b->x, b->y, owner, b->power);
}

bool bomb_push(Bomb* b, int dx, int dy)
{
	if (b->dead || !b->active)
		return false;
	Game* g = b->game;
	int nx = b->x + dx;
	int ny = b->y + dy;
	Cell* c = map_cell(&g->map, nx, ny);
	if (!c || c->brick || c->stone || c->bomb >= 0 || c->tool == -1)
		return false;

	Cell* old = map_cell(&g->map, b->x, b->y);
	if (old && old->bomb == (int16_t)(b - g->fx.bombs))
		old->bomb = -1;
	float fx0 = b->px;
	float fy0 = b->py;
	b->x = nx;
	b->y = ny;
	c->bomb = (int16_t)(b - g->fx.bombs);
	b->px = (float)(nx * GRID);
	b->py = (float)(ny * GRID);
	b->fx = fx0;
	b->fy = fy0;
	b->slide_t = 0;
	b->slide_ticks = 6;
	b->sliding = true;
	return true;
}

static void bomb_step(Bomb* b)
{
	if (!b->active || b->dead)
		return;
	b->t++;
	if (b->sliding) {
		b->slide_t++;
		float k = (float)b->slide_t / (float)b->slide_ticks;
		if (k > 1.0f)
			k = 1.0f;
		b->px = b->fx + ((float)(b->x * GRID) - b->fx) * k;
		b->py = b->fy + ((float)(b->y * GRID) - b->fy) * k;
		if (b->slide_t >= b->slide_ticks)
			b->sliding = false;
		return;
	}
	if (--b->fuse <= 0 || map_fire_at(&b->game->map, b->x, b->y))
		bomb_explode(b);
}

/* ---------------- explosions ---------------- */

static Explosion* explosion_alloc(FX* fx)
{
	for (int i = 0; i < MAX_EXPLOSIONS; i++)
		if (!fx->explosions[i].active)
			return &fx->explosions[i];
	return NULL;
}

static void explosion_add_cell(Explosion* e, int x, int y, int dir, int ring, bool center)
{
	if (e->n_cells >= MAX_EXPL_CELLS)
		return;
	ExplCell* cell = &e->cells[e->n_cells++];
	cell->x = (signed char)x;
	cell->y = (signed char)y;
	cell->dir = (signed char)dir;
	cell->ring = (signed char)ring;
	cell->center = center;

	Game* g = e->game;
	int ticks = e->power + 10 - (ring > 0 ? ring : 0);
	map_ignite(&g->map, x, y, ticks, e->owner);
	Cell* c = map_cell(&g->map, x, y);
	if (c && c->tool == -1)
		game_break_ice_at(g, x, y, game_hero(g, e->owner));
	else
		game_break_brick_at(g, x, y, game_hero(g, e->owner));
}

Explosion* fx_boom_at(FX* fx, int x, int y, Hero* owner, int power)
{
	Explosion* e = explosion_alloc(fx);
	if (!e)
		return NULL;
	memset(e, 0, sizeof(*e));
	e->active = true;
	e->game = fx->game;
	e->x = x;
	e->y = y;
	e->owner = owner ? owner->slot : 0;
	e->hero_id = owner ? owner->hero_id : 1;
	e->power = power > 0 ? power : 0;
	e->ring = 0;
	e->t = 0;
	e->dead = false;
	for (int d = 0; d < 4; d++)
		e->dirs[d] = true;

	explosion_add_cell(e, x, y, -1, 0, true);
	for (int d = 0; d < 4; d++) {
		Cell* c = map_cell(&fx->game->map, x + DIR_VEC[d][0], y + DIR_VEC[d][1]);
		if (!c || c->stone)
			e->dirs[d] = false;
	}

	game_burst(fx->game, x * GRID + GRID / 2.0f, y * GRID + GRID / 2.0f,
	           3, 8 + power * 3, 0.6f, 2.6f, 22.0f, 1.5f, 4.2f, 0.08f);
	return e;
}

static void explosion_step(Explosion* e)
{
	if (!e->active)
		return;
	e->t++;
	if (e->ring < e->power) {
		e->ring++;
		for (int d = 0; d < 4; d++) {
			if (!e->dirs[d])
				continue;
			const int* v = DIR_VEC[d];
			int x = e->x + v[0] * e->ring;
			int y = e->y + v[1] * e->ring;
			Cell* c = map_cell(&e->game->map, x, y);
			if (!c || c->stone) {
				e->dirs[d] = false;
				continue;
			}
			bool blocked = c->brick || c->tool == -1;
			explosion_add_cell(e, x, y, d, e->ring, false);
			if (blocked || e->ring >= e->power)
				e->dirs[d] = false;
		}
	}
	if (e->t >= e->power + 15) {
		e->dead = true;
		e->active = false;
	}
}

/* ---------------- special weapons ---------------- */

static Weapon* weapon_alloc(FX* fx, int kind)
{
	for (int i = 0; i < MAX_WEAPONS; i++) {
		Weapon* w = &fx->weapons[i];
		if (w->active)
			continue;
		memset(w, 0, sizeof(*w));
		w->game = fx->game;
		w->active = true;
		w->kind = kind;
		return w;
	}
	return NULL;
}

void fx_rocket(FX* fx, Hero* hero)
{
	Weapon* w = weapon_alloc(fx, WPN_ROCKET);
	if (!w)
		return;
	const int* v = DIR_VEC[hero->dir];
	w->owner = hero->slot;
	w->hero_id = hero->hero_id;
	w->x = hero->x;
	w->y = hero->y;
	w->px = (float)(w->x * GRID);
	w->py = (float)(w->y * GRID);
	w->dir = hero->dir;
	w->dx = v[0];
	w->dy = v[1];
	w->t = 0;
}

void fx_mine(FX* fx, Hero* hero)
{
	Weapon* w = weapon_alloc(fx, WPN_MINE);
	if (!w)
		return;
	w->owner = hero->slot;
	w->hero_id = hero->hero_id;
	w->x = hero->x;
	w->y = hero->y;
	w->px = (float)(w->x * GRID);
	w->py = (float)(w->y * GRID);
	w->offset = 0;
}

bool fx_grenade(FX* fx, Hero* hero)
{
	const int* v = DIR_VEC[hero->dir];
	int tx = hero->x + v[0] * 2;
	int ty = hero->y + v[1] * 2;
	Cell* c = map_cell(&fx->game->map, tx, ty);
	if (!c || c->stone)
		return false;
	Weapon* w = weapon_alloc(fx, WPN_GRENADE);
	if (!w)
		return false;
	w->owner = hero->slot;
	w->hero_id = hero->hero_id;
	w->sx = hero->x;
	w->sy = hero->y;
	w->tx = tx;
	w->ty = ty;
	w->x = hero->x;
	w->y = hero->y;
	w->t = 0;
	return true;
}

void fx_gas(FX* fx, Hero* hero)
{
	Weapon* w = weapon_alloc(fx, WPN_GAS);
	if (!w)
		return;
	w->owner = hero->slot;
	w->hero_id = hero->hero_id;
	w->x = hero->x;
	w->y = hero->y;
	w->px = (float)(w->x * GRID);
	w->py = (float)(w->y * GRID);
}

bool fx_ice_arrow(FX* fx, Hero* hero)
{
	const int* v = DIR_VEC[hero->dir];
	Cell* c = map_cell(&fx->game->map, hero->x + v[0], hero->y + v[1]);
	if (!c || c->brick || c->stone)
		return false;
	Weapon* w = weapon_alloc(fx, WPN_ICE);
	if (!w)
		return false;
	w->owner = hero->slot;
	w->hero_id = hero->hero_id;
	w->x = hero->x;
	w->y = hero->y;
	w->dir = hero->dir;
	w->dx = v[0];
	w->dy = v[1];
	return true;
}

bool fx_fire_arrow(FX* fx, Hero* hero)
{
	const int* v = DIR_VEC[hero->dir];
	Cell* c = map_cell(&fx->game->map, hero->x + v[0], hero->y + v[1]);
	if (!c || c->stone)
		return false;
	Weapon* w = weapon_alloc(fx, WPN_FIRE);
	if (!w)
		return false;
	w->owner = hero->slot;
	w->hero_id = hero->hero_id;
	w->x = hero->x;
	w->y = hero->y;
	w->dir = hero->dir;
	w->dx = v[0];
	w->dy = v[1];
	return true;
}

static void weapon_step(Weapon* w)
{
	if (!w->active)
		return;
	Game* g = w->game;

	switch (w->kind) {
	case WPN_ROCKET: {
		w->t++;
		w->vx += (float)w->dx;
		w->vy += (float)w->dy;
		w->px += w->vx;
		w->py += w->vy;
		if (w->t % 2 == 0)
			fx_smoke(&g->fx, w->px + GRID / 2.0f, w->py + GRID / 2.0f);
		int cx = (int)lroundf(w->px / GRID);
		int cy = (int)lroundf(w->py / GRID);
		for (int i = 0; i < g->hero_count; i++) {
			Hero* h = &g->heroes[i];
			if (h->slot == w->owner || !h->alive)
				continue;
			float dx = h->px - w->px;
			float dy = h->py - w->py;
			if (dx * dx + dy * dy < GRID * GRID / 4.0f) {
				fx_boom_at(&g->fx, cx, cy, game_hero(g, w->owner), 1);
				w->active = false;
				return;
			}
		}
		Cell* c = map_cell(&g->map, cx, cy);
		if (!c || c->stone || c->brick || c->bomb >= 0) {
			fx_boom_at(&g->fx, cx, cy, game_hero(g, w->owner), 1);
			w->active = false;
			return;
		}
		w->x = cx;
		w->y = cy;
		if (w->t > 220)
			w->active = false;
		break;
	}
	case WPN_MINE: {
		w->t++;
		if (w->offset++ < 10)
			return;
		if (map_fire_at(&g->map, w->x, w->y)) {
			w->active = false;
			fx_boom_at(&g->fx, w->x, w->y, game_hero(g, w->owner), 0);
			return;
		}
		for (int i = 0; i < g->hero_count; i++) {
			Hero* h = &g->heroes[i];
			if (h->slot == w->owner || !h->alive)
				continue;
			if (h->x == w->x && h->y == w->y) {
				w->active = false;
				fx_boom_at(&g->fx, w->x, w->y, game_hero(g, w->owner), 0);
				return;
			}
		}
		break;
	}
	case WPN_GRENADE: {
		w->t++;
		if (w->t >= 22) {
			w->active = false;
			fx_boom_at(&g->fx, w->tx, w->ty, game_hero(g, w->owner), 1);
		}
		break;
	}
	case WPN_GAS: {
		w->t++;
		if (map_fire_at(&g->map, w->x, w->y)) {
			w->active = false;
			return;
		}
		for (int i = 0; i < g->hero_count; i++) {
			Hero* h = &g->heroes[i];
			if (h->slot == w->owner || !h->alive)
				continue;
			if (h->x == w->x && h->y == w->y) {
				h->slow_time = 600;
				h->slow_v_level = 20;
				w->active = false;
				return;
			}
		}
		break;
	}
	case WPN_ICE: {
		w->t++;
		if (w->t <= 12) {
			if (w->t % 4 == 0) {
				w->offset++;
				int x = w->x + w->dx * w->offset;
				int y = w->y + w->dy * w->offset;
				Cell* c = map_cell(&g->map, x, y);
				if (c && !c->brick && !c->stone && c->bomb < 0) {
					map_set_ice(&g->map, x, y, 40);
					fx_anim(&g->fx, SET_FREEZE_EFFECT, x, y, 40, 1.0f, 0.8f);
				} else {
					w->t = 13;
				}
			}
		} else if (w->offset > 0 || w->t > 16) {
			w->active = false;
		}
		break;
	}
	case WPN_FIRE: {
		w->t++;
		if (w->t <= 12) {
			if (w->t % 4 == 0) {
				w->offset++;
				int x = w->x + w->dx * w->offset;
				int y = w->y + w->dy * w->offset;
				Cell* c = map_cell(&g->map, x, y);
				if (!c || c->stone) {
					w->t = 13;
				} else {
					map_ignite(&g->map, x, y, 44, w->owner);
					game_break_brick_at(g, x, y, game_hero(g, w->owner));
					fx_anim(&g->fx, SET_FIRE_EFFECT, x, y, 44, 1.0f, 0.9f);
				}
			}
		} else if (w->t > 16) {
			w->active = false;
		}
		if (w->t > 60)
			w->active = false;
		break;
	}
	}
}

/* ---------------- one-shot animations ---------------- */

void fx_anim(FX* fx, int set_id, int x, int y, int frames, float scale, float alpha)
{
	for (int i = 0; i < MAX_EFFECTS; i++) {
		Anim* a = &fx->effects[i];
		if (a->active)
			continue;
		memset(a, 0, sizeof(*a));
		a->active = true;
		a->key_set = set_id;
		a->x = x;
		a->y = y;
		a->frames = frames;
		a->t = 0;
		a->scale = scale > 0.0f ? scale : 1.0f;
		a->alpha = alpha > 0.0f ? alpha : 1.0f;
		return;
	}
}

void fx_smoke(FX* fx, float px, float py)
{
	fx_anim(fx, SET_ROCKET_SMOKE, (int)floorf(px / GRID), (int)floorf(py / GRID), 4, 1.1f, 0.7f);
}

static void anim_step(Anim* a)
{
	if (!a->active)
		return;
	if (++a->t > a->frames) {
		a->dead = true;
		a->active = false;
	}
}

/* ---------------- manager ---------------- */

void fx_init(FX* fx, Game* g)
{
	memset(fx, 0, sizeof(*fx));
	fx->game = g;
}

void fx_clear(FX* fx)
{
	Game* g = fx->game;
	memset(fx->bombs, 0, sizeof(fx->bombs));
	memset(fx->weapons, 0, sizeof(fx->weapons));
	memset(fx->explosions, 0, sizeof(fx->explosions));
	memset(fx->effects, 0, sizeof(fx->effects));
	fx->game = g;
	fx->tick = 0;
}

void fx_step(FX* fx)
{
	fx->tick++;
	for (int i = 0; i < MAX_BOMBS; i++)
		bomb_step(&fx->bombs[i]);
	for (int i = 0; i < MAX_WEAPONS; i++)
		weapon_step(&fx->weapons[i]);
	for (int i = 0; i < MAX_EXPLOSIONS; i++)
		explosion_step(&fx->explosions[i]);
	for (int i = 0; i < MAX_EFFECTS; i++)
		anim_step(&fx->effects[i]);
}
