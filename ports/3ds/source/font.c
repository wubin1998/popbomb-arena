#include "game.h"

#include <citro2d.h>

static C2D_TextBuf s_buf;

void font_init(void)
{
	if (!s_buf)
		s_buf = C2D_TextBufNew(2048);
}

void font_shutdown(void)
{
	if (s_buf) {
		C2D_TextBufDelete(s_buf);
		s_buf = NULL;
	}
}

static void parse(C2D_Text* t, const char* text)
{
	C2D_TextBufClear(s_buf);
	C2D_TextParse(t, s_buf, text);
	C2D_TextOptimize(t);
}

void font_draw(float x, float y, float scale, uint32_t color, const char* text)
{
	if (!s_buf || !text || !*text)
		return;
	C2D_Text t;
	parse(&t, text);
	u32 c = C2D_Color32((color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff, 255);
	C2D_DrawText(&t, C2D_WithColor, x, y, 0.0f, scale, scale, c);
}

float font_width(float scale, const char* text)
{
	if (!s_buf || !text || !*text)
		return 0.0f;
	C2D_Text t;
	parse(&t, text);
	return t.width * scale;
}
