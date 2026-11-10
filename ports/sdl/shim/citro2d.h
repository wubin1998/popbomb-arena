#pragma once
/* Minimal citro2d/ctru surface on top of SDL2, so the 3DS renderer
   (render.c, font.c, assets.c) compiles for the R36S port unchanged. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <SDL.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

enum { GFX_TOP = 0, GFX_BOTTOM = 1, GFX_LEFT = 0 };
enum {
	C3D_FRAME_SYNCDRAW = 0,
	C3D_DEFAULT_CMDBUF_SIZE = 0x40000,
	C2D_DEFAULT_MAX_OBJECTS = 4096,
};

typedef struct {
	float left, top, right, bottom;
	uint16_t width, height;
} Tex3DS_SubTexture;

typedef struct ShTexture ShTexture;

typedef struct {
	ShTexture* tex;
	Tex3DS_SubTexture* subtex;
} C2D_Image;

typedef struct {
	u32 color;
	float blend;
} C2D_Tint;

typedef struct {
	C2D_Tint corners[4];
} C2D_ImageTint;

typedef struct ShTarget C3D_RenderTarget;
typedef struct ShSheet* C2D_SpriteSheet;

u32 C2D_Color32(u8 r, u8 g, u8 b, u8 a);
void C2D_PlainImageTint(C2D_ImageTint* tint, u32 color, float blend);
void C2D_AlphaImageTint(C2D_ImageTint* tint, float alpha);

void C3D_Init(size_t cmd_buf_size);
void C3D_Fini(void);
void C3D_FrameBegin(u32 flags);
void C3D_FrameEnd(u32 flags);
void C2D_Init(size_t max_objects);
void C2D_Fini(void);
void C2D_Prepare(void);

C3D_RenderTarget* C2D_CreateScreenTarget(int screen, int eye);
void C2D_TargetClear(C3D_RenderTarget* target, u32 color);
void C2D_SceneBegin(C3D_RenderTarget* target);

C2D_SpriteSheet C2D_SpriteSheetLoad(const char* path);
size_t C2D_SpriteSheetCount(C2D_SpriteSheet sheet);
C2D_Image C2D_SpriteSheetGetImage(C2D_SpriteSheet sheet, size_t index);
void C2D_SpriteSheetFree(C2D_SpriteSheet sheet);

void C2D_DrawImageAt(C2D_Image img, float x, float y, float z,
                     const C2D_ImageTint* tint, float scale_x, float scale_y);
void C2D_DrawImageAtRotated(C2D_Image img, float x, float y, float z, float angle,
                            const C2D_ImageTint* tint, float scale_x, float scale_y);
void C2D_DrawRectSolid(float x, float y, float z, float w, float h, u32 color);
void C2D_DrawEllipseSolid(float x, float y, float z, float w, float h, u32 color);
void C2D_DrawCircleSolid(float x, float y, float z, float r, u32 color);

/* ---- platform helpers (not part of citro2d) ---- */
bool shim_gfx_init(int width, int height, const char* title, bool hidden);
void shim_gfx_shutdown(void);
SDL_Renderer* shim_renderer(void);
/* Maps canvas coordinates (400x240 on 3DS) to window pixels. */
void shim_set_transform(float scale, float ox, float oy);
/* Saves the current framebuffer (for tests). */
bool shim_save_bmp(const char* path);
/* Window size in pixels. */
void shim_window_size(int* w, int* h);
