/* A look: how the screens are drawn in one design. Each look is a file of its own, look_<name>.c (its art drawn
 * by look_<name>.py into art_<name>.c), and fills this table; ui.c and the widgets drawn with it (widget.h) lay out
 * every screen once and call the table where the looks differ. A look draws only with ui.h and kit.h. Stock is the
 * fallback. (G1 plan, B1) */
#ifndef LOOK_H
#define LOOK_H

#include "ui.h"

/* The looks by their number, the settings' (settings.h: design=stock|frost|night|prism) and ui_look's; Stock is
 * the one every build holds and stands in for one a build does not hold. */
enum { LOOK_STOCK, LOOK_FROST, LOOK_NIGHT, LOOK_PRISM, LOOK_COUNT };

/* The bar's parts (look_draw.bar): the left tab, the right one, both, or the whole bar. */
enum { BAR_LEFT = 1, BAR_RIGHT = 2, BAR_WIDE = 4 };

/* The loader's square (look_draw.loader), px: ui.c sets its line LOADER_GAP under it. */
#define LOADER_BOX (2 * LOADER_BEAD_R + LOADER_BEAD)

typedef struct look_draw {                  /* functions only; a look gives every one */
    void (*load)(void);                     /* chosen: make own textures */
    void (*unload)(void);
    void (*refresh)(int scheme, int pattern);   /* the atlas (pattern -1) or the pattern's tile just changed */
    void (*ground)(float y0, float y1, float alpha);    /* the screen drawn now (ui_f.top) from y0 to y1 (the whole:
                                                           0, SCREEN_H), a moving background too (A1) */
    void (*rule)(float y);                  /* the top screen's line at y */
    void (*card)(rect r, int glass);        /* glass -1: the card; else the glass card in that style (GLASS_*) */
    rect (*face)(int kind, rect r, int state, int symbol);   /* FACE_*, UI_DOWN | UI_OFF, the symbol on it (-1
                                                                none); returns where the content is laid out */
    void (*select)(rect r, const int radius[4]);
    void (*bar)(int parts, int held);       /* BAR_LEFT | BAR_RIGHT, or BAR_WIDE; held -1, 0, 1, 2 */
    void (*bar_label)(const char *s, float x, float baseline, int px, int align, int down);
    void (*popup_panel)(rect r);
    void (*popup_buttons)(const char *left, const char *right, int held, int ring, int removes);
    void (*loader)(float cx, float cy, u64 ms, float alpha);    /* the ring shown while the application works:
                                                                   its middle at cx, cy on the screen drawn now, in
                                                                   LOADER_BOX around it; moving by the clock ms; as
                                                                   solid as alpha (0 to 1) */
} look_draw;

typedef struct look_metrics {               /* shorts only, none 0 but the last two */
    short dim_pct, dlg_h;                   /* the popup's dim over the screen (% alpha), its panel's height (the
                                               text stays in the DLG_H - DLG_BTN_H above the buttons' band) */
    short tab_face, tab_face_down, tab_h, tab_icon_x, tab_icon_y;   /* the settings tab: images, height, wrench */
    short corner_h, corner_icon_x, corner_icon_y;                   /* the folder button: height, its icon */
    short help_size, help_mark_y, help_mark_down;  /* the help button: its element (HELP_FACE in every look, centred
                                               on HELP_X and HELP_Y's HELP_SIZE), its mark's top in it, how far the
                                               mark sinks held */
    short pane_rim_big, pane_rim_small;     /* the carousel's panes: their rims in px of their images */
    short pane_apart;                       /* ... the neighbours this far further out than the slots (0: tucked) */
    short ground_sun;                       /* 1: the look's ground draws a light of its own (ART_DROP_SUN) on
                                               every top screen, so a screen that would draw one leaves it out */
} look_metrics;

typedef struct look {
    const char *name;
    const look_art *art;
    look_draw draw;
    look_metrics m;
} look;

/* A look registers by its file being there: make's LOOKS (every look_<name>.c present, Stock always) writes
 * looks_built.h, LOOKS_HELD and LOOKS_BUILT(X) with an X(name, NAME) per look held, in the numbers' order; look.c
 * lists them. */
extern const look look_stock, look_frost, look_night, look_prism;

/* The look of that number when this build holds it, else Stock. */
const look *look_get(int id);
/* 1 when this build holds the look of that number. */
int look_built(int id);

#endif
