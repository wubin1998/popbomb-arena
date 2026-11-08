#include "game.h"
#include "assets.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <citro2d.h>

#define TOP_W 400.0f
#define TOP_H 240.0f
#define TAU 6.2831853f

static float S;
static float OX;
static float OY;

static inline float lx(float x) { return OX + x * S; }
static inline float ly(float y) { return OY + y * S; }

typedef struct {
	int row;
	float sort;
	int type; /* 0 hero, 1 bomb, 2 weapon */
	int idx;
} Actor;

static void spr(int set, int frame, float x, float y, float w, float h, float alpha)
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
	C2D_DrawImageAt(im, lx(x), ly(y), 0.0f, tp,
	                (w * S) / im.subtex->width, (h * S) / im.subtex->height);
}

static void spr_rot(int set, int frame, float cx, float cy, float size, float angle, float alpha)
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
	C2D_DrawImageAtRotated(im, lx(cx), ly(cy), 0.0f, angle, tp,
	                       (size * S) / im.subtex->width,
	                       (size * S) / im.subtex->height);
}

static void img_size(int set, int frame, float* w, float* h)
{
	C2D_Image im = assets_frame(set, frame);
	if (!im.tex || !im.subtex) {
		*w = 0.0f;
		*h = 0.0f;
		return;
	}
	*w = im.subtex->width;
	*h = im.subtex->height;
}

static u32 rgb_alpha(uint32_t rgb, float alpha)
{
	u32 a = (u32)(alpha <= 0.0f ? 0.0f : (alpha >= 1.0f ? 255.0f : alpha * 255.0f));
	return C2D_Color32((rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff, a);
}

static void shadow(float cx, float by, float rx, float ry)
{
	C2D_DrawEllipseSolid(lx(cx - rx), ly(by - ry), 0.0f, rx * 2.0f * S, ry * 2.0f * S,
	                     C2D_Color32(0, 0, 0, 56));
}

/* ---------------- ground ---------------- */

static void draw_ground(Game* g)
{
	GameMap* m = &g->map;
	const SceneDef* scene = &SCENES[m->scene_id];

	for (int y = 0; y < MAP_H; y++) {
		for (int x = 0; x < MAP_W; x++) {
			int f = scene->bg[(x * 5 + y * 3) % scene->bg_count];
			spr(SET_BG, f, (float)(x * GRID), (float)(y * GRID), GRID, GRID, 1.0f);
		}
	}

	for (int y = 0; y < MAP_H; y++) {
		for (int x = 0; x < MAP_W; x++) {
			Cell* c = &m->cells[y][x];
			if (c->tool == -1) {
				C2D_DrawRectSolid(lx(x * GRID + 1), ly(y * GRID + 1), 0.0f,
				                  (GRID - 2) * S, (GRID - 2) * S,
				                  C2D_Color32(150, 230, 255, 115));
				float w, h;
				img_size(SET_FREEZE_EFFECT, 1, &w, &h);
				(void)w;
				(void)h;
				spr(SET_FREEZE_EFFECT, 1, (float)(x * GRID - 3), (float)(y * GRID - 6),
				    GRID + 6, GRID + 10, 0.7f);
			} else if (c->tool > 0) {
				float w, h;
				img_size(SET_TOOL, c->tool, &w, &h);
				float bob = sinf((float)(c->tool_bob + c->tool_age) * 0.18f) * 1.6f;
				float tw = w * 1.1f;
				float th = h * 1.1f;
				shadow((float)x * GRID + GRID / 2.0f, (float)y * GRID + GRID - 3.0f, 9.0f, 3.0f);
				spr(SET_TOOL, c->tool,
				    (float)x * GRID + GRID / 2.0f - tw / 2.0f,
				    (float)y * GRID + GRID - th - 3.0f + bob, tw, th, 1.0f);
			}
		}
	}
}

/* ---------------- blocks ---------------- */

static void draw_block_row(Game* g, int y)
{
	GameMap* m = &g->map;
	for (int x = 0; x < MAP_W; x++) {
		Cell* c = &m->cells[y][x];
		if (c->stone) {
			if (map_is_big_cell(m, x, y))
				continue;
			float w, h;
			img_size(SET_BRICK, c->stone_frame, &w, &h);
			if (w < 30.0f) w = 30.0f;
			if (h < 30.0f) h = 30.0f;
			w *= 0.8f;
			h *= 0.8f;
			spr(SET_BRICK, c->stone_frame,
			    (float)x * GRID + GRID / 2.0f - w / 2.0f,
			    (float)y * GRID + GRID - h + 2.0f, w, h, 1.0f);
		} else if (c->brick) {
			float w, h;
			img_size(SET_BRICK, c->frame, &w, &h);
			w *= 0.86f;
			h *= 0.86f;
			spr(SET_BRICK, c->frame,
			    (float)x * GRID + GRID / 2.0f - w / 2.0f,
			    (float)y * GRID + GRID - h + 2.0f, w, h, 1.0f);
		}
	}

	if (y == m->big_y + 1) {
		float w, h;
		img_size(SET_BRICK, m->big_frame, &w, &h);
		(void)w;
		float bw = 68.0f;
		float bh = h * 0.78f;
		if (bh > 74.0f)
			bh = 74.0f;
		float cx = (float)(m->big_x + 1) * GRID;
		float bottom = (float)(m->big_y + 2) * GRID + 4.0f;
		spr(SET_BRICK, m->big_frame, cx - bw / 2.0f, bottom - bh, bw, bh, 1.0f);
	}
}

/* ---------------- actors ---------------- */

static void draw_hero(Game* g, Hero* h)
{
	if (!h->alive)
		return;
	const HeroInfo* info = &HEROES[h->hero_id - 1];
	float w, hgt;
	img_size(info->set_id, h->dir + 1, &w, &hgt);
	float bob = 0.0f;
	if (h->moving) {
		float k = h->move.ticks > 0 ? (float)h->move.t / (float)h->move.ticks : 0.0f;
		bob = sinf(k * 3.14159265f) * 2.4f;
	}
	float cx = h->px + GRID / 2.0f;
	float by = h->py + GRID + 3.0f;
	float dw = w > 0.0f ? (w * 1.02f > 28.0f ? w * 1.02f : 28.0f) : 34.0f;
	float dh = hgt > 0.0f ? (hgt * 1.02f > 40.0f ? hgt * 1.02f : 40.0f) : 54.0f;

	shadow(cx, by - 1.0f, dw * 0.32f, 4.4f);

	if (h->shield) {
		C2D_DrawEllipseSolid(lx(cx - dw * 0.56f), ly(by - dh * 0.52f - dh * 0.5f), 0.0f,
		                     dw * 1.12f * S, dh * S, rgb_alpha(info->color, 0.18f));
	}

	float alpha = 1.0f;
	if (h->var_shield > 0 && (h->var_shield % 2) == 1)
		alpha = 0.42f;
	spr(info->set_id, h->dir + 1, cx - dw / 2.0f, by - dh - bob, dw, dh, alpha);

	if (h->slow_time > 0) {
		C2D_DrawEllipseSolid(lx(cx - dw * 0.5f), ly(by - dh * 0.94f), 0.0f,
		                     dw * S, dh * 0.88f * S, C2D_Color32(120, 220, 255, 71));
	}

	if (h->human && g->players == 2) {
		char label[4];
		snprintf(label, sizeof(label), h->layout == 2 ? "P2" : "P1");
		float pw = font_width(0.3f, label);
		float px = lx(cx) - pw / 2.0f - 3.0f * S;
		float py = ly(by - dh - 12.0f);
		C2D_DrawRectSolid(px, py, 0.0f, pw + 6.0f * S, 14.0f * S,
		                  rgb_alpha(info->dark, 1.0f));
		font_draw(lx(cx) - pw / 2.0f, py + 2.0f * S, 0.3f, 0xffffff, label);
	}

	if (h->special.kind != TOOL_NONE) {
		spr(SET_TOOL, h->special.kind, cx - 9.0f, by - dh - 26.0f, 18.0f, 18.0f, 1.0f);
		char cnt[8];
		snprintf(cnt, sizeof(cnt), "x%d", h->special.count);
		font_draw(lx(cx + 10.0f), ly(by - dh - 22.0f), 0.3f, 0xffffff, cnt);
	}

	if (h->stop_time > 0) {
		spr(SET_FREEZE_EFFECT, 1 + (h->age % 40),
		    cx - dw * 0.6f, by - dh * 1.02f, dw * 1.2f, dh * 1.05f, 0.55f);
	}
}

static void draw_bomb(Game* g, Bomb* b)
{
	(void)g;
	/* The sprite keeps ~1/4 of its height as a baked shadow under the ball, so
	   anchoring the sprite's bottom on the cell floor would leave the ball
	   pressed against the top of the cell. Anchor lower so the ball sits in
	   the cell instead. */
	int frame = 1 + (b->t % 40);
	float w, h;
	img_size(SET_BOMB, frame, &w, &h);
	if (w <= 0.0f)
		return;
	float cx = b->px + GRID / 2.0f;
	float by = b->py + GRID + 9.0f;
	float pulse = 1.0f + sinf((float)b->t * 0.6f) * 0.03f;
	float dw = w * 1.02f * pulse;
	float dh = h * 1.02f * pulse;
	shadow(cx, by - 7.0f, 10.0f, 4.0f);
	spr(SET_BOMB, frame, cx - dw / 2.0f, by - dh, dw, dh, 1.0f);
}

static float weapon_row(Weapon* w)
{
	switch (w->kind) {
	case WPN_ROCKET:
		return roundf(w->py / GRID);
	case WPN_GRENADE: {
		float k = (float)w->t / 22.0f;
		if (k > 1.0f) k = 1.0f;
		return roundf((float)w->sy + (float)(w->ty - w->sy) * k);
	}
	case WPN_ICE:
	case WPN_FIRE:
		return (float)(w->y + w->dy * w->offset);
	default:
		return (float)w->y;
	}
}

static float weapon_sort_x(Weapon* w)
{
	switch (w->kind) {
	case WPN_ROCKET:
		return w->px;
	case WPN_GRENADE: {
		float k = (float)w->t / 22.0f;
		if (k > 1.0f) k = 1.0f;
		return ((float)w->sx + (float)(w->tx - w->sx) * k) * GRID;
	}
	case WPN_ICE:
	case WPN_FIRE:
		return (float)(w->x + w->dx * w->offset) * GRID;
	default:
		return (float)w->x * GRID;
	}
}

static void draw_weapon(Game* g, Weapon* w)
{
	(void)g;
	float ww, hh;
	switch (w->kind) {
	case WPN_ROCKET: {
		img_size(SET_ROCKET, w->dir + 1, &ww, &hh);
		ww = ww > 18.0f ? ww : 18.0f;
		hh = hh > 18.0f ? hh : 18.0f;
		spr(SET_ROCKET, w->dir + 1, w->px + GRID / 2.0f - ww / 2.0f,
		    w->py + GRID / 2.0f - hh / 2.0f, ww, hh, 1.0f);
		break;
	}
	case WPN_MINE: {
		img_size(SET_MINE, 1, &ww, &hh);
		ww = ww > 20.0f ? ww : 20.0f;
		hh = hh > 20.0f ? hh : 20.0f;
		spr(SET_MINE, 1 + (w->t % 60), (float)w->x * GRID + GRID / 2.0f - ww / 2.0f,
		    (float)w->y * GRID + GRID - hh, ww, hh, 1.0f);
		break;
	}
	case WPN_GRENADE: {
		float k = (float)w->t / 22.0f;
		if (k > 1.0f) k = 1.0f;
		float px = ((float)w->sx + (float)(w->tx - w->sx) * k) * GRID;
		float py = ((float)w->sy + (float)(w->ty - w->sy) * k) * GRID
		           - sinf(k * 3.14159265f) * 64.0f;
		img_size(SET_GRENADE, 1, &ww, &hh);
		ww = ww > 16.0f ? ww : 16.0f;
		hh = hh > 16.0f ? hh : 16.0f;
		spr(SET_GRENADE, 1 + ((w->t / 5) % 4), px + GRID / 2.0f - ww / 2.0f,
		    py + GRID - hh, ww, hh, 1.0f);
		break;
	}
	case WPN_GAS: {
		img_size(SET_GAS, 1, &ww, &hh);
		ww = ww > 24.0f ? ww : 24.0f;
		hh = hh > 24.0f ? hh : 24.0f;
		spr(SET_GAS, 1 + (w->t % 101), (float)w->x * GRID + GRID / 2.0f - ww / 2.0f,
		    (float)w->y * GRID + GRID - hh, ww, hh, 0.9f);
		break;
	}
	default:
		break;
	}
}

static void draw_explosions(Game* g)
{
	for (int i = 0; i < MAX_EXPLOSIONS; i++) {
		Explosion* e = &g->fx.explosions[i];
		if (!e->active)
			continue;
		float fade = e->t > e->power + 10
		                 ? 1.0f - (float)(e->t - e->power - 10) / 5.0f
		                 : 1.0f;
		if (fade < 0.0f)
			fade = 0.0f;
		for (int k = 0; k < e->n_cells; k++) {
			ExplCell* cell = &e->cells[k];
			int frame;
			if (cell->center)
				frame = e->hero_id;
			else if (cell->ring >= e->power)
				frame = e->hero_id + 8;
			else
				frame = e->hero_id + 4;
			float rot = cell->dir < 0 ? 0.0f : ((float)(cell->dir + 2) * 3.14159265f / 2.0f);
			float size = cell->center ? 36.0f : 34.0f;
			spr_rot(SET_EXP, frame, (float)cell->x * GRID + GRID / 2.0f,
			        (float)cell->y * GRID + GRID / 2.0f, size, -rot, fade);
		}
	}
}

static void draw_effects(Game* g)
{
	for (int i = 0; i < MAX_EFFECTS; i++) {
		Anim* a = &g->fx.effects[i];
		if (!a->active)
			continue;
		int frame = a->t < a->frames ? a->t : a->frames;
		if (frame < 1)
			frame = 1;
		float w, h;
		img_size(a->key_set, frame, &w, &h);
		w *= a->scale;
		h *= a->scale;
		spr(a->key_set, frame,
		    (float)a->x * GRID + GRID / 2.0f - w / 2.0f + a->dx,
		    (float)a->y * GRID + GRID - h + a->dy, w, h, a->alpha);
	}
}

static void draw_particles(Game* g)
{
	for (int i = 0; i < MAX_PARTICLES; i++) {
		Particle* p = &g->particles[i];
		if (!p->active)
			continue;
		float t = 1.0f - (float)p->life / (float)p->max_life;
		if (t < 0.0f)
			t = 0.0f;
		float r = p->size * (0.5f + t * 0.7f);
		if (p->square) {
			C2D_DrawRectSolid(lx(p->x - r / 2.0f), ly(p->y - r / 2.0f), 0.0f,
			                  r * S, r * S, rgb_alpha(p->color, t));
		} else {
			C2D_DrawCircleSolid(lx(p->x), ly(p->y), 0.0f, r * S, rgb_alpha(p->color, t));
		}
	}
}

static void sort_actors(Actor* a, int n)
{
	for (int i = 1; i < n; i++) {
		Actor key = a[i];
		int j = i - 1;
		while (j >= 0 && a[j].sort < key.sort) {
			a[j + 1] = a[j];
			j--;
		}
		a[j + 1] = key;
	}
}

void game_render(Game* g, float scale)
{
	S = scale;
	OX = (TOP_W - VIEW_W * S) / 2.0f;
	OY = (TOP_H - VIEW_H * S) / 2.0f;

	draw_ground(g);

	Actor actors[MAX_HEROES + MAX_BOMBS + MAX_WEAPONS];
	int actor_rows[MAP_H][MAX_HEROES + MAX_BOMBS + MAX_WEAPONS];
	int actor_counts[MAP_H];
	memset(actor_counts, 0, sizeof(actor_counts));

	int n = 0;
	for (int i = 0; i < g->hero_count; i++) {
		Hero* h = &g->heroes[i];
		if (!h->alive)
			continue;
		actors[n].row = (int)lroundf(h->py / GRID);
		actors[n].sort = h->px;
		actors[n].type = 0;
		actors[n].idx = i;
		n++;
	}
	for (int i = 0; i < MAX_BOMBS; i++) {
		Bomb* b = &g->fx.bombs[i];
		if (!b->active)
			continue;
		actors[n].row = b->y;
		actors[n].sort = b->px + 5.0f;
		actors[n].type = 1;
		actors[n].idx = i;
		n++;
	}
	for (int i = 0; i < MAX_WEAPONS; i++) {
		Weapon* w = &g->fx.weapons[i];
		if (!w->active)
			continue;
		actors[n].row = (int)lroundf(weapon_row(w));
		actors[n].sort = weapon_sort_x(w) + 5.0f;
		actors[n].type = 2;
		actors[n].idx = i;
		n++;
	}

	for (int i = 0; i < n; i++) {
		int r = actors[i].row;
		if (r < 0) r = 0;
		if (r >= MAP_H) r = MAP_H - 1;
		actor_rows[r][actor_counts[r]++] = i;
	}

	for (int y = 0; y < MAP_H; y++) {
		draw_block_row(g, y);

		Actor list[MAX_HEROES + MAX_BOMBS + MAX_WEAPONS];
		int cnt = actor_counts[y];
		for (int k = 0; k < cnt; k++)
			list[k] = actors[actor_rows[y][k]];
		sort_actors(list, cnt);
		for (int k = 0; k < cnt; k++) {
			if (list[k].type == 0)
				draw_hero(g, &g->heroes[list[k].idx]);
			else if (list[k].type == 1)
				draw_bomb(g, &g->fx.bombs[list[k].idx]);
			else
				draw_weapon(g, &g->fx.weapons[list[k].idx]);
		}
	}

	draw_explosions(g);
	draw_effects(g);
	draw_particles(g);
}
