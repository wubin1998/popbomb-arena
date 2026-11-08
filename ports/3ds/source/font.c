#include "game.h"

#include <citro2d.h>

#include "assets.h"
#include "zh_glyphs.h"

static C2D_Image s_atlas;
static bool s_ready;

void font_init(void)
{
	C2D_Image im = assets_frame(SET_ZH, 1);
	if (!im.tex || !im.subtex)
		return;
	s_atlas = im;
	s_ready = true;
}

void font_shutdown(void)
{
	s_ready = false;
}

static const ZhGlyph* find_glyph(uint32_t cp)
{
	int lo = 0, hi = ZH_GLYPH_COUNT - 1;
	while (lo <= hi) {
		int mid = (lo + hi) / 2;
		if (ZH_GLYPHS[mid].cp == cp)
			return &ZH_GLYPHS[mid];
		if (ZH_GLYPHS[mid].cp < cp)
			lo = mid + 1;
		else
			hi = mid - 1;
	}
	return NULL;
}

static uint32_t utf8_next(const char** p)
{
	const unsigned char* s = (const unsigned char*)*p;
	if (s[0] < 0x80) {
		(*p)++;
		return s[0];
	}
	uint32_t cp;
	int n;
	if ((s[0] & 0xE0) == 0xC0) {
		cp = s[0] & 0x1F;
		n = 1;
	} else if ((s[0] & 0xF0) == 0xE0) {
		cp = s[0] & 0x0F;
		n = 2;
	} else if ((s[0] & 0xF8) == 0xF0) {
		cp = s[0] & 0x07;
		n = 3;
	} else {
		(*p)++;
		return 0xFFFD;
	}
	for (int i = 1; i <= n; i++) {
		if ((s[i] & 0xC0) != 0x80) {
			*p += i;
			return 0xFFFD;
		}
		cp = (cp << 6) | (s[i] & 0x3F);
	}
	*p += n + 1;
	return cp;
}

/* The atlas covers the whole tex3ds image (frame 0), so its subtexture maps
   the full PNG and we can derive any glyph rect from it. */
static void make_subtex(Tex3DS_SubTexture* out, const ZhGlyph* g)
{
	const Tex3DS_SubTexture* b = s_atlas.subtex;
	float du = b->right - b->left;
	float dv = b->bottom - b->top;

	out->width = g->w;
	out->height = g->h;
	out->left = b->left + (float)g->x / (float)b->width * du;
	out->right = b->left + (float)(g->x + g->w) / (float)b->width * du;
	out->top = b->top + (float)g->y / (float)b->height * dv;
	out->bottom = b->top + (float)(g->y + g->h) / (float)b->height * dv;
}

static void tint_color(C2D_ImageTint* tint, uint32_t color)
{
	u32 c = C2D_Color32((color >> 16) & 0xff, (color >> 8) & 0xff,
	                    color & 0xff, 255);
	C2D_PlainImageTint(tint, c, 0.0f);
}

void font_draw(float x, float y, float scale, uint32_t color, const char* text)
{
	if (!s_ready || !text)
		return;

	C2D_ImageTint tint;
	tint_color(&tint, color);

	float pen = x;
	while (*text) {
		const ZhGlyph* g = find_glyph(utf8_next(&text));
		if (!g)
			continue;
		if (g->w && g->h) {
			Tex3DS_SubTexture sub;
			make_subtex(&sub, g);
			C2D_Image im = { s_atlas.tex, &sub };
			C2D_DrawImageAt(im, pen + g->ox * scale, y + g->oy * scale,
			                0.0f, &tint, scale, scale);
		}
		pen += g->adv * scale;
	}
}

float font_width(float scale, const char* text)
{
	if (!text)
		return 0.0f;

	float w = 0.0f;
	while (*text) {
		const ZhGlyph* g = find_glyph(utf8_next(&text));
		if (g)
			w += g->adv * scale;
	}
	return w;
}
