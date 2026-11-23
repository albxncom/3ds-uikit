/* The top screen's parts and the boxes of the card's material: the header with its dotted line (or, on a screen
 * without one, the line alone at HERO_DOTS_Y), the card (plain, or glass in the chosen glass style), the footer with a
 * symbol, with a path cut at its beginning, or with a warning at its right end; on the bottom screen a box of facts
 * and the information box. The bar's right tab (or A) switches. A dotted line stands only where every look draws it
 * rightly: under the header, over the footer, or at the top of a screen without a header (docs/elements.md). */
#include <stdio.h>
#include "gallery.h"
#include "widget.h"

#define FACTS_Y 10
/* the information box's room: under the box of facts, down to INFO_AIR above the bar's shadow (as ui_info_box's own) */
#define INFO_TOP (FACTS_Y + 2 * FACT_PAD_Y + 3 * FACT_ROW_H + GAP)
#define INFO_LOW (BAR_Y - BAR_SHADOW - INFO_AIR)
#define INFO_TEXT "A problem or an empty state explains itself in a box like this one, never bare on the ground.\n" \
                  "Its text may break lines."
#define INFO_PATH "sdmc:/gallery/an/empty/folder"
#define KINDS 3
static int kind;

static void press(int i)
{
    (void)i;                    /* the right tab: the only target */
    sfx(SFX_CONFIRM);
    kind = (kind + 1) % KINDS;
}

static void top(void)
{
    static const char *const CARDS[KINDS] = {"A plain card, the footer with a symbol.",
                                             "No header, its line alone, and a glass card.",
                                             "A plain card, the footer with a warning at its right end."};
    char line[64];
    rect c = ui_card_rect();
    if (kind == 1)
        ui_dots(HERO_DOTS_Y);   /* a screen without a header: its line alone, first */
    else
        ui_head("Top screen", "Its parts");
    if (kind == 1)
        ui_card_glass(c);
    else
        ui_card(c);
    ui_text(CARDS[kind], c.x + CARD_PAD, ui_baseline(c.y + CARD_PAD, CARD_LH, CARD_PX), CARD_PX, INK, UI_LEFT);
    snprintf(line, sizeof line, "The information box below is %.0f px high.",
             ui_info_height(NULL, INFO_TEXT, INFO_PATH, INFO_LOW - INFO_TOP));
    ui_text_fit(line, c.x + CARD_PAD, ui_baseline(c.y + c.h / 2 + CARD_PAD, CARD_LH, CARD_PX), c.w - 2 * CARD_PAD,
                CARD_PX, INK, UI_LEFT, 0);
    if (kind == 0)
        ui_foot(FOOT_DOTS_Y, ART_WARN_FOOT, "A footer with a symbol", MUTED, 0, "Right");
    else if (kind == 1)
        ui_foot(FOOT_DOTS_Y, -1, "sdmc:/gallery/a/long/path/to/a/file/in/a/deep/folder.txt", MUTED, 1, NULL);
    else {
        ui_foot(FOOT_DOTS_Y, -1, "A footer", MUTED, 0, NULL);
        ui_foot_warn("Almost full");
    }
}

static void bottom(int held)
{
    static const ui_fact FACTS[] = {{ART_CHECK_ROW, "A box of facts, a row each", 0},
                                    {ART_DOT, "Each row a symbol and a line", 0},
                                    {ART_WARN_FOOT, "sdmc:/a/line/cut/at/its/beginning/when/it/is/a/path", 1}};
    (void)held;
    ui_facts(FACTS, 3, FACTS_Y);
    ui_info_box(NULL, INFO_TEXT, INFO_PATH, INFO_TOP, INFO_LOW);
}

const page page_top = {.name = "Top", .symbol = ART_SD, .top = top, .bottom = bottom, .press = press,
                       .right = "Switch"};
