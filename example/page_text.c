/* Text: the top screen shows sizes, the three alignments, a measured width and lines cut to fit at their end or at
 * their beginning; the bottom screen a paragraph wrapped to a width the wide tile switches, shrinking down to a
 * smallest size and cut with "..." when it still does not fit. */
#include <stdio.h>
#include "gallery.h"
#include "widget.h"

#define PARA "A paragraph is wrapped at the width it is given. When it takes more than four lines it is set smaller, "\
             "down to the smallest size it may have, and when even that is not enough its last line ends in dots."
#define BLOCK_Y 10
/* the wide tile at its face's height, the cursor's corners round it (11 px beyond) above the bar's shadow */
#define WIDE_Y (BAR_Y - BAR_SHADOW - SELECT_OUT - SELECT_AMP - SELECT_W - WIDE_H)
static const int SIZES[] = {10, 14, 18, 22, 26};
static int narrow;
static ui_block block;

static rect target(int i)
{
    (void)i;
    return (rect){PAD, WIDE_Y, BOTTOM_W - 2 * PAD, WIDE_H};
}

static void press(int i)
{
    (void)i;
    sfx(SFX_CONFIRM);
    narrow = !narrow;
}

static void top(void)
{
    char line[64];
    float y = HEAD_DOTS_Y + 6, x = TOP_PAD;
    ui_ground_flat();           /* this screen sets its text on the ground's light without depth */
    ui_head("Text", narrow ? "Narrow" : "Wide");    /* an atlas image first: no text straight on the ground */
    for (unsigned k = 0; k < sizeof SIZES / sizeof *SIZES; k++) {
        snprintf(line, sizeof line, "%d px", SIZES[k]);
        ui_text(line, x, ui_baseline(y, SIZES[k] + 4, SIZES[k]), SIZES[k], INK, UI_LEFT);
        x += ui_text_width(line, SIZES[k]) + 14;
    }
    y += 30;
    ui_text("Left", TOP_PAD, ui_baseline(y, 20, 14), 14, INK, UI_LEFT);
    ui_text("Centre", TOP_W / 2, ui_baseline(y, 20, 14), 14, INK, UI_CENTER);
    ui_text("Right", TOP_W - TOP_PAD, ui_baseline(y, 20, 14), 14, INK, UI_RIGHT);
    y += 28;
    snprintf(line, sizeof line, "This line is %.0f px wide at 14 px", ui_text_width("This line is 000 px wide at 14 px", 14));
    ui_text(line, TOP_PAD, ui_baseline(y, 20, 14), 14, INK, UI_LEFT);
    y += 22;
    ui_text_fit("A line too long for its room is cut at its end, as this one is", TOP_PAD,
                ui_baseline(y, 20, 14), 260, 14, INK, UI_LEFT, 0);
    y += 22;
    ui_text_fit("sdmc:/a/path/too/long/for/its/room/is/cut/at/its/beginning.txt", TOP_PAD, ui_baseline(y, 20, 14), 260,
                14, MUTED, UI_LEFT, 1);
    ui_foot(FOOT_DOTS_Y, -1, "All text is the console's system font", MUTED, 0, NULL);
}

static void bottom(int held)
{
    char note[64];
    float w = narrow ? 110 : BOTTOM_W - 2 * PAD;
    ui_wide(FACE_WIDE, target(0), held == 0 ? UI_DOWN : 0, ART_HELP_MARK, narrow ? "Wider" : "Narrower", LABEL_PX + 2,
            LABEL_GAP + 4, TILE_INK);   /* first: an atlas image before any text on the ground */
    int fit = ui_wrap(&block, PARA, w, 15, 10, 4, 0, 19);
    ui_block_draw(&block, BOTTOM_W / 2, BLOCK_Y, TILE_INK, UI_CENTER);
    snprintf(note, sizeof note, "%.0f px high, %s", ui_block_height(&block), fit ? "all of it fits" : "cut");
    ui_text(note, BOTTOM_W / 2, ui_baseline(WIDE_Y - 26, 20, 13), 13, MUTED, UI_CENTER);
}

const page page_text = {.name = "Text", .symbol = ART_KEEP, .count = 1, .columns = 1, .target = target,
                        .press = press, .top = top, .bottom = bottom};
