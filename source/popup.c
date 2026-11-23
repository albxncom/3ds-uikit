#include "popup.h"

#include "ui_layout.h"

void guard_arm(popup_guard *g, int touching)
{
    g->armed = 1;
    g->timed = 0;
    g->from = 0;
    g->stale = touching;
}

void guard_from(popup_guard *g, uint64_t now)
{
    if (g->timed)
        return;
    g->timed = 1;
    g->from = now;
}

void guard_lift(popup_guard *g)
{
    g->stale = 0;
}

int guard_takes(const popup_guard *g, uint64_t down_at)
{
    return !g->armed || (g->timed && !g->stale && down_at >= g->from + POPUP_GUARD_MS);
}

void focus_open(popup_focus *f)
{
    f->ring = 0;
    f->at = 0;
}

int popup_key(popup_focus *f, int two, int key)
{
    if (!two)
        return key == POPUP_KEY_A || key == POPUP_KEY_B ? 1 : -1;
    if (key == POPUP_KEY_B)
        return 0;
    if (!f->ring) {             /* the first press only shows the cursor, on "Abbrechen" */
        f->ring = 1;
        return -1;
    }
    if (key == POPUP_KEY_A)
        return f->at;
    f->at = key == POPUP_KEY_RIGHT;
    return -1;
}
