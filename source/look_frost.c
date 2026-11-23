/* Frost, frosted glass over the colour schemes, made for the Shard icons (its art: look_frost.py; its numbers:
 * look_frost.h). Its glass is drawn in the delivered sprites' places, so most of the look is the atlas. What is drawn
 * another way is here: the pattern as every screen's ground and, blurred (the ghost, a texture of its own), under
 * every pane; the strips along the top screen's edges in place of the dotted lines, and the bar as one strip; the
 * popup's buttons as lozenges on its panel. Ported from the branch ds-glass-frost (ui.c's Frost block, 22078ce).
 *
 * The drawing rule (kit.h): the ghost is a ground, drawn at once over a glass's face, and the letters on that glass
 * come after it; so a glass is drawn in three layers, its body, the ghost, its rim (look_frost.py, rim_of: its top
 * rows, an atlas image with real pixels), the rim's rows being ones the ghost never reaches. */
#include "kit.h"
#include "look.h"
#include "look_frost.h"

#include <math.h>

#define TAU 6.2831853f

/* the pattern's tile as it shows through glass (look_frost.py, ghost_tile): made in load, unpacked with the
 * pattern's own (refresh) */
static C3D_Tex ghost_tex;
static int ghost_made, ghost_ok;

static void load(void)
{
    ghost_made = C3D_TexInit(&ghost_tex, PATTERN_TILE, PATTERN_TILE, GPU_RGBA8);
    if (ghost_made) {
        C3D_TexSetFilter(&ghost_tex, GPU_LINEAR, GPU_LINEAR);
        C3D_TexSetWrap(&ghost_tex, GPU_REPEAT, GPU_REPEAT);
    }
    ghost_ok = 0;
}

static void unload(void)
{
    if (ghost_made)
        C3D_TexDelete(&ghost_tex);
    ghost_made = ghost_ok = 0;
}

static void refresh(int scheme, int pattern)
{
    ghost_ok = ghost_made && pattern >= 0 &&
               ui_unpack(&ghost_tex, ART_FROST_GHOST_Z[scheme < 0 ? 0 : scheme][pattern],
                         ART_FROST_GHOST_Z_SIZE[scheme < 0 ? 0 : scheme][pattern]);
}

/* The ghost over x, y, w, h of a glass drawn in the current view, each pixel showing the tile where the ground's
 * pattern is for this eye (ui_pattern_over); pct: how much of it this glass lets through. */
static void ghost(float x, float y, float w, float h, int pct)
{
    ui_pattern_over(ghost_ok ? &ghost_tex : NULL, x, y, w, h, pct / 100.0f);
}

/* The glass the pattern shows through: its corners' radius, how much of its side shows below its face
 * (look_frost.py, lip_of; 0: a thin pane), whether it is held down (the face sunk by that much), how much it lets
 * through, and its rim (look_frost.py, rim_of). */
static const struct {
    short id, rim;
    signed char rad, lip, down, pct;
} FROST_GLASS[] = {
    {ART_TILE, ART_FROST_RIM_TILE, TILE_R, FROST_LIP_TILE, 0, 100},
    {ART_TILE_DOWN, ART_FROST_RIM_TILE_DOWN, TILE_R, FROST_LIP_TILE, 1, 100},
    {ART_DTILE, ART_FROST_RIM_DTILE, TILE_R, FROST_LIP_TILE, 0, 100},
    {ART_DTILE_DOWN, ART_FROST_RIM_DTILE_DOWN, TILE_R, FROST_LIP_TILE, 1, 100},
    {ART_WIDE, ART_FROST_RIM_WIDE, TILE_R, FROST_LIP_TILE, 0, 100},
    {ART_WIDE_DOWN, ART_FROST_RIM_WIDE_DOWN, TILE_R, FROST_LIP_TILE, 1, 100},
    {ART_CHOICE, ART_FROST_RIM_CHOICE, TILE_R, FROST_LIP_TILE, 0, 100},
    {ART_CHOICE_DOWN, ART_FROST_RIM_CHOICE_DOWN, TILE_R, FROST_LIP_TILE, 1, 100},
    {ART_ALT, ART_FROST_RIM_ALT, GO_R, FROST_LIP_TILE, 0, 100},
    {ART_ALT_DOWN, ART_FROST_RIM_ALT_DOWN, GO_R, FROST_LIP_TILE, 1, 100},
    /* green glass: the white shapes would glare */
    {ART_GO, ART_FROST_RIM_GO, GO_R, FROST_LIP_TILE, 0, 28},
    {ART_GO_DOWN, ART_FROST_RIM_GO_DOWN, GO_R, FROST_LIP_TILE, 1, 28},
    {ART_ROW, ART_FROST_RIM_ROW, ROW_R, FROST_LIP_ROW, 0, 100},
    {ART_ROW_DOWN, ART_FROST_RIM_ROW_DOWN, ROW_R, FROST_LIP_ROW, 1, 100},
    {ART_FOLDER_ROW, ART_FROST_RIM_FOLDER_ROW, ROW_R, FROST_LIP_ROW, 0, 100},
    {ART_FOLDER_ROW_DOWN, ART_FROST_RIM_FOLDER_ROW_DOWN, ROW_R, FROST_LIP_ROW, 1, 100},
    {ART_SET_ROW, ART_FROST_RIM_SET_ROW, ROW_R, FROST_LIP_ROW, 0, 100},
    {ART_SET_ROW_DOWN, ART_FROST_RIM_SET_ROW_DOWN, ROW_R, FROST_LIP_ROW, 1, 100},
    {ART_TILE_OFF, ART_FROST_RIM_TILE_OFF, TILE_R, 0, 0, 100},
    {ART_ROW_OFF, ART_FROST_RIM_ROW_OFF, ROW_R, 0, 0, 100},
    {ART_CARD, ART_FROST_RIM_CARD, CARD_R, 0, 0, 100},
    {ART_CARD_GLASS_SHARD, ART_FROST_RIM_CARD_GLASS_SHARD, CARD_R, 0, 0, 100},
};

/* After the body of the glass id was drawn on r: the pattern under it on its face up to FROST_RIM px from the
 * face's edge, then its rim. A quad has square corners and the face round ones, so the first rows from above and the
 * last ones from below are narrower bands, each as wide as the round corner leaves at its outermost row. */
static void glass_over(int id, rect r)
{
    unsigned k = 0;
    while (k < sizeof FROST_GLASS / sizeof *FROST_GLASS && FROST_GLASS[k].id != id)
        k++;
    if (k == sizeof FROST_GLASS / sizeof *FROST_GLASS)
        return;
    int lip = FROST_GLASS[k].lip, down = FROST_GLASS[k].down, pct = FROST_GLASS[k].pct;
    float in = FROST_RIM, ri = FROST_GLASS[k].rad - down - in;
    float x = roundf(r.x) + down + in, w = roundf(r.w) - 2 * (down + in);
    float y = roundf(r.y) + (down ? lip - 1 : 0) + in, h = roundf(r.h) - (down ? lip - 1 : lip) - 2 * in;
    static const float band[] = {1, 3, 6};
    for (unsigned b = 0; b < sizeof band / sizeof *band && band[b] < ri; b++) {
        float from = band[b], to = b + 1 < sizeof band / sizeof *band && band[b + 1] < ri ? band[b + 1] : ri;
        float cut = ceilf(ri - sqrtf(ri * ri - (ri - from) * (ri - from)));
        ghost(x + cut, y + from, w - 2 * cut, to - from, pct);
        ghost(x + cut, y + h - to, w - 2 * cut, to - from, pct);
    }
    ghost(x, y + ri, w, h - 2 * ri, pct);
    ui_sprite_draw(FROST_GLASS[k].rim, (rect){r.x, r.y, r.w, ui_eh(FROST_GLASS[k].rim)}, -1e9f, 1e9f, NULL);
}

/* The screen's background from y0 to y1: that slice of its vertical gradient, wider than the screen for a view that
 * is zoomed, and the pattern over it on both screens (the ground the glass stands on). */
static void ground(float y0, float y1, float alpha)
{
    uint32_t a = ui_f.top ? SC(COL_TOP_BG_A) : SC(COL_BOT_BG_A), b = ui_f.top ? SC(COL_TOP_BG_B) : SC(COL_BOT_BG_B);
    uint32_t c0 = ui_mix(a, b, y0 / SCREEN_H, alpha), c1 = ui_mix(a, b, y1 / SCREEN_H, alpha);
    ui_rect(-BLEED, y0, TOP_W + 2 * BLEED, y1 - y0, c0, c0, c1, c1);
    ui_pattern(y0, y1, alpha);
}

/* A strip of glass across the top screen in place of a dotted line at y: above it for the header's line, below it
 * for the footer's; none for the line a screen without a header has at its top. As the bar, it stands on the
 * screen's edge: not zoomed with the content. */
static void rule(float y)
{
    if (ui_f.discard || y <= HERO_DOTS_Y)
        return;
    int low = y > SCREEN_H / 2;
    rect r = {-BLEED, low ? y + FROST_STRIP_GAP : 0, TOP_W + 2 * BLEED,
              low ? SCREEN_H - y - FROST_STRIP_GAP : y - FROST_STRIP_GAP};
    C3D_Mtx content;
    ui_edge_begin(&content);
    ui_sprite_box(low ? ART_FROST_STRIP_LOW : ART_FROST_STRIP_TOP, r);
    ghost(r.x, r.y + (low ? FROST_STRIP_RIM : 0), r.w, r.h - FROST_STRIP_RIM, 100);
    if (low)
        ui_sprite_draw(ART_FROST_RIM_FROST_STRIP_LOW, (rect){r.x, r.y, r.w, FROST_STRIP_RIM}, -1e9f, 1e9f, NULL);
    else
        ui_sprite_draw(ART_FROST_RIM_FROST_STRIP_TOP, (rect){r.x, r.y + r.h - FROST_STRIP_RIM, r.w, FROST_STRIP_RIM},
                       -1e9f, 1e9f, NULL);
    ui_edge_end(&content);
}

static void card(rect r, int glass)
{
    int id = glass < 0 ? ART_CARD : ART_CARD_GLASS + glass;
    ui_sprite_box(id, r);
    glass_over(id, r);
}

/* The face of the glass on r, for what is laid out on it: a thick glass's face ends above the box's lower edge and,
 * held, lies lower (look_frost.py, thick and sunk; a caller adds its own pixel when held); a greyed tile or row is a
 * thin pane. */
static rect face(int kind, rect r, int state, int symbol)
{
    static const int faces[][3] = {     /* ui_face_plain's: plain, held, greyed */
        [FACE_TILE] = {ART_TILE, ART_TILE_DOWN, ART_TILE_OFF},
        [FACE_CHOICE] = {ART_CHOICE, ART_CHOICE_DOWN, ART_CHOICE},
        [FACE_ROW] = {ART_ROW, ART_ROW_DOWN, ART_ROW_OFF},
        [FACE_FOLDER] = {ART_FOLDER_ROW, ART_FOLDER_ROW_DOWN, ART_FOLDER_ROW},
        [FACE_GO] = {ART_GO, ART_GO_DOWN, ART_GO},
        [FACE_ALT] = {ART_ALT, ART_ALT_DOWN, ART_ALT},
        [FACE_DTILE] = {ART_DTILE, ART_DTILE_DOWN, ART_DTILE},
        [FACE_WIDE] = {ART_WIDE, ART_WIDE_DOWN, ART_WIDE},
        [FACE_SET] = {ART_SET_ROW, ART_SET_ROW_DOWN, ART_SET_ROW},
    };
    (void)symbol;
    ui_face_plain(kind, r, state);
    glass_over(state & UI_DOWN ? faces[kind][1] : state & UI_OFF ? faces[kind][2] : faces[kind][0], r);
    int lip = kind == FACE_ROW || kind == FACE_FOLDER || kind == FACE_SET ? FROST_LIP_ROW : FROST_LIP_TILE;
    if (state & UI_DOWN)
        return (rect){r.x, r.y + lip - 2, r.w, r.h - (lip - 1)};
    if ((state & UI_OFF) && (kind == FACE_TILE || kind == FACE_ROW))
        return r;
    return (rect){r.x, r.y, r.w, r.h - lip};
}

/* The selection's breathing corners around r, Stock's shapes and pulse in the scheme's tone, from Frost's atlas
 * (look_frost.py, corners: every corner art.py draws, in its order). */
_Static_assert(ART_FROST_CORNER_R16_T8_BR_OUT - ART_FROST_CORNER_R0_T3_TL_IN ==
                   ART_CORNER_R16_T8_BR_OUT - ART_CORNER_R0_T3_TL_IN,
               "look_frost.py draws every corner art.py does, in the same order");

static void brackets(rect r, const int radius[4])
{
    ui_select_brackets(r, radius, ART_FROST_CORNER_R0_T3_TL_IN);
}

/* The bottom screen's bar: one strip of glass across the screen whatever parts are asked for; held (0 the left tab's
 * share, 1 the right one's, 2 all of it, -1 none) that share is lit from inside. */
static void bar(int parts, int held)
{
    (void)parts;
    float right = BOTTOM_W - (TAB_W + BAR_SHADOW);
    ui_sprite(held == 2 ? ART_BAR_DOWN : ART_BAR, 0, BAR_Y);
    if (held == 0)
        ui_sprite(ART_TAB_L_DOWN, 0, BAR_Y);
    if (held == 1)
        ui_sprite(ART_TAB_R_DOWN, right, BAR_Y);
    ghost(0, BAR_Y + FROST_STRIP_RIM, BOTTOM_W, BAR_H - FROST_STRIP_RIM, 100);
    ui_sprite(held == 2 ? ART_FROST_RIM_BAR_DOWN : ART_FROST_RIM_BAR, 0, BAR_Y);
    if (held == 0)
        ui_sprite(ART_FROST_RIM_TAB_L_DOWN, 0, BAR_Y);
    if (held == 1)
        ui_sprite(ART_FROST_RIM_TAB_R_DOWN, right, BAR_Y);
}

/* On glass a label is ink: nothing is engraved. */
static void bar_label(const char *s, float x, float baseline, int px, int align, int down)
{
    ui_text(s, x, baseline, px, down ? UI_COLOR(SC(COL_FROST_LABEL_HELD)) : UI_COLOR(SC(COL_FROST_LABEL)), align);
}

/* The popup's panel, FROST_DLG_GROW taller below than the panel laid out, as its buttons are taller. */
static void popup_panel(rect r)
{
    ui_sprite_box(ART_DLG, (rect){r.x, r.y, r.w, r.h + FROST_DLG_GROW});
}

/* The popup's buttons: a lozenge of glass in each half of the band (or one in the whole band), FROST_DLG_BTN_UP
 * above the band's top; what is touched stays the band (hits.c). */
static rect popup_button(rect part)
{
    return (rect){part.x + FROST_DLG_BTN_IN, part.y - FROST_DLG_BTN_UP, part.w - 2 * FROST_DLG_BTN_IN, FROST_DLG_BTN_H};
}

static void popup_buttons(const char *left, const char *right, int held, int ring, int removes)
{
    (void)removes;
    for (int i = left ? 0 : 1; i < 2; i++) {
        rect b = popup_button(left ? ui_popup_button(1, i) : ui_popup_button(0, 0));
        int down = left ? held == i : held >= 0;
        ui_sprite_box(down ? ART_FROST_DLG_BTN_DOWN : ART_FROST_DLG_BTN, b);
        /* the label on the face: the face ends FROST_LIP_ROW above the box, and held it lies that much lower */
        ui_text(i ? right : left, b.x + b.w / 2,
                ui_baseline(b.y + (down ? FROST_LIP_ROW - 1 : 0), b.h - FROST_LIP_ROW + (down ? 1 : 0), DLG_BTN_PX),
                DLG_BTN_PX, UI_COLOR(down ? SC(COL_PRESS_INK) : SC(COL_TILE_INK)), UI_CENTER);
    }
    if (left && ring >= 0) {
        const int round[4] = {ROW_R, ROW_R, ROW_R, ROW_R};
        ui_select_corners(popup_button(ui_popup_button(1, ring)), round);
    }
}

/* The loader: eight small panes of frosted glass set like a dial (look_frost.py,
 * loader_dial: one image of the eight at rest round LOADER_BOX) and over each its lit image (loader_lit: the pane in
 * the scheme's loader tone, FROST_LOADER_LIT px square at its place) as solid as Stock's beads glow (look_stock.c,
 * loader): the light goes round clockwise from the top once in LOADER_MS, bright on a pane when it comes, fading
 * behind it and brightening LOADER_RISE_PCT before it comes again. All from the atlas at whole pixels. */
static void loader(float cx, float cy, u64 ms, float alpha)
{
    float t = (float)(ms % LOADER_MS) / LOADER_MS, rise = LOADER_RISE_PCT / 100.0f;
    rect box = {roundf(cx - LOADER_BOX / 2.0f), roundf(cy - LOADER_BOX / 2.0f), LOADER_BOX, LOADER_BOX};
    C2D_ImageTint tint;
    C2D_AlphaImageTint(&tint, alpha);
    ui_sprite_draw(ART_FROST_LOADER_DIAL, box, -1e9f, 1e9f, alpha < 1 ? &tint : NULL);
    for (int i = 0; i < LOADER_DOTS; i++) {
        float since = fmodf(1.0f + t - (float)i / LOADER_DOTS, 1.0f), glow;
        if (since < 1 - rise) {
            glow = 1 - since / (1 - rise);
            glow *= glow;
        } else {
            glow = (since - (1 - rise)) / rise;
            glow = glow * glow * (3 - 2 * glow);
        }
        if (glow <= 0.01f)
            continue;
        /* the pane's middle FROST_LOADER_DIAL_R2 / 2 from the box's, from the top, clockwise; the lit image round it
         * at whole pixels, a quarter pixel added so that no middle lies on a pixel's edge (look_frost.py, dial_at) */
        float a = TAU * i / LOADER_DOTS - TAU / 4, r = FROST_LOADER_DIAL_R2 / 2.0f;
        rect at = {box.x + floorf(LOADER_BOX / 2.0f + r * cosf(a) + 0.25f) - FROST_LOADER_LIT / 2,
                   box.y + floorf(LOADER_BOX / 2.0f + r * sinf(a) + 0.25f) - FROST_LOADER_LIT / 2, FROST_LOADER_LIT,
                   FROST_LOADER_LIT};
        C2D_AlphaImageTint(&tint, glow * alpha);
        ui_sprite_draw(ART_FROST_LOADER_LIT_0 + i, at, -1e9f, 1e9f, &tint);
    }
}

const look look_frost = {
    .name = "frost",
    .art = &ART_FROST,
    .draw = {
        .load = load,
        .unload = unload,
        .refresh = refresh,
        .ground = ground,
        .rule = rule,
        .card = card,
        .face = face,
        .select = brackets,
        .bar = bar,
        .bar_label = bar_label,
        .popup_panel = popup_panel,
        .popup_buttons = popup_buttons,
        .loader = loader,
    },
    .m = {
        .dim_pct = DIM_ALPHA,
        .dlg_h = DLG_H,
        .tab_face = ART_CORNER,
        .tab_face_down = ART_CORNER_DOWN,
        .tab_h = CORNER_H,
        .tab_icon_x = CORNER_PAD + FROST_TAB_ICON_DX,   /* on Frost's taller face, with even air round it */
        .tab_icon_y = FROST_TAB_ICON_Y,
        .corner_h = CORNER_H,
        .corner_icon_x = CORNER_PAD,
        .corner_icon_y = (CORNER_H - CORNER_ICON) / 2,
        .help_size = HELP_FACE,             /* its mark centred on the face with room round it */
        .help_mark_y = FROST_HELP_MARK_Y,
        .help_mark_down = FROST_HELP_MARK_DOWN,
        .pane_rim_big = PANE_BIG_RIM,
        .pane_rim_small = PANE_SMALL_RIM,
        .pane_apart = 0,                    /* the panes' rims are glass, a neighbour tucks behind as a pane */
    },
};
