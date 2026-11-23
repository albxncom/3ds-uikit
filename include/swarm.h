/* The list's carousel, meant to look like a swarm. The selected item's pane in the
 * middle and up to SWARM_SIDE on each side, each one further out smaller, lower, tucked further behind the nearer one
 * and fainter (ui_layout.h, SWARM_*). Plain C in this header alone (no .c file of its own): the
 * drawing (an application's carousel, ui.c's icon slots) asks where each pane stands and which icon slot it draws
 * from; the host tests check both.
 *
 * A pane's place follows from its distance d to the middle, which need not be whole: the selection's position glides
 * (ease.h) from one item to the next in SWARM_MOVE_MS, and item i is drawn at d = i - that position, between the two
 * slots round d. At |d| = SWARM_SIDE + 1 a pane is gone: the one coming in at the far end comes from there, the one
 * leaving goes there. Nothing wraps round: at a list's ends and in a short list the side with fewer items has fewer
 * panes. */
#ifndef SWARM_H
#define SWARM_H

#include "ease.h"
#include "ui_layout.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#define SWARM_GONE (SWARM_SIDE + 1)
#define ICON_SLOTS 16           /* theme icons drawn per frame: 64x64 slots stacked in one 64x1024 texture */
#define SWARM_KEYED 8           /* of them the carousel's, one per list index modulo 8: seven panes and one coming in */

static const short SWARM_SLOT[SWARM_GONE + 1][8] = SWARM_SLOTS;
enum { SLOT_H, SLOT_NARROW, SLOT_OVER, SLOT_Y_LEFT, SLOT_Y_RIGHT, SLOT_OPACITY, SLOT_EDGE, SLOT_DEPTH };

/* A pane: its box (x, y its upper left corner), how solid it is drawn (opacity; 1 - opacity is the veil), how much
 * fainter its outer side is (edge, a share of the opacity), how see-through (alpha: 1 but on the way to and from the
 * gone slot), its depth in 3D, and on which side it stands (-1 left, 0 the middle, 1 right). */
typedef struct swarm_pane {
    float x, y, w, h, opacity, edge, alpha, depth;
    int side;
} swarm_pane;

/* Slot k's width and the distance from the screen's middle to its pane's middle; the neighbours, and every pane beyond
 * them with them, apart px further out than the slot positions (the look's pane_apart, ui_layout.h: Stock's
 * SWARM_STOCK_APART; a glass rim keeps the slots' tuck with 0). */
static inline void swarm_slot_x(int k, float apart, float *dx, float *w)
{
    float sum = 0, prev_w = 0;
    for (int j = 0; j <= k; j++) {
        float wj = SWARM_SLOT[j][SLOT_H] * SWARM_SLOT[j][SLOT_NARROW] / 100.0f;
        if (j)
            sum += prev_w / 2 + wj / 2 - (SWARM_SLOT[j][SLOT_OVER] - (j == 1) * apart);
        prev_w = wj;
    }
    *dx = sum;
    *w = prev_w;
}

/* The pane at distance d from the middle (negative: on the left), the neighbours apart px further out. */
static inline swarm_pane swarm_at(float d, float apart)
{
    float a = fabsf(d);
    if (a > SWARM_GONE)
        a = SWARM_GONE;
    int k = (int)a;
    float t = a - (float)k;
    if (k == SWARM_GONE)
        k = SWARM_GONE - 1, t = 1;
    int side = d < 0 ? -1 : d > 0 ? 1 : 0, ycol = side < 0 ? SLOT_Y_LEFT : SLOT_Y_RIGHT;
    float dx0, w0, dx1, w1;
    swarm_slot_x(k, apart, &dx0, &w0);
    swarm_slot_x(k + 1, apart, &dx1, &w1);
#define SWARM_LERP(col) (SWARM_SLOT[k][col] + (SWARM_SLOT[k + 1][col] - SWARM_SLOT[k][col]) * t)
    float dx = dx0 + (dx1 - dx0) * t, w = w0 + (w1 - w0) * t, h = SWARM_LERP(SLOT_H), cy = SWARM_LERP(ycol);
    swarm_pane p = {TOP_W / 2.0f + (side < 0 ? -dx : dx) - w / 2, cy - h / 2, w, h, SWARM_LERP(SLOT_OPACITY) / 100.0f,
                    SWARM_LERP(SLOT_EDGE) / 100.0f, k == SWARM_GONE - 1 ? 1 - t : 1, SWARM_LERP(SLOT_DEPTH), side};
#undef SWARM_LERP
    return p;
}

/* The items drawn round position pos in a list of count: first to last (none when last < first), each closer than
 * the gone slot. */
static inline void swarm_span(float pos, size_t count, long *first, long *last)
{
    *first = (long)floorf(pos) - SWARM_SIDE;
    *last = (long)ceilf(pos) + SWARM_SIDE;
    if (floorf(pos) == pos)     /* at rest exactly SWARM_SIDE each side; between two items one more on the far side */
        *first = (long)pos - SWARM_SIDE, *last = (long)pos + SWARM_SIDE;
    if (*first < 0)
        *first = 0;
    if (*last > (long)count - 1)
        *last = (long)count - 1;
}

/* The selection's position on its way: it glides to item, or is there at once when the carousel starts afresh (a
 * list that was not shown in the last frame, or another one: key) or jumps by more than SWARM_SIDE items (a row touched
 * further down: that would sweep the whole swarm past in one movement). */
typedef struct swarm {
    eased pos;
    const void *list;
    uint32_t frame;             /* the last frame it was drawn in */
    uint64_t still;             /* when the panes came, or will come, to rest */
} swarm;

static inline float swarm_follow(swarm *s, const void *key, size_t item, uint64_t now, uint32_t frame)
{
    float to = (float)item;
    if (key != s->list || frame - s->frame > 1 || fabsf(to - eased_at(&s->pos, now)) > SWARM_SIDE) {
        eased_rest(&s->pos, to);
        s->still = now;
    } else if (to != s->pos.to) {
        eased_to(&s->pos, to, now, SWARM_MOVE_MS);
        s->still = now + SWARM_MOVE_MS;
    }
    s->list = key;
    s->frame = frame;
    return eased_at(&s->pos, now);
}

/* The idle light on the pane at distance d at now: how far it has crossed it, 0 to 1, or -1 when it is not on it
 * (only on the middle pane and its two neighbours, and only while the panes rest). */
static inline float swarm_light(const swarm *s, uint64_t now, int d)
{
    if (d < -1 || d > 1 || now < s->still + SWARM_LIGHT_AFTER_MS)
        return -1;
    uint64_t at = (now - s->still - SWARM_LIGHT_AFTER_MS) % SWARM_LIGHT_EVERY_MS, from = (uint64_t)(d + 1) * SWARM_LIGHT_MS;
    return at >= from && at < from + SWARM_LIGHT_MS ? (float)(at - from) / SWARM_LIGHT_MS : -1;
}

/* The icon texture's slots (ui.c, ui_icon). The first SWARM_KEYED are the carousel's: item i's picture is always in
 * slot i % SWARM_KEYED, so a pane keeps its slot while it glides and a move converts only the picture that comes in
 * (no two items drawn at once are SWARM_KEYED apart). The others are handed to the remaining pictures in the order
 * they are asked for, each frame afresh. The right eye's top screen asks for the same pictures in the same order as
 * the left eye's: it gets the same slots again, from where the left eye's top screen began, so it converts nothing
 * and needs no slots of its own. */
typedef struct icon_slots {
    int next, top_from;
} icon_slots;

static inline int icon_slot_keyed(size_t index)
{
    return (int)(index % SWARM_KEYED);
}

static inline void icon_slots_frame(icon_slots *s)
{
    s->next = s->top_from = SWARM_KEYED;
}

/* A top screen begins: the left eye's notes where, the right eye's starts there again. */
static inline void icon_slots_top(icon_slots *s, int right_eye)
{
    if (right_eye)
        s->next = s->top_from;
    else
        s->top_from = s->next;
}

/* The next slot in call order; -1 when none is left (the picture is not drawn). */
static inline int icon_slot_next(icon_slots *s)
{
    return s->next < ICON_SLOTS ? s->next++ : -1;
}

#endif
