/* The gallery's frame and the input every page shares. A touch presses a target when the stylus is lifted on it
 * (touch.h); the first D-pad press shows the cursor and the next ones move it, A presses the target under it; B goes
 * back to the start page, START leaves from any page. A page may take input itself first (page.own). */
#include <stdarg.h>
#include <stdio.h>
#include "gallery.h"
#include "ease.h"
#include "touch.h"
#include "widget.h"

enum { T_LEFT = 100, T_RIGHT, T_WIDE };     /* the bar's targets, beside a page's 0 to count - 1 */

int cursor_shown, cursor;
char sound_line[160];
u64 frame_now;
static const page *shown = &page_start;
static u64 opened_at, started_at;
static touch_state touch;
static int stylus_x, stylus_y;  /* where the stylus was last: a lift reads no position */

static void sound_log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(sound_line, sizeof sound_line, fmt, ap);
    va_end(ap);
}

const char *of(int n, int count)
{
    static char s[24];
    snprintf(s, sizeof s, "%d of %d", n, count);
    return s;
}

void card_text(const char *s)
{
    static ui_block b;
    rect c = ui_card_rect();
    ui_card(c);
    ui_wrap(&b, s, c.w - 2 * CARD_PAD, CARD_PX, CARD_PX - 3, 0, c.h - 2 * CARD_PAD, CARD_LH);
    ui_block_draw(&b, c.x + CARD_PAD, c.y + CARD_PAD, INK, UI_LEFT);
}

void open_page(const page *p)
{
    shown = p;
    cursor = 0;
    cursor_shown = 0;
    opened_at = frame_now;
    touch.target = -1;          /* a touch begun on the last page presses nothing here */
    if (p->open)
        p->open();
}

void forget_touch(void)
{
    touch.target = -1;
}

void back(void)
{
    sfx(SFX_BACK);
    open_page(&page_start);
}

static int target_at(int x, int y)
{
    for (int i = 0; i < shown->count; i++)
        if (ui_hit(shown->target(i), x, y))
            return i;
    if (shown == &page_start)
        return ui_hit(ui_bar_wide_rect(), x, y) ? T_WIDE : -1;
    int tabs = shown->tabs ? shown->tabs() : TAB_LEFT | (shown->right ? TAB_RIGHT : 0);   /* those the bar has */
    if (tabs & TAB_LEFT && ui_hit(ui_bar_rect(), x, y) && ui_tab_hit(0, x, y))
        return T_LEFT;
    return tabs & TAB_RIGHT && ui_tab_hit(1, x, y) ? T_RIGHT : -1;
}

/* 0: leave */
static int act(int target)
{
    if (target == T_WIDE)
        return 0;
    if (target == T_LEFT)
        back();
    else if (target == T_RIGHT)
        shown->press(shown->count);
    else if (target >= 0)
        shown->press(target);
    return 1;
}

static int keys(const input *in)
{
    if (in->down & KEY_B && shown != &page_start) {
        back();
        return 1;
    }
    if (in->down & KEY_Y && shown->right)
        return act(T_RIGHT);
    if (in->down & KEY_A && !shown->count && shown->right)
        return act(T_RIGHT);
    u32 dirs = KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT;
    if (!shown->count || !(in->down & (dirs | KEY_A)))
        return 1;
    if (!cursor_shown) {        /* the first press only shows the cursor where it is */
        cursor_shown = 1;
        sfx(SFX_MOVE);
        return 1;
    }
    if (in->down & KEY_A)
        return act(cursor);
    int to = cursor + (in->down & KEY_LEFT ? -1 : in->down & KEY_RIGHT ? 1 : 0) +
             (in->down & KEY_UP ? -shown->columns : in->down & KEY_DOWN ? shown->columns : 0);
    to = to < 0 ? 0 : to >= shown->count ? shown->count - 1 : to;
    if (to != cursor)
        sfx(SFX_MOVE);
    cursor = to;
    return 1;
}

/* 0: leave */
static int take(const input *in)
{
    if (in->down & KEY_START)
        return 0;
    const page *p = shown;
    if (p->own && p->own(in))
        return 1;
    if (in->down & KEY_TOUCH) {
        cursor_shown = 0;
        touch_down(&touch, target_at(in->x, in->y), 0, 0, in->x, in->y, in->now);
    } else if (in->held & KEY_TOUCH) {
        touch_move(&touch, in->x, in->y, touch.target >= 0 && target_at(in->x, in->y) == touch.target);
    } else if (in->up & KEY_TOUCH) {
        return act(touch_up(&touch, touch.target >= 0 && target_at(in->x, in->y) == touch.target));
    }
    return keys(in);
}

static void draw(u64 now)
{
    int held = touch_held(&touch, now);
    ui_begin(ease_span(now, opened_at, TRANSITION_MS));    /* the plain span: the kit eases it */
    ui_black(1 - ease_span(now, started_at, FADE_MS));
    do {
        ui_top();
        shown->top();
        ui_bottom();
        shown->bottom(held >= 0 && held < shown->count ? held : -1);
        int tab = held == T_LEFT ? 0 : held == T_RIGHT ? 1 : -1;
        if (shown->bar)
            shown->bar(tab);
        else if (shown == &page_start)
            ui_bar_wide(UI_KEY_START, "Leave", held == T_WIDE);
        else if (shown->right)
            ui_bar2("Back", shown->right, tab);
        else
            ui_bar(UI_KEY_B, "Back", tab == 0);
        if (cursor_shown && shown->count && shown->select)
            shown->select(cursor);
        else if (cursor_shown && shown->count)
            ui_select(shown->target(cursor), TILE_R);
    } while (ui_next_eye());
    ui_end();
}

/* One frame: the input, then the drawing; 0 when the gallery is left. */
static int frame(void)
{
    touchPosition t;
    hidScanInput();
    hidTouchRead(&t);
    input in = {hidKeysDown(), hidKeysHeld(), hidKeysUp(), stylus_x, stylus_y, osGetTime()};
    if (in.held & KEY_TOUCH)
        in.x = stylus_x = t.px, in.y = stylus_y = t.py;
    frame_now = in.now;
    if (!take(&in))
        return 0;
    draw(in.now);
    return 1;
}

static void start(void)
{
    osSetSpeedupEnable(true);   /* first, as the application: a New 3DS at its higher clock (libctru asks ptm:sysm) */
    audio_log = sound_log;      /* before audio_init: its line says whether there is sound */
    ui_init();
    audio_init();
    audio_menu(-1);             /* no music until the sound page asks for it */
    started_at = opened_at = frame_now = osGetTime();
}

static void finish(void)
{
    audio_exit();
    ui_exit();
}

int main(void)
{
    start();
    while (aptMainLoop() && frame())
        ;
    finish();
    return 0;
}
