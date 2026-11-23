/* One clock and one family of curves for what moves on the screens, so that the application moves
 * alike everywhere. The installing screen's step marks and the loader use it; a screen's transition and a popup's
 * motion can: ease_out is the way a screen's content comes in, ease_both the way a popup grows and shrinks.
 * Everything goes by the clock, in ms, never by counting frames: a late frame skips a part of the way and makes
 * nothing longer. Plain C in this header alone (no .c file of its own); the host tests run
 * it. */
#ifndef EASE_H
#define EASE_H

#include <stdint.h>

/* How far a movement that began at since and takes ms is at now: 0 to 1, evenly by the clock. */
static inline float ease_span(uint64_t now, uint64_t since, unsigned ms)
{
    if (now <= since)
        return 0;
    if (!ms || now - since >= ms)
        return 1;
    return (float)(now - since) / (float)ms;
}

/* Coming in: fast at first, no speed at the end (what is left of the way falls with the square of the time left). */
static inline float ease_out(float t)
{
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    return 1 - (1 - t) * (1 - t);
}

/* Changing in place: no speed at either end; its largest step is 1.5 times the even one. */
static inline float ease_both(float t)
{
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    return t * t * (3 - 2 * t);
}

/* A value that moves to where it is sent within a time (ease_both) and may be sent on before it has arrived: it
 * goes on from where it is and takes only that time again, so it is never more than one movement behind. */
typedef struct eased {
    float from, to;
    uint64_t since;
    unsigned ms;
} eased;

static inline void eased_rest(eased *e, float at)
{
    *e = (eased){at, at, 0, 0};
}

static inline float eased_at(const eased *e, uint64_t now)
{
    return e->from + (e->to - e->from) * ease_both(ease_span(now, e->since, e->ms));
}

/* Sends it to to, to arrive ms after now; nothing changes if it is on its way there already. */
static inline void eased_to(eased *e, float to, uint64_t now, unsigned ms)
{
    if (to == e->to)
        return;
    *e = (eased){eased_at(e, now), to, now, ms};
}

static inline int eased_moving(const eased *e, uint64_t now)
{
    return e->from != e->to && ease_span(now, e->since, e->ms) < 1;
}

#endif
