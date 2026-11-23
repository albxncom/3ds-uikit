/* The start page: a tile a page (ui_wide on a settings row's face, the kit's shortest face that grows), the settings
 * button in the top left corner; its top screen says what the gallery is, its footer the drawing's peaks (from
 * ui_stats). */
#include <stdio.h>
#include "gallery.h"
#include "widget.h"

#define TILES 9
/* The tiles stand clear of the settings button and of the cursor's corners round it (SELECT_OUT + SELECT_AMP +
 * SELECT_W = 11 px beyond a tile), and those corners end above the bar's shadow: the tiles from 44 to 188. A label
 * stands whole beside its symbol: a page's name and symbol are chosen so (the long names with the small symbols). */
#define TILE_TOP 44
#define TILE_HIGH 44

static const page *const PAGES[TILES] = {&page_tiles, &page_buttons, &page_list, &page_text, &page_popups,
                                         &page_loader, &page_top, &page_pictures, &page_sound};
static int near_limit;

/* 0: the settings button; 1 to TILES: the pages */
static rect target(int i)
{
    if (i == 0)
        return ui_settings_rect();
    float w = (BOTTOM_W - 2 * PAD - 2 * GAP) / 3;
    i--;
    return (rect){PAD + (i % 3) * (w + GAP), TILE_TOP + (i / 3) * (TILE_HIGH + GAP), w, TILE_HIGH};
}

static void press(int i)
{
    sfx(SFX_CONFIRM);
    open_page(i == 0 ? &page_settings : PAGES[i - 1]);
}

static void top(void)
{
    char stats[200], line[100] = "";
    unsigned quads, quads_max, glyphs, glyphs_max;
    ui_head("3ds-uikit example", "Gallery");
    card_text("Every element of the kit, a page each. Touch a tile, or press the D-pad to show the cursor and A to "
              "open it. B goes back here, START leaves.");
    ui_stats(stats, sizeof stats);
    /* the log's line in short, the two peaks nearest a limit only (the frames' count changes every frame) */
    if (sscanf(stats, "%*u frames; quads a frame at most %u of %u, glyphs %u of %u", &quads, &quads_max, &glyphs,
               &glyphs_max) == 4)
        snprintf(line, sizeof line, "Peaks: quads %u of %u, glyphs %u of %u", quads, quads_max, glyphs, glyphs_max);
    ui_foot(FOOT_DOTS_Y, -1, line[0] ? line : stats, MUTED, 0, NULL);
    if (near_limit)
        ui_foot_warn("Near a limit");
}

/* the alarm is asked in the input, not while drawing (it answers once) */
static int own(const input *in)
{
    (void)in;
    near_limit |= ui_stats_alarm();
    return 0;
}

static void bottom(int held)
{
    for (int i = 1; i <= TILES; i++)
        ui_wide(FACE_SET, target(i), held == i ? UI_DOWN : 0, PAGES[i - 1]->symbol, PAGES[i - 1]->name, LABEL_PX,
                LABEL_GAP, TILE_INK);
    ui_settings_button(held == 0);
}

/* the settings button lies in the screen's corner: its outer corners square, its inner ones round */
static void select_one(int i)
{
    static const int corners[4] = {0, CORNER_R, 0, CORNER_R};
    if (i == 0)
        ui_select_corners(target(0), corners);
    else
        ui_select(target(i), TILE_R);
}

const page page_start = {.name = "Start", .symbol = ART_INSTALLED, .count = TILES + 1, .columns = 3,
                         .target = target, .press = press, .top = top, .bottom = bottom, .own = own,
                         .select = select_one};
