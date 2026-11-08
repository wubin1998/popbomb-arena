#include <stdio.h>
#include <stdlib.h>

#include "assets.h"

SpriteSet g_sets[SET_COUNT];

static const char* const kSetNames[SET_COUNT] = {
	"bg", "brick", "tool", "bomb", "exp", "brickExp", "heroExp", "dieSmoke",
	"rocket", "rocketSmoke", "mine", "grenade", "gas", "fireEffect",
	"freezeEffect", "pointer", "hero1", "hero2", "hero3", "hero4", "zh", "zh16",
	"title", "btn", "btnSel", "heroBg",
};

bool assets_load_all(void)
{
	for (int i = 0; i < SET_COUNT; i++) {
		char path[96];
		snprintf(path, sizeof(path), "romfs:/gfx/%s.t3x", kSetNames[i]);

		C2D_SpriteSheet sheet = C2D_SpriteSheetLoad(path);
		if (!sheet)
			return false;

		size_t count = C2D_SpriteSheetCount(sheet);
		C2D_Image* frames = calloc(count, sizeof(*frames));
		if (!frames) {
			C2D_SpriteSheetFree(sheet);
			return false;
		}
		for (size_t f = 0; f < count; f++)
			frames[f] = C2D_SpriteSheetGetImage(sheet, f);

		g_sets[i].sheet = sheet;
		g_sets[i].frames = frames;
		g_sets[i].count = (int)count;
	}
	return true;
}

void assets_free_all(void)
{
	for (int i = 0; i < SET_COUNT; i++) {
		free(g_sets[i].frames);
		g_sets[i].frames = NULL;
		if (g_sets[i].sheet)
			C2D_SpriteSheetFree(g_sets[i].sheet);
		g_sets[i].sheet = NULL;
		g_sets[i].count = 0;
	}
}

C2D_Image assets_frame(int set_id, int frame)
{
	C2D_Image empty = { NULL, NULL };
	if (set_id < 0 || set_id >= SET_COUNT)
		return empty;
	SpriteSet* s = &g_sets[set_id];
	if (frame < 1 || frame > s->count)
		return empty;
	return s->frames[frame - 1];
}

int assets_set_count(int set_id)
{
	if (set_id < 0 || set_id >= SET_COUNT)
		return 0;
	return g_sets[set_id].count;
}
