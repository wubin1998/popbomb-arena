#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "citro2d.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct ShTexture {
	SDL_Texture* tex;
	int w, h;
};

struct ShSheet {
	int count;
	C2D_Image* frames;
	Tex3DS_SubTexture* subtex;
	ShTexture** owned;
};

struct ShTarget {
	int screen;
};

static SDL_Window* g_win;
static SDL_Renderer* g_ren;
static int g_win_w = 640, g_win_h = 480;
static float g_scale = 2.0f, g_ox = -80.0f, g_oy = 0.0f;
static u32 g_clear = 0x000000ffu;
static struct ShTarget g_targets[2];

/* ---------------- platform ---------------- */

bool shim_gfx_init(int width, int height, const char* title, bool hidden)
{
	if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
		return false;
	Uint32 flags = SDL_WINDOW_SHOWN;
	if (hidden)
		flags = SDL_WINDOW_HIDDEN;
	else if (getenv("POPBOMB_FULLSCREEN"))
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	g_win = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
	                         width, height, flags);
	if (!g_win)
		return false;
	SDL_GetWindowSize(g_win, &g_win_w, &g_win_h);

	const char* forced = getenv("POPBOMB_RENDERER");
	Uint32 rf = SDL_RENDERER_ACCELERATED;
	if (forced && strcmp(forced, "software") == 0)
		rf = SDL_RENDERER_SOFTWARE;
	g_ren = SDL_CreateRenderer(g_win, -1, rf);
	if (!g_ren)
		g_ren = SDL_CreateRenderer(g_win, -1, SDL_RENDERER_SOFTWARE);
	if (!g_ren)
		return false;
	SDL_SetRenderDrawBlendMode(g_ren, SDL_BLENDMODE_BLEND);
	return true;
}

void shim_gfx_shutdown(void)
{
	if (g_ren) SDL_DestroyRenderer(g_ren);
	if (g_win) SDL_DestroyWindow(g_win);
	g_ren = NULL;
	g_win = NULL;
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

SDL_Renderer* shim_renderer(void) { return g_ren; }

void shim_set_transform(float scale, float ox, float oy)
{
	g_scale = scale;
	g_ox = ox;
	g_oy = oy;
}

void shim_window_size(int* w, int* h)
{
	if (w) *w = g_win_w;
	if (h) *h = g_win_h;
}

bool shim_save_bmp(const char* path)
{
	if (!g_ren)
		return false;
	SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, g_win_w, g_win_h, 32,
	                                                   SDL_PIXELFORMAT_ARGB8888);
	if (!surf)
		return false;
	bool ok = SDL_RenderReadPixels(g_ren, NULL, SDL_PIXELFORMAT_ARGB8888,
	                               surf->pixels, surf->pitch) == 0;
	if (ok)
		ok = SDL_SaveBMP(surf, path) == 0;
	SDL_FreeSurface(surf);
	return ok;
}

/* ---------------- colours and tints ---------------- */

u32 C2D_Color32(u8 r, u8 g, u8 b, u8 a)
{
	return ((u32)r << 24) | ((u32)g << 16) | ((u32)b << 8) | a;
}

static void color_parts(u32 c, Uint8* r, Uint8* g, Uint8* b, Uint8* a)
{
	*r = (Uint8)(c >> 24);
	*g = (Uint8)(c >> 16);
	*b = (Uint8)(c >> 8);
	*a = (Uint8)c;
}

void C2D_PlainImageTint(C2D_ImageTint* tint, u32 color, float blend)
{
	for (int i = 0; i < 4; i++) {
		tint->corners[i].color = color;
		tint->corners[i].blend = blend;
	}
}

void C2D_AlphaImageTint(C2D_ImageTint* tint, float alpha)
{
	C2D_PlainImageTint(tint, 0xffffffffu, alpha);
}

static void apply_tint(ShTexture* t, const C2D_ImageTint* tint)
{
	Uint8 r = 255, g = 255, b = 255, a = 255;
	if (tint) {
		u32 c = tint->corners[0].color;
		float k = tint->corners[0].blend;
		if (k < 0.0f) k = 0.0f;
		if (k > 1.0f) k = 1.0f;
		Uint8 cr, cg, cb, ca;
		color_parts(c, &cr, &cg, &cb, &ca);
		r = (Uint8)(255.0f + ((float)cr - 255.0f) * k);
		g = (Uint8)(255.0f + ((float)cg - 255.0f) * k);
		b = (Uint8)(255.0f + ((float)cb - 255.0f) * k);
		a = (Uint8)(255.0f * k);
	}
	SDL_SetTextureColorMod(t->tex, r, g, b);
	SDL_SetTextureAlphaMod(t->tex, a);
}

/* ---------------- ctru / citro2d lifecycle ---------------- */

void C3D_Init(size_t cmd_buf_size) { (void)cmd_buf_size; }
void C3D_Fini(void) {}
void C3D_FrameBegin(u32 flags) { (void)flags; }
void C3D_FrameEnd(u32 flags)
{
	(void)flags;
	if (g_ren)
		SDL_RenderPresent(g_ren);
}
void C2D_Init(size_t max_objects) { (void)max_objects; }
void C2D_Fini(void) {}
void C2D_Prepare(void) {}

C3D_RenderTarget* C2D_CreateScreenTarget(int screen, int eye)
{
	(void)eye;
	g_targets[screen].screen = screen;
	return &g_targets[screen];
}

void C2D_TargetClear(C3D_RenderTarget* target, u32 color)
{
	(void)target;
	g_clear = color;
}

void C2D_SceneBegin(C3D_RenderTarget* target)
{
	(void)target;
	Uint8 r, g, b, a;
	color_parts(g_clear, &r, &g, &b, &a);
	SDL_SetRenderDrawColor(g_ren, r, g, b, a);
	SDL_RenderClear(g_ren);
	SDL_SetRenderDrawBlendMode(g_ren, SDL_BLENDMODE_BLEND);
}

/* ---------------- sprite sheets ---------------- */

static char* read_text(const char* path)
{
	FILE* f = fopen(path, "rb");
	if (!f)
		return NULL;
	fseek(f, 0, SEEK_END);
	long n = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (n <= 0) {
		fclose(f);
		return NULL;
	}
	char* buf = malloc((size_t)n + 1);
	if (!buf) {
		fclose(f);
		return NULL;
	}
	size_t got = fread(buf, 1, (size_t)n, f);
	fclose(f);
	buf[got] = 0;
	return buf;
}

static bool file_exists(const char* path)
{
	FILE* f = fopen(path, "rb");
	if (!f)
		return false;
	fclose(f);
	return true;
}

static ShTexture* load_png(const char* path)
{
	int w = 0, h = 0, comp = 0;
	unsigned char* px = stbi_load(path, &w, &h, &comp, 4);
	if (!px)
		return NULL;
	SDL_Texture* tex = SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_RGBA32,
	                                     SDL_TEXTUREACCESS_STATIC, w, h);
	if (!tex) {
		stbi_image_free(px);
		return NULL;
	}
	SDL_UpdateTexture(tex, NULL, px, w * 4);
	stbi_image_free(px);
	SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
#if SDL_VERSION_ATLEAST(2, 0, 12)
	SDL_SetTextureScaleMode(tex, SDL_ScaleModeLinear);
#endif
	ShTexture* out = calloc(1, sizeof(*out));
	out->tex = tex;
	out->w = w;
	out->h = h;
	return out;
}

static bool resolve_entry(const char* sheet_dir, const char* root, const char* entry,
                          char* out, size_t out_n)
{
	if (entry[0] == '/') {
		snprintf(out, out_n, "%s", entry);
		return file_exists(out);
	}
	char joined[1024];
	snprintf(joined, sizeof(joined), "%s/%s", sheet_dir, entry);
	char real[PATH_MAX];
	if (realpath(joined, real)) {
		snprintf(out, out_n, "%s", real);
		return true;
	}
	const char* base = strrchr(entry, '/');
	base = base ? base + 1 : entry;
	const char* const dirs[] = { "gfx", "img", "src/assets/img" };
	for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
		snprintf(out, out_n, "%s/%s/%s", root, dirs[i], base);
		if (file_exists(out))
			return true;
	}
	return false;
}

C2D_SpriteSheet C2D_SpriteSheetLoad(const char* path)
{
	const char* root = getenv("POPBOMB_DATA");
	char root_buf[1024];
	if (!root) {
		const char* base = SDL_GetBasePath();
		if (!base)
			return NULL;
		snprintf(root_buf, sizeof(root_buf), "%s", base);
		root = root_buf;
	}

	const char* slash = strrchr(path, '/');
	const char* name = slash ? slash + 1 : path;
	char stem[128];
	snprintf(stem, sizeof(stem), "%s", name);
	char* dot = strrchr(stem, '.');
	if (dot)
		*dot = 0;

	char t3s[1024];
	const char* const layouts[] = { "%s/gfx/%s.t3s", "%s/romfs/gfx/%s.t3s", "%s/%s.t3s" };
	bool found = false;
	for (size_t i = 0; i < 3 && !found; i++) {
		snprintf(t3s, sizeof(t3s), layouts[i], root, stem);
		found = file_exists(t3s);
	}
	if (!found)
		return NULL;

	char* text = read_text(t3s);
	if (!text)
		return NULL;

	char sheet_dir[1024];
	snprintf(sheet_dir, sizeof(sheet_dir), "%s", t3s);
	char* sslash = strrchr(sheet_dir, '/');
	if (sslash)
		*sslash = 0;

	struct ShSheet* sheet = calloc(1, sizeof(*sheet));
	int cap = 16;
	sheet->frames = calloc((size_t)cap, sizeof(*sheet->frames));
	sheet->subtex = calloc((size_t)cap, sizeof(*sheet->subtex));
	sheet->owned = calloc((size_t)cap, sizeof(*sheet->owned));

	char* line = text;
	while (line && *line) {
		char* nl = strchr(line, '\n');
		if (nl)
			*nl = 0;
		size_t len = strlen(line);
		if (len && line[len - 1] == '\r')
			line[len - 1] = 0;
		if (line[0] && line[0] != '-') {
			char full[1024];
			if (resolve_entry(sheet_dir, root, line, full, sizeof(full))) {
				ShTexture* st = load_png(full);
				if (st) {
					if (sheet->count == cap) {
						cap *= 2;
						sheet->frames = realloc(sheet->frames, (size_t)cap * sizeof(*sheet->frames));
						sheet->subtex = realloc(sheet->subtex, (size_t)cap * sizeof(*sheet->subtex));
						sheet->owned = realloc(sheet->owned, (size_t)cap * sizeof(*sheet->owned));
					}
					Tex3DS_SubTexture* sub = &sheet->subtex[sheet->count];
					sub->width = (uint16_t)st->w;
					sub->height = (uint16_t)st->h;
					sub->left = 0.0f;
					sub->top = 0.0f;
					sub->right = 1.0f;
					sub->bottom = 1.0f;
					sheet->frames[sheet->count].tex = st;
					sheet->frames[sheet->count].subtex = sub;
					sheet->owned[sheet->count] = st;
					sheet->count++;
				}
			}
		}
		line = nl ? nl + 1 : NULL;
	}
	free(text);
	return sheet;
}

size_t C2D_SpriteSheetCount(C2D_SpriteSheet sheet)
{
	return sheet ? (size_t)sheet->count : 0;
}

C2D_Image C2D_SpriteSheetGetImage(C2D_SpriteSheet sheet, size_t index)
{
	C2D_Image empty = { NULL, NULL };
	if (!sheet || index >= (size_t)sheet->count)
		return empty;
	return sheet->frames[index];
}

void C2D_SpriteSheetFree(C2D_SpriteSheet sheet)
{
	if (!sheet)
		return;
	for (int i = 0; i < sheet->count; i++)
		if (sheet->owned[i])
			SDL_DestroyTexture(sheet->owned[i]->tex);
	for (int i = 0; i < sheet->count; i++) {
		free(sheet->owned[i]);
	}
	free(sheet->owned);
	free(sheet->subtex);
	free(sheet->frames);
	free(sheet);
}

/* ---------------- drawing ---------------- */

static void canvas_rect(float x, float y, float w, float h, SDL_FRect* out)
{
	out->x = g_ox + x * g_scale;
	out->y = g_oy + y * g_scale;
	out->w = w * g_scale;
	out->h = h * g_scale;
}

static void draw_image(C2D_Image img, float x, float y, const C2D_ImageTint* tint,
                       float scale_x, float scale_y, float angle, bool centered)
{
	if (!img.tex || !img.subtex || !img.tex->tex)
		return;
	const Tex3DS_SubTexture* sub = img.subtex;
	SDL_Rect src;
	src.x = (int)(sub->left * (float)img.tex->w);
	src.y = (int)(sub->top * (float)img.tex->h);
	src.w = (int)((sub->right - sub->left) * (float)img.tex->w);
	src.h = (int)((sub->bottom - sub->top) * (float)img.tex->h);
	if (src.w <= 0 || src.h <= 0)
		return;

	float w = (float)sub->width * scale_x;
	float h = (float)sub->height * scale_y;
	float dx = centered ? x - w / 2.0f : x;
	float dy = centered ? y - h / 2.0f : y;
	SDL_FRect dst;
	canvas_rect(dx, dy, w, h, &dst);

	apply_tint(img.tex, tint);
	if (angle == 0.0f) {
		SDL_RenderCopyF(g_ren, img.tex->tex, &src, &dst);
	} else {
		SDL_FPoint center = { dst.w / 2.0f, dst.h / 2.0f };
		SDL_RenderCopyExF(g_ren, img.tex->tex, &src, &dst,
		                  (double)(angle * 57.29578f), &center, SDL_FLIP_NONE);
	}
	SDL_SetTextureColorMod(img.tex->tex, 255, 255, 255);
	SDL_SetTextureAlphaMod(img.tex->tex, 255);
}

void C2D_DrawImageAt(C2D_Image img, float x, float y, float z,
                     const C2D_ImageTint* tint, float scale_x, float scale_y)
{
	(void)z;
	draw_image(img, x, y, tint, scale_x, scale_y, 0.0f, false);
}

void C2D_DrawImageAtRotated(C2D_Image img, float x, float y, float z, float angle,
                            const C2D_ImageTint* tint, float scale_x, float scale_y)
{
	(void)z;
	draw_image(img, x, y, tint, scale_x, scale_y, angle, true);
}

void C2D_DrawRectSolid(float x, float y, float z, float w, float h, u32 color)
{
	(void)z;
	SDL_FRect r;
	canvas_rect(x, y, w, h, &r);
	Uint8 cr, cg, cb, ca;
	color_parts(color, &cr, &cg, &cb, &ca);
	SDL_SetRenderDrawColor(g_ren, cr, cg, cb, ca);
	SDL_RenderFillRectF(g_ren, &r);
}

static void fill_ellipse(float cx, float cy, float rx, float ry, u32 color)
{
#if SDL_VERSION_ATLEAST(2, 0, 18)
	const int seg = 40;
	SDL_Vertex verts[seg + 2];
	Uint8 cr, cg, cb, ca;
	color_parts(color, &cr, &cg, &cb, &ca);
	SDL_Color col = { cr, cg, cb, ca };
	verts[0].position.x = g_ox + cx * g_scale;
	verts[0].position.y = g_oy + cy * g_scale;
	verts[0].color = col;
	verts[0].tex_coord.x = 0.0f;
	verts[0].tex_coord.y = 0.0f;
	for (int i = 0; i <= seg; i++) {
		float t = (float)i * 6.2831853f / (float)seg;
		verts[i + 1].position.x = g_ox + (cx + cosf(t) * rx) * g_scale;
		verts[i + 1].position.y = g_oy + (cy + sinf(t) * ry) * g_scale;
		verts[i + 1].color = col;
		verts[i + 1].tex_coord.x = 0.0f;
		verts[i + 1].tex_coord.y = 0.0f;
	}
	int indices[seg * 3];
	for (int i = 0; i < seg; i++) {
		indices[i * 3 + 0] = 0;
		indices[i * 3 + 1] = i + 1;
		indices[i * 3 + 2] = i + 2;
	}
	SDL_RenderGeometry(g_ren, NULL, verts, seg + 2, indices, seg * 3);
#else
	SDL_FRect r;
	canvas_rect(cx - rx, cy - ry, rx * 2.0f, ry * 2.0f, &r);
	Uint8 cr, cg, cb, ca;
	color_parts(color, &cr, &cg, &cb, &ca);
	SDL_SetRenderDrawColor(g_ren, cr, cg, cb, ca);
	SDL_RenderFillRectF(g_ren, &r);
#endif
}

void C2D_DrawEllipseSolid(float x, float y, float z, float w, float h, u32 color)
{
	(void)z;
	fill_ellipse(x + w / 2.0f, y + h / 2.0f, w / 2.0f, h / 2.0f, color);
}

void C2D_DrawCircleSolid(float x, float y, float z, float r, u32 color)
{
	(void)z;
	fill_ellipse(x, y, r, r, color);
}
