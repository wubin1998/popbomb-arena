#pragma once

#include <stdbool.h>
#include <citro2d.h>

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
	SET_COUNT
};

typedef struct {
	C2D_SpriteSheet sheet;
	C2D_Image* frames;
	int count;
} SpriteSet;

extern SpriteSet g_sets[SET_COUNT];

bool assets_load_all(void);
void assets_free_all(void);

/* Frame numbers start at 1, matching the original SWF. */
C2D_Image assets_frame(int set_id, int frame);
int assets_set_count(int set_id);
