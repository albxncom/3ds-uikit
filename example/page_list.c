/* A list long enough to scroll (scroll.h): the stylus drags the rows and flings them, the scroll bar's thumb moves
 * them, the D-pad selects (its first press only selects the first row that shows whole) and A chooses; the corner
 * button over the rows goes back, one folder up. A touch on the list or the bar clears the selection. */
#include <stdio.h>
#include "gallery.h"
#include "touch.h"
#include "widget.h"

#define ROWS 40
#define LIST_TOP 42             /* the first row at scroll 0, below the corner button */
#define LIST_LOW 200            /* the rows' area ends here, above the bar */
#define ROW_GAP 4
#define TRACK_X 304
#define TRACK_Y 8
#define THUMB_HIT_X 298         /* the scroll bar takes touches from here to the screen's edge, as the application's */
#define PATH "sdmc:/gallery/list"
enum { CORNER = ROWS };

static const scroll_shape SHAPE = {ROWS, 0, 0, FOLDER_H, ROW_GAP, LIST_TOP, LIST_LOW, 8, SELECT_OUT};
static const rect TRACK = {TRACK_X, TRACK_Y, SCROLL_W, LIST_LOW - TRACK_Y};
static const rect THUMB_AREA = {THUMB_HIT_X, 0, BOTTOM_W - THUMB_HIT_X, LIST_LOW};
static scroll_view view;
static touch_state touch;
static int mine, thumb, chosen = -1;

static const char *row_name(int i)
{
    static char s[80];
    if (i % 7 == 6)
        snprintf(s, sizeof s, "Row %d, with a name too long to stand whole on its row", i + 1);
    else
        snprintf(s, sizeof s, "Row %d", i + 1);
    return s;
}

static rect row_rect(int i)
{
    return (rect){PAD, scroll_y(&SHAPE, &view, (size_t)i), TRACK_X - GAP - PAD, scroll_row_h(&SHAPE, (size_t)i)};
}

static int hit(int x, int y)
{
    if (ui_hit(ui_corner_rect(PATH), x, y))
        return CORNER;
    for (int i = 0; i < ROWS; i++)
        if (y < LIST_LOW && ui_hit(row_rect(i), x, y))
            return i;
    return -1;
}

static void choose(int t)
{
    if (t == CORNER) {
        back();
        return;
    }
    sfx(SFX_CONFIRM);
    chosen = t;
}

static void open(void)
{
    view = (scroll_view){0};
    chosen = -1;
}

static int input_now(const input *in)
{
    if (in->down & KEY_TOUCH) {
        mine = in->y < BAR_Y;
        if (!mine)
            return 0;           /* the bar's: main.c */
        int t = hit(in->x, in->y);
        view.selected = 0;
        thumb = ui_hit(THUMB_AREA, in->x, in->y);
        touch_down(&touch, thumb ? -1 : t, 1, scroll_moving(&view), in->x, in->y, in->now);
        scroll_grab(&SHAPE, &view, in->y, !thumb && t != CORNER && in->y < LIST_LOW);
        if (thumb)
            scroll_thumb_to(&SHAPE, &view, in->y, TRACK.y, TRACK.h);
        return 1;
    }
    if ((in->held & KEY_TOUCH) || (in->up & KEY_TOUCH)) {
        if (!mine)
            return 0;
        int over = touch.target >= 0 && hit(in->x, in->y) == touch.target;
        if (in->held & KEY_TOUCH) {
            touch_move(&touch, in->x, in->y, over);
            if (thumb)
                scroll_thumb_to(&SHAPE, &view, in->y, TRACK.y, TRACK.h);
            else
                scroll_drag(&SHAPE, &view, in->y);
            return 1;
        }
        int t = touch_up(&touch, over), dragged = scroll_release(&view);
        mine = thumb = 0;
        if (t >= 0 && !dragged)
            choose(t);
        return 1;
    }
    long move = in->down & KEY_UP ? -1 : in->down & KEY_DOWN ? 1 : in->down & KEY_LEFT ? -5 : in->down & KEY_RIGHT ? 5 : 0;
    if (move || (in->down & KEY_A)) {
        if (move || !view.selected) {
            if (scroll_move(&SHAPE, &view, view.selected ? move : 0))
                sfx(SFX_MOVE);
        } else {
            choose((int)view.cursor);
        }
        return 1;
    }
    return 0;
}

static int own(const input *in)
{
    int taken = input_now(in);
    scroll_frame(&SHAPE, &view);
    return taken;
}

static void top(void)
{
    char foot[96];
    ui_head("List", view.selected ? of((int)view.cursor + 1, ROWS) : NULL);
    card_text("Drag the rows with the stylus and let go to fling them, or hold the scroll bar's thumb. The D-pad's "
              "first press selects the first row that shows whole, the next ones move it, left and right by five; A "
              "or a tap chooses. The corner button goes back.");
    snprintf(foot, sizeof foot, "Chosen: %s", chosen < 0 ? "none" : row_name(chosen));
    ui_foot(FOOT_DOTS_Y, -1, foot, MUTED, 0, NULL);
}

static void bottom(int held)
{
    (void)held;
    int down = touch_held(&touch, frame_now);
    float at, size;
    for (int i = 0; i < ROWS; i++) {
        rect r = row_rect(i);
        if (r.y + r.h > 0 && r.y < LIST_LOW)
            ui_row_folder(r, down == i ? UI_DOWN : 0, row_name(i));
    }
    scroll_thumb(&SHAPE, &view, &at, &size);
    ui_scrollbar(TRACK, at, size, thumb);
    ui_corner(PATH, 1, down == CORNER);
    if (view.selected)
        ui_select(row_rect((int)view.cursor), ROW_R);
}

const page page_list = {.name = "List", .symbol = ART_FOLDER, .target = NULL, .press = NULL, .top = top,
                        .bottom = bottom, .open = open, .own = own};
