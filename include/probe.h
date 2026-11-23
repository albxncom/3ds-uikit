/* The flicker probe's comparison (a UI_PROBE build, never delivered; the white flicker, order.h): per screen of the
 * console (the bottom one, the top one) the last frame read is compared with the one before it, as the GPU drew it
 * (the render target) and as the screen reads it (the framebuffer), by their CRC-32s (ui.c reads them, the application says
 * when a screen stands still and logs). A frame counts only once its screen has stood still for more than
 * PROBE_STILL frames in a row: anything that moves on purpose (a key, a transition, the cursor ring, the 3D slider)
 * resets the run, so its frames are never compared.
 *
 * Plain C in this header alone, so the host tests check it (host/test_probe.c). */
#ifndef PROBE_H
#define PROBE_H

#include <stdint.h>

#define PROBE_STILL 30          /* frames a screen stands still before its frames are compared */

/* One shown screen's count on one screen of the console. */
typedef struct probe_tally {
    unsigned compared, target_changes, fb_changes;
} probe_tally;

/* One frame of one screen of the console. run: the frames it has stood still in a row so far; still: whether it
 * stands still now (t may be NULL when it does not); read: whether the last two frames were read, with their CRCs.
 * -1: not compared; else which of the two changed, 1 the render target, 2 the framebuffer, 3 both, 0 neither. */
static inline int probe_compare(unsigned *run, probe_tally *t, int still, int read, uint32_t target_was,
                                uint32_t target_is, uint32_t fb_was, uint32_t fb_is)
{
    *run = still ? *run + 1 : 0;
    if (*run <= PROBE_STILL || !read)
        return -1;
    int target = target_was != target_is, fb = fb_was != fb_is;
    t->compared++;
    t->target_changes += (unsigned)target;
    t->fb_changes += (unsigned)fb;
    return target | fb << 1;
}

#endif
