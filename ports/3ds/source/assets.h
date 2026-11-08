#pragma once

#include <stdbool.h>
#include <citro2d.h>

#include "game.h"

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
