/* Buttons: the green button shows the bar's next kind (one tab, two, B and Y, the right tab alone), the second button
 * greys the green one out and back, the round help button swaps the top screen's text. */
#include <stdio.h>
#include "gallery.h"
#include "widget.h"

#define BARS 4
static const char *const BAR_NAMES[BARS] = {"One tab", "Two tabs", "B and Y", "Right tab alone"};
static int bar_kind, go_off, help, right_presses;

/* 0 the help button (top right), 1 the green button, 2 the second one, side by side under it: a button's face is
 * GO_H high whatever its rectangle says, and two of them stacked do not fit above the bar */
static rect target(int i)
{
    float w = (BOTTOM_W - 2 * PAD - GAP) / 2;
    if (i == 0)
        return ui_help_rect();
    return (rect){PAD + (i - 1) * (w + GAP), ui_help_low() + GAP, w, GO_H};
}

static void press(int i)
{
    if (i == 1 && go_off) {
        sfx(SFX_ERROR);
        return;
    }
    sfx(SFX_CONFIRM);
    if (i == 1)
        bar_kind = (bar_kind + 1) % BARS;
    else if (i == 2)
        go_off = !go_off;
    else if (i == 0)
        help = !help;
    else
        right_presses++;        /* the bar's right tab, or Y */
}

static void top(void)
{
    char note[40];
    ui_head("Buttons", BAR_NAMES[bar_kind]);
    card_text(help ? "The help button sits at the top right; its touch target is larger than its face. Press it again "
                     "for the other text."
                   : "The green button is for the main action, the second one for the other choice. The green one "
                     "shows the bar's next kind; the second greys the green one out. B goes back whatever the bar shows.");
    snprintf(note, sizeof note, "Right tab pressed %d times", right_presses);
    ui_foot(FOOT_DOTS_Y, -1, note, MUTED, 0, NULL);
}

static void bottom(int held)
{
    ui_go(target(1), (go_off ? UI_OFF : 0) | (held == 1 ? UI_DOWN : 0), ART_DOWNLOAD, "Next bar");
    ui_alt(target(2), held == 2 ? UI_DOWN : 0, ART_NO_MUSIC, go_off ? "Green on" : "Green off");
    ui_help(held == 0);
}

static void bar(int held)
{
    if (bar_kind == 0)
        ui_bar(UI_KEY_B, "Back", held == 0);
    else if (bar_kind == 1)
        ui_bar2("Back", "OK", held);
    else if (bar_kind == 2)
        ui_bar_y("Back", "Next", held);
    else
        ui_bar_right(UI_KEY_A, "OK", held == 1);
}

static int tabs(void)
{
    return bar_kind == 0 ? TAB_LEFT : bar_kind == 3 ? TAB_RIGHT : TAB_LEFT | TAB_RIGHT;
}

static void select_one(int i)
{
    ui_select(target(i), i == 0 ? HELP_SIZE / 2 : GO_R);
}

const page page_buttons = {.name = "Buttons", .symbol = ART_HELP_MARK, .count = 3, .columns = 2, .target = target,
                           .press = press, .top = top, .bottom = bottom, .right = "OK", .bar = bar,
                           .tabs = tabs, .select = select_one};
