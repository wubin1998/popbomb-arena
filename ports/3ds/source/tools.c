#include "game.h"

#include <stddef.h>

const int DIR_VEC[4][2] = { { -1, 0 }, { 0, -1 }, { 1, 0 }, { 0, 1 } };

const HeroInfo HEROES[4] = {
	{ 1, "红", 0xff5b9d, 0xc92f70, 16 },
	{ 2, "黄", 0xffcf3d, 0xd79a12, 17 },
	{ 3, "蓝", 0x56b7ff, 0x2277cc, 18 },
	{ 4, "绿", 0x6ee08a, 0x2f9a53, 19 },
};

/* ids 1..12; frame ids match DefineSprite_294 */
const ToolDef TOOLS[12] = {
	{ 1, "生命", TOOL_LIFE, "生命+1" },
	{ 2, "拳套", TOOL_GLOVE, "推动炸弹" },
	{ 3, "炸弹糖", TOOL_BOMB, "炸弹+1" },
	{ 4, "盾牌", TOOL_SHIELD, "抵挡一次" },
	{ 5, "威力药", TOOL_POWER, "范围+1" },
	{ 6, "飞鞋", TOOL_SPEED, "移动加速" },
	{ 7, "火箭", TOOL_ROCKET, "直线飞行" },
	{ 8, "地雷", TOOL_MINE, "踩中即爆" },
	{ 9, "手榴弹", TOOL_GRENADE, "投掷两格" },
	{ 10, "汽弹", TOOL_GAS, "减速敌人" },
	{ 11, "冰箭", TOOL_ICE, "冻结三格" },
	{ 12, "火焰箭", TOOL_FIRE, "贯穿火焰" },
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
