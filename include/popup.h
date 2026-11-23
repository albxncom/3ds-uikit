/* What a popup and the screen under it take, in plain C so the host tests run it. A popup opens where a button was
 * just pressed and closes where its own button was, and what lies under the stylus changes both times: a question's
 * delete button lay over the last rows of the list behind it, a help's close button over a screen's delete button
 * and a note's close button over an install button, so a double tap deleted a file or started an install.
 *
 * So a touch guard stands at both changes: the popup takes a touch only when the stylus came down POPUP_GUARD_MS or
 * more after its first frame, and the screen under it, once it is closed, only when the stylus came down that long
 * after the close; a touch that was down when the popup opened or closed presses nothing, however long it lasts.
 * Keys: a question (two buttons) has its cursor on the left one, "Abbrechen", hidden until a key shows it; its first
 * press only shows it, as everywhere else. B always cancels; a popup with one button closes on A or B. */
#ifndef POPUP_H
#define POPUP_H

#include <stdint.h>

typedef struct popup_guard {
    int armed;                  /* 0: takes every touch (no popup has opened or closed yet) */
    int timed;                  /* the guard time runs */
    uint64_t from;              /* since then */
    int stale;                  /* the stylus was down when it was armed and has not been lifted since */
} popup_guard;

void guard_arm(popup_guard *g, int touching);               /* a popup opens or closes; touching: the stylus is down */
void guard_from(popup_guard *g, uint64_t now);              /* the guard time runs from now (the first call counts):
                                                               the popup's first frame, or the close */
void guard_lift(popup_guard *g);                            /* the stylus is lifted */
int guard_takes(const popup_guard *g, uint64_t down_at);    /* a touch that came down at down_at may press */

enum { POPUP_KEY_LEFT, POPUP_KEY_RIGHT, POPUP_KEY_A, POPUP_KEY_B };

typedef struct popup_focus {
    int ring;                   /* the cursor shows */
    int at;                     /* 0 the left button (the default), 1 the right one */
} popup_focus;

void focus_open(popup_focus *f);
/* A key in a popup with two buttons (two) or one: -1 nothing to press (the cursor shown or moved), 0 the left button,
 * 1 the right one, or the only one. */
int popup_key(popup_focus *f, int two, int key);

#endif
