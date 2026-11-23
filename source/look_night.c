/* Nachtglas, a dark look of smoked glass made for the Shard icons (its art: look_night.py, its numbers: look_night.h).
 * The ground is drawn here: a deep gradient with a few slow lights, made when the colour scheme is set; every surface
 * on it is glass the ground shows through. What can be pressed is cut crystal with a light behind it in the colour of
 * what it does. (G1: the branch ds-glass-night's drawing, carried behind the look boundary.) */
#include "kit.h"
#include "look.h"
#include "look_night.h"

#include <math.h>

#define TAU 6.2831853f

/* ---------------------------------------------------------------- the ground
 * A dark gradient shows every step of its 8 bits, so the ground is not drawn as a rectangle between two colours: per
 * screen a strip of the gradient with a grain of half a step in every pixel (repeated across the screen), and the
 * lights over it as discs whose alpha carries a grain too. All of it is made when the colour scheme is set, from the
 * scheme's ground (COL_TOP_BG_A to COL_BOT_BG_B) and its lights (COL_NG_LAMP_*); a frame draws one strip and its
 * lights, nothing is computed per pixel. */
#define NG_COLOURS 3            /* the lights' colours */
/* where a light is at rest, how far it drifts each way, how long its two swings take (ms), which colour, which
 * screen (0 top, 1 bottom) */
static const struct {
    short x, y, ax, ay;
    unsigned tx, ty;
    unsigned char colour, bottom;
} NG_LAMPS[] = {
    {66, 52, 58, 34, 61000, 47000, 0, 0},  {338, 196, 64, 40, 53000, 71000, 1, 0},
    {214, 104, 120, 56, 89000, 59000, 2, 0}, {48, 26, 52, 44, 67000, 43000, 2, 1},
    {270, 204, 66, 38, 57000, 79000, 1, 1},
};
static C3D_Tex tex[2 + NG_COLOURS];     /* the top screen's strip, the bottom one's, then the lights */
static int inited, made, made_for = -1;

/* A value from 0 to 1 that differs from pixel to pixel and has no clumps (interleaved gradient noise, Jorge Jimenez,
 * "Next Generation Post Processing in Call of Duty: Advanced Warfare", 2014). */
static float grain(unsigned x, unsigned y)
{
    float f = 0.06711056f * (float)x + 0.00583715f * (float)y;
    f = 52.9829189f * (f - floorf(f));
    return f - floorf(f);
}

/* The texel x, y of an RGBA8 texture in the GPU's order (8 x 8 tiles, Z order inside). */
static void texel(C3D_Tex *t, unsigned x, unsigned y, uint32_t rgb, unsigned alpha)
{
    unsigned z = (x & 1) | ((y & 1) << 1) | ((x & 2) << 1) | ((y & 2) << 2) | ((x & 4) << 2) | ((y & 4) << 3);
    ((uint32_t *)t->data)[(((y >> 3) * (t->width >> 3) + (x >> 3)) << 6) | z] =
        (rgb >> 16 & 255) << 24 | (rgb >> 8 & 255) << 16 | (rgb & 255) << 8 | alpha;
}

static void unload(void)
{
    while (inited > 0)
        C3D_TexDelete(&tex[--inited]);
    made = 0;
}

/* one texel a pixel, never between two: smoothing would take the grain out again */
static void load(void)
{
    for (inited = 0; inited < 2 + NG_COLOURS; inited++) {
        int strip = inited < 2;
        if (!C3D_TexInit(&tex[inited], strip ? NG_STRIP : NG_LAMP, strip ? 256 : NG_LAMP, GPU_RGBA8))
            break;
        C3D_TexSetFilter(&tex[inited], GPU_NEAREST, GPU_NEAREST);
        if (strip)
            C3D_TexSetWrap(&tex[inited], GPU_REPEAT, GPU_REPEAT);
    }
    made = inited == 2 + NG_COLOURS;
    made_for = -1;
}

/* The scheme's ground into the textures, when it is not the one in them: called when the atlas has just changed. */
static void refresh(int scheme, int pattern)
{
    static uint8_t disc[NG_LAMP * NG_LAMP];     /* a light's alpha: the same for every colour, worked out once */
    static int disc_made;
    if (!made || pattern >= 0 || made_for == scheme)
        return;
    made_for = scheme;
    const uint32_t ends[2][2] = {{SC(COL_TOP_BG_A), SC(COL_TOP_BG_B)}, {SC(COL_BOT_BG_A), SC(COL_BOT_BG_B)}};
    const uint32_t lights[NG_COLOURS] = {SC(COL_NG_LAMP_A), SC(COL_NG_LAMP_B), SC(COL_NG_LAMP_C)};
    for (int k = 0; k < 2; k++) {
        for (unsigned y = 0; y < 256; y++) {
            float t = ((float)y + 0.5f) / SCREEN_H;
            for (unsigned x = 0; x < NG_STRIP; x++) {
                uint32_t rgb = 0;
                for (int sh = 16; sh >= 0; sh -= 8) {
                    float a = (float)(ends[k][0] >> sh & 255), b = (float)(ends[k][1] >> sh & 255);
                    float v = a + (b - a) * t + grain(x + (unsigned)sh, y) - 0.5f;
                    rgb = rgb << 8 | (uint32_t)(v < 0 ? 0 : v > 255 ? 255 : v + 0.5f);
                }
                texel(&tex[k], x, y, rgb, 255);
            }
        }
        C3D_TexFlush(&tex[k]);
    }
    if (!disc_made) {
        /* a bell that ends at the disc's edge, at most NG_LAMP_PCT % solid; the grain fades out with it */
        float half = NG_LAMP / 2.0f, edge = expf(-3.4f), most = NG_LAMP_PCT * 255 / 100.0f;
        for (unsigned y = 0; y < NG_LAMP; y++) {
            for (unsigned x = 0; x < NG_LAMP; x++) {
                float dx = ((float)x + 0.5f - half) / half, dy = ((float)y + 0.5f - half) / half, q = dx * dx + dy * dy;
                float a = q < 1 ? (expf(-3.4f * q) - edge) / (1 - edge) : 0, g = a * 12 < 1 ? a * 12 : 1;
                float v = a * most + (grain(x, y) - 0.5f) * (NG_LAMP_GRAIN_10 / 10.0f) * g;
                disc[y * NG_LAMP + x] = (uint8_t)(v < 0 ? 0 : v + 0.5f);
            }
        }
        disc_made = 1;
    }
    for (int k = 0; k < NG_COLOURS; k++) {
        for (unsigned y = 0; y < NG_LAMP; y++)
            for (unsigned x = 0; x < NG_LAMP; x++)
                texel(&tex[2 + k], x, y, lights[k], disc[y * NG_LAMP + x]);
        C3D_TexFlush(&tex[2 + k]);
    }
}

/* The screen drawn now from y0 to y1, alpha solid: the band of the screen's strip, then the parts of its lights in it
 * where the clock has them (each swinging about its place, whole pixels; in stereo behind the screen), then on the
 * top screen the chosen pattern (A1: the moving ground behind every top screen), on the bottom one a faint light
 * along its foot. Without its textures: the band of the scheme's gradient. */
static void ground(float y0, float y1, float alpha)
{
    int top = ui_f.top == 1;
    if (!made) {
        uint32_t a = top ? SC(COL_TOP_BG_A) : SC(COL_BOT_BG_A), b = top ? SC(COL_TOP_BG_B) : SC(COL_BOT_BG_B);
        uint32_t c0 = ui_mix(a, b, y0 / SCREEN_H, alpha), c1 = ui_mix(a, b, y1 / SCREEN_H, alpha);
        ui_rect(-BLEED, y0, TOP_W + 2 * BLEED, y1 - y0, c0, c0, c1, c1);
    } else {
        float x0 = -BLEED, x1 = TOP_W + BLEED;
        float shift = top ? roundf(DEPTH_NG_LAMPS * ui_f.slider / 2) : 0;
        C2D_ImageTint tint;
        C2D_AlphaImageTint(&tint, alpha);
        /* whole strips more (8) keep the coordinate above 0, as the pattern's */
        Tex3DS_SubTexture strip = {(u16)(x1 - x0), (u16)(y1 - y0), 8 + x0 / NG_STRIP, 1 - y0 / 256, 8 + x1 / NG_STRIP,
                                   1 - y1 / 256};
        C2D_DrawParams all = {{x0, y0, x1 - x0, y1 - y0}, {0, 0}, 0, 0};
        ui_ground_image(&tex[top ? 0 : 1], &strip, &all, alpha < 1 ? &tint : NULL);
        u64 ms = ui_f.moving_ms;     /* still in the probe build */
        for (unsigned k = 0; k < sizeof NG_LAMPS / sizeof *NG_LAMPS; k++) {
            if (NG_LAMPS[k].bottom == top)
                continue;
            float x = NG_LAMPS[k].x + NG_LAMPS[k].ax * sinf(TAU * (float)(ms % NG_LAMPS[k].tx) / NG_LAMPS[k].tx);
            float y = NG_LAMPS[k].y + NG_LAMPS[k].ay * cosf(TAU * (float)(ms % NG_LAMPS[k].ty) / NG_LAMPS[k].ty);
            x = roundf(x - NG_LAMP / 2.0f) + (ui_f.eye ? -shift : shift), y = roundf(y - NG_LAMP / 2.0f);
            float from = y > y0 ? y : y0, to = y + NG_LAMP < y1 ? y + NG_LAMP : y1;
            if (to <= from)
                continue;
            Tex3DS_SubTexture sub = {NG_LAMP, (u16)(to - from), 0, 1 - (from - y) / NG_LAMP, 1, 1 - (to - y) / NG_LAMP};
            C2D_DrawParams at = {{x, from, NG_LAMP, to - from}, {0, 0}, 0, 0};
            ui_ground_image(&tex[2 + NG_LAMPS[k].colour], &sub, &at, alpha < 1 ? &tint : NULL);
        }
    }
    if (top) {
        ui_pattern(y0, y1, alpha);  /* every top screen begins with an atlas image before its letters (ui_head) */
    } else if (made && y1 >= SCREEN_H) {
        /* the drawing rule after a ground from textures of its own (kit.h): an atlas image with real pixels before
         * any letters, as a screen with nothing but text on it has none (the badges' restart-failed screens): the
         * glass's light along the screen's foot, faint, where the bar covers it on every other screen */
        C2D_ImageTint foot;
        C2D_AlphaImageTint(&foot, NG_FOOT_PCT / 100.0f * alpha);
        ui_sprite_draw(ART_NG_LINE, (rect){TOP_PAD, SCREEN_H - 1, BOTTOM_W - 2 * TOP_PAD, 1}, -1e9f, 1e9f, &foot);
    }
}

/* ---------------------------------------------------------------- surfaces */

/* A hair of light across the top screen, through the middle row of Stock's dots. */
static void rule(float y)
{
    ui_layer(DEPTH_DOTS);
    ui_sprite_box(ART_NG_LINE, (rect){TOP_PAD, y + DOT / 2, TOP_W - 2 * TOP_PAD, 1});
}

static void card(rect r, int glass)
{
    ui_sprite_box(glass < 0 ? ART_CARD : ART_CARD_GLASS + glass, r);
}

/* A face, and over it on a crystal (tiles, wide buttons, choices, the cream button) the light behind it, under what
 * stands on it, in the colour of what its symbol does: what brings in or keeps teal, what removes coral, the Badges
 * amber (the caller's UI_AMBER), the SD card violet; the others (the Designs, the installed house) in the look's own
 * light, which turns with "Farbe". Held, the light fills the crystal. Rows, the settings row and the green button have no light behind them. */
static rect face(int kind, rect r, int state, int symbol)
{
    ui_face_plain(kind, r, state);
    if (kind == FACE_GO || kind == FACE_ROW || kind == FACE_FOLDER || kind == FACE_SET || (state & UI_OFF))
        return r;
    int id = symbol == ART_QR || symbol == ART_KEEP || symbol == ART_NO_MUSIC || symbol == ART_DOWNLOAD
                 ? ART_NG_GLOW_TEAL :
             symbol == ART_DELETE_THEME || symbol == ART_DELETE_FILE || symbol == ART_TRASH ? ART_NG_GLOW_CORAL :
             state & UI_AMBER ? ART_NG_GLOW_AMBER :
             symbol == ART_SD ? ART_NG_GLOW_VIOLET : symbol == ART_INSTALLED ? ART_NG_GLOW_LIGHT : ART_NG_GLOW_DEEP;
    ui_sprite_box(state & UI_DOWN ? id + 1 : id, r);
    return r;
}

/* The selection: a line of light round r (look_night.py rim: three images alike but for their corners; each corner
 * is taken from the image of its radius, the straight parts from the first), breathing a little (NG_RIM_*). */
static void selection(rect r, const int radius[4])
{
    const art_sprite *first = ui_sprite_info(ART_NG_RIM_R0);
    C3D_Tex *atlas = ui_sprite_tex(ART_NG_RIM_R0, 0);
    if (ui_f.discard || !first || !atlas)
        return;
    float t = (float)(ui_f.ms % SELECT_MS) / SELECT_MS, low = NG_RIM_LOW_PCT / 100.0f;
    C2D_ImageTint tint;
    C2D_AlphaImageTint(&tint, (low + (1 - low) * (0.5f + 0.5f * cosf(TAU * t))) * ui_f.solid);
    float x = roundf(r.x) - NG_RIM_PAD, y = roundf(r.y) - NG_RIM_PAD;
    float w = roundf(r.w) + 2 * NG_RIM_PAD, h = roundf(r.h) + 2 * NG_RIM_PAD, n = NG_RIM_PAD + NG_RIM_CAP;
    float cw = n < floorf(w / 2) ? n : floorf(w / 2), ch = n < floorf(h / 2) ? n : floorf(h / 2);
    float mid = first->w - 2 * n;        /* the images' middle, which stretches */
    for (int k = 0; k < 4; k++) {       /* top left, top right, bottom left, bottom right */
        int id = radius[k] >= TILE_R ? ART_NG_RIM_R16 : radius[k] >= ROW_R ? ART_NG_RIM_R10 : ART_NG_RIM_R0;
        const art_sprite *c = ui_sprite_info(id);
        ui_piece(atlas, c->x + (k & 1 ? c->w - cw : 0), c->y + (k & 2 ? c->h - ch : 0), cw, ch,
                 k & 1 ? x + w - cw : x, k & 2 ? y + h - ch : y, cw, ch, &tint);
    }
    ui_piece(atlas, first->x + n, first->y, mid, ch, x + cw, y, w - 2 * cw, ch, &tint);
    ui_piece(atlas, first->x + n, first->y + first->h - ch, mid, ch, x + cw, y + h - ch, w - 2 * cw, ch, &tint);
    ui_piece(atlas, first->x, first->y + n, cw, mid, x, y + ch, cw, h - 2 * ch, &tint);
    ui_piece(atlas, first->x + first->w - cw, first->y + n, cw, mid, x + w - cw, y + ch, cw, h - 2 * ch, &tint);
}

/* The bar across the screen's lower edge, or its tabs: the left one at the edge, the right one with its shadow
 * inside the screen. */
static void bar(int parts, int held)
{
    if (parts & BAR_WIDE)
        ui_sprite(held == 2 ? ART_BAR_DOWN : ART_BAR, 0, BAR_Y);
    if (parts & BAR_LEFT)
        ui_sprite(held == 0 ? ART_TAB_L_DOWN : ART_TAB_L, 0, BAR_Y);
    if (parts & BAR_RIGHT)
        ui_sprite(held == 1 ? ART_TAB_R_DOWN : ART_TAB_R, BOTTOM_W - (TAB_W + BAR_SHADOW), BAR_Y);
}

/* Light on glass: the label alone, nothing engraved. */
static void bar_label(const char *s, float x, float baseline, int px, int align, int down)
{
    ui_text(s, x, baseline, px, down ? UI_COLOR(SC(COL_PRESS_INK)) : UI_COLOR(COL_LABEL), align);
}

static void popup_panel(rect r)
{
    ui_sprite_box(ART_DLG, r);
}

/* The popup's buttons: the band, and under each button a light in the colour of what it does (the left one goes
 * back: blue; the right one goes on: teal, or removes: coral); the labels without a shadow. */
static void popup_buttons(const char *left, const char *right, int held, int ring, int removes)
{
    uint32_t text_ink = UI_COLOR(SC(COL_DLG_INK));
    /* the band from one row above the button (the panel's row in its shadow); held: the held material on the whole
     * button or on one half; between two halves the measured columns, which differ with the one held */
    rect whole = ui_popup_button(0, 0), band = {whole.x, whole.y - 1, whole.w, whole.h + 1};
    ui_sprite_box(held >= 0 && !left ? ART_DLG_BUTTON_DOWN : ART_DLG_BUTTON, band);
    if (left && held >= 0) {
        rect b = ui_popup_button(1, held);
        ui_sprite_part(ART_DLG_BUTTON_DOWN, band, b.x, b.x + b.w);
    }
    for (int i = left ? 0 : 1; i < 2; i++) {
        rect b = left ? ui_popup_button(1, i) : whole;
        ui_sprite_part((i == 0 ? ART_NG_DLG_BLUE : removes ? ART_NG_DLG_CORAL : ART_NG_DLG_TEAL) + (held == i), band,
                       b.x, b.x + b.w);
    }
    if (left)
        ui_sprite(held == 0 ? ART_DLG_DIVIDER_LEFT_DOWN : held == 1 ? ART_DLG_DIVIDER_RIGHT_DOWN : ART_DLG_DIVIDER,
                  whole.x + whole.w / 2 - 20, whole.y);
    for (int i = left ? 0 : 1; i < 2; i++) {
        rect b = left ? ui_popup_button(1, i) : whole;
        float base = ui_baseline(b.y + 2, b.h - 4, DLG_BTN_PX);
        ui_text(i ? right : left, b.x + b.w / 2, base, DLG_BTN_PX, held == i ? UI_COLOR(COL_DLG_HELD_INK) : text_ink,
                UI_CENTER);
    }
    if (left && ring >= 0) {    /* square where the band meets the panel and the other half; the panel's corner below */
        const int corners[2][4] = {{0, 0, (int)(DLG_BTN_R_10 / 10), 0}, {0, 0, 0, (int)(DLG_BTN_R_10 / 10)}};
        ui_select_corners(ui_popup_button(1, ring), corners[ring]);
    }
}

/* The loader: a nearly clear tube of glass seen from the front as a ring, lit from above,
 * its light shade beneath (NG_LOADER_BODY); in it a light in the scheme's loader tone, a small bright core with a wide
 * soft glow and its tail, one image turned about the middle once per LOADER_MS, clockwise from the top
 * (NG_LOADER_LIGHT); over it the tube's fine light edges and the highlight along the top (NG_LOADER_FRONT), so the
 * light lies in the glass. The body and the front are drawn at once; the light, read smoothed, waits as a picture
 * does (order.h) until the front, which covers it, draws it first: the line under the loader follows the front. */
static void loader(float cx, float cy, u64 ms, float alpha)
{
    C2D_ImageTint tint;
    C2D_AlphaImageTint(&tint, alpha);
    const C2D_ImageTint *t = alpha < 1 ? &tint : NULL;
    rect box = {roundf(cx - LOADER_BOX / 2.0f), roundf(cy - LOADER_BOX / 2.0f), LOADER_BOX, LOADER_BOX};
    ui_sprite_draw(ART_NG_LOADER_BODY, box, -1e9f, 1e9f, t);
    ui_sprite_turned(ART_NG_LOADER_LIGHT, box.x + LOADER_BOX / 2.0f, box.y + LOADER_BOX / 2.0f,
                     TAU * (float)(ms % LOADER_MS) / LOADER_MS, t);
    ui_sprite_draw(ART_NG_LOADER_FRONT, box, -1e9f, 1e9f, t);
}

const look look_night = {
    .name = "night",
    .art = &ART_NIGHT,
    .draw = {
        .load = load,
        .unload = unload,
        .refresh = refresh,
        .ground = ground,
        .rule = rule,
        .card = card,
        .face = face,
        .select = selection,
        .bar = bar,
        .bar_label = bar_label,
        .popup_panel = popup_panel,
        .popup_buttons = popup_buttons,
        .loader = loader,
    },
    .m = {
        .dim_pct = NG_DIM_ALPHA,
        .dlg_h = DLG_H,
        .tab_face = ART_NG_TAB,
        .tab_face_down = ART_NG_TAB_DOWN,
        .tab_h = NG_TAB_H,
        .tab_icon_x = NG_TAB_ICON_X,
        .tab_icon_y = (NG_TAB_H - CORNER_ICON) / 2,
        .corner_h = CORNER_H,
        .corner_icon_x = CORNER_PAD,
        .corner_icon_y = (CORNER_H - CORNER_ICON) / 2,
        .help_size = HELP_FACE,
        .help_mark_y = (HELP_FACE - HELP_ICON) / 2,
        .help_mark_down = 1,
        .pane_rim_big = PANE_BIG_RIM,
        .pane_rim_small = PANE_SMALL_RIM,
        .pane_apart = 0,        /* the neighbours tuck under the middle pane's glass rim, at the slot positions */
    },
};
