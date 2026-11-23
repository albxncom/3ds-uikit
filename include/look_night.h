/* Nachtglas's own numbers (look_night.c, and look_night.py through art.py's read_layout): #defines only. */
#ifndef LOOK_NIGHT_H
#define LOOK_NIGHT_H

/* The ground: per screen a strip of its gradient (NG_STRIP px wide, repeated across the screen, made when the colour
 * scheme is set, dithered so that the dark gradient shows no steps) and lights over it: soft discs of NG_LAMP px, each
 * drifting on a slow figure of its own (look_night.c, NG_LAMPS), at most NG_LAMP_PCT % solid in their middle, with a
 * grain of NG_LAMP_GRAIN_10 tenths of an alpha step for the same reason. Colours: the scheme's COL_TOP_BG_A to
 * COL_BOT_BG_B and the look's COL_NG_LAMP_A to _C (look_night.py). */
#define NG_STRIP 64
#define NG_LAMP 256
#define NG_LAMP_PCT 40
#define NG_LAMP_GRAIN_10 28
#define DEPTH_NG_LAMPS (-5)         /* the lights lie behind everything, as the start screen's pattern does */
/* The bottom screen's ground ends on the line's image (NG_LINE) along the screen's foot at NG_FOOT_PCT % of it: an
 * atlas image before any letters (the drawing rule), under the bar where there is one. */
#define NG_FOOT_PCT 20
/* The selection: a line of light NG_RIM_OUT px outside the element (look_night.py rim; the images' fixed corners
 * NG_RIM_CAP, their glow NG_RIM_PAD past the line), breathing between NG_RIM_LOW_PCT and 100 % once per SELECT_MS. */
#define NG_RIM_OUT 2
#define NG_RIM_CAP 20
#define NG_RIM_PAD 8
#define NG_RIM_LOW_PCT 72
/* Behind a popup the screen keeps less of its light than DIM_ALPHA leaves: the bar's white labels would show
 * through under the panel's lower edge. */
#define NG_DIM_ALPHA 80
/* The line across the top screen in the dotted lines' place: it fades out over NG_LINE_FADE px at both ends. */
#define NG_LINE_FADE 56
/* The settings button in the top left corner (ui_settings_button): 1 px taller than the corner button (CORNER_H) and
 * its wrench 1 px further left than CORNER_PAD, so the wrench has as much air below it, where its shadow falls, as
 * above it, and as much at its left as at its right. Its touch area stays ui_settings_rect's. */
#define NG_TAB_H 33
#define NG_TAB_ICON_X 12

#endif
