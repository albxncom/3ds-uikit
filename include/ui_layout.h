/* The kit's share of the interface's layout, in console pixels: every colour (COL_*) and each size, position and
 * time the kit's C uses, directly or through another define, or that art.py reads to draw one of the
 * kit's sprites (its own or one an application draws too). An application's layout header includes this file
 * first and holds the rest; art.py reads this file, or with --app the header the hook names (LAYOUT). */
#ifndef UI_LAYOUT_H
#define UI_LAYOUT_H

/* ---------------------------------------------------------------- colours, 0xRRGGBB */

#define COL_TOP_BG_A 0xF2EBB3       /* top screen: a vertical gradient from A (y 0) to B (y 240) */
#define COL_TOP_BG_B 0xF2E9A4
#define COL_BOT_BG_A 0xF1E9A4       /* bottom screen, the same */
#define COL_BOT_BG_B 0xF1E793
#define COL_BG 0xF1E9A4             /* the background where one colour is enough (banner, scan frame) */
#define COL_INK 0x615A4B            /* text on the top screen */
#define COL_MUTED 0x63593F          /* header, footer, notes */
#define COL_TILE_INK 0x756E60       /* text on tiles and rows */
#define COL_PRESS_INK 0xC88201      /* the same while held down */
#define COL_DOTS 0xAFA56E           /* the dotted lines */
#define COL_CARD 0xDCD49A
/* tiles, rows, the corner button: a light face that darkens towards the bottom, shaded towards its sides and
 * strongly towards its lower edge, a 1 px edge that darkens from top to bottom, a soft shadow below */
#define COL_FACE_TOP 0xFEFCF5       /* the face 2 px below the top edge */
#define COL_FACE_LOW 0xE0DCCB       /* ... and 16 px above the lower edge (the gradient goes on beyond) */
#define COL_SHADE 0xA8A07A          /* the inner shading's colour */
#define SHADE_SIDE 25               /* its strength next to a side, in percent, falling by e every 4.5 px */
#define SHADE_LOW 80                /* ... next to the lower edge, falling by e every 5 px */
#define COL_EDGE_TOP 0xDCD4A6       /* the edge at the top ... */
#define COL_EDGE_LOW 0x887E4E       /* ... and at the bottom */
#define COL_SHADOW 0x5A4E28         /* the drop shadow: 1 px narrower, its lower edge SHADOW_DROP_10 tenths of a px */
#define SHADOW_ALPHA 62             /* below the element's, blurred (sigma 1.2); re-measured under a resting tile */
#define SHADOW_DROP_10 15
#define EDGE_LOW_100 140            /* the edge along the bottom, in hundredths of a px (re-measured) */
#define PRESS_REF_H 78
/* held down (the System Settings' held tile, re-measured 2026-11-13): 1 px smaller, no shadow, yellow; a rim of two
 * rows along the top and the last eight rows along the bottom as measured (from the bottom up), the face between.
 * That tile is PRESS_REF_H px high. On a lower element (a list row, the corner button) the same eight rows and the
 * shade above them took up to half of the face and read as a dark band: there the shade towards the lower
 * edge is as much shallower and as much weaker as the element is lower, and the face's gradient goes no further
 * than on that tile, so a held row ends in a deeper yellow, not in olive. */
#define COL_PRESS_TOP 0xFFED7F
#define COL_PRESS_LOW 0xEADB6A
#define COL_PRESS_SHADE 0xACA630
#define COL_PRESS_RIM_0 0xE3D58C
#define COL_PRESS_RIM_1 0xF1DE7F
#define COL_PRESS_LOW_0 0xD0C06A
#define COL_PRESS_LOW_1 0xC1B248
#define COL_PRESS_LOW_2 0xCBC04B
#define COL_PRESS_LOW_3 0xD0C550
#define COL_PRESS_LOW_4 0xD4C854
#define COL_PRESS_LOW_5 0xD8CC58
#define COL_PRESS_LOW_6 0xDBCE5B
#define COL_PRESS_LOW_7 0xDDD05D
#define COL_SYMBOL_LIGHT 0xFFFFFF   /* the light under each symbol, 1 px down, that sets it into the face */
#define SYMBOL_LIGHT_ALPHA 65
#define COL_SELECT_IN 0x1787C4      /* the selection's corners when in (our blue, the colour of the earlier ring) */
#define COL_SELECT_OUT 0x3197CE     /* ... and when out: from COL_SELECT_IN as HOME's in 1DC6DE becomes its out
                                       3BDCEC (art.py held_like: luminance x1.27, saturation as far towards full) */
#define COL_OFF 0xE9E0B5            /* greyed: "Schon installiert", the corner button at the card's root, Badges */
#define COL_OFF_EDGE 0xCFC493
#define COL_OFF_INK 0x8F8558
#define COL_TEAL 0x16877C           /* symbols */
#define COL_ORANGE 0xB55A14
#define COL_ALERT 0xC4281C          /* red: the pack is nearly full (own choice, no console reference) */
#define COL_FOLDER 0xC2600C
#define COL_GREEN 0x0F766D          /* progress, steps, the check after an install */
#define COL_GO_TOP 0x128074         /* the green button, the same make as a tile */
#define COL_GO_LOW 0x0B5B54
#define COL_GO_SHADE 0x06403A
#define COL_GO_EDGE_TOP 0x2F8F84
#define COL_GO_EDGE_LOW 0x063F39
/* the bar along the bottom screen's lower edge: grey, three stops from top to bottom, a light rim along its top
 * edge, a neutral shadow around it (sigma 3) */
#define COL_BAR_TOP 0x7A7A7A
#define COL_BAR_MID 0x787878        /* 10 px below its top */
#define COL_BAR_LOW 0x676767
/* a half held down turns yellow (console screenshot of 2026-11-13): its rows from the top (the number at a name's
 * end), linear between; no shadow above it; label and key in COL_PRESS_INK, not moved, and where the dark copy is
 * at rest (LABEL_SHADOW_DY px above) a white one (re-measured on the same screenshot against the bar at
 * rest: white, 1.21 times as strong as the dark copy, the median of 66 label pixels, quartiles 1.20 and 1.22) */
#define COL_BAR_PRESS_00 0xFFF4B8
#define COL_BAR_PRESS_01 0xFFF1A2
#define COL_BAR_PRESS_02 0xFFEA75
#define COL_BAR_PRESS_03 0xFFE65B
#define COL_BAR_PRESS_04 0xFFE65A
#define COL_BAR_PRESS_10 0xFFE65A
#define COL_BAR_PRESS_30 0xEFD94E
#define COL_BAR_LIGHT 0xFFFFFF      /* the rim: this colour, at most BAR_LIGHT_ALPHA percent, about 2 px wide */
#define BAR_LIGHT_ALPHA 56
#define COL_BAR_SHADOW 0x3C3C3C
#define BAR_SHADOW_ALPHA 71
#define COL_LABEL 0xFFFFFF          /* its label, engraved: a dark copy 2 px above it */
#define COL_LABEL_SHADOW 0x000000
#define LABEL_SHADOW_ALPHA 38
#define LABEL_SHADOW_DY 2
#define COL_LABEL_HELD_SHADOW 0xFFFFFF
#define LABEL_HELD_SHADOW_ALPHA 46  /* LABEL_SHADOW_ALPHA x 1.21 */
/* the teal liquid of the download's bar and the installing screen's line (in glass by style, art.py
 * "installing"), towards the light and away from it. The stripes over the bar's fill: as strong over its whole
 * height, cut by the fill's own outline (art.py stripe_ends). */
#define COL_TUBE_FILL_TOP 0x2FA295
#define COL_TUBE_FILL_LOW 0x09504A
#define COL_STRIPE 0xFFFFFF
#define STRIPE_ALPHA 28
#define COL_TRACK 0xD5CB99          /* the scroll bar */
#define COL_THUMB_LOW 0xE0D8B6
#define COL_THUMB_EDGE 0xA59A6B
#define COL_THUMB_PRESS_A 0xFFE65A  /* the thumb held: NOT MEASURED, the bar's held colours until a console */
#define COL_THUMB_PRESS_B 0xEFD94E  /* screenshot of it exists */
#define COL_THUMB_PRESS_EDGE 0xB9A640
#define COL_CAMERA 0x3A3832         /* the top screen before the first camera frame */
#define COL_WHITE 0xFFFFFF

/* ---------------------------------------------------------------- screens and text */

#define TOP_W 400
#define BOTTOM_W 320
#define SCREEN_H 240
#define FONT_PX 26                  /* a system font at scale 1 matches the prototype's font at 26 px */
#define FONT_CAP 19                 /* cap height of the system font at scale 1 */

/* ---------------------------------------------------------------- top screen; an application's own in its layout header */

#define TOP_PAD 16                  /* left and right */
#define DOT 3                       /* the dotted lines: dots of 3 px, one every 6 px */
#define DOT_STEP 6
#define LINE_PX 14                  /* header and footer lines */
#define LINE_LH 21
/* an application's own numbers of this group: in its layout header */
#define CAP_LINE 10                /* LINE_PX */
#define CAP_ROW_NAME 10             /* ROW_NAME_PX */

/* an application's own numbers of this group: in its layout header */
#define HERO_DOTS_Y 20

/* every other screen: header line, dotted line, card, dotted line, footer line */
#define HEAD_Y 12                   /* the header's line box */
#define HEAD_DOTS_Y 42
#define CARD_Y 54
#define CARD_H 132
#define FOOT_DOTS_Y 195             /* the lower dotted line: ui_foot's dots_y, its footer line under it at FOOT_Y */
#define FOOT_Y 207                  /* the footer's line box */
#define CARD_R 10
#define CARD_PAD 16                 /* inside the card, left and right; above choices at its top (ui_choices_top) */
#define DETAIL_ICON 48
#define DETAIL_ICON_R 6             /* the theme icon's rounded corners, at 48 px */

/* an application's own numbers of this group: in its layout header */
#define PROGRESS_H 12
#define PROGRESS_R 6
#define STRIPE_W 12                 /* the stripes: one light and one dark band every 12 px, at 45 degrees */
#define STRIPE_MS 600               /* they move one period in this time */

/* an application's own numbers of this group: in its layout header */
#define FOOT_ICON_GAP 6             /* after a symbol in the footer (the house of the status line, the folder) */
#define FOOT_WARN_GAP 5             /* between the warning symbol and its text at the footer's right end */
#define WARN_ICON 40

/* ---------------------------------------------------------------- bottom screen; an application's own in its layout header */

/* the bar across the screen, on the start screen only: as in the System Settings, 2 px in from the screen's
 * sides, 31 px high, its upper corners curves 28 px across and down to the screen's edge (quarter ellipses), the
 * label's capitals 14 px high. Every other screen has a tab in a lower corner for each option: one option, the left
 * tab alone; two, B left and A right */
#define BAR_Y 209
#define BAR_H 31
#define BAR_X 2
#define BAR_RX 28
#define BAR_SHADOW 9                /* how far its shadow reaches above it */
#define BAR_PX 19
/* two options: two tabs instead of the bar, one from each lower corner, the middle empty (the System Settings', as
 * measured: the body to x 119 at the bottom and its upper inner corner the bar's quarter ellipse; mirrored on the
 * right); each label centred on its tab's straight part (measured: the console's labels' middles at 54 and 265),
 * its key in front of it while the two fit within TAB_TEXT_W */
#define TAB_W 120
#define TAB_LABEL_X1 54
#define TAB_LABEL_X2 265
#define TAB_TEXT_W 104
#define TAB_RX 31
#define TAB_RY 27                   /* from the rim's first row 209 */
#define TAB_EDGE_10 1185            /* the inner edge's x, in tenths of a px, fitted with TAB_RX and TAB_RY */
#define BAR_LABEL_DY 0              /* the label below its measured place (the console's); 2 is under discussion */
/* the keys stand on the rows of the console's own key symbol (rows 217 to 231 of the System Settings' bar), engraved
 * as the label is */
#define KEYCAP_Y 217
#define KEYCAP 15                   /* the A and B keys: our own ring and letter, 15 px across */
#define KEYCAP_GAP 8
#define START_H 15                  /* the START key: a pill with the word in it */
#define START_R 7
#define START_PAD 5
#define START_PX 9
#define TILE_H 144

/* an application's own numbers of this group: in its layout header */
#define TILE_X1 12
#define TILE_R 16                   /* the System Settings' tiles: 17 at 78 px high */
#define TILE_PAD 8
#define MAIN_ICON 56
/* the two symbols of the start screen's tiles and of the Designs and Badges screens' tops: glass as the
 * logo's, in the three glass styles. "Designs" a fan of three colour samples, SWATCH_FAN degrees apart on one pin;
 * "Badges" a seal. Shard's glass is made of the colours below; Clear's samples keep SWATCH_CLEAR_TINT % of theirs,
 * so they still read as three colours. */
#define COL_SWATCH_A 0x8F6CF2       /* the samples from the left, each with the shade inside its glass */
#define COL_SWATCH_A_SHADE 0x3B2F7A
#define COL_SWATCH_B 0x16B8A4
#define COL_SWATCH_B_SHADE 0x0B4F48
#define COL_SWATCH_C 0xF0683E
#define COL_SWATCH_C_SHADE 0x7E2A12
#define COL_SEAL 0xF0A92A
#define COL_SEAL_SHADE 0x8A4B0A
#define DTILE_H 112
#define WIDE_H 66
#define DESIGNS_ICON 40
/* The selection, as the HOME Menu's (measured on its folder selector, bottom screen, 2026-11-13, on the plain
 * wallpaper): four corners, each a quarter of a rounded outline around the item, concentric with the item's own
 * corner (radius SELECT_RADII: 0, a row's and a tile's), SELECT_W px thick from SELECT_OUT px outside the item's
 * edge inwards (so 1 px over the item, as HOME's), the arms SELECT_TAIL px straight past the curve to the middle of
 * their round ends; soft edges (SELECT_AA_10 / 10 px) and a faint haze outside (SELECT_HAZE % at 1 px, falling by
 * SELECT_HAZE_FALL % a px); breathing SELECT_AMP px out and back once per SELECT_MS (a sine), lighter the further
 * out (COL_SELECT_IN to COL_SELECT_OUT). */
#define SELECT_W 4                  /* HOME's is 5 on the wallpaper; one thinner, as asked */
#define SELECT_OUT 4
/* the arms' straight part past the curve, to the middle of the round end: SELECT_TAIL_PCT % of the item's shorter
 * side, but never closer than SELECT_GAP_MIN to the arm from the next corner, and between SELECT_TAIL_MIN and
 * SELECT_TAIL_MAX; drawn for the lengths the items need (art.py select_corners) */
#define SELECT_TAIL_PCT 16
#define SELECT_GAP_MIN 5
#define SELECT_TAIL_MIN 3
#define SELECT_TAIL_MAX 8
#define SELECT_AA_10 12
#define SELECT_HAZE 30
#define SELECT_HAZE_FALL 55
#define SELECT_AMP 3
#define SELECT_MS 1000
#define SELECT_PAD 12               /* the item's corner this far in from the corner image's: SELECT_OUT and
                                       SELECT_FADE */
#define SELECT_FADE 8               /* the haze's room in a corner image, outside the band and past the arms' ends:
                                       there it has fallen to 1/255 (with 2 and 3 px it ended at a visible
                                       edge along the image's sides) */
#define FACE_PAD_X 3                /* how far a drop shadow reaches beside and below its element */
#define FACE_PAD_LOW 7

/* the folder view: the rows scroll from the screen's top edge down to the list's end, under the corner button that
 * floats over them at the top left. The button holds the folder-up symbol and the folder's path; it grows with the
 * path up to CORNER_MAX_W, a longer path is cut at its beginning ("...") so the last folder stays readable. */
#define CORNER_W 48                 /* the button at its smallest */
#define CORNER_H 32
#define CORNER_R 16
#define CORNER_ICON 22
#define CORNER_PAD 13               /* left of the symbol */
#define CORNER_GAP 6                /* between symbol and path */
#define CORNER_PAD_R 14             /* right of the path, where the corner is round */
#define CORNER_MAX_W 192            /* 60 % of the screen */
#define CORNER_PX 13
#define ROW_H 50
/* an application's own numbers of this group: in its layout header */
#define ROW_R 10
#define ROW_PAD 10
#define ROW_NAME_PX 14
#define ROW_SUB_PX 11
#define FOLDER_H 38

#define FOLDER_ICON 22
#define FOLDER_TEXT_X 42
#define SCROLL_W 10
#define SCROLL_R 5
#define THUMB_MIN 20
#define DRAG_START 6                /* a touch that moves this far scrolls instead of choosing */
#define TOUCH_HOLD_MS 80            /* a touch on a row shows held after resting this long (0: at once) */
#define POPUP_GUARD_MS 350          /* a touch counts only this long after a popup's first frame or its close */
#define GLIDE_PCT 25                /* the list glides this share of the way to where it should be, per frame */
#define FLING_PCT 92                /* after a drag, the list's speed keeps this share per frame */
#define GO_H 84

/* an application's own numbers of this group: in its layout header */
#define GO_R 16
#define GO_ICON 28
#define GO_PX 22
#define GO_GAP 10
/* an application's own numbers of this group: in its layout header */
#define ALT_PX 18

/* What is known about a design, as one family: a short list in a box of the card's material (information,
 * not a button), centred on the bottom screen. Each row a symbol FACT_ICON across and one line; the rows aligned
 * left as a list, the box as wide as its longest row; a line too long is cut at its end, a path at its beginning. */
#define FACT_PX 15
#define FACT_ROW_H 22
#define FACT_ICON 16
#define FACT_GAP 8
#define FACT_PAD_X 14
#define FACT_PAD_Y 8
#define CHOICE_H 110
#define CHOICE_ICON 36

/* The information box (ui_info_box): every problem, refusal and empty state explains itself on the bottom
 * screen in a box of the card's material (as the facts and the pill), never as bare text on the ground.
 * Text only, no sign: the text from the box's inner left edge, the block centred in the
 * box's height, in a box INFO_WIDE_W wide. The text at INFO_PX (down to INFO_MIN_PX, the floor of bare body
 * text), a path or a code in a line of its own at INFO_PATH_PX in the muted ink. The box stands centred in the
 * height left above the bar, INFO_AIR clear of the bar's shadow. A popup that refuses shows a sign, INFO_ICON across
 * (ART_INFO_*), above its text, INFO_POP_GAP over it. INFO_LONG: a text that does not fit the box even at
 * INFO_MIN_PX (an install that failed and lost designs: the reason, the way back and the log) takes the wide box:
 * the whole width but INFO_TIGHT_AIR at each side, the height down to the bar's shadow (when the caller gave it all),
 * INFO_TIGHT_PAD inside.
 * Every sentence stays: the text has priority over the form */
#define INFO_ICON 28
#define INFO_WIDE_W 296
#define INFO_PAD_X 14
#define INFO_PAD_Y 12
#define INFO_PX 14
#define INFO_MIN_PX BODY_MIN_PX
#define INFO_TIGHT_AIR 4
#define INFO_TIGHT_PAD 8
#define INFO_LH_PCT 135
#define INFO_PATH_PX 12
#define INFO_PATH_GAP 4
#define INFO_AIR 8
#define INFO_POP_GAP 8
/* the help button at the top right (X or a tap): a round face with the help symbol */
#define HELP_SIZE 32
#define HELP_X 278
#define HELP_Y 10
#define HELP_ICON 22
#define HELP_FACE 34                /* its element in every look, centred on HELP_SIZE's square */
#define HELP_HIT 44                 /* its touch target, a square around its middle */

/* A popup over the bottom screen, rebuilt by measuring the console's own dialog (a screenshot of 2026-11-13; nothing
 * taken from it): the screen behind dimmed, a cream panel with round corners and a soft shadow, the text centred,
 * one button across the panel's lower edge. */
#define COL_DIM 0x000000            /* the screen behind keeps 37 % of its light */
#define DIM_ALPHA 63
#define DLG_X 10
#define DLG_Y 13
#define DLG_W 300
#define DLG_H 215
#define DLG_R 14
/* the panel's colours, measured column by column and row by row: popup_band.h */
#define COL_DLG_SHADOW 0x000000
#define DLG_SHADOW_ALPHA 62
#define DLG_SHADOW_BLUR 8
#define COL_DLG_INK 0x141414
#define DLG_PX 18                   /* the text: capitals 13 px high, lines 23 px apart, paragraphs 20 px more */
#define DLG_LH 23
#define DLG_PARA 20
#define DLG_MIN_PX 11               /* a longer text shrinks down to this (wrap_paragraphs) */
#define DLG_TEXT_W 268
#define DLG_PAD_Y 10                /* above and below the text, at least */
/* the button band: DLG_BTN_H px from a dark line across the panel to its lower edge, its rows and edge columns as
 * measured (popup_band.h); one row of the panel above it in its shadow (the panel's colour times
 * DLG_SHADOW_ROW_PER_MILLE / 1000); its lower corners of radius DLG_BTN_R_10 / 10, fitted to the console's
 * anti-aliased corners (16.0 to 16.5 on both corners of two screenshots; the panel's top corners fit DLG_R) */
#define DLG_BTN_H 41
#define DLG_BTN_R_10 165
#define DLG_SHADOW_ROW_PER_MILLE 843
/* the labels: a copy DLG_LABEL_SHADOW_DY px below as a shadow, then the text; at rest dark text on a white shadow,
 * held white text on a dark one (measured: the shadow 2 px below, 71 % and 84 % under a whole glyph pixel) */
#define DLG_LABEL_SHADOW_DY 2
#define COL_DLG_LABEL_LIGHT 0xFFFFFF
#define DLG_LABEL_LIGHT_ALPHA 71
#define COL_DLG_HELD_INK 0xFFFFFF
#define DLG_LABEL_DARK_ALPHA 84
#define DLG_BTN_PX 19

/* stereoscopic 3D on the top screen: how far each element stands in front of the screen with the 3D slider at the
 * top, as parallax between the eyes in px (each eye is moved by half of it, rounded, in opposite directions; 0 is
 * the screen's plane, below 0 behind it). The slider scales them; at 0 only one eye is drawn. One value per element,
 * so any element, a new one too, gets its own depth here and one layer() call where it is drawn. Only short titles
 * and solid symbols leave the screen's plane; fine text and labels stay on it (as on the console's own screens,
 * where only a screen's title and its big symbol stand out). No measured reference yet: retune after the console
 * test. */
#define DEPTH_DOTS 0                /* the dotted lines */
#define DEPTH_HEADER 0              /* the header line */
#define DEPTH_FOOTER 0              /* the footer (status) line and its symbol */
#define DEPTH_CARD 0                /* the card */
#define DEPTH_CARD_TEXT 0           /* everything else written on a card: tags, notes, the address, progress, steps */
#define DEPTH_CAMERA 0              /* the camera picture */
#define DEPTH_PROGRESS 3            /* the progress bar while it runs (its text stays at 0) */
/* an application's own numbers of this group: in its layout header */
#define DEPTH_DROPS_BEHIND -2       /* installing: the big paint drop behind the card */

/* The one glass setting: every glass element (the tiles' symbols, the glass icons, the picture's badges) in one
 * of three styles, ui.h's GLASS_*. Clear: white glass, GLASS_CLEAR_BOOST % more white in it than the sketches' first
 * clear glass. Shard: coloured glass; the house icon's runs from COL_SHARD_A into COL_SHARD_B. Platin: two bright tones
 * that meet at a horizon PLATIN_HORIZON_PCT % down the element, as polished metal mirrors sky and ground, without a
 * colour of its own; on the picture's badges its symbol is cut through the metal and the cut filled with clear
 * glass, PLATIN_CUT_BOOST % more white in it than the first clear glass, so the picture still shows through. */
#define GLASS_CLEAR_BOOST 7
#define PLATIN_HORIZON_PCT 54
#define COL_GLASS_SHADE 0x55626E    /* the shade inside clear glass */
#define COL_GLASS_SHADOW 0x282B2E   /* what glass casts and its hairline: one neutral grey in every colour scheme,
                                       so the glass images are shared by all of them (not the scheme's COL_SHADOW) */
#define COL_SHARD_A 0x8F6CF2
#define COL_SHARD_B 0x16C2AC
#define COL_SHARD_EDGE 0xD9D3F7     /* glass cut through, its edge seen from the side */
#define COL_PLATIN_SKY_A 0xFBFCFD
#define COL_PLATIN_SKY_B 0xD9E0E8
#define COL_PLATIN_GROUND_A 0xA9B5C3
#define COL_PLATIN_GROUND_B 0xD5DCE4
#define COL_PLATIN_SHADE 0x55616E
#define COL_PLATIN_EDGE 0xE3E8EE
/* Shard's tones for the glass icons on buttons: a ring of six, each icon takes two neighbours by
 * what its button does, the first at its upper left, the second at its lower right (art.py GLASS_ICONS): bring in or
 * keep teal into blue, help blue into violet, storage and settings violet into rose, remove coral into rose, folders
 * amber into coral; the house icon keeps COL_SHARD_A into COL_SHARD_B. Clear and Platin take no tone. */
#define COL_SHARD_TEAL 0x16B8A4
#define COL_SHARD_BLUE 0x3E6FE3
#define COL_SHARD_VIOLET 0x8F6CF2
#define COL_SHARD_ROSE 0xD9557E
#define COL_SHARD_CORAL 0xF0683E
#define COL_SHARD_AMBER 0xF0A92A
/* the start screen's word in Shard: each line from blue at its left into violet at its right */
#define COL_SHARD_WORD_A 0x2F6CE0
#define COL_SHARD_WORD_B 0x8A4FE3

/* an application's own numbers of this group: in its layout header */
#define PATTERN_TILE 64
#define PATTERN_DRIFT 8
#define PATTERN_ALPHA 78
/* where each pattern drifts, in PATTERN_DRIFT (x to the right, y down), in the order of ui.h's PATTERN_*: the plus
 * signs and rings down to the right, the houses to the left, the bubbles upwards */
#define PATTERN_PLUS_DX 1
#define PATTERN_PLUS_DY 1
#define PATTERN_HOUSES_DX -1
#define PATTERN_HOUSES_DY 0
#define PATTERN_BUBBLES_DX 0
#define PATTERN_BUBBLES_DY -1
#define COL_PATTERN_LIGHT 0xFCF8DA  /* the pattern's two tones, either side of the background's */
#define COL_PATTERN_DARK 0xE2D68C
/* TRAY_FLOOR: only schemes.py still takes it (its TRAY ground, which no colour uses, as there is no tray); it goes
 * when schemes.py drops that ground */
#define TRAY_FLOOR 7
/* an application's own numbers of this group: in its layout header */
#define DEPTH_PATTERN -4            /* the drifting pattern: behind the screen's plane */

/* an application's own numbers of this group: in its layout header */
#define COL_BADGE_TOP 0x7A5CC8      /* the music badge's violet, lighter at the top (Shard's glass) */
#define COL_BADGE_LOW 0x4E3596
/* The single tile and the whole carousel stand GAL_LOWER px lower (their places add it),
 * clear of the dotted line above; the card, the head, the dotted lines and the footer stay. What hangs on the
 * tile moves with it: the music badge (GAL_BADGE_IN from its corner), the carousel's side panes (SWARM_SLOTS), the
 * empty pane and the name. The name keeps its rule (gallery_baseline, its check gallery_name_middle): as much
 * room from the tile's lower edge down to its capitals as from its baseline down to the card's lower edge, then
 * GAL_NAME_UP px up; so it follows the tile by half of GAL_LOWER. */
#define GAL_LOWER 3

/* an application's own numbers of this group: in its layout header */
#define DROP_SUN_X 330              /* the drop behind, 100 across */
#define DROP_SUN_Y -30
#define DROP_SUN 100
#define COL_DROP_SUN 0xF4D98A

/* an application's own numbers of this group: in its layout header */
/* Screen changes (no slide): the new screen's content grows from TRANSITION_ZOOM_PCT % of its size to all of
 * it, about the screen's middle, and comes out of the background, easing out. Its first frame shows it already,
 * TRANSITION_FADE_PCT % faded (never an empty frame between two screens); the bar, the tabs and the corner button
 * neither grow nor fade. By the clock: TRANSITION_MS from the frame after the screen's first one, whatever that
 * first frame took (it converts the screen's icons), so a late frame skips a step and shortens nothing (ui_begin's
 * entered goes from 0 to 1 within it). */
#define TRANSITION_MS 200
#define TRANSITION_ZOOM_PCT 94
#define TRANSITION_FADE_PCT 55
/* an application's own numbers of this group: in its layout header */
#define POPUP_ZOOM_PCT 88

/* longer text: a problem's details, the folder view when it is empty */
#define BODY_PAD 16
#define BODY_MIN_PX 10

/* ---- flow: the loader. While a job runs during which nothing can be pressed (a folder or the installed
 * designs read, a file checked, a delete), the bottom screen does not stand there as if it could be used: what it
 * shows goes back into the background and the loader takes its place, a ring of dots whose bright one goes round by
 * the clock, with one line under it. A job that is over within LOADER_WAIT_MS shows no loader at all, so a short load
 * does not flash. When the job is done the ring fades first; then what the screen shows comes out of the background
 * again as it went, or a new screen comes in by its own transition. Our own symbol: none of the reference pictures
 * shows the console's. */
#define LOADER_WAIT_MS 150          /* a job runs this long before the bottom screen's content begins to leave */
#define LOADER_DOTS 8              /* the beads (Shard's loader; layout.h "installing") */
#define LOADER_MS 1100              /* one round: the slug (Clear, Platin) or the bright bead (Shard) goes round once */
#define LOADER_RISE_PCT 12          /* of a round: how long before the bright one arrives a dot begins to brighten */
#define LOADER_GAP 14               /* from the ring to the line */
#define LOADER_PX 13
#define LOADER_LH 18
#define SET_Y 14                    /* the first row */
#define SET_H 34                    /* the face's image (art.py; not Stock's); five rows */
#define SET_GAP 3                   /* between rows */
#define SET_AIR 4                   /* below the last row that fits, above the bar's shadow (196 to 200) */
#define SET_H_MAX 44                /* a row's largest height (a theme's row is ROW_H) */
#define SET_SHOWN 4                 /* rows shown whole at once; more scroll */

#include "popup_band.h"

/* ---------------------------------------------------------------- the settings; an application's own in its layout header */

/* an application's own numbers of this group: in its layout header */
#define SET_NAME_LH 15              /* the row's two lines: its name ROW_NAME_PX, the choice ROW_SUB_PX */
#define SET_SUB_LH 14
#define SET_SUB_GAP 1
#define SET_NOTE_PX 11              /* a row's "2 / 3" on the right */
#define SET_LIST_PX 14              /* the top screen's list: a line a choice, SET_LIST_LH apart */
#define SET_LIST_LH 22
#define SET_LIST_INDENT 20          /* its text after the check's place */

/* ---- carousel ---- */
/* The list's top screen as a swarm (swarm.h): the selected item's pane in the middle, where its picture was
 * (GAL_PICTURE at GAL_PICTURE_X, GAL_PICTURE_Y), up to SWARM_SIDE more on each side, all in front of the card. Per
 * slot, from d = 0 (the middle) to SWARM_SIDE + 1 (gone: where a pane comes from and goes to): height, width in % of
 * the height, px tucked behind the nearer pane, middle y on the left and on the right, opacity in % (the rest is the
 * veil: the pane is drawn as solid as it is and mixed towards what lies behind it), how much fainter its outer side
 * is in %, depth in 3D (as DEPTH_*). */
#define SWARM_SIDE 3
#define SWARM_SLOTS {{80, 100, 0, 88 + GAL_LOWER, 88 + GAL_LOWER, 100, 0, 4}, \
                     {58, 78, 4, 93 + GAL_LOWER, 93 + GAL_LOWER, 80, 0, 2}, \
                     {46, 72, 8, 96 + GAL_LOWER, 98 + GAL_LOWER, 52, 0, 1}, \
                     {36, 66, 11, 101 + GAL_LOWER, 99 + GAL_LOWER, 28, 60, 0}, \
                     {28, 62, 14, 103 + GAL_LOWER, 101 + GAL_LOWER, 0, 60, 0}}
#define SWARM_MOVE_MS 200           /* a move of the selection: the panes glide to their new slots (ease_both) */
/* Stock only (swarm.h, SWARM_APART): its frame is a card round the picture (like a print's border: 4 px of white, the
 * small class 3; look_stock.py, pane_rim) and the rest of a pane's box stays clear, so where the slots tuck the
 * neighbours 4 px behind the middle pane their cards would overlap. In Stock the neighbours, and the panes further
 * out with them, stand SWARM_STOCK_APART px further out: about 4.5 px clear between the middle pane's card and each
 * neighbour's, at rest and all through a move, where the two passing each other in the middle keep it without a
 * change of the drawing order. */
#define SWARM_STOCK_APART 9
/* an application's own numbers of this group: in its layout header */
#define PANE_BIG_RIM 4
#define PANE_SMALL_RIM 3
/* The idle light: once the panes have rested SWARM_LIGHT_AFTER_MS, and then every
 * SWARM_LIGHT_EVERY_MS, a band of light crosses the left neighbour, the middle pane and the right neighbour,
 * SWARM_LIGHT_MS each, from the picture's upper left corner to its lower right, under the rim. The band: PANE_LIGHT_PCT
 * white at its middle, PANE_LIGHT_WIDE_PCT % of the class's size wide each side; drawn as a window, as large as the
 * picture, moved along a strip of PANE_LIGHT px high that holds the band once (art.py, pane_light). */
#define SWARM_LIGHT_AFTER_MS 3500
#define SWARM_LIGHT_EVERY_MS 5000
#define SWARM_LIGHT_MS 500

/* ---------------------------------------------------------------- installing (liquid glass); an application's own in its layout header */

/* an application's own numbers of this group: in its layout header */
#define LOADER_BEAD 10
#define LOADER_BEAD_R 16
#define PROGRESS_COLLAR_W 8
/* The download's bar by glass style: Platin's two collars PROGRESS_COLLAR_W wide, the left one
 * PROGRESS_COLLAR_OUT px outside the bar's left end, the right one PROGRESS_COLLAR_IN px inside its right end */
#define PROGRESS_COLLAR_OUT 1
#define PROGRESS_COLLAR_IN 7

#endif
