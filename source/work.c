#include "work.h"

#include <string.h>

void work_reset(work_state *w, uint64_t now)
{
    memset(w, 0, sizeof *w);
    w->at = now;
    w->menu = 1.0f;
}

void work_begin(work_state *w, uint64_t now)
{
    if (w->depth++ == 0 && !w->sounding)
        w->since = now;         /* while the loop still sounds from the work before, this one goes on with it */
}

void work_end(work_state *w)
{
    if (w->depth > 0)
        w->depth--;
}

/* v a step of dt ms closer to target, at a pace that crosses the whole range in over ms. */
static float towards(float v, float target, uint64_t dt, float over)
{
    float step = (float)dt / over;
    if (v < target)
        return v + step > target ? target : v + step;
    return v - step < target ? target : v - step;
}

int work_step(work_state *w, uint64_t now, int enabled)
{
    uint64_t dt = now > w->at ? now - w->at : 0;
    int event = WORK_NONE;
    w->at = now;
    int want = enabled && w->depth > 0 && now >= w->since && now - w->since >= WORK_DELAY_MS;
    if (want && !w->sounding) {
        w->sounding = 1;
        w->loop = 0;
        event = WORK_START;
        dt = 0;                 /* the fades begin with this step */
    }
    w->loop = towards(w->loop, want ? 1.0f : 0.0f, dt, WORK_LOOP_FADE_MS);
    w->menu = towards(w->menu, want ? 0.0f : 1.0f, dt, WORK_FADE_MS);
    if (!want && w->sounding && w->loop <= 0) {
        w->sounding = 0;
        event = WORK_STOP;
    }
    return event;
}
