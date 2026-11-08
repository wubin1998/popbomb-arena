#include "game.h"

#include <string.h>

const SceneDef SCENES[5] = {
	{ 0, "森林", { 1, 2, 3 }, 3, { 1, 2, 3, 4, 5, 6 }, 6, { 7, 8, 9 }, 3, 52, 0x68c96b },
	{ 1, "沙漠", { 4, 5 }, 2, { 10, 11, 12 }, 3, { 13, 14, 15, 16, 17, 18, 19 }, 7, 53, 0xe7b45a },
	{ 2, "城市", { 6, 7 }, 2, { 20, 21, 22, 23 }, 4, { 24, 25, 26, 27, 28, 29, 30, 31 }, 8, 55, 0x8ea4c8 },
	{ 3, "宝藏", { 8, 9, 10 }, 3, { 32, 33, 34, 35, 36, 37, 38 }, 7, { 39, 40, 41 }, 3, 57, 0xd6a75a },
	{ 4, "糖果", { 11, 12 }, 2, { 42, 43, 44, 45, 46, 47 }, 6, { 48, 49, 50, 51 }, 4, 58, 0xff9ecb },
};

static const int SPAWN_X[4] = { 3, 11, 3, 11 };
static const int SPAWN_Y[4] = { 3, 3, 9, 9 };

static const int AROUND[8][2] = {
	{ -1, 0 }, { -1, -1 }, { 0, -1 }, { 1, -1 },
	{ 1, 0 }, { 1, 1 }, { 0, 1 }, { -1, 1 },
};

bool map_in_bounds(int x, int y)
{
	return x >= 0 && x < MAP_W && y >= 0 && y < MAP_H;
}

Cell* map_cell(GameMap* m, int x, int y)
{
	if (!map_in_bounds(x, y))
		return NULL;
	return &m->cells[y][x];
}

bool map_is_big_cell(GameMap* m, int x, int y)
{
	return x >= m->big_x && x < m->big_x + 2 && y >= m->big_y && y < m->big_y + 2;
}

static bool blocked_at(GameMap* m, int x, int y)
{
	if (!map_in_bounds(x, y))
		return true;
	return m->cells[y][x].stone || map_is_big_cell(m, x, y);
}

static void shuffle_ints(int* a, int n)
{
	for (int i = n - 1; i > 0; i--) {
		int j = rng_int(0, i);
		int t = a[i];
		a[i] = a[j];
		a[j] = t;
	}
}

/* AS2 pocket-fill: unbreakables only go where the 8-cell ring around an
   empty cell is almost one continuous run. */
static void fill_pockets(GameMap* m, const int* stone_styles)
{
	int rx[MAP_W * MAP_H + 16];
	int ry[MAP_W * MAP_H + 16];
	int n = 0;

	for (int y = 0; y < MAP_H; y++) {
		for (int x = 0; x < MAP_W; x++) {
			bool skip = false;
			for (int s = 0; s < 4 && !skip; s++) {
				if (x == SPAWN_X[s] && y == SPAWN_Y[s])
					skip = true;
				else if ((x == SPAWN_X[s] - 1 || x == SPAWN_X[s] + 1) && y == SPAWN_Y[s])
					skip = true;
				else if (x == SPAWN_X[s] && (y == SPAWN_Y[s] - 1 || y == SPAWN_Y[s] + 1))
					skip = true;
			}
			if (!skip) {
				rx[n] = x;
				ry[n] = y;
				n++;
			}
		}
	}

	for (int i = 0; i < n; i++) {
		int r = rng_int(0, n - 1);
		int t = rx[i]; rx[i] = rx[r]; rx[r] = t;
		t = ry[i]; ry[i] = ry[r]; ry[r] = t;
	}

	int len = n;
	for (int s = 0; s < 4; s++) {
		for (int j = 1; j < 8; j += 2) {
			memmove(&rx[1], &rx[0], (size_t)len * sizeof(int));
			memmove(&ry[1], &ry[0], (size_t)len * sizeof(int));
			rx[0] = SPAWN_X[s] + AROUND[j][0];
			ry[0] = SPAWN_Y[s] + AROUND[j][1];
			len++;
		}
	}

	for (int i = 0; i < n; i++) {
		int x0 = rx[i];
		int y0 = ry[i];
		if (!map_in_bounds(x0, y0))
			continue;
		if (m->cells[y0][x0].stone)
			continue;

		int j = 0;
		for (; j < 8; j += 2) {
			int k1 = (j + 1) % 8;
			int k2 = (j + 2) % 8;
			if (blocked_at(m, x0 + AROUND[j][0], y0 + AROUND[j][1]) &&
			    blocked_at(m, x0 + AROUND[k1][0], y0 + AROUND[k1][1]) &&
			    blocked_at(m, x0 + AROUND[k2][0], y0 + AROUND[k2][1]))
				break;
		}
		if (j < 8)
			continue;

		bool empty = !blocked_at(m, x0 - 1, y0);
		int total_empty = empty ? 1 : 0;
		int total_brick = empty ? 0 : 1;
		for (int k = 1; k < 8; k++) {
			bool is_brick = blocked_at(m, x0 + AROUND[k][0], y0 + AROUND[k][1]);
			if (empty) {
				if (is_brick) {
					empty = false;
					total_brick++;
				}
			} else if (!is_brick) {
				if (k % 2 == 0) {
					empty = true;
					total_empty++;
				} else {
					int kk = (k + 1) % 8;
					if (!blocked_at(m, x0 + AROUND[kk][0], y0 + AROUND[kk][1])) {
						empty = true;
						total_empty++;
					} else {
						k += 2;
					}
				}
			}
		}

		if (total_empty + total_brick <= 3) {
			Cell* c = &m->cells[y0][x0];
			c->stone = 1;
			c->stone_frame = (uint8_t)stone_styles[rng_int(0, 1)];
		}
	}
}

void map_init(GameMap* m, int scene_id)
{
	memset(m, 0, sizeof(*m));
	m->scene_id = scene_id;
	m->big_x = 6;
	m->big_y = 5;
	m->big_frame = SCENES[scene_id].big;
	for (int i = 0; i < 4; i++) {
		m->spawn_x[i] = SPAWN_X[i];
		m->spawn_y[i] = SPAWN_Y[i];
	}
	for (int y = 0; y < MAP_H; y++) {
		for (int x = 0; x < MAP_W; x++) {
			m->cells[y][x].bomb = -1;
			m->fire_owner[y][x] = -1;
		}
	}

	const SceneDef* scene = &SCENES[scene_id];

	int brick_styles[8];
	int brick_n = scene->brick_count;
	for (int i = 0; i < brick_n; i++)
		brick_styles[i] = scene->bricks[i];
	shuffle_ints(brick_styles, brick_n);
	int brick_limit = brick_n < 4 ? brick_n : 4;

	int stone_styles[8];
	int stone_n = scene->stone_count;
	for (int i = 0; i < stone_n; i++)
		stone_styles[i] = scene->stones[i];
	shuffle_ints(stone_styles, stone_n);

	for (int y = m->big_y; y < m->big_y + 2; y++)
		for (int x = m->big_x; x < m->big_x + 2; x++)
			m->cells[y][x].stone = 1;

	fill_pockets(m, stone_styles);

	for (int y = 0; y < MAP_H; y++) {
		for (int x = 0; x < MAP_W; x++) {
			Cell* c = &m->cells[y][x];
			if (c->stone)
				continue;
			c->brick = rng_chance(0.6f) ? 1 : 0;
			c->frame = c->brick ? (uint8_t)brick_styles[rng_int(0, brick_limit - 1)] : 0;
		}
	}
	for (int i = 0; i < 4; i++) {
		Cell* c = &m->cells[m->spawn_y[i]][m->spawn_x[i]];
		c->brick = 0;
		c->frame = 0;
	}

	/* Never box a spawn in: if all four neighbours rolled bricks, open one. */
	for (int i = 0; i < 4; i++) {
		bool open = false;
		for (int k = 0; k < 8; k += 2) {
			Cell* n = map_cell(m, m->spawn_x[i] + AROUND[k][0],
			                   m->spawn_y[i] + AROUND[k][1]);
			if (n && !n->brick && !n->stone)
				open = true;
		}
		if (open)
			continue;
		int k = 2 * rng_int(0, 3);
		Cell* n = map_cell(m, m->spawn_x[i] + AROUND[k][0],
		                   m->spawn_y[i] + AROUND[k][1]);
		if (n) {
			n->brick = 0;
			n->frame = 0;
		}
	}
}

bool map_is_solid(GameMap* m, int x, int y)
{
	Cell* c = map_cell(m, x, y);
	if (!c)
		return true;
	return c->brick || c->stone || c->tool == -1;
}

bool map_can_walk(GameMap* m, int x, int y, Hero* hero)
{
	Cell* c = map_cell(m, x, y);
	if (!c)
		return false;
	if (c->brick || c->stone || c->tool == -1)
		return false;
	if (c->bomb >= 0) {
		if (!hero || !hero->push_bomb_able)
			return false;
		Bomb* b = &hero->game->fx.bombs[c->bomb];
		return b->active && b->owner == hero->slot;
	}
	return true;
}

int map_break_brick(GameMap* m, int x, int y)
{
	Cell* c = map_cell(m, x, y);
	if (!c || !c->brick)
		return 0;
	c->brick = 0;
	int f = c->frame;
	c->frame = 0;
	return f;
}

void map_place_tool(GameMap* m, int x, int y, int tool_id, int bob)
{
	Cell* c = map_cell(m, x, y);
	if (!c)
		return;
	c->tool = (int8_t)tool_id;
	c->ice_saved = 0;
	c->ice_ticks = 0;
	c->tool_age = 0;
	c->tool_bob = (uint8_t)bob;
}

int map_take_tool(GameMap* m, int x, int y)
{
	Cell* c = map_cell(m, x, y);
	if (!c || c->tool == 0 || c->tool == -1)
		return 0;
	int t = c->tool;
	c->tool = 0;
	return t;
}

void map_update_tools(GameMap* m)
{
	for (int y = 0; y < MAP_H; y++)
		for (int x = 0; x < MAP_W; x++) {
			Cell* c = &m->cells[y][x];
			if (c->tool > 0)
				c->tool_age++;
		}
}

void map_ignite(GameMap* m, int x, int y, int ticks, int owner_slot)
{
	if (!map_in_bounds(x, y))
		return;
	if (ticks <= 0)
		return;
	if (m->fire[y][x] > 0) {
		if ((uint16_t)ticks > m->fire[y][x])
			m->fire[y][x] = (uint16_t)ticks;
		if (owner_slot > 0)
			m->fire_owner[y][x] = (int8_t)owner_slot;
	} else {
		m->fire[y][x] = (uint16_t)ticks;
		m->fire_owner[y][x] = (int8_t)(owner_slot > 0 ? owner_slot : -1);
	}
}

bool map_fire_at(GameMap* m, int x, int y)
{
	if (!map_in_bounds(x, y))
		return false;
	return m->fire[y][x] > 0;
}

int map_fire_owner(GameMap* m, int x, int y)
{
	if (!map_in_bounds(x, y))
		return -1;
	if (m->fire[y][x] == 0)
		return -1;
	return m->fire_owner[y][x];
}

void map_update_fire(GameMap* m)
{
	for (int y = 0; y < MAP_H; y++)
		for (int x = 0; x < MAP_W; x++) {
			if (m->fire[y][x] == 0)
				continue;
			if (m->fire[y][x] <= 1) {
				m->fire[y][x] = 0;
				m->fire_owner[y][x] = -1;
			} else {
				m->fire[y][x]--;
			}
		}
}

void map_set_ice(GameMap* m, int x, int y, int ticks)
{
	Cell* c = map_cell(m, x, y);
	if (!c || c->tool == -1)
		return;
	c->ice_saved = c->tool;
	c->tool = -1;
	c->ice_ticks = (uint16_t)ticks;
}

bool map_melt_ice(GameMap* m, int x, int y)
{
	Cell* c = map_cell(m, x, y);
	if (!c || c->ice_ticks == 0)
		return false;
	c->ice_ticks = 0;
	if (c->tool == -1) {
		c->tool = c->ice_saved;
		c->ice_saved = 0;
	}
	return true;
}

void map_update_ice(GameMap* m)
{
	for (int y = 0; y < MAP_H; y++)
		for (int x = 0; x < MAP_W; x++) {
			Cell* c = &m->cells[y][x];
			if (c->ice_ticks == 0)
				continue;
			if (c->ice_ticks <= 1) {
				c->ice_ticks = 0;
				if (c->tool == -1) {
					c->tool = c->ice_saved;
					c->ice_saved = 0;
				}
			} else {
				c->ice_ticks--;
			}
		}
}
