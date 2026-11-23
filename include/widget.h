/* What a widget drawn outside the drawing layer (an application's own) may use from ui.c besides
 * ui.h and kit.h: the state and helpers ui.c shares with such a widget, under ui.c's own names with the ui_ prefix.
 * ui.c holds the state; ui_exit frees the part's pictures' texture, which ui.c makes and unpacks (ui_part_pictures). */
#ifndef WIDGET_H
#define WIDGET_H

#include "kit.h"
#include "look.h"
#include "order.h"

#define INK UI_COLOR(SC(COL_INK))
#define MUTED UI_COLOR(SC(COL_MUTED))
#define TILE_INK UI_COLOR(SC(COL_TILE_INK))
#define PRESS_INK UI_COLOR(SC(COL_PRESS_INK))
#define TAU 6.2831853f

#ifdef UI_PROBE
/* The probe build: the start screen's house and word and the pattern behind the top screens stand still (their
 * clocks at 0), or no two frames of a top screen would be the same (the application's flicker probe). */
#define MOVING_MS(ms) ((ms) * 0)
#else
#define MOVING_MS(ms) (ms)
#endif

extern const look *ui_lk;       /* the look drawn (look.h; its number: ui_f.look) */
extern int ui_glass_want;       /* the glass style drawn (GLASS_*, ui_look) */
extern float ui_zoom;           /* the transition's scale this frame: the screen's content about its middle */
extern ui_block ui_scratch;     /* for the cards' wrapped text; one frame draws one at a time */

static inline float ui_scale_of(int px)
{
    return (float)px / FONT_PX;
}

/* A pixel's place in an 8x8 tile of the GPU's order: the Z order of its x and y bits. */
static inline unsigned ui_morton(unsigned x, unsigned y)
{
    return (x & 1) | ((y & 1) << 1) | ((x & 2) << 1) | ((y & 2) << 2) | ((x & 4) << 2) | ((y & 4) << 3);
}

/* A picture: drawn from tex as params say, with tint (NULL: none), when order.h says; at covers what it draws on. */
void ui_picture(C3D_Tex *tex, const Tex3DS_SubTexture *sub, const C2D_DrawParams *params,
                const C2D_ImageTint *tint, order_rect at);
/* The application's part's pictures (art_part, ui_part) in the scheme wanted, unpacked into their texture when they
 * are not the ones in it (the first time asked, after a change of scheme or of look): the texture, or NULL where the
 * part brings none, the texture could not be made or they did not unpack. */
C3D_Tex *ui_part_pictures(void);
/* An SMDH icon into its slot of the icons' texture (swarm.h, icon_slots), waiting (order.h) to be drawn into at, with
 * tint when not NULL. */
void ui_icon_put(const uint8_t *smdh_icon, int slot, order_rect at, const C2D_ImageTint *tint);
/* s's first n bytes in out (n + 1 bytes or more), each control character as one space. */
const char *ui_plain(char *out, const char *s, size_t n);
/* A symbol that stands in a line of text: its ink's bottom on the baseline, its top at the cap height. */
void ui_inline_symbol(int id, float x, float baseline);
/* The part's pictures (ui_part_pictures) and the chosen pattern's tile in their textures, when the choice is not the
 * one in the texture. */
void ui_start_textures(void);

#endif
