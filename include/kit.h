/* What a look (look.h, look_<name>.c) may use from the drawing layer (ui.c) besides ui.h: the frame's state,
 * the images, the order of the pictures (order.h) and the pieces every look draws with. A look may copy from
 * look_stock.c; it may not call another look's file. (G1 plan, B2) What an application's own widget may use
 * besides: widget.h.
 *
 * The drawing rule (the white flicker, order.h): text never directly after an image from a texture other than the
 * atlas. A look's own texture is drawn either as a piece, which waits in the order list until an image could cover
 * it or the screen is done (ui_piece), or as a ground, which is drawn at once under everything (ui_ground_image);
 * after a ground, the next thing drawn before any letters is a visible atlas image with real pixels (a head line, a
 * card, a row), never text. */
#ifndef KIT_H
#define KIT_H

#include "ui.h"

#define BLEED 32                /* a background reaches this far past the screen's sides (it may be drawn zoomed) */

/* The frame as ui.c draws it: looks read it, ui.c alone writes it. */
struct ui_frame {
    u64 ms;                     /* the frame's time, the same for both eyes */
    u64 moving_ms;              /* the grounds' clock (the pattern's drift): ms, standing still in the probe build */
    u32 no;                     /* the frame's number (ui_frame) */
    float slider;               /* the 3D slider this frame, 0 to 1; 0: one eye only */
    float solid;                /* how solid images and text are drawn: below 1 while a popup fades in or out */
    float shift;                /* how far the view moves what is drawn now sideways, px, for this eye: the last
                                   ui_layer's (ui_view's); 0 at a scene's start and between ui_edge_begin and _end */
    int eye;                    /* the pass: 0 the left eye (or the only one) with the bottom screen, 1 the right eye */
    int top;                    /* drawing on the top screen (1), the bottom one (0), neither (-1) */
    int discard;                /* the right eye's pass after its top screen: the bottom screen is not drawn twice */
    int scheme;                 /* the scheme whose atlas and colours are in use (-1 none yet) */
    int pattern;                /* the pattern whose tile is in its texture (-1 none yet, or a new scheme's) */
    int look;                   /* the look drawn (look.h, LOOK_*) */
    int ground_flat;            /* the screen drawn last frame asked for the ground's own light on the screen's
                                   plane, without depth (ui_ground_flat); a frame late, the ground being drawn
                                   before the screen knows what it shows */
};
extern struct ui_frame ui_f;

/* ---- images (the look's art: look_art) */
/* A sprite's entry: the atlas's (ids below ART_COUNT), the sheet's (ART_COUNT up to ART_ALL) or the application's
 * part's (ui_part, from ART_ALL on); NULL for none. */
const art_sprite *ui_sprite_info(int id);
/* The texture a sprite is read from, on whole pixels or smoothed; NULL while it holds nothing to draw. */
C3D_Tex *ui_sprite_tex(int id, int smooth);
/* An image's element size: without the shadow, ring or light it reaches past it with. */
float ui_ew(int id);
float ui_eh(int id);
/* A sprite over r (its caps stretched), cut to the columns from x = from to x = to; tint NULL: as solid as drawn. */
void ui_sprite_draw(int id, rect r, float from, float to, const C2D_ImageTint *tint);
/* A sprite scaled by kx, ky from x, y; about its middle cx, cy by k with alpha. */
void ui_sprite_tinted(int id, float x, float y, float kx, float ky, const C2D_ImageTint *tint);
void ui_sprite_scaled(int id, float cx, float cy, float k, float alpha);
/* A sprite turned clockwise by angle (radians) about its image's middle at cx, cy, read smoothed; a picture, it waits
 * (the drawing rule: text may follow it); tint NULL: as solid as drawn. */
void ui_sprite_turned(int id, float cx, float cy, float angle, const C2D_ImageTint *tint);
/* A zlib blob into tex's memory, which it has to fill exactly: 1, else 0. */
int ui_unpack(C3D_Tex *tex, const uint8_t *z, uint32_t z_size);

/* ---- order (the drawing rule above) */
/* Part of tex (sx, sy, sw, sh in its pixels) to dx, dy, dw, dh: the atlas's drawn at once, another texture's waits. */
void ui_piece(C3D_Tex *tex, float sx, float sy, float sw, float sh, float dx, float dy, float dw, float dh,
              const C2D_ImageTint *tint);
/* A ground's image from a texture of its own, at once: the pieces waiting come first. */
void ui_ground_image(C3D_Tex *tex, const Tex3DS_SubTexture *sub, const C2D_DrawParams *params,
                     const C2D_ImageTint *tint);
/* A rectangle in four colours (top left, top right, bottom left, bottom right), at once. */
void ui_rect(float x, float y, float w, float h, uint32_t c0, uint32_t c1, uint32_t c2, uint32_t c3);
/* Before an image drawn at once over x, y, w, h: the pieces waiting that it could cover; ui_flush: all of them. */
void ui_under(float x, float y, float w, float h);
void ui_flush(void);

/* ---- view */
/* The top screen's depth for what follows (layout.h, DEPTH_*); a view scaled by k about cx, cy, moved shift px. */
void ui_layer(float depth);
void ui_view(float k, float cx, float cy, float shift);
/* What stands on the screen's edge, between these two: neither zoomed nor faded with the screen's content. */
void ui_edge_begin(C3D_Mtx *content);
void ui_edge_end(const C3D_Mtx *content);
/* a and b (0xRRGGBB) mixed by t, with alpha, as citro2d's colour */
uint32_t ui_mix(uint32_t a, uint32_t b, float t, float alpha);

/* ---- pieces every look may draw with */
/* The chosen pattern's tile from y0 to y1, drifting by the clock (its texture made when it changed), as solid as
 * alpha: a ground; on the top screen behind the screen's plane in 3D (DEPTH_PATTERN), on the bottom screen not moved
 * by the slider. */
void ui_pattern(float y0, float y1, float alpha);
/* tex (a tile PATTERN_TILE px square, read with GPU_REPEAT, as the pattern's) over x, y, w, h in the current view,
 * each pixel showing the tile where the ground's pattern (ui_pattern) shows at this moment for this eye: by the
 * grounds' clock, along the pattern's direction, its depth less ui_f.shift; as solid as alpha * ui_f.solid; a
 * ground (drawn at once, ui_ground_image), so an atlas image comes after it before any text. Nothing when no pattern
 * is unpacked, or w or h <= 0. (A view zoomed by a screen's entry is not followed: the tile stays at its place.) */
void ui_pattern_over(C3D_Tex *tex, float x, float y, float w, float h, float alpha);
/* Engraved text, as the System Settings' bar: a dark copy above, then the text; down: held. */
void ui_engraved(const char *s, float x, float baseline, int px, int align, int down);
/* A face of kind (FACE_*) on r in state (UI_DOWN, UI_OFF), from the look's sprites of that kind. */
void ui_face_plain(int kind, rect r, int state);
/* The selection's breathing corners around r, each concentric with r's corner of that radius: top left, top right,
 * bottom left, bottom right. first -1: from the corner texture (pieces); else from the look's atlas, its own corners
 * in art.py's order (select_corners) from the atlas id first, as solid as ui_f.solid. */
void ui_select_brackets(rect r, const int radius[4], int first);

#endif
