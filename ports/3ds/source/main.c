#include <3ds.h>
#include <citro2d.h>

#include <stdio.h>

#include "assets.h"

#define TOP_W 400.0f
#define TOP_H 240.0f
#define BOT_W 320.0f
#define BOT_H 240.0f

/* Logical playfield: 15 x 13 cells of 30 px. */
#define VIEW_W 450.0f
#define VIEW_H 390.0f

static float fit_scale(void)
{
	float sx = TOP_W / VIEW_W;
	float sy = TOP_H / VIEW_H;
	return sx < sy ? sx : sy;
}

static void draw_frame_at(C2D_Image img, float x, float y, float scale)
{
	if (!img.tex || !img.subtex)
		return;
	C2D_DrawImageAt(img, x, y, 0.0f, NULL, scale, scale);
}

static void draw_test_scene(float scale, float ox, float oy)
{
	/* Ground */
	for (int row = 0; row < 13; row++) {
		for (int col = 0; col < 15; col++) {
			C2D_Image bg = assets_frame(SET_BG, 1 + ((row + col) % 3));
			draw_frame_at(bg, ox + col * 30 * scale, oy + row * 30 * scale, scale);
		}
	}

	/* A few solid bricks */
	for (int row = 1; row < 13; row += 2) {
		for (int col = 1; col < 15; col += 2) {
			C2D_Image brick = assets_frame(SET_BRICK, 1);
			draw_frame_at(brick, ox + col * 30 * scale, oy + row * 30 * scale, scale);
		}
	}

	/* Heroes at the four corners, bomb and tool in the middle */
	draw_frame_at(assets_frame(SET_HERO1, 1), ox + 30 * scale, oy + 30 * scale, scale);
	draw_frame_at(assets_frame(SET_HERO2, 1), ox + (14 * 30) * scale, oy + 30 * scale, scale);
	draw_frame_at(assets_frame(SET_HERO3, 1), ox + 30 * scale, oy + (12 * 30) * scale, scale);
	draw_frame_at(assets_frame(SET_HERO4, 1), ox + (14 * 30) * scale, oy + (12 * 30) * scale, scale);
	draw_frame_at(assets_frame(SET_BOMB, 1), ox + (7 * 30) * scale, oy + (6 * 30) * scale, scale);
	draw_frame_at(assets_frame(SET_TOOL, 1), ox + (5 * 30) * scale, oy + (6 * 30) * scale, scale);
}

static void draw_asset_grid(void)
{
	const int cols = 5;
	const float cell_w = BOT_W / cols;
	const float cell_h = BOT_H / 4.0f;

	for (int i = 0; i < SET_COUNT; i++) {
		C2D_Image img = assets_frame(i, 1);
		if (!img.tex || !img.subtex)
			continue;

		float sx = cell_w / (img.subtex->width + 8.0f);
		float sy = cell_h / (img.subtex->height + 8.0f);
		float s = sx < sy ? sx : sy;

		float x = (i % cols) * cell_w + (cell_w - img.subtex->width * s) * 0.5f;
		float y = (i / cols) * cell_h + (cell_h - img.subtex->height * s) * 0.5f;
		C2D_DrawImageAt(img, x, y, 0.0f, NULL, s, s);
	}
}

int main(int argc, char** argv)
{
	(void)argc;
	(void)argv;

	romfsInit();
	gfxInitDefault();
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
	C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
	C2D_Prepare();

	C3D_RenderTarget* top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
	C3D_RenderTarget* bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

	bool assets_ok = assets_load_all();

	float scale = fit_scale();
	float ox = (TOP_W - VIEW_W * scale) * 0.5f;
	float oy = (TOP_H - VIEW_H * scale) * 0.5f;

	while (aptMainLoop()) {
		hidScanInput();
		u32 down = hidKeysDown();
		if (down & KEY_START)
			break;

		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

		C2D_TargetClear(top, C2D_Color32(0x10, 0x18, 0x28, 0xFF));
		C2D_SceneBegin(top);
		if (assets_ok)
			draw_test_scene(scale, ox, oy);
		else
			C2D_DrawRectSolid(0, 0, 0, TOP_W, TOP_H, C2D_Color32(0xC0, 0x20, 0x20, 0xFF));

		C2D_TargetClear(bottom, C2D_Color32(0x08, 0x0C, 0x14, 0xFF));
		C2D_SceneBegin(bottom);
		if (assets_ok) {
			C2D_DrawRectSolid(0, 0, 0, BOT_W, 6, C2D_Color32(0x30, 0xC0, 0x60, 0xFF));
			draw_asset_grid();
		} else {
			C2D_DrawRectSolid(0, 0, 0, BOT_W, 6, C2D_Color32(0xC0, 0x20, 0x20, 0xFF));
		}

		C3D_FrameEnd(0);
	}

	assets_free_all();
	C2D_Fini();
	C3D_Fini();
	gfxExit();
	romfsExit();
	return 0;
}
