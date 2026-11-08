#pragma once

#include <stdbool.h>
#include <stdint.h>

#define GRID 30
#define MAP_W 15
#define MAP_H 13
#define VIEW_W 450
#define VIEW_H 390
#define FPS 35
#define TOTAL_TIME 180

#define MAX_HEROES 4
#define MAX_BOMBS 32
#define MAX_WEAPONS 32
#define MAX_EXPLOSIONS 8
#define MAX_EFFECTS 64
#define MAX_EXPL_CELLS 48
#define MAX_PARTICLES 192

/* sprite atlas ids, keep in sync with assets.c */
enum {
	SET_BG = 0,
	SET_BRICK,
	SET_TOOL,
	SET_BOMB,
	SET_EXP,
	SET_BRICK_EXP,
	SET_HERO_EXP,
	SET_DIE_SMOKE,
	SET_ROCKET,
	SET_ROCKET_SMOKE,
	SET_MINE,
	SET_GRENADE,
	SET_GAS,
	SET_FIRE_EFFECT,
	SET_FREEZE_EFFECT,
	SET_POINTER,
	SET_HERO1,
	SET_HERO2,
	SET_HERO3,
	SET_HERO4,
	SET_ZH,
	SET_TITLE,
	SET_BTN,
	SET_BTN_SEL,
	SET_HERO_BG,
	SET_COUNT
};

extern const int DIR_VEC[4][2]; /* left, up, right, down */

typedef struct Game Game;
typedef struct Hero Hero;

typedef struct {
	int id;
	const char* name;
	uint32_t color; /* 0xRRGGBB */
	uint32_t dark;
	int set_id;     /* SET_HEROn */
} HeroInfo;

extern const HeroInfo HEROES[4];

/* layout: 0 solo, 1 p1, 2 p2 */
bool input_dir_held(int layout, int d);
bool input_fire_held(int layout);

/* ---------------- tools ---------------- */
enum {
	TOOL_NONE = 0,
	TOOL_LIFE = 1,
	TOOL_GLOVE,
	TOOL_BOMB,
	TOOL_SHIELD,
	TOOL_POWER,
	TOOL_SPEED,
	TOOL_ROCKET,
	TOOL_MINE,
	TOOL_GRENADE,
	TOOL_GAS,
	TOOL_ICE,
	TOOL_FIRE,
};

typedef struct {
	int id;
	const char* name;
	int kind; /* same numeric ids as above */
	const char* desc;
} ToolDef;

extern const ToolDef TOOLS[12];

const ToolDef* tool_by_id(int id);
bool tool_is_special(const ToolDef* def);
const ToolDef* roll_tool(const int* pair, int pair_len);

typedef struct {
	int kind; /* TOOL_NONE or tool id */
	int count;
} Special;

/* ---------------- map ---------------- */
typedef struct {
	uint8_t brick;
	uint8_t stone;
	uint8_t frame;      /* brick frame */
	uint8_t stone_frame;
	int8_t tool;        /* 0 none, 1..12, -1 ice wall */
	int8_t ice_saved;
	uint16_t ice_ticks;
	uint16_t tool_age;
	uint8_t tool_bob;
	int16_t bomb;       /* bomb index, -1 none */
} Cell;

typedef struct {
	int id;
	const char* name;
	int bg[3];
	int bg_count;
	int bricks[7];
	int brick_count;
	int stones[8];
	int stone_count;
	int big;
	uint32_t tint;
} SceneDef;

extern const SceneDef SCENES[5];

typedef struct {
	int scene_id;
	Cell cells[MAP_H][MAP_W];
	int big_x, big_y, big_frame;
	int spawn_x[4], spawn_y[4];
	uint16_t fire[MAP_H][MAP_W];
	int8_t fire_owner[MAP_H][MAP_W];
} GameMap;

void map_init(GameMap* m, int scene_id);
bool map_in_bounds(int x, int y);
Cell* map_cell(GameMap* m, int x, int y);
bool map_is_big_cell(GameMap* m, int x, int y);
bool map_is_solid(GameMap* m, int x, int y);
bool map_can_walk(GameMap* m, int x, int y, Hero* hero);
int map_break_brick(GameMap* m, int x, int y);
void map_place_tool(GameMap* m, int x, int y, int tool_id, int bob);
int map_take_tool(GameMap* m, int x, int y);
void map_update_tools(GameMap* m);
void map_ignite(GameMap* m, int x, int y, int ticks, int owner_slot);
bool map_fire_at(GameMap* m, int x, int y);
int map_fire_owner(GameMap* m, int x, int y);
void map_update_fire(GameMap* m);
void map_set_ice(GameMap* m, int x, int y, int ticks);
bool map_melt_ice(GameMap* m, int x, int y);
void map_update_ice(GameMap* m);

/* ---------------- hero ---------------- */
typedef struct {
	int sx, sy, tx, ty;
	int dx, dy;
	int t, ticks;
} Move;

struct Hero {
	Game* game;
	int slot;
	int hero_id;
	bool human;
	int layout; /* 0 solo, 1 p1, 2 p2 */
	int spawn_x, spawn_y;
	int lives;

	int x, y;
	float px, py;
	int dir;
	Move move;
	bool moving;

	int v_level;
	int slow_v_level;
	int slow_time;
	int stop_time;

	int bomb_level;
	int total_bomb;
	int bombs_out;
	bool push_bomb_able;
	bool shield;
	int var_shield;
	Special special;

	int score, brick_score, kill_score, tool_score, time_score, bonus;

	bool alive;
	int age;
	bool prev_fire;
	int last_fire_tick;
};

void hero_init(Hero* h, Game* g, int slot, int hero_id, bool human, int layout,
               int spawn_x, int spawn_y, int lives);
void hero_step(Hero* h);
void hero_try_move(Hero* h, int dir);
void hero_try_fire(Hero* h);
bool hero_put_bomb(Hero* h);
bool hero_hurt(Hero* h, Hero* owner);
void hero_add_score(Hero* h, int kind, int v);
bool hero_invulnerable(Hero* h);

enum { SCORE_BRICK = 1, SCORE_KILL, SCORE_TOOL, SCORE_TIME, SCORE_BONUS };

/* ---------------- bombs and effects ---------------- */
typedef struct {
	Game* game;
	bool active;
	int x, y;
	int owner; /* hero slot, 0 none */
	int power;
	int fuse;
	int t;
	float px, py;
	bool dead;
	bool sliding;
	float fx, fy;
	int slide_t;
	int slide_ticks;
} Bomb;

typedef struct {
	signed char x, y;
	signed char dir;
	signed char ring;
	bool center;
} ExplCell;

typedef struct {
	Game* game;
	bool active;
	int x, y;
	int power;
	int owner;
	int hero_id;
	int ring;
	int t;
	bool dead;
	int n_cells;
	ExplCell cells[MAX_EXPL_CELLS];
	bool dirs[4];
} Explosion;

enum { WPN_ROCKET = 1, WPN_MINE, WPN_GRENADE, WPN_GAS, WPN_ICE, WPN_FIRE };

typedef struct {
	Game* game;
	bool active;
	int kind;
	int owner; /* hero slot */
	int x, y;
	float px, py;
	int dir;
	int dx, dy;
	int t;
	int offset;
	bool dead;
	int sx, sy, tx, ty;
	float vx, vy;
	int hero_id;
} Weapon;

typedef struct {
	bool active;
	int key_set;     /* asset set */
	int x, y;        /* cell */
	int frames;
	int t;
	bool dead;
	float scale;
	float dx, dy;
	float alpha;
	float rot;
} Anim;

typedef struct {
	bool active;
	float x, y, vx, vy;
	int life, max_life;
	float size;
	uint32_t color;
	float gravity;
	bool square;
} Particle;

typedef struct {
	Game* game;
	Bomb bombs[MAX_BOMBS];
	Weapon weapons[MAX_WEAPONS];
	Explosion explosions[MAX_EXPLOSIONS];
	Anim effects[MAX_EFFECTS];
	int tick;
} FX;

void fx_init(FX* fx, Game* g);
void fx_step(FX* fx);
void fx_clear(FX* fx);
Bomb* fx_plant(FX* fx, Hero* hero);
bool bomb_push(Bomb* b, int dx, int dy);
Explosion* fx_boom_at(FX* fx, int x, int y, Hero* owner, int power);
void fx_rocket(FX* fx, Hero* hero);
void fx_mine(FX* fx, Hero* hero);
bool fx_grenade(FX* fx, Hero* hero);
void fx_gas(FX* fx, Hero* hero);
bool fx_ice_arrow(FX* fx, Hero* hero);
bool fx_fire_arrow(FX* fx, Hero* hero);
void fx_anim(FX* fx, int set_id, int x, int y, int frames, float scale, float alpha);
void fx_smoke(FX* fx, float px, float py);

/* ---------------- game ---------------- */
enum { STATE_COUNTDOWN = 0, STATE_PLAYING, STATE_ENDING, STATE_OVER };
enum { END_NONE = 0, END_WIN, END_LOSE, END_TIMEUP };

struct Game {
	Hero heroes[MAX_HEROES];
	int hero_count;
	GameMap map;
	FX fx;
	Particle particles[MAX_PARTICLES];

	int players;
	int diff; /* 0 easy, 1 normal, 2 hard */
	int stage;
	int total_stage;
	int scene_id;
	int brick_score;
	int escape_rate;
	int special_pair[2];

	int tick;
	int time;
	int time_acc;
	bool hud_dirty;
	bool paused;
	int state;
	int count_ticks;
	int end_tick;
	int end_kind;
	bool stage_clear;
	bool final_stage;
	bool can_retry;
};

void game_init(Game* g, int players, int diff, int stage, int total_stage, int scene_id,
               const int* human_ids, const int* lives);
void game_step(Game* g);
Hero* game_hero(Game* g, int slot);
Hero* game_hero_by_id(Game* g, int hero_id);
int game_diff_brick_score(int diff);
int game_diff_escape(int diff);
void game_update_particles(Game* g);
void game_burst(Game* g, float x, float y, int palette, int count,
                float speed_min, float speed_max, float life_max,
                float size_min, float size_max, float gravity);
void game_break_brick_at(Game* g, int x, int y, Hero* owner);
bool game_break_ice_at(Game* g, int x, int y, Hero* owner);
bool game_is_over(Game* g);

/* ---------------- rng ---------------- */
void rng_seed(uint32_t seed);
uint32_t rng_next(void);
float rng_float(void);
int rng_int(int a, int b);
bool rng_chance(float p);

/* ---------------- render ---------------- */
void game_render(Game* g, float scale);

/* ---------------- font ---------------- */
void font_init(void);
void font_shutdown(void);
void font_draw(float x, float y, float scale, uint32_t color, const char* text);
float font_width(float scale, const char* text);
