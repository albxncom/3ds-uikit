/* The working loop's timing, in plain C so the host tests run it: while the application works (an install, a load
 * that shows the loader) the menu music gives way to a short loop and comes back after it. audio.c steps this on
 * its own thread, about 200 times a second, and does what it says; the application only says when work begins and ends.
 *
 * Work that ends within WORK_DELAY_MS changes nothing: the loop starts with the loader, not with the job, so a load
 * that is over at once stays as silent as it stays without a loader. Then the loop comes in over
 * WORK_LOOP_FADE_MS while the menu music goes out over WORK_FADE_MS (and pauses, so it goes on later where it
 * was); when the work ends the loop goes out and the menu music comes back the same way. Work that begins again
 * while the loop still sounds (a check right after a download) keeps it going. With the interface sounds switched
 * off there is no loop and the menu music stays as it is. */
#ifndef WORK_H
#define WORK_H

#include <stdint.h>

#include "ui_layout.h"

#define WORK_DELAY_MS LOADER_WAIT_MS    /* as long as the loader waits before it shows: one value for both */
#define WORK_FADE_MS 350        /* the menu music out, and in again */
#define WORK_LOOP_FADE_MS 120   /* the loop in, and out again */

enum { WORK_NONE, WORK_START, WORK_STOP };

typedef struct work_state {
    int depth;                  /* work begun and not ended (an install's own loads count once more) */
    uint64_t since;             /* when the work under way began */
    uint64_t at;                /* the last step's time */
    int sounding;               /* the loop plays (also while it fades out) */
    float menu, loop;           /* the levels now, 0 to 1: the menu music's and the loop's */
} work_state;

/* Nothing under way, the menu music at its level. */
void work_reset(work_state *w, uint64_t now);
void work_begin(work_state *w, uint64_t now);
void work_end(work_state *w);
/* A step to `now` (ms); enabled: the interface sounds are on. Sets the two levels and says what to do with the
 * loop: WORK_START (play it from its beginning), WORK_STOP (it has faded out: stop it), or WORK_NONE. */
int work_step(work_state *w, uint64_t now, int enabled);

#endif
