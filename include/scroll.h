/* Every list's scrolling, in plain C so the host tests run it (the folder view, the installed designs, the Badges
 * part's folder and its installed sets: one behaviour). The rows scroll in an area from the screen's top edge down to
 * the bar, under the corner button that floats over them. At scroll 0 the first row starts inset px down, below the
 * button; a row the cursor moves to is scrolled to show whole between inset and the area's lower edge, so it never
 * ends up under the button. Rows are placed by their top below the first row's top. */
#ifndef SCROLL_H
#define SCROLL_H

#include <stddef.h>

typedef struct scroll_area {
    float h;                    /* the area's height */
    float inset;                /* above the first row at scroll 0; the cursor's row shows below it */
    float content;              /* the rows and the gaps between them */
} scroll_area;

/* The furthest the rows scroll: the last row's lower edge at the area's lower edge; 0 when everything shows. */
float scroll_max(const scroll_area *a);
float scroll_clamp(const scroll_area *a, float v);
/* 1 when the row from top to top + h shows whole below the inset at scroll s. */
int scroll_shows(const scroll_area *a, float s, float top, float h);
/* The scroll nearest to now at which the row from top to top + h, with margin px around it, shows whole below the
 * inset (as far as the limits allow). */
float scroll_to_show(const scroll_area *a, float now, float top, float h, float margin);

/* The folder view's selection. Nothing is selected when the view opens and after any touch on the list or the
 * scroll bar. With nothing selected, a move (D-pad, circle pad, L, R) or A selects first, the first row shown
 * whole, and does nothing else; with a row selected, a move goes delta rows (within the list) and A opens it. */
enum { SELECT_TOUCH, SELECT_MOVE, SELECT_A };

/* selected (0 or 1) and cursor after event; returns 1 when the row under the cursor is to be opened. */
int select_next(int *selected, long *cursor, int event, long delta, long first, long count);

/* A list as its screen has it: how many rows and how high, where they scroll. The screen keeps the rows' data and
 * draws each row where scroll_y says; nothing here knows what a row is. */
typedef struct scroll_shape {
    size_t count;               /* rows */
    size_t heads;               /* the first heads of them (the folders, which come first) are head_h high, the
                                   others row_h */
    float head_h, row_h;
    float gap;                  /* between two rows */
    float inset;                /* above the first row at scroll 0 */
    float h;                    /* the area's lower edge, from the screen's top edge */
    float end;                  /* under the last row at the list's end */
    float margin;               /* kept free round the cursor's row when the list shows it (its selection's reach) */
} scroll_shape;

/* What the list does, the same in every list. Continuous: it stands at any pixel and its rows are cut at the area's
 * edges. A move (select_next) makes it glide to show the cursor's row whole, GLIDE_PCT of the way per frame. A touch
 * on the rows grabs them: once it has moved DRAG_START px they follow the stylus 1 to 1, and when it is lifted they
 * go on at the drag's speed, which keeps FLING_PCT per frame. Both stop at the ends. A selected row that leaves the
 * view hands the selection to the nearest row that shows whole. */
typedef struct scroll_view {
    size_t cursor;              /* the selected row, or where a selection comes back to */
    int selected;
    float scroll;               /* px, 0 to scroll_max */
    float fling;                /* px a frame, after a drag */
    int follow;                 /* gliding to show the cursor's row */
    int armed, dragging;        /* a touch on the rows may drag them; it does */
    int grab_y;                 /* where that touch came down, and the scroll then */
    float grab_scroll;
} scroll_view;

/* Row i: its top below the first row's top, its height, its top on the screen (on a whole pixel). */
float scroll_top(const scroll_shape *s, size_t i);
float scroll_row_h(const scroll_shape *s, size_t i);
float scroll_y(const scroll_shape *s, const scroll_view *v, size_t i);
/* The area the rows scroll in (scroll_max, scroll_clamp). */
scroll_area scroll_of(const scroll_shape *s);
/* 1 when row i shows whole below the inset. */
int scroll_whole(const scroll_shape *s, const scroll_view *v, size_t i);
/* The first row that shows whole (0 when none does): where a selection starts. */
long scroll_first(const scroll_shape *s, const scroll_view *v);
/* The scroll at which the cursor's row shows whole with the margin round it, nearest to now. */
float scroll_target(const scroll_shape *s, const scroll_view *v);
/* A move by delta rows (0: only select); 1 when the selection showed or moved. */
int scroll_move(const scroll_shape *s, scroll_view *v, long delta);
/* Once per frame: the fling goes on and slows, or the list glides to the cursor's row. */
void scroll_frame(const scroll_shape *s, scroll_view *v);
/* It still moves: a touch now only stops it. */
int scroll_moving(const scroll_view *v);
/* The stylus comes down at y; may_drag: on the rows (not on a button over them). Stops every motion. */
void scroll_grab(const scroll_shape *s, scroll_view *v, int y, int may_drag);
/* The stylus is at y. */
void scroll_drag(const scroll_shape *s, scroll_view *v, int y);
/* The stylus is lifted; 1 when it dragged (then it presses nothing, and the list glides out). */
int scroll_release(scroll_view *v);
/* The scroll bar: the thumb's middle under y on the track from track_y, track_h high; and where the thumb is and how
 * long, as shares of the track (ui_scrollbar). */
void scroll_thumb_to(const scroll_shape *s, scroll_view *v, float y, float track_y, float track_h);
void scroll_thumb(const scroll_shape *s, const scroll_view *v, float *at, float *size);

#endif
