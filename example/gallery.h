/* The kit's example: a gallery of its elements, one page each, built on the kit's headers, libctru, citro2d and
 * citro3d alone. main.c runs the frame and the input every page shares (touch and keys, the cursor, B back, START
 * leaves); each page_<name>.c says what its page shows and what its targets do. All text is the example's own. */
#ifndef GALLERY_H
#define GALLERY_H

#include <3ds.h>
#include "ui.h"
#include "look.h"
#include "audio.h"

/* The example's own numbers, which the kit leaves to an application's layout header. */
#define PAD 12                  /* the bottom screen's targets from its sides */
#define GAP 6                   /* between two targets */
#define LABEL_PX 13             /* a target's label */
#define LABEL_LH 16
#define LABEL_GAP 4             /* between a symbol and its label */
#define CARD_PX 14              /* text on the top screen's card */
#define CARD_LH 19
#define FADE_MS 400             /* the gallery fades in from black at its start */

/* What a frame's input holds: the keys as libctru gives them, the stylus, the clock. */
typedef struct input {
    u32 down, held, up;
    int x, y;
    u64 now;
} input;

/* A page. Its targets are what a touch or the cursor can press, from 0 to count - 1, laid out in columns for the
 * D-pad; press runs when one is pressed (the stylus lifted on it, or A on the cursor's). bottom draws the bottom
 * screen with held, the target the stylus holds (-1 none). Optional: open when the page opens, own for input the
 * page takes itself before the shared rules (1: taken), right for the bar's right tab (its label; NULL: none; it
 * presses target count), a page's own bar (bar) with the tabs it has (tabs: TAB_LEFT | TAB_RIGHT; else the left, and
 * the right with right) and its own cursor (select; else ui_select at TILE_R). top and bottom only draw: they read the
 * frame's time from frame_now and change nothing, as the right eye's pass runs them again with the 3D slider up. */
typedef struct page {
    const char *name;
    int symbol;
    int count, columns;
    rect (*target)(int i);
    void (*press)(int i);
    void (*top)(void);
    void (*bottom)(int held);
    void (*open)(void);
    int (*own)(const input *in);
    const char *right;
    void (*bar)(int held);      /* held: -1, 0 the left tab, 1 the right */
    int (*tabs)(void);
    void (*select)(int i);
} page;

enum { TAB_LEFT = 1, TAB_RIGHT = 2 };

extern const page page_start, page_tiles, page_buttons, page_list, page_settings, page_text, page_popups,
    page_loader, page_top, page_pictures, page_sound;

/* main.c: the gallery's shared state and helpers. */
extern int cursor_shown;        /* the cursor shows (the first D-pad press shows it, a touch hides it) */
extern int cursor;              /* the target it is on */
extern char sound_line[160];    /* the sound's last log line (audio_log) */
extern u64 frame_now;           /* the frame's time, read once at its input (in.now): the pages' clock */
void open_page(const page *p);  /* a touch that is still down presses nothing on it */
void forget_touch(void);        /* the touch that is still down presses nothing: a popup opens or closes */
void back(void);
void card_text(const char *s);  /* a paragraph wrapped on the top screen's card */
const char *of(int n, int count);   /* "n of count", for a note */

/* page_settings.c: what the settings page chose, as ui_look takes it. */
extern int look_now, scheme_now, glass_now, pattern_now;

#endif
