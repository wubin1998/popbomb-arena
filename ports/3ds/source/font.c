#include "game.h"

#include <citro2d.h>

#include "assets.h"
#include "zh_glyphs.h"

/* Native sizes, largest first: index 0 = 24px headings, 1 = 16px body text.
   Glyphs are blitted 1:1, so the requested scale only selects a size. */
static const int k_font_sets[ZH_FONT_COUNT] = { SET_ZH, SET_ZH16 };
static C2D_Image s_atlas[ZH_FONT_COUNT];
static bool s_ready;

/* The draw API keeps taking a scale (call sites lay text out with one), but
   anything below this is body text and snaps to the 16px atlas. */
#define BODY_SCALE_CUT 0.73f

void font_init(void)
{
	int ok = 0;
	for (int i = 0; i < ZH_FONT_COUNT; i++) {
		C2D_Image im = assets_frame(k_font_sets[i], 1);
		if (!im.tex || !im.subtex)
			continue;
		s_atlas[i] = im;
		ok++;
	}
	s_ready = ok == ZH_FONT_COUNT;
}

void font_shutdown(void)
{
	s_ready = false;
}

static int font_index(float scale)
{
	return scale < BODY_SCALE_CUT && ZH_FONT_COUNT > 1 ? 1 : 0;
}

static const ZhGlyph* find_glyph(const ZhFontDef* f, uint32_t cp)
{
	int lo = 0, hi = f->count - 1;
	while (lo <= hi) {
		int mid = (lo + hi) / 2;
		if (f->glyphs[mid].cp == cp)
			return &f->glyphs[mid];
		if (f->glyphs[mid].cp < cp)
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
static void make_subtex(Tex3DS_SubTexture* out, const C2D_Image* atlas,
                        const ZhFontDef* f, const ZhGlyph* g)
{
	const Tex3DS_SubTexture* b = atlas->subtex;
	float du = b->right - b->left;
	float dv = b->bottom - b->top;

	out->width = g->w;
	out->height = g->h;
	out->left = b->left + (float)g->x / (float)f->atlas_w * du;
	out->right = b->left + (float)(g->x + g->w) / (float)f->atlas_w * du;
	out->top = b->top + (float)g->y / (float)f->atlas_h * dv;
	out->bottom = b->top + (float)(g->y + g->h) / (float)f->atlas_h * dv;
}

static void tint_color(C2D_ImageTint* tint, uint32_t color)
{
	u32 c = C2D_Color32((color >> 16) & 0xff, (color >> 8) & 0xff,
	                    color & 0xff, 255);
	/* blend must be 1.0: at 0.0 citro2d returns the untinted texture (white). */
	C2D_PlainImageTint(tint, c, 1.0f);
}

void font_draw(float x, float y, float scale, uint32_t color, const char* text)
{
	if (!s_ready || !text)
		return;

	int idx = font_index(scale);
	const ZhFontDef* f = &ZH_FONTS[idx];
	const C2D_Image* atlas = &s_atlas[idx];

	/* Glyphs blit 1:1 at their native size. Only display text (the countdown,
	   the lobby banner) asks for more than native, so scale that up. */
	float k = (idx == 0 && scale > 1.05f) ? scale : 1.0f;

	C2D_ImageTint tint;
	tint_color(&tint, color);

	float pen = x;
	while (*text) {
		const ZhGlyph* g = find_glyph(f, utf8_next(&text));
		if (!g)
			continue;
		if (g->w && g->h) {
			Tex3DS_SubTexture sub;
			make_subtex(&sub, atlas, f, g);
			C2D_Image im = { atlas->tex, &sub };
			C2D_DrawImageAt(im, pen + g->ox * k, y + g->oy * k, 0.0f, &tint, k, k);
		}
		pen += g->adv * k;
	}
}

float font_width(float scale, const char* text)
{
	if (!s_ready || !text)
		return 0.0f;

	int idx = font_index(scale);
	const ZhFontDef* f = &ZH_FONTS[idx];
	float k = (idx == 0 && scale > 1.05f) ? scale : 1.0f;

	float w = 0.0f;
	while (*text) {
		const ZhGlyph* g = find_glyph(f, utf8_next(&text));
		if (g)
			w += g->adv * k;
	}
	return w;
}
