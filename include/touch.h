/* What a touch presses, in plain C so the host tests run it. A touch on something that lies in or over the
 * scrolling list (a row, the corner button) presses it only while the stylus rests: it shows held after
 * TOUCH_HOLD_MS, and once the stylus has moved more than DRAG_START px it is a scroll and never presses again,
 * however it ends. A touch while the list still moves stops it and presses nothing. Anything else (tiles, buttons,
 * the bar, the popup's button) shows held at once while the stylus is on it and acts when the stylus is lifted on
 * it; sliding off cancels, sliding back on presses again. */
#ifndef TOUCH_H
#define TOUCH_H

#include <stdint.h>

typedef struct touch_state {
    int target;                 /* what the touch is on, or -1 */
    int in_list;                /* it lies in or over the scrolling list */
    int moved;                  /* moved past DRAG_START: a scroll */
    int over;                   /* the stylus is on the target now */
    int x, y;                   /* where the touch began */
    uint64_t since;             /* when */
} touch_state;

/* A touch begins at x, y on target (-1: nothing); blocked: the list still moves or an action is under way, so it
 * presses nothing. */
void touch_down(touch_state *t, int target, int in_list, int blocked, int x, int y, uint64_t now);
/* The stylus is at x, y; over: still on the target. */
void touch_move(touch_state *t, int x, int y, int over);
/* The stylus is lifted (over: on the target where it was last); returns the target to act on, or -1. */
int touch_up(touch_state *t, int over);
/* The target shows held now, or -1. */
int touch_held(const touch_state *t, uint64_t now);

#endif
