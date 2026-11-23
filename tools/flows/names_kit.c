/* Scratch renderer: the names for a program that registers no part (the kit's example, example.c): the names of what
 * an image call samples, for rec.c's second form. A call on the kit's atlas or glass sheet is named by the sprite of
 * the look in use whose place in that texture holds the texels (its name in ui_art.h, "+dx,dy" where the call starts
 * inside it), one on the selection's corners by its corner, any other "-": the camera picture, a wallpaper, a theme's
 * icon, the pattern, a look's own texture. A region of the atlas or the sheet that no sprite's place holds is
 * "-atlas" or "-sheet". Without a part there are no part textures to word (rec_texture_word: NULL). */
#include <stdio.h>
#include <string.h>
#include <zlib.h>
#include "kit.h"
#include "ui_art.h"
#include "art_names.h"

static const char *named(const char *name, int sx, int sy, int x0, int y0)
{
    static char buf[96];
    if (x0 == sx && y0 == sy)
        snprintf(buf, sizeof buf, "%s", name);
    else
        snprintf(buf, sizeof buf, "%s+%d,%d", name, x0 - sx, y0 - sy);
    return buf;
}
static int holds(const art_sprite *s, int x0, int y0, int x1, int y1)
{
    return s && s->w && x0 >= s->x && y0 >= s->y && x1 <= s->x + s->w && y1 <= s->y + s->h;
}
/* the corners' texture is known by its pixels: ui.c keeps it to itself */
static int is_corners(const void *data)
{
    static const void *known;
    static unsigned char want[ART_CORNERS_W * ART_CORNERS_H * 4];
    static int unpacked;
    if (data == known)
        return 1;
    if (!unpacked) {
        uLongf size = sizeof want;
        unpacked = uncompress(want, &size, ART_CORNERS_Z, ART_CORNERS_Z_SIZE) == Z_OK ? 1 : -1;
    }
    if (unpacked < 0 || memcmp(data, want, sizeof want))
        return 0;
    known = data;
    return 1;
}

const char *rec_texture_word(const void *data)
{
    (void)data;
    return NULL;
}
const char *rec_sprite_name(const void *data, int w, int h, int x0, int y0, int x1, int y1)
{
    static const int first_id[3] = {0, ART_COUNT, ART_ALL};
    static const char *const word[2] = {"atlas", "sheet"};
    static char none[16];
    for (int t = 0; t < 2; t++) {
        C3D_Tex *tex = ui_sprite_tex(first_id[t], 0);
        if (!tex || data != tex->data)
            continue;
        for (int id = first_id[t]; id < first_id[t + 1]; id++) {
            const art_sprite *s = ui_sprite_info(id);
            if (s && holds(s, x0, y0, x1, y1))
                return named(ART_NAME[id], s->x, s->y, x0, y0);
        }
        snprintf(none, sizeof none, "-%s", word[t]);
        return none;
    }
    if (w == ART_CORNERS_W && h == ART_CORNERS_H && is_corners(data))
        for (int k = 0; k < ART_CORNER_COUNT; k++)
            if (holds(&ART_CORNER_SPRITES[k], x0, y0, x1, y1))
                return named(ART_CORNER_NAME[k], ART_CORNER_SPRITES[k].x, ART_CORNER_SPRITES[k].y, x0, y0);
    return "-";
}
