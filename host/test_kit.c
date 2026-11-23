/* The kit's plain C on the Mac, on its own: the popup's guard and keys (popup.c), the selection and the stylus
 * (scroll.c, touch.c), runs of spaces and a cut path (wrap.c), the order of what reaches the GPU (order.h) and
 * the curves for what moves (ease.h). The kit's numbers only (ui_layout.h); the application tests the same code
 * with its own texts and list shapes in its tests.
 *
 *   test_kit   prints a few lines and "test_kit: N checks, F failed"; exit 1 if any failed */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "ease.h"
#include "order.h"
#include "popup.h"
#include "scroll.h"
#include "touch.h"
#include "ui_fakes.h"
#include "ui_layout.h"
#include "wrap.h"

/* A width function where a space is half as wide as any other character. */
static float half_space_width(const char *s, size_t n, int px, void *ctx)
{
    (void)ctx, (void)px;
    float w = 0;
    for (size_t i = 0; i < n && s[i]; i += wrap_unit(s + i))
        w += s[i] == ' ' ? 0.5f : 1.0f;
    return w;
}

/* ---------------------------------------------------------------- a popup's guard and its keys */

static void test_popup(void)
{
    popup_guard g;
    popup_focus f;
    /* nothing before the first frame */
    guard_arm(&g, 0);
    CHECK(!guard_takes(&g, 1000000));
    guard_from(&g, 2000);
    CHECK(!guard_takes(&g, 2000 + POPUP_GUARD_MS - 1) && guard_takes(&g, 2000 + POPUP_GUARD_MS));

    /* keys in a question: the cursor starts hidden on "Abbrechen"; A twice cancels, it never deletes */
    focus_open(&f);
    CHECK_EQ(popup_key(&f, 1, POPUP_KEY_A), -1);
    CHECK(f.ring && f.at == 0);
    CHECK_EQ(popup_key(&f, 1, POPUP_KEY_A), 0);
    focus_open(&f);
    CHECK_EQ(popup_key(&f, 1, POPUP_KEY_RIGHT), -1);    /* the first press only shows it, on "Abbrechen" */
    CHECK_EQ(f.at, 0);
    CHECK_EQ(popup_key(&f, 1, POPUP_KEY_RIGHT), -1);
    CHECK_EQ(f.at, 1);
    CHECK_EQ(popup_key(&f, 1, POPUP_KEY_A), 1);         /* deleting takes the cursor moved there, then A */
    CHECK_EQ(popup_key(&f, 1, POPUP_KEY_LEFT), -1);
    CHECK_EQ(popup_key(&f, 1, POPUP_KEY_A), 0);
    focus_open(&f);
    CHECK_EQ(popup_key(&f, 1, POPUP_KEY_B), 0);         /* B cancels, with the cursor hidden or not */
    f.ring = 1;
    f.at = 1;
    CHECK_EQ(popup_key(&f, 1, POPUP_KEY_B), 0);
    /* one button: A and B close it, the D-pad does nothing */
    focus_open(&f);
    CHECK_EQ(popup_key(&f, 0, POPUP_KEY_A), 1);
    CHECK_EQ(popup_key(&f, 0, POPUP_KEY_B), 1);
    CHECK_EQ(popup_key(&f, 0, POPUP_KEY_LEFT), -1);
}

/* ---------------------------------------------------------------- a list's selection and the stylus */

static void test_select_and_touch(void)
{
    /* the folder view's selection */
    int sel = 1;
    long cur = 5;
    CHECK(select_next(&sel, &cur, SELECT_TOUCH, 0, 3, 11) == 0 && !sel);        /* a touch clears it */
    CHECK(select_next(&sel, &cur, SELECT_MOVE, 1, 3, 11) == 0 && sel && cur == 3); /* first press: first shown */
    CHECK(select_next(&sel, &cur, SELECT_MOVE, 1, 3, 11) == 0 && cur == 4);     /* then it moves */
    CHECK(select_next(&sel, &cur, SELECT_MOVE, -9, 3, 11) == 0 && cur == 0);
    CHECK(select_next(&sel, &cur, SELECT_MOVE, 99, 3, 11) == 0 && cur == 10);
    CHECK(select_next(&sel, &cur, SELECT_A, 0, 3, 11) == 1 && cur == 10);       /* A opens a selected row */
    sel = 0;
    CHECK(select_next(&sel, &cur, SELECT_A, 0, 7, 11) == 0 && sel && cur == 7); /* A with none only selects */
    sel = 0;
    CHECK(select_next(&sel, &cur, SELECT_MOVE, 3, 7, 11) == 0 && cur == 7);     /* L, R: page, then this */
    sel = 0;
    CHECK(select_next(&sel, &cur, SELECT_MOVE, 1, 0, 0) == 0 && !sel);          /* an empty list */

    /* the stylus (touch.h): a button outside the list */
    touch_state t;
    touch_down(&t, 3, 0, 0, 100, 100, 1000);
    CHECK(touch_held(&t, 1000) == 3);                                           /* held at once */
    touch_move(&t, 104, 100, 1);
    CHECK(touch_up(&t, 1) == 3);                                                /* a tap acts on lifting */
    touch_down(&t, 3, 0, 0, 100, 100, 1000);
    touch_move(&t, 200, 100, 0);                                                /* slides off: not held */
    CHECK(touch_held(&t, 1100) == -1);
    touch_move(&t, 101, 100, 1);                                                /* back on: held again */
    CHECK(touch_held(&t, 1100) == 3);
    touch_move(&t, 200, 100, 0);
    CHECK(touch_up(&t, 0) == -1);                                               /* lifted off it: nothing */
    /* a row in the list: a quick tap opens it, without the look before the delay */
    touch_down(&t, 20, 1, 0, 50, 80, 1000);
    CHECK(touch_held(&t, 1000 + TOUCH_HOLD_MS - 1) == -1);
    CHECK(touch_up(&t, 1) == 20);
    /* moves before the delay: never held; back at the start point: still a scroll, nothing opens */
    touch_down(&t, 20, 1, 0, 50, 80, 1000);
    touch_move(&t, 50, 80 + DRAG_START + 1, 1);
    CHECK(touch_held(&t, 1000 + TOUCH_HOLD_MS + 50) == -1);
    touch_move(&t, 50, 80, 1);
    CHECK(touch_held(&t, 2000) == -1 && touch_up(&t, 1) == -1);
    /* moves after the delay: held, then the look goes at once and nothing opens */
    touch_down(&t, 20, 1, 0, 50, 80, 1000);
    CHECK(touch_held(&t, 1000 + TOUCH_HOLD_MS) == 20);
    touch_move(&t, 50, 80 - DRAG_START - 1, 1);
    CHECK(touch_held(&t, 1200) == -1 && touch_up(&t, 1) == -1);
    /* a small wobble stays a press */
    touch_down(&t, 20, 1, 0, 50, 80, 1000);
    touch_move(&t, 50 + DRAG_START, 80 - DRAG_START, 1);
    CHECK(touch_held(&t, 1200) == 20 && touch_up(&t, 1) == 20);
    /* a touch while the list glides, and a second touch while an action is under way: blocked */
    touch_down(&t, 20, 1, 1, 50, 80, 1000);
    CHECK(touch_held(&t, 2000) == -1 && touch_up(&t, 1) == -1);
    touch_down(&t, -1, 0, 1, 50, 80, 1000);
    CHECK(touch_held(&t, 1000) == -1 && touch_up(&t, 1) == -1);
}

/* ---------------------------------------------------------------- what is measured: runs of spaces, a cut path */

static void test_spaces_and_cut(void)
{
    static ui_block b;
    static char in_words[4096], out_words[4096], s[512];

    /* two cases that once failed: 30 spaces measured as one and drawn in full (a line 405 px wide in a
     * 274 px box); 150 spaces after "\n", not measured, pushed a word out of the line's buffer (52 bytes lost) */
    snprintf(s, sizeof s, "Ben 10%30sFour Arms", "");
    int all = wrap_text(&b, s, 274, 22, 22, 2, 0, 25, fake_width, NULL);
    CHECK(all);
    check_block(&b, all, 274);
    for (int i = 0; i < b.count; i++)
        CHECK(!strstr(b.line[i], "  ") && b.line[i][0] != ' ');
    words_of(s, in_words, sizeof in_words);
    block_words(&b, out_words, sizeof out_words);
    CHECK(!strcmp(in_words, out_words));
    int n = snprintf(s, sizeof s, "x\n");
    memset(s + n, ' ', 150);
    memset(s + n + 150, 'a', 100);
    strcpy(s + n + 250, " tail");
    all = wrap_text(&b, s, 270, 13, 13, 0, 0, 18, half_space_width, NULL);
    CHECK(all);
    for (int i = 0; i < b.count; i++) {
        CHECK(half_space_width(b.line[i], strlen(b.line[i]), 13, NULL) <= 270);
        CHECK(!strstr(b.line[i], "...") && b.line[i][0] != ' ');
    }
    words_of(s, in_words, sizeof in_words);
    block_words(&b, out_words, sizeof out_words);
    CHECK(!strcmp(in_words, out_words));

    /* the corner button's path: cut at its beginning, so the last folder stays */
    float most = CORNER_MAX_W - CORNER_PAD - CORNER_ICON - CORNER_GAP - CORNER_PAD_R;
    char shown[UI_LINE_BYTES + 4];
    wrap_cut(shown, sizeof shown, "/Themes", most, CORNER_PX, 1, fake_width, NULL);
    CHECK(!strcmp(shown, "/Themes"));
    const char *deep = "/Themes/Downloaded/2026/November/Pokemon";
    wrap_cut(shown, sizeof shown, deep, most, CORNER_PX, 1, fake_width, NULL);
    CHECK(fake_width(shown, strlen(shown), CORNER_PX, NULL) <= most);
    CHECK(!strncmp(shown, "...", 3) && strlen(shown) >= 3 + strlen("/Pokemon"));
    CHECK(!strcmp(shown + strlen(shown) - strlen("/Pokemon"), "/Pokemon"));
    CHECK(!strcmp(deep + strlen(deep) - (strlen(shown) - 3), shown + 3));
    wrap_cut(shown, sizeof shown, "/", most, CORNER_PX, 1, fake_width, NULL);
    CHECK(!strcmp(shown, "/"));

    /* a cut line keeps characters whole and is marked */
    char out[16];
    wrap_cut(out, sizeof out, "\xc3\xbc\xc3\xbc\xc3\xbc\xc3\xbc\xc3\xbc\xc3\xbc\xc3\xbc\xc3\xbc", 1000, 22, 0, fake_width, NULL);
    CHECK(strlen(out) < sizeof out && !strcmp(out + strlen(out) - 3, "...") && valid_utf8_shown(out));
}

/* ---------------------------------------------------------------- the white flicker (order.h) */

static void test_order(void)
{
    order o = {.draw = order_drawn};
    /* an image beside a picture leaves it waiting unless it comes within reach (the top screen's layers shift) */
    order_log[0] = 0;
    order_picture(&o, (order_rect){100, 100, 40, 40});
    sim_image(&o, 145, 100, 10, 10);            /* 5 px to the right */
    sim_image(&o, 100, 40, 40, 55);             /* ends 5 px above */
    CHECK(!strcmp(order_log, "II") && o.count == 1);
    sim_image(&o, 143, 100, 10, 10);            /* 3 px to the right */
    CHECK(!strcmp(order_log, "IIPI") && o.count == 0);
    /* two pictures wait: an image over the second draws both, in the order they were asked for */
    order_log[0] = 0;
    order_picture(&o, (order_rect){10, 10, 40, 40});
    order_picture(&o, (order_rect){200, 10, 40, 40});
    sim_image(&o, 210, 20, 5, 5);
    CHECK(!strcmp(order_log, "PPI") && order_last.x == 200);

    /* more pictures than can wait: the waiting ones are drawn first, none is lost */
    order_log[0] = 0;
    for (int i = 0; i < ORDER_MAX + 3; i++)
        CHECK_EQ(order_picture(&o, (order_rect){(float)(i * 50), 0, 40, 40}), i % ORDER_MAX);
    CHECK_EQ(order_count('P'), ORDER_MAX);
    CHECK_EQ(o.count, 3);
    order_flush(&o);
    CHECK_EQ(order_count('P'), ORDER_MAX + 3);
}

/* the one clock and the curves for what moves (ease.h) */
static void test_ease(void)
{
    CHECK(ease_span(100, 100, 200) == 0 && ease_span(50, 100, 200) == 0 && ease_span(200, 100, 200) == 0.5f);
    CHECK(ease_span(300, 100, 200) == 1 && ease_span(99999, 100, 200) == 1);
    CHECK(ease_span(5, 5, 0) == 0 && ease_span(6, 5, 0) == 1);   /* no time: there at once */
    CHECK(ease_out(0) == 0 && ease_out(1) == 1 && ease_out(-3) == 0 && ease_out(7) == 1 && ease_out(0.5f) == 0.75f);
    CHECK(ease_both(0) == 0 && ease_both(1) == 1 && ease_both(-3) == 0 && ease_both(7) == 1 && ease_both(0.5f) == 0.5f);
    /* from 0 to 1 and never back; in a frame of 16 ms of 200 at most twice, and 1.5 times, the even step */
    float out_was = 0, both_was = 0, out_most = 0, both_most = 0;
    for (unsigned ms = 16; ms <= 208; ms += 16) {
        float out = ease_out(ease_span(ms, 0, 200)), both = ease_both(ease_span(ms, 0, 200));
        CHECK(out >= out_was && both >= both_was);
        out_most = out - out_was > out_most ? out - out_was : out_most;
        both_most = both - both_was > both_most ? both - both_was : both_most;
        out_was = out;
        both_was = both;
    }
    CHECK(out_was == 1 && both_was == 1);
    CHECK(out_most <= 2 * 16 / 200.0f + 0.0001f && both_most <= 1.5f * 16 / 200.0f + 0.0001f);
    /* a value at rest, sent on its way, and there */
    eased e;
    eased_rest(&e, 1);
    CHECK(eased_at(&e, 0) == 1 && eased_at(&e, 99999) == 1 && !eased_moving(&e, 5));
    eased_to(&e, 2, 1000, 200);
    CHECK(eased_at(&e, 1000) == 1 && eased_at(&e, 1100) == 1.5f && eased_at(&e, 1200) == 2 && eased_at(&e, 5000) == 2);
    CHECK(eased_moving(&e, 1000) && eased_moving(&e, 1199) && !eased_moving(&e, 1200));
    eased_to(&e, 2, 1100, 200);                 /* sent where it is going already: nothing changes */
    CHECK(e.since == 1000 && eased_at(&e, 1200) == 2);
    /* sent on before it has arrived: from where it is, without a jump, and there in the same time again */
    eased_rest(&e, 0);
    eased_to(&e, 1, 0, 200);
    float at = eased_at(&e, 100);
    eased_to(&e, 2, 100, 200);
    CHECK(at == 0.5f && eased_at(&e, 100) == at && eased_at(&e, 300) == 2 && eased_at(&e, 299) < 2);
    float was = at;
    for (unsigned ms = 100; ms <= 300; ms += 4) {
        CHECK(eased_at(&e, ms) >= was);
        was = eased_at(&e, ms);
    }
}

int main(void)
{
    test_popup();
    test_select_and_touch();
    test_spaces_and_cut();
    test_order();
    test_ease();
    return check_summary("test_kit");
}
