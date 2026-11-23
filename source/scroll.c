#include "scroll.h"

#include <math.h>
#include <stdlib.h>

#include "ui_layout.h"

float scroll_max(const scroll_area *a)
{
    float m = a->inset + a->content - a->h;
    return m > 0 ? m : 0;
}

float scroll_clamp(const scroll_area *a, float v)
{
    float m = scroll_max(a);
    return v < 0 ? 0 : v > m ? m : v;
}

int scroll_shows(const scroll_area *a, float s, float top, float h)
{
    return top >= s && a->inset + top + h <= a->h + s;
}

int select_next(int *selected, long *cursor, int event, long delta, long first, long count)
{
    if (event == SELECT_TOUCH || count <= 0) {
        *selected = 0;
        return 0;
    }
    if (!*selected) {
        *selected = 1;
        *cursor = first < 0 ? 0 : first >= count ? count - 1 : first;
        return 0;
    }
    if (event == SELECT_A)
        return 1;
    long c = *cursor + delta;
    *cursor = c < 0 ? 0 : c >= count ? count - 1 : c;
    return 0;
}

float scroll_to_show(const scroll_area *a, float now, float top, float h, float margin)
{
    if (top - margin < now)
        return scroll_clamp(a, top - margin);
    if (a->inset + top + h + margin > now + a->h)
        return scroll_clamp(a, a->inset + top + h + margin - a->h);
    return now;
}

float scroll_top(const scroll_shape *s, size_t i)
{
    size_t f = s->heads;
    return (float)(i < f ? i : f) * (s->head_h + s->gap) + (float)(i > f ? i - f : 0) * (s->row_h + s->gap);
}

float scroll_row_h(const scroll_shape *s, size_t i)
{
    return i < s->heads ? s->head_h : s->row_h;
}

scroll_area scroll_of(const scroll_shape *s)
{
    return (scroll_area){s->h, s->inset, s->count ? scroll_top(s, s->count) - s->gap + s->end : 0};
}

float scroll_y(const scroll_shape *s, const scroll_view *v, size_t i)
{
    return s->inset + scroll_top(s, i) - roundf(v->scroll);
}

int scroll_whole(const scroll_shape *s, const scroll_view *v, size_t i)
{
    scroll_area a = scroll_of(s);
    return scroll_shows(&a, roundf(v->scroll), scroll_top(s, i), scroll_row_h(s, i));
}

long scroll_first(const scroll_shape *s, const scroll_view *v)
{
    for (size_t i = 0; i < s->count; i++)
        if (scroll_whole(s, v, i))
            return (long)i;
    return 0;
}

float scroll_target(const scroll_shape *s, const scroll_view *v)
{
    scroll_area a = scroll_of(s);
    return scroll_to_show(&a, v->scroll, scroll_top(s, v->cursor), scroll_row_h(s, v->cursor), s->margin);
}

/* After a drag or fling a selected row stays one that shows whole. */
static void keep_cursor(const scroll_shape *s, scroll_view *v)
{
    if (!s->count || !v->selected || scroll_whole(s, v, v->cursor))
        return;
    size_t i = v->cursor;
    if (scroll_y(s, v, v->cursor) < s->inset)
        while (i + 1 < s->count && !scroll_whole(s, v, i))
            i++;
    else
        while (i > 0 && !scroll_whole(s, v, i))
            i--;
    v->cursor = i;
}

int scroll_move(const scroll_shape *s, scroll_view *v, long delta)
{
    long c = (long)v->cursor;
    int was = v->selected;
    select_next(&v->selected, &c, SELECT_MOVE, delta, scroll_first(s, v), (long)s->count);
    int moved = (size_t)c != v->cursor || v->selected != was;
    v->cursor = (size_t)c;
    v->follow = 1;
    v->fling = 0;
    return moved;
}

void scroll_frame(const scroll_shape *s, scroll_view *v)
{
    if (v->dragging || !s->count)
        return;
    scroll_area a = scroll_of(s);
    if (fabsf(v->fling) > 0.3f) {
        v->scroll += v->fling;
        v->fling *= FLING_PCT / 100.0f;
        if (v->scroll < 0 || v->scroll > scroll_max(&a))
            v->fling = 0;
        v->scroll = scroll_clamp(&a, v->scroll);
        keep_cursor(s, v);
        return;
    }
    v->fling = 0;
    if (v->follow) {
        float target = scroll_target(s, v);
        v->scroll += (target - v->scroll) * GLIDE_PCT / 100.0f;
        if (fabsf(target - v->scroll) < 0.5f) {
            v->scroll = target;
            v->follow = 0;
        }
    }
    v->scroll = scroll_clamp(&a, v->scroll);
}

int scroll_moving(const scroll_view *v)
{
    return fabsf(v->fling) > 0.3f || v->follow;
}

void scroll_grab(const scroll_shape *s, scroll_view *v, int y, int may_drag)
{
    scroll_area a = scroll_of(s);
    v->armed = may_drag && scroll_max(&a) > 0;
    v->dragging = 0;
    v->grab_y = y;
    v->grab_scroll = v->scroll;
    v->fling = 0;
    v->follow = 0;
}

void scroll_drag(const scroll_shape *s, scroll_view *v, int y)
{
    if (!v->armed)
        return;
    if (!v->dragging && abs(y - v->grab_y) >= DRAG_START)
        v->dragging = 1;
    if (!v->dragging)
        return;
    scroll_area a = scroll_of(s);
    float was = v->scroll;
    v->scroll = scroll_clamp(&a, v->grab_scroll - (float)(y - v->grab_y));
    v->fling = 0.6f * v->fling + 0.4f * (v->scroll - was);
    v->follow = 0;
    keep_cursor(s, v);
}

int scroll_release(scroll_view *v)
{
    int dragged = v->dragging;
    v->armed = v->dragging = 0;
    if (!dragged)
        v->fling = 0;
    return dragged;
}

void scroll_thumb_to(const scroll_shape *s, scroll_view *v, float y, float track_y, float track_h)
{
    scroll_area a = scroll_of(s);
    v->scroll = scroll_clamp(&a, (y - track_y) / track_h * (a.inset + a.content) - a.h / 2.0f);
    v->follow = 0;
    v->fling = 0;
    keep_cursor(s, v);
}

void scroll_thumb(const scroll_shape *s, const scroll_view *v, float *at, float *size)
{
    scroll_area a = scroll_of(s);
    float total = a.inset + a.content;
    *at = total > 0 ? v->scroll / total : 0;
    *size = total > 0 ? a.h / total : 1;
}
