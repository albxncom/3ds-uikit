/* Frost's own numbers (look_frost.c, and look_frost.py through art.py's read_layout): only what its code needs as
 * numbers; its colours and strengths are look_frost.py's (they follow the scheme). #defines only. */
#ifndef LOOK_FROST_H
#define LOOK_FROST_H

#define FROST_STRIP_GAP 2           /* a strip on the top screen ends this far before the dotted line's place */
#define FROST_LIP_TILE 4            /* a thick glass's side below its face: tiles and buttons (radius 16) */
#define FROST_LIP_ROW 3             /* ... and rows (radius 10) */
#define FROST_RIM 3                 /* the pattern under glass keeps this far from the glass's edge; the glass's rim,
                                       drawn after the pattern (look_frost.py, rim_of), is what lies outside that */
#define FROST_STRIP_RIM 2           /* a strip's pattern ends this far before its edge on the screen's side */
#define FROST_DLG_BTN_H 36          /* the popup's buttons: lozenges on the panel, FROST_DLG_BTN_UP above */
#define FROST_DLG_BTN_UP 2          /* ... the band's top (hits.c), so that the panel's face shows below them */
#define FROST_DLG_BTN_IN 6          /* ... each this far inside its half of the band (hits.c) at the sides */
#define FROST_DLG_GROW 4            /* the popup's panel this much taller below than DLG_H, as its buttons are taller
                                       than they were (32): the air below them stays; they stay inside their band */
#define FROST_LOADER_DIAL_R2 31     /* the loader: eight panes like a dial, their middles half this (15.5 px) */
#define FROST_LOADER_LIT 24         /* ... from LOADER_BOX's middle; a pane's lit image this many px square round it */
#define FROST_HELP_LIP 3            /* the help button (HELP_FACE as every look's): its face (the element
                                       less this lip below) holds the mark centred */
#define FROST_HELP_MARK_Y 4         /* ... with 2 px or more round it: the mark's top this far down the element, */
#define FROST_HELP_MARK_DOWN 3      /* ... this much lower held, as the held face sinks into the lip */
#define FROST_TAB_GROW 1            /* the corner buttons (the settings tab, the way back) this much taller below */
#define FROST_TAB_ICON_DX (-1)      /* the settings tab's symbol this far from CORNER_PAD, and its top at */
#define FROST_TAB_ICON_Y 3          /* ... this: even air above and below it on the face, clear of the round corner */

#endif
