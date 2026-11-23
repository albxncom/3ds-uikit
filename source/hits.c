/* Where the buttons drawn by ui.c take touches: plain C on ui_layout.h, so the host tests drive an application's input with
 * the same rectangles the console uses. */
#include "ui.h"

int ui_hit(rect r, int x, int y)
{
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

rect ui_bar_rect(void)
{
    return (rect){0, BAR_Y, TAB_W, BAR_H};
}

rect ui_bar_wide_rect(void)
{
    return (rect){0, BAR_Y, BOTTOM_W, BAR_H};
}

/* As art.py's tabs() draws them, tested at the pixel's centre: from the screen's edge to TAB_EDGE_10, from BAR_Y down,
 * the inner upper corner a quarter ellipse TAB_RX across and TAB_RY high; the right tab is the left one mirrored. */
int ui_tab_hit(int right, int x, int y)
{
    float px = (right ? BOTTOM_W - 1 - x : x) + 0.5f, py = y + 0.5f, edge = TAB_EDGE_10 / 10.0f;
    if (px < 0 || px > edge || py < BAR_Y || py > SCREEN_H)
        return 0;
    float dx = (px - (edge - TAB_RX)) / TAB_RX, dy = (py - (BAR_Y + TAB_RY)) / TAB_RY;
    return dx <= 0 || dy >= 0 || dx * dx + dy * dy <= 1;
}

rect ui_help_rect(void)
{
    float cx = HELP_X + HELP_SIZE / 2.0f, cy = HELP_Y + HELP_SIZE / 2.0f;
    return (rect){cx - HELP_HIT / 2.0f, cy - HELP_HIT / 2.0f, HELP_HIT, HELP_HIT};
}

rect ui_popup_button(int two, int right)
{
    rect b = {DLG_X, DLG_Y + DLG_H - DLG_BTN_H, DLG_W, DLG_BTN_H};
    if (two) {
        b.w = DLG_W / 2.0f;
        b.x += right ? b.w : 0;
    }
    return b;
}

/* The settings button on the start screen: the corner button at its smallest. */
rect ui_settings_rect(void)
{
    return (rect){0, 0, CORNER_W, CORNER_H};
}
