#include "game.h"

#include <stddef.h>

const int DIR_VEC[4][2] = { { -1, 0 }, { 0, -1 }, { 1, 0 }, { 0, 1 } };

const HeroInfo HEROES[4] = {
	{ 1, "RED", 0xff5b9d, 0xc92f70, 16 },
	{ 2, "YELLOW", 0xffcf3d, 0xd79a12, 17 },
	{ 3, "BLUE", 0x56b7ff, 0x2277cc, 18 },
	{ 4, "GREEN", 0x6ee08a, 0x2f9a53, 19 },
};

/* ids 1..12; frame ids match DefineSprite_294 */
const ToolDef TOOLS[12] = {
	{ 1, "LIFE", TOOL_LIFE, "Life +1" },
	{ 2, "GLOVE", TOOL_GLOVE, "Push bombs" },
	{ 3, "BOMB CANDY", TOOL_BOMB, "Bombs +1" },
	{ 4, "SHIELD", TOOL_SHIELD, "Block one hit" },
	{ 5, "POWER", TOOL_POWER, "Blast +1" },
	{ 6, "FLY SHOES", TOOL_SPEED, "Move faster" },
	{ 7, "ROCKET", TOOL_ROCKET, "Flies straight" },
	{ 8, "MINE", TOOL_MINE, "Explodes on touch" },
	{ 9, "GRENADE", TOOL_GRENADE, "Toss two cells" },
	{ 10, "GAS BOMB", TOOL_GAS, "Slows enemies" },
	{ 11, "ICE ARROW", TOOL_ICE, "Freezes three cells" },
	{ 12, "FIRE ARROW", TOOL_FIRE, "Piercing flame" },
};

const ToolDef* tool_by_id(int id)
{
	if (id < 1 || id > 12)
		return &TOOLS[0];
	return &TOOLS[id - 1];
}

bool tool_is_special(const ToolDef* def)
{
	return def && def->id >= 7;
}

/* Original: 65% drop, then 80% common / 20% special.
   Common weights: life 1, glove 14, bomb 25, shield 10, power 25, speed 25. */
const ToolDef* roll_tool(const int* pair, int pair_len)
{
	if (!rng_chance(0.65f))
		return NULL;
	if (rng_chance(0.8f)) {
		float r = rng_float() * 100.0f;
		if (r < 1.0f) return tool_by_id(1);
		if (r < 15.0f) return tool_by_id(2);
		if (r < 40.0f) return tool_by_id(3);
		if (r < 50.0f) return tool_by_id(4);
		if (r < 75.0f) return tool_by_id(5);
		return tool_by_id(6);
	}
	if (!pair || pair_len <= 0)
		return tool_by_id(7 + rng_int(0, 1));
	return tool_by_id(pair[rng_int(0, pair_len - 1)]);
}
