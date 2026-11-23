/* The order in which a screen's pictures and texts reach the GPU (against a white flicker). On the console a batch
 * of letters drawn directly after the texture of the themes' icons sometimes took its texels from stale data for one
 * frame: the letters on the text's first one or two font sheets vanished. A test build that drew each row's icon
 * after the row's texts had no changed frame in 708 still ones.
 *
 * The rule, kept in one place so that no screen can break it: a picture is not drawn when it is asked for. It waits,
 * and the waiting pictures are drawn, in their order, only before an image that could cover one of them and when
 * the screen is done. Text never draws them. So the texts that follow a picture come before it, and what comes after
 * a picture's texture is an image. Text is therefore never laid over a picture.
 *
 * Plain C in this header alone (no .c file of its own): the drawing layer (ui.c) says what is
 * asked for and draws when told; the host tests check the order. */
#ifndef ORDER_H
#define ORDER_H

#define ORDER_MAX 32            /* pictures that can wait at once (every picture waits: symbols, panes, corners) */

typedef struct order_rect {
    float x, y, w, h;
} order_rect;

typedef struct order {
    void (*draw)(void *ctx, int k, order_rect at);  /* draws the picture that waited as k-th */
    void *ctx;
    order_rect at[ORDER_MAX];
    int count;
} order;

/* The screen is done, or something of no known place is about to be drawn: the waiting pictures are drawn. */
static inline void order_flush(order *o)
{
    int n = o->count;
    o->count = 0;               /* first: draw may ask for images itself */
    for (int k = 0; k < n; k++)
        o->draw(o->ctx, k, o->at[k]);
}

/* A picture is asked for at r: it waits. Returns its place k among the waiting ones (when ORDER_MAX wait already,
 * they are drawn first). */
static inline int order_picture(order *o, order_rect r)
{
    if (o->count == ORDER_MAX)
        order_flush(o);
    o->at[o->count] = r;
    return o->count++;
}

/* An image is about to be drawn on r: if it could cover a waiting picture, all of them are drawn first. reach: how
 * far a picture and an image may be shifted against each other on the screen (the top screen's layers). */
static inline void order_image(order *o, order_rect r, float reach)
{
    for (int k = 0; k < o->count; k++) {
        const order_rect *p = &o->at[k];
        if (r.x < p->x + p->w + reach && r.x + r.w > p->x - reach && r.y < p->y + p->h + reach &&
            r.y + r.h > p->y - reach) {
            order_flush(o);
            return;
        }
    }
}

#endif
