/* Stock, the look of the console's own menus rebuilt from measurements (its art: look_stock.py). Its entries are
 * the drawing every screen had before there were looks, moved here unchanged (G1 0.3); another look may copy from
 * them. (G1 plan, B1) */
#include "kit.h"
#include "look.h"

#include <math.h>

#define TAU 6.2831853f

static void none(void)
{
}

static void refresh(int scheme, int pattern)
{
    (void)scheme;
    (void)pattern;
}

/* The screen's background from y0 to y1: that slice of its vertical gradient, wider than the screen for a view that
 * is zoomed; on the top screen the pattern over it. */
static void ground(float y0, float y1, float alpha)
{
    uint32_t a = ui_f.top ? SC(COL_TOP_BG_A) : SC(COL_BOT_BG_A), b = ui_f.top ? SC(COL_TOP_BG_B) : SC(COL_BOT_BG_B);
    uint32_t c0 = ui_mix(a, b, y0 / SCREEN_H, alpha), c1 = ui_mix(a, b, y1 / SCREEN_H, alpha);
    ui_rect(-BLEED, y0, TOP_W + 2 * BLEED, y1 - y0, c0, c0, c1, c1);
    if (ui_f.top)               /* A1: the start screen's pattern moves behind every top screen, over all of it */
        ui_pattern(y0, y1, 1.0f);   /* every top screen begins with an atlas image before its letters (ui_head) */
}

/* A row of dots across the top screen inside its margins. */
static void rule(float y)
{
    ui_layer(DEPTH_DOTS);
    int n = (TOP_W - 2 * TOP_PAD - DOT) / DOT_STEP + 1;
    float x = TOP_PAD + (TOP_W - 2 * TOP_PAD - ((n - 1) * DOT_STEP + DOT)) / 2;
    for (int i = 0; i < n; i++)
        ui_sprite(ART_DOT, x + i * DOT_STEP, y);
}

static void card(rect r, int glass)
{
    ui_sprite_box(glass < 0 ? ART_CARD : ART_CARD_GLASS + glass, r);
}

static rect face(int kind, rect r, int state, int symbol)
{
    (void)symbol;
    ui_face_plain(kind, r, state);
    return r;
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

static void popup_panel(rect r)
{
    ui_sprite_box(ART_DLG, r);
}

static void popup_buttons(const char *left, const char *right, int held, int ring, int removes)
{
    (void)removes;
    uint32_t text_ink = UI_COLOR(SC(COL_DLG_INK));
    /* the band from one row above the button (the panel's row in its shadow); held: the held material on the whole
     * button or on one half; between two halves the measured columns, which differ with the one held */
    rect whole = ui_popup_button(0, 0), band = {whole.x, whole.y - 1, whole.w, whole.h + 1};
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
        float cx = b.x + b.w / 2, base = ui_baseline(b.y + 2, b.h - 4, DLG_BTN_PX);
        uint32_t shadow = down ? C2D_Color32(0x14, 0x14, 0x14, DLG_LABEL_DARK_ALPHA * 255 / 100)
                               : C2D_Color32((SC(COL_DLG_LABEL_LIGHT) >> 16) & 255, (SC(COL_DLG_LABEL_LIGHT) >> 8) & 255,
                                             SC(COL_DLG_LABEL_LIGHT) & 255, DLG_LABEL_LIGHT_ALPHA * 255 / 100);
        ui_text(label, cx, base + DLG_LABEL_SHADOW_DY, DLG_BTN_PX, shadow, UI_CENTER);
        ui_text(label, cx, base, DLG_BTN_PX, down ? UI_COLOR(COL_DLG_HELD_INK) : text_ink, UI_CENTER);
    }
    if (left && ring >= 0) {    /* square where the band meets the panel and the other half; the panel's corner below */
        const int corners[2][4] = {{0, 0, (int)(DLG_BTN_R_10 / 10), 0}, {0, 0, 0, (int)(DLG_BTN_R_10 / 10)}};
        ui_select_corners(ui_popup_button(1, ring), corners[ring]);
    }
}

/* The selection's corners from the corner texture. */
static void brackets(rect r, const int radius[4])
{
    ui_select_brackets(r, radius, -1);
}

/* The loader (Shard's): eight beads LOADER_BEAD across, LOADER_BEAD_R from the middle, grooves in the
 * screen, the lit one in the scheme's loader tone (COL_LOADER) over each by how long ago the light was on it, in
 * rounds of LOADER_MS: bright then, fading until LOADER_RISE_PCT before it comes again, brightening from there; the
 * same curve for every bead, so the light glides round. */
static void loader(float cx, float cy, u64 ms, float alpha)
{
    float t = (float)(ms % LOADER_MS) / LOADER_MS, rise = LOADER_RISE_PCT / 100.0f;
    C2D_ImageTint tint;
    for (int i = 0; i < LOADER_DOTS; i++) {
        float a = TAU * i / LOADER_DOTS - TAU / 4;      /* from the top, clockwise */
        float since = fmodf(1.0f + t - (float)i / LOADER_DOTS, 1.0f), glow;
        if (since < 1 - rise) {
            glow = 1 - since / (1 - rise);
            glow *= glow;
        } else {
            glow = (since - (1 - rise)) / rise;
            glow = glow * glow * (3 - 2 * glow);
        }
        rect at = {roundf(cx + LOADER_BEAD_R * cosf(a) - LOADER_BEAD / 2.0f),
                   roundf(cy + LOADER_BEAD_R * sinf(a) - LOADER_BEAD / 2.0f), LOADER_BEAD, LOADER_BEAD};
        C2D_AlphaImageTint(&tint, alpha);
        ui_sprite_draw(ART_LOADER_BEAD, at, -1e9f, 1e9f, alpha < 1 ? &tint : NULL);
        if (glow > 0.01f) {
            C2D_AlphaImageTint(&tint, glow * alpha);
            ui_sprite_draw(ART_LOADER_BEAD_ON, at, -1e9f, 1e9f, &tint);
        }
    }
}

const look look_stock = {
    .name = "stock",
    .art = &ART_STOCK,
    .draw = {
        .load = none,
        .unload = none,
        .refresh = refresh,
        .ground = ground,
        .rule = rule,
        .card = card,
        .face = face,
        .select = brackets,
        .bar = bar,
        .bar_label = ui_engraved,
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
        .tab_icon_x = CORNER_PAD - 1,   /* the wrench one pixel left of and above the folder's symbol */
        .tab_icon_y = (CORNER_H - CORNER_ICON) / 2 - 1,
        .corner_h = CORNER_H,
        .corner_icon_x = CORNER_PAD,
        .corner_icon_y = (CORNER_H - CORNER_ICON) / 2,
        .help_size = HELP_FACE,
        .help_mark_y = (HELP_FACE - HELP_ICON) / 2,
        .help_mark_down = 1,
        .pane_rim_big = PANE_BIG_RIM,
        .pane_rim_small = PANE_SMALL_RIM,
        .pane_apart = SWARM_STOCK_APART,    /* its frame a card, the neighbours clear of it */
    },
};
