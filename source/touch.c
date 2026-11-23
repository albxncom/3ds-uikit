#include "touch.h"

#include <stdlib.h>

#include "ui_layout.h"

void touch_down(touch_state *t, int target, int in_list, int blocked, int x, int y, uint64_t now)
{
    t->target = blocked ? -1 : target;
    t->in_list = in_list;
    t->moved = 0;
    t->over = 1;
    t->x = x;
    t->y = y;
    t->since = now;
}

void touch_move(touch_state *t, int x, int y, int over)
{
    if (t->target < 0)
        return;
    if (t->in_list && (abs(x - t->x) > DRAG_START || abs(y - t->y) > DRAG_START))
        t->moved = 1;
    t->over = over;
}

int touch_up(touch_state *t, int over)
{
    int target = t->target;
    t->over = over;
    int act = target >= 0 && !t->moved && t->over;
    t->target = -1;
    return act ? target : -1;
}

int touch_held(const touch_state *t, uint64_t now)
{
    if (t->target < 0 || t->moved || !t->over)
        return -1;
    if (t->in_list && now - t->since < TOUCH_HOLD_MS)
        return -1;
    return t->target;
}
