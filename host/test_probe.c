/* The flicker probe's comparison (probe.h), with frames of the top screen's size and their CRC-32s taken as
 * ui.c takes them (zlib): still frames that are the same count no change, one pixel changed counts one, and a frame
 * while something moves on purpose (the run of still frames broken) is not compared, nor are the PROBE_STILL after.
 *
 *   test_probe   prints one line per check and "N failed"; exit 1 if any failed */
#include "probe.h"

#include <stdio.h>
#include <string.h>
#include <zlib.h>

#define TARGET_BYTES (240 * 400 * 4)
#define FB_BYTES (240 * 400 * 3)

static unsigned char target[2][TARGET_BYTES], fb[2][FB_BYTES];
static int failed;

static void judge(const char *what, int ok)
{
    printf("%s: %s\n", what, ok ? "ok" : "FAILED");
    failed += !ok;
}

static uint32_t crc(const void *data, size_t size)
{
    return (uint32_t)crc32(0L, data, (uInt)size);
}

/* Frame n of a stream, read into slot n & 1 as ui.c reads (the frame before stays in the other slot) and compared:
 * the base picture, with one pixel of the render target (at) or of the framebuffer (fb_at) flipped, -1 none. */
static unsigned char base_target[TARGET_BYTES], base_fb[FB_BYTES];

static int frame(unsigned *run, probe_tally *t, int still, unsigned n, long at, long fb_at)
{
    int k = (int)(n & 1);
    memcpy(target[k], base_target, TARGET_BYTES);
    memcpy(fb[k], base_fb, FB_BYTES);
    if (at >= 0)
        target[k][at] ^= 1;
    if (fb_at >= 0)
        fb[k][fb_at] ^= 0x80;
    return probe_compare(run, t, still, n > 0, crc(target[k ^ 1], TARGET_BYTES), crc(target[k], TARGET_BYTES),
                         crc(fb[k ^ 1], FB_BYTES), crc(fb[k], FB_BYTES));
}

int main(void)
{
    for (size_t i = 0; i < TARGET_BYTES; i++)
        base_target[i] = (unsigned char)(i * 7 + i / 1600);
    for (size_t i = 0; i < FB_BYTES; i++)
        base_fb[i] = (unsigned char)(i * 3 + i / 1200);

    /* the same frames, standing still: compared only after PROBE_STILL frames, none changed */
    unsigned run = 0, n = 0;
    probe_tally t = {0};
    int early = 0;
    for (int i = 0; i < PROBE_STILL; i++)
        early += frame(&run, &t, 1, n++, -1, -1) != -1;
    judge("the first PROBE_STILL still frames are not compared", !early && t.compared == 0);
    for (int i = 0; i < 100; i++)
        frame(&run, &t, 1, n++, -1, -1);
    judge("100 still frames that are the same: 100 compared, 0 changed",
          t.compared == 100 && t.target_changes == 0 && t.fb_changes == 0);
    judge("the last two frames not read: not compared",
          probe_compare(&run, &t, 1, 0, 1, 2, 3, 4) == -1 && t.compared == 100);

    /* one frame with one pixel of the render target changed: it differs from the frame before and the one after */
    int got = frame(&run, &t, 1, n++, TARGET_BYTES / 2, -1);
    judge("one pixel of the render target changed: counted 1 (the target only)",
          got == 1 && t.target_changes == 1 && t.fb_changes == 0 && t.compared == 101);
    got = frame(&run, &t, 1, n++, -1, -1);
    judge("the frame after it, the same as before again: counted 1 more", got == 1 && t.target_changes == 2);
    got = frame(&run, &t, 1, n++, -1, -1);
    judge("then the same frames: 0 changed", got == 0 && t.target_changes == 2 && t.compared == 103);

    /* the last pixel of the framebuffer */
    got = frame(&run, &t, 1, n++, -1, FB_BYTES - 1);
    judge("one pixel of the framebuffer changed: counted 1 (the framebuffer only)",
          got == 2 && t.fb_changes == 1 && t.target_changes == 2);
    got = frame(&run, &t, 1, n++, -1, -1);
    judge("the frame after it: counted 1 more", got == 2 && t.fb_changes == 2 && t.target_changes == 2);

    /* a frame during a motion (not still): not compared though it differs, no tally touched (NULL allowed) */
    probe_tally before = t;
    got = probe_compare(&run, NULL, 0, 1, 1, 2, 3, 4);
    judge("a frame during a motion is not compared", got == -1 && run == 0 && !memcmp(&before, &t, sizeof t));
    early = 0;
    for (int i = 0; i < PROBE_STILL; i++)
        early += frame(&run, &t, 1, n++, i == 3 ? 17 : -1, -1) != -1;
    judge("nor the PROBE_STILL frames after it, changed or not", !early && !memcmp(&before, &t, sizeof t));
    got = frame(&run, &t, 1, n++, -1, -1);
    judge("then still frames are compared again", got == 0 && t.compared == before.compared + 1 &&
                                                     t.target_changes == 2 && t.fb_changes == 2);

    printf("%d failed\n", failed);
    return failed != 0;
}
