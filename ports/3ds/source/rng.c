#include "game.h"

static uint32_t g_state = 0x9e3779b9u;

void rng_seed(uint32_t seed)
{
	if (seed == 0)
		seed = 0x9e3779b9u;
	g_state = seed;
	for (int i = 0; i < 8; i++)
		rng_next();
}

uint32_t rng_next(void)
{
	uint32_t x = g_state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	g_state = x;
	return x;
}

float rng_float(void)
{
	return (float)(rng_next() >> 8) / 16777216.0f;
}

/* inclusive like BI.randInt(a, b) */
int rng_int(int a, int b)
{
	if (b <= a)
		return a;
	return a + (int)(rng_next() % (uint32_t)(b - a + 1));
}

bool rng_chance(float p)
{
	return rng_float() < p;
}
