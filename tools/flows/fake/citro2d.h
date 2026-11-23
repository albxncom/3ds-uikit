/* Scratch renderer: citro2d and citro3d as a recorder. Every draw call ui.c makes is written down with its final
 * place (the view applied), and rec_dump() writes the frame's list and the textures for raster.py. */
#ifndef FAKE_C2D_H
#define FAKE_C2D_H
#include <stdint.h>
#include <3ds.h>
typedef struct { float k, tx, ty; } C3D_Mtx;
typedef struct C3D_Tex { void *data; u16 width, height; int id, linear, repeat, fmt; } C3D_Tex;
typedef struct C3D_RenderTarget { int id; } C3D_RenderTarget;
typedef struct { u16 width, height; float left, top, right, bottom; } Tex3DS_SubTexture;
typedef struct { C3D_Tex *tex; const Tex3DS_SubTexture *subtex; } C2D_Image;
typedef struct { struct { float x, y, w, h; } pos; struct { float x, y; } center; float depth, angle; } C2D_DrawParams;
typedef struct { u32 color; float blend; } C2D_Tint;
typedef struct { C2D_Tint corners[4]; } C2D_ImageTint;
typedef enum { C2D_TopLeft, C2D_TopRight, C2D_BotLeft, C2D_BotRight } C2D_Corner;
typedef struct { const char *s; } C2D_Text;
typedef struct rec_textbuf *C2D_TextBuf;
enum { GPU_RGBA8, GPU_RGB565 };
enum { GPU_NEAREST, GPU_LINEAR };
enum { GPU_CLAMP_TO_EDGE, GPU_REPEAT };
enum { C2D_AtBaseline = 1, C2D_WithColor = 2, C2D_AlignLeft = 0, C2D_AlignRight = 4, C2D_AlignCenter = 8 };
#define C3D_DEFAULT_CMDBUF_SIZE 0x40000
#define C2D_DEFAULT_MAX_OBJECTS 4096
#define C3D_FRAME_SYNCDRAW 1
static inline uint32_t C2D_Color32(uint8_t r, uint8_t g, uint8_t b, uint8_t a) { return r | g << 8 | b << 16 | (uint32_t)a << 24; }
bool C3D_Init(size_t size); void C3D_Fini(void); bool C2D_Init(size_t n); void C2D_Fini(void); void C2D_Prepare(void);
C3D_RenderTarget *C2D_CreateScreenTarget(gfxScreen_t screen, gfx3dSide_t side);
bool C3D_TexInit(C3D_Tex *tex, u16 w, u16 h, int fmt); void C3D_TexDelete(C3D_Tex *tex);
void C3D_TexSetFilter(C3D_Tex *tex, int mag, int min); void C3D_TexSetWrap(C3D_Tex *tex, int s, int t);
void C3D_TexFlush(C3D_Tex *tex);
bool C3D_FrameBegin(int flags); void C3D_FrameEnd(int flags); float C3D_GetCmdBufUsage(void);
void C2D_TargetClear(C3D_RenderTarget *t, u32 color); void C2D_SceneBegin(C3D_RenderTarget *t);
void C2D_ViewReset(void); void C2D_ViewTranslate(float x, float y); void C2D_ViewScale(float x, float y);
void C2D_ViewSave(C3D_Mtx *m); void C2D_ViewRestore(const C3D_Mtx *m);
void C2D_Fade(u32 color);
bool C2D_DrawRectangle(float x, float y, float z, float w, float h, u32 c0, u32 c1, u32 c2, u32 c3);
bool C2D_DrawRectSolid(float x, float y, float z, float w, float h, u32 c);
bool C2D_DrawImage(C2D_Image img, const C2D_DrawParams *p, const C2D_ImageTint *tint);
bool C2D_DrawImageAt(C2D_Image img, float x, float y, float z, const C2D_ImageTint *tint, float sx, float sy);
void C2D_AlphaImageTint(C2D_ImageTint *tint, float alpha);
void C2D_SetImageTint(C2D_ImageTint *tint, C2D_Corner corner, u32 color, float blend);
void C2D_PlainImageTint(C2D_ImageTint *tint, u32 color, float blend);
C2D_TextBuf C2D_TextBufNew(size_t glyphs); void C2D_TextBufDelete(C2D_TextBuf b); void C2D_TextBufClear(C2D_TextBuf b);
size_t C2D_TextBufGetNumGlyphs(C2D_TextBuf b);
const char *C2D_TextParse(C2D_Text *t, C2D_TextBuf b, const char *s); void C2D_TextOptimize(const C2D_Text *t);
void C2D_TextGetDimensions(const C2D_Text *t, float sx, float sy, float *w, float *h);
/* ui.c always passes a colour (C2D_WithColor) */
void rec_text(const C2D_Text *t, u32 flags, float x, float y, float z, float sx, float sy, u32 color);
#define C2D_DrawText rec_text
/* the recorder */
extern void (*rec_frame_done)(void);
void rec_dump(const char *prefix);
#endif
