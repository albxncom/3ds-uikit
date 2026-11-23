/* Prisma, the Glass design of ds-glass-prism behind the look boundary (its art: look_prism.py; its numbers:
 * look_prism.h). Light and angular, made for the Shard icons: a ground of quiet facets from pearl into the scheme's
 * tint, plates with cut corners and a spectral line, buttons as cut stones, hairlines with a lozenge at each end, and
 * the selection as the stone's bevel lit in the scheme's spectrum. (G1 plan, B1) */
#include "kit.h"
#include "look.h"
#include "look_prism.h"

#include <math.h>

#define TAU 6.2831853f
#define PRESS_INK UI_COLOR(SC(COL_PRESS_INK))

/* both screens' facets in one texture, the same in every scheme: made and unpacked when the look is chosen */
static C3D_Tex ground_tex;
static int ground_made, ground_ok;

static void load(void)
{
    ground_made = C3D_TexInit(&ground_tex, PRISM_GROUND_W, PRISM_GROUND_H, GPU_RGBA8);
    if (!ground_made)
        return;
    C3D_TexSetFilter(&ground_tex, GPU_NEAREST, GPU_NEAREST);
    ground_ok = ui_unpack(&ground_tex, ART_PRISM_GROUND_Z[0][0], ART_PRISM_GROUND_Z_SIZE[0][0]);
}

static void unload(void)
{
    if (ground_made)
        C3D_TexDelete(&ground_tex);
    ground_made = ground_ok = 0;
}

static void refresh(int scheme, int pattern)
{
    (void)scheme;
    (void)pattern;
}

static u32 tone(uint32_t colour, float alpha)
{
    return C2D_Color32((colour >> 16) & 255, (colour >> 8) & 255, colour & 255,
                       (u8)(alpha * ui_f.solid * 255.0f + 0.5f));
}

/* A rectangle whose colour runs from its left edge to its right one (down 0), or from its top to its bottom. */
static void run(float x, float y, float w, float h, u32 from, u32 to, int down)
{
    if (w <= 0 || h <= 0)
        return;
    ui_rect(x, y, w, h, from, down ? from : to, down ? to : from, to);
}

/* A sprite without caps at x, y, only its rows from y0 to y1 (the whole of it: ui_sprite_draw's one piece). */
static void band_sprite(int id, float x, float y, float y0, float y1, const C2D_ImageTint *tint)
{
    const art_sprite *s = ui_sprite_info(id);
    C3D_Tex *tex = ui_sprite_tex(id, 0);
    if (!s || !tex)
        return;
    float left = roundf(x) - s->pad[0], top = roundf(y) - s->pad[1];
    float from = fmaxf(y0, top), to = fminf(y1, top + s->h);
    if (to > from)
        ui_piece(tex, s->x, s->y + (from - top), s->w, to - from, left, from, s->w, to - from, tint);
}

/* The beam of split light behind the top screen's content (look_prism.py beam), a little behind the screen's plane,
 * on it when the screen asks for that (kit.h ui_f.ground_flat; such a screen draws no beam of its own): Prisma's
 * moving ground (A1), the light wandering along the screen's upper edge from where the start screen had it
 * PRISM_BEAM_SWEEP px to the left and back once per PRISM_BEAM_MS, eased at both ends, on whole pixels; on the
 * grounds' clock (kit.h ui_f.moving_ms), so it stands still in the probe build. */
static void beam(float y0, float y1)
{
    float t = (float)(ui_f.moving_ms % PRISM_BEAM_MS) / PRISM_BEAM_MS;
    float x = roundf(DROP_SUN_X - PRISM_BEAM_SWEEP * (0.5f - 0.5f * cosf(TAU * t)));
    float shift = ui_f.ground_flat ? 0 : roundf(DEPTH_DROPS_BEHIND * ui_f.slider / 2);
    C3D_Mtx content;
    C2D_ViewSave(&content);
    ui_view(1.0f, 0, 0, ui_f.eye ? -shift : shift);
    band_sprite(ART_DROP_SUN, x, DROP_SUN_Y, y0, y1, NULL);
    C2D_ViewRestore(&content);
}

/* The screen's ground: the scheme's two ground colours from the screen's upper left corner to its lower right one (a
 * plane, so the two triangles of the rectangle meet without a seam), at the sides the edge's colours for a view that
 * is zoomed; over it the screen's facets (white; the dark crystal takes a share of it), their last rows empty in the
 * texture and drawn from the atlas, so the ground ends on an atlas image (kit.h); on the top screen the beam. Only
 * the rows from y0 to y1 (the whole: 0, SCREEN_H). */
static void ground(float y0, float y1, float alpha)
{
    float w = ui_f.top ? TOP_W : BOTTOM_W, end = SCREEN_H - PRISM_GROUND_END;
    uint32_t a = SC(COL_TOP_BG_A), b = SC(COL_TOP_BG_B);
    u32 l0 = ui_mix(a, b, 0.5f * (y0 / SCREEN_H), alpha), r0 = ui_mix(a, b, 0.5f * (1.0f + y0 / SCREEN_H), alpha);
    u32 l1 = ui_mix(a, b, 0.5f * (y1 / SCREEN_H), alpha), r1 = ui_mix(a, b, 0.5f * (1.0f + y1 / SCREEN_H), alpha);
    ui_rect(-BLEED, y0, BLEED, y1 - y0, l0, l0, l1, l1);
    ui_rect(0, y0, w, y1 - y0, l0, r0, l1, r1);
    ui_rect(w, y0, BLEED, y1 - y0, r0, r0, r1, r1);
    if (ground_ok) {
        C2D_ImageTint tint;
        C2D_AlphaImageTint(&tint, alpha * (ui_f.scheme == ART_SCHEME_NACHT ? PRISM_FACETS_NIGHT / 100.0f : 1.0f));
        float n = PRISM_GROUND_W, row = ui_f.top ? 0 : PRISM_GROUND_ROW;
        Tex3DS_SubTexture sub = {(u16)w, (u16)(y1 - y0), 0, 1.0f - (row + y0) / n, w / n, 1.0f - (row + y1) / n};
        C2D_DrawParams params = {{0, y0, w, y1 - y0}, {0, 0}, 0, 0};
        ui_ground_image(&ground_tex, &sub, &params, &tint);
        band_sprite(ui_f.top ? ART_PRISM_GROUND_END_TOP : ART_PRISM_GROUND_END_BOTTOM, 0, end, y0, y1, &tint);
    }
    if (ui_f.top)
        beam(y0, y1);
}

static void rule(float y)
{
    ui_layer(DEPTH_DOTS);
    u32 ink = tone(SC(COL_DOTS), PRISM_RULE_ALPHA / 100.0f), none = tone(SC(COL_DOTS), 0);
    float x = TOP_PAD, w = TOP_W - 2 * TOP_PAD, gem = PRISM_GEM, in = gem + 2, fade = PRISM_RULE_FADE;
    y += DOT / 2;
    ui_sprite(ART_PRISM_GEM, x, y - (gem - 1) / 2);
    ui_sprite(ART_PRISM_GEM, x + w - gem, y - (gem - 1) / 2);
    run(x + in, y, fade, 1, none, ink, 0);
    run(x + in + fade, y, w - 2 * (in + fade), 1, ink, ink, 0);
    run(x + w - in - fade, y, fade, 1, ink, none, 0);
}

/* The spectral line along a plate's upper edge, from its cut corner on: the scheme's spectrum one tone into the
 * next, the last one fading out; a fainter row under it, so the line reads at the screen's size. */
static void spectrum(rect r)
{
    const uint32_t tones[] = {SC(COL_SHARD_PRISM_0), SC(COL_SHARD_PRISM_1), SC(COL_SHARD_PRISM_2),
                              SC(COL_SHARD_PRISM_3)};
    float x = roundf(r.x) + PRISM_PLATE_CUT, y = roundf(r.y), all = roundf(r.w * PRISM_SPECTRUM_PCT / 100.0f);
    float fade = roundf(all * PRISM_SPECTRUM_FADE_PCT / 100.0f), step = roundf((all - fade) / 3);
    for (int row = 0; row < 2; row++) {
        float k = row ? 0.30f : 1.0f;
        for (int i = 0; i < 3; i++)
            run(x + i * step, y + row, step, 1, tone(tones[i], k), tone(tones[i + 1], k), 0);
        run(x + 3 * step, y + row, fade, 1, tone(tones[3], k), tone(tones[3], 0), 0);
    }
}

/* A plate: the card's image and its spectral line; the glass card is the plate too (the glass is Shard's always). */
static void card(rect r, int glass)
{
    (void)glass;
    ui_sprite_box(ART_CARD, r);
    spectrum(r);
}

static rect face(int kind, rect r, int state, int symbol)
{
    (void)symbol;
    ui_face_plain(kind, r, state);
    return r;
}

/* The selection: the stone's bevel lit in the scheme's spectrum (Perle's: teal at the upper left, blue at the upper
 * right, violet at the lower right, rose at the lower left), each edge running from its one corner's tone to the
 * other's; the corners are images (their cut), the edges rectangles. A line on the outline and, fainter, the bevel
 * inside it, breathing once per SELECT_MS. big: a tile's or a button's cut, else a row's. */
static void lit(rect r, int big)
{
    const uint32_t tones[] = {SC(COL_SHARD_PRISM_0), SC(COL_SHARD_PRISM_1), SC(COL_SHARD_PRISM_3),
                              SC(COL_SHARD_PRISM_2)};
    float t = (float)(ui_f.ms % SELECT_MS) / SELECT_MS, out = 0.5f - 0.5f * cosf(TAU * t);
    float k = PRISM_RING_LOW / 100.0f + (1 - PRISM_RING_LOW / 100.0f) * out, in = k * PRISM_RING_BEVEL / 100.0f;
    float n = (big ? PRISM_CUT_BIG : PRISM_CUT_SMALL) + PRISM_RING_STUB, bevel = big ? PRISM_BEVEL_BIG : PRISM_BEVEL_SMALL;
    float x0 = roundf(r.x), y0 = roundf(r.y), x1 = roundf(r.x + r.w), y1 = roundf(r.y + r.h), line = PRISM_RING_LINE;
    int id = big ? ART_PRISM_RING_BIG_TL : ART_PRISM_RING_SMALL_TL;
    u32 full[4], faint[4];
    for (int c = 0; c < 4; c++)
        full[c] = tone(tones[c], k), faint[c] = tone(tones[c], in);
    C2D_ImageTint tint;
    C2D_AlphaImageTint(&tint, k * ui_f.solid);
    /* top and bottom, left and right: the line half outside the outline, the bevel inside it */
    run(x0 + n, y0 - line / 2, x1 - x0 - 2 * n, line, full[0], full[1], 0);
    run(x0 + n, y0 + line / 2, x1 - x0 - 2 * n, bevel - line / 2, faint[0], faint[1], 0);
    run(x0 + n, y1 - line / 2, x1 - x0 - 2 * n, line, full[2], full[3], 0);
    run(x0 + n, y1 - bevel, x1 - x0 - 2 * n, bevel - line / 2, faint[2], faint[3], 0);
    run(x0 - line / 2, y0 + n, line, y1 - y0 - 2 * n, full[0], full[2], 1);
    run(x0 + line / 2, y0 + n, bevel - line / 2, y1 - y0 - 2 * n, faint[0], faint[2], 1);
    run(x1 - line / 2, y0 + n, line, y1 - y0 - 2 * n, full[1], full[3], 1);
    run(x1 - bevel, y0 + n, bevel - line / 2, y1 - y0 - 2 * n, faint[1], faint[3], 1);
    ui_sprite_tinted(id, x0, y0, 1, 1, &tint);
    ui_sprite_tinted(id + 1, x1 - n, y0, 1, 1, &tint);
    ui_sprite_tinted(id + 2, x0, y1 - n, 1, 1, &tint);
    ui_sprite_tinted(id + 3, x1 - n, y1 - n, 1, 1, &tint);
}

static void selected(rect r, const int radius[4])
{
    lit(r, radius[0] >= TILE_R);
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

/* The popup's panel: a solid plate with its spectral line, PRISM_DLG_GROW px taller than DLG_H (the metrics). */
static void popup_panel(rect r)
{
    ui_sprite_box(ART_DLG, r);
    spectrum(r);
}

/* The popup's buttons: stones on the panel's lower end (the band PRISM_DLG_GROW px taller below than hits.c's); a
 * held stone lit, its label in the held ink without the copy; the ring lights the stone, not the band's half. */
static void popup_buttons(const char *left, const char *right, int held, int ring, int removes)
{
    (void)removes;
    int grow = PRISM_DLG_GROW;
    uint32_t text_ink = UI_COLOR(SC(COL_DLG_INK));
    rect whole = ui_popup_button(0, 0), band = {whole.x, whole.y - 1, whole.w, whole.h + 1 + grow};
    ui_sprite_box(held >= 0 && !left ? ART_DLG_BUTTON_DOWN : ART_DLG_BUTTON, band);
    if (left && held >= 0) {
        rect b = ui_popup_button(1, held);
        ui_sprite_part(ART_DLG_BUTTON_DOWN, band, b.x, b.x + b.w);
    }
    if (left)
        ui_sprite(held == 0 ? ART_DLG_DIVIDER_LEFT_DOWN : held == 1 ? ART_DLG_DIVIDER_RIGHT_DOWN : ART_DLG_DIVIDER,
                  whole.x + whole.w / 2 - 20, whole.y);
    for (int i = left ? 0 : 1; i < 2; i++) {
        rect b = left ? ui_popup_button(1, i) : whole;
        int down = held == i;
        const char *label = i ? right : left;
        float cx = b.x + b.w / 2, base = ui_baseline(b.y + 2, b.h - 4 + grow, DLG_BTN_PX);
        if (down) {
            ui_text(label, cx, base, DLG_BTN_PX, PRESS_INK, UI_CENTER);
            continue;
        }
        uint32_t shadow = C2D_Color32((SC(COL_DLG_LABEL_LIGHT) >> 16) & 255, (SC(COL_DLG_LABEL_LIGHT) >> 8) & 255,
                                      SC(COL_DLG_LABEL_LIGHT) & 255, DLG_LABEL_LIGHT_ALPHA * 255 / 100);
        ui_text(label, cx, base + DLG_LABEL_SHADOW_DY, DLG_BTN_PX, shadow, UI_CENTER);
        ui_text(label, cx, base, DLG_BTN_PX, text_ink, UI_CENTER);
    }
    if (left && ring >= 0) {
        rect b = ui_popup_button(1, ring);
        float l = ring ? PRISM_DLG_GAP / 2.0f : PRISM_DLG_SIDE, r = ring ? PRISM_DLG_SIDE : PRISM_DLG_GAP / 2.0f;
        lit((rect){b.x + l, b.y + PRISM_DLG_TOP, b.w - l - r, b.h + grow - PRISM_DLG_TOP - PRISM_DLG_LOW}, 1);
    }
}

/* How lit a part of the loader is that the light reached since of a round ago (0 to 1): bright then, fading until
 * LOADER_RISE_PCT before it comes again, brightening from there (Stock's curve for its beads). */
static float glow_since(float since)
{
    float rise = LOADER_RISE_PCT / 100.0f, glow;
    if (since < 1 - rise) {
        glow = 1 - since / (1 - rise);
        return glow * glow;
    }
    glow = (since - (1 - rise)) / rise;
    return glow * glow * (3 - 2 * glow);
}

/* The loader: a cut crystal in the scheme's loader tone (look_prism.py loader_sprites), its eight facets
 * catching the light one after the other once per LOADER_MS, from the upper left (where Prisma's light comes from)
 * clockwise (no spark). Atlas images only, the last one lit (the drawing rule: the line follows). */
static void loader(float cx, float cy, u64 ms, float alpha)
{
    float t = (float)(ms % LOADER_MS) / LOADER_MS, n = PRISM_LOADER;
    rect at = {roundf(cx - n / 2), roundf(cy - n / 2), n, n};
    C2D_ImageTint tint;
    C2D_AlphaImageTint(&tint, alpha);
    ui_sprite_draw(ART_PRISM_LOADER_STONE, at, -1e9f, 1e9f, alpha < 1 ? &tint : NULL);
    for (int i = 0; i < 8; i++) {
        float glow = glow_since(fmodf(1.0f + t - i / 8.0f, 1.0f));
        if (glow > 0.01f) {
            C2D_AlphaImageTint(&tint, glow * alpha);
            ui_sprite_draw(ART_PRISM_LOADER_LIT_0 + (7 + i) % 8, at, -1e9f, 1e9f, &tint);   /* 7: the upper left */
        }
    }
}

const look look_prism = {
    .name = "prism",
    .art = &ART_PRISM,
    .draw = {
        .load = load,
        .unload = unload,
        .refresh = refresh,
        .ground = ground,
        .rule = rule,
        .card = card,
        .face = face,
        .select = selected,
        .bar = bar,
        .bar_label = ui_engraved,
        .popup_panel = popup_panel,
        .popup_buttons = popup_buttons,
        .loader = loader,
    },
    .m = {
        .dim_pct = DIM_ALPHA,
        .dlg_h = DLG_H + PRISM_DLG_GROW,
        .tab_face = ART_CORNER,
        .tab_face_down = ART_CORNER_DOWN,
        .tab_h = PRISM_CORNER_H,
        .tab_icon_x = PRISM_CORNER_ICON_X,
        .tab_icon_y = PRISM_CORNER_ICON_Y,
        .corner_h = PRISM_CORNER_H,
        .corner_icon_x = PRISM_CORNER_ICON_X,
        .corner_icon_y = PRISM_CORNER_ICON_Y,
        .help_size = HELP_FACE,
        .help_mark_y = (HELP_FACE - HELP_ICON) / 2,
        .help_mark_down = 1,
        .pane_rim_big = PANE_BIG_RIM,
        .pane_rim_small = PANE_SMALL_RIM,
        .pane_apart = 0,                    /* the cut panes tuck behind the middle one's rim, at rest and moving */
        .ground_sun = 1,                    /* the beam is the ground's (beam), on every top screen */
    },
};
