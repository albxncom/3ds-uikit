/* The loader and the progress bar: a pretended job of WORK_MS, its progress known (the bar fills) or not (a striped
 * piece slides to and fro). While it runs the bottom screen's content goes back behind the loader, which takes no
 * input, and the sound's working loop is asked for (audio_work); at its end the done sound. */
#include "gallery.h"
#include "ease.h"
#include "widget.h"

#define WORK_MS 2400
#define IN_MS 300               /* the content goes back, and the loader comes in, in this time */
static int job = -1;            /* 0 known, 1 unknown; -1 none */
static u64 since, ended;

static rect target(int i)
{
    return (rect){PAD, PAD + i * (WIDE_H + GAP), BOTTOM_W - 2 * PAD, WIDE_H};
}

static float fraction(u64 now)
{
    return job == 1 ? -1 : ease_span(now, since, WORK_MS);
}

static void press(int i)
{
    sfx(SFX_CONFIRM);
    job = i;
    since = frame_now;
    ended = 0;
    audio_work(1);
}

static int own(const input *in)
{
    if (job >= 0 && !ended && in->now - since >= WORK_MS) {
        ended = in->now;
        audio_work(0);
        sfx(SFX_DONE);
    }
    if (ended && in->now - ended >= IN_MS)
        job = -1;
    return job >= 0;            /* nothing under the loader takes input */
}

static void top(void)
{
    rect c = ui_card_rect();
    ui_head("Loader", job < 0 ? NULL : ended ? "Done" : "Working");
    card_text(job == 1 ? "A job of unknown length: the bar's piece slides to and fro." :
              "The progress bar fills by the job's progress; the loader's bright dot goes round by the clock, never by "
              "the job.");
    ui_progress((rect){c.x + CARD_PAD, c.y + c.h - CARD_PAD - PROGRESS_H, c.w - 2 * CARD_PAD, PROGRESS_H},
                job < 0 ? 0 : ended ? 1 : fraction(frame_now));
    ui_foot(FOOT_DOTS_Y, -1, job < 0 ? "Choose a job" : ended ? "The job is done" : "Working", MUTED, 0, NULL);
}

static void bottom(int held)
{
    ui_wide(FACE_WIDE, target(0), held == 0 ? UI_DOWN : 0, ART_KEEP, "A job of known length", LABEL_PX + 2,
            LABEL_GAP + 4, TILE_INK);
    ui_wide(FACE_WIDE, target(1), held == 1 ? UI_DOWN : 0, ART_QR, "A job of unknown length", LABEL_PX + 2,
            LABEL_GAP + 4, TILE_INK);
    if (job < 0)
        return;
    u64 now = frame_now;
    float in = ease_both(ease_span(now, since + LOADER_WAIT_MS, IN_MS));
    float out = ended ? ease_span(now, ended, IN_MS) : 0;
    ui_loader(ended ? "Done" : "Working", job == 1 ? "Its length is not known" : NULL, in * (1 - out), in * (1 - out));
}

const page page_loader = {.name = "Loader", .symbol = ART_DOWNLOAD, .count = 2, .columns = 1, .target = target,
                          .press = press, .top = top, .bottom = bottom, .own = own};
