/* Prisma's own numbers (look_prism.c draws by them, look_prism.py draws its images by them; #defines only).
 *
 * Every cut is at 45 degrees. A stone (tile, button, row): its corners cut PRISM_CUT_* px off along each edge, a
 * bevel of PRISM_BEVEL_* px (BIG: tiles and buttons, SMALL: rows). The selection lights a stone's bevel in the
 * scheme's spectrum (COL_SHARD_PRISM_0 to _3, the look's own colours; Perle's: teal at the upper left, blue, violet,
 * rose clockwise): a line PRISM_RING_LINE px wide on the outline, the bevel inside it PRISM_RING_BEVEL % as strong,
 * breathing between PRISM_RING_LOW % and all of it once per SELECT_MS; its corner images reach PRISM_RING_STUB px
 * past the cut. A plate (card): upper left and lower right corner cut PRISM_PLATE_CUT px, the other two nicked
 * PRISM_PLATE_NICK px, a spectral line along its upper edge from the cut over PRISM_SPECTRUM_PCT % of its width, the
 * last PRISM_SPECTRUM_FADE_PCT % of that fading out. The separators are hairlines in COL_DOTS, PRISM_RULE_ALPHA %
 * strong, fading over PRISM_RULE_FADE px at both ends, a lozenge (PRISM_GEM px) on each end. */
#ifndef LOOK_PRISM_H
#define LOOK_PRISM_H

#define PRISM_CUT_BIG 9
#define PRISM_BEVEL_BIG 4
#define PRISM_CUT_SMALL 5
#define PRISM_BEVEL_SMALL 2
#define PRISM_RING_STUB 3
#define PRISM_RING_LINE 2
#define PRISM_RING_BEVEL 55
#define PRISM_RING_LOW 62
#define PRISM_PLATE_CUT 10
#define PRISM_PLATE_NICK 3
#define PRISM_SPECTRUM_PCT 62
#define PRISM_SPECTRUM_FADE_PCT 30
#define PRISM_RULE_ALPHA 42
#define PRISM_RULE_FADE 28
#define PRISM_GEM 7

/* The ground: the scheme's two ground colours from the upper left to the lower right, the facets' white over it (in
 * the dark crystal PRISM_FACETS_NIGHT % of it). Both screens' facets are one texture PRISM_GROUND_W x PRISM_GROUND_H,
 * the top screen's from row 0, the bottom's from PRISM_GROUND_ROW; their last PRISM_GROUND_END rows are empty there,
 * in the atlas instead and drawn from it, so every screen's ground ends on an atlas image with real pixels (the
 * drawing rule). */
#define PRISM_FACETS_NIGHT 11
#define PRISM_GROUND_W 512
#define PRISM_GROUND_H 512
#define PRISM_GROUND_ROW 256
#define PRISM_GROUND_END 8

/* The corner button (the settings tab and the folder button) stands PRISM_CORNER_H px high, a pixel lower than
 * CORNER_H (its touch area stays), its symbol PRISM_CORNER_ICON_X px from the left and PRISM_CORNER_ICON_Y px from the
 * top: the symbol's ink has as much air above it as between it and the bevel (6 and 5 px), the folder's path is
 * centred on it. */
#define PRISM_CORNER_H 33
#define PRISM_CORNER_ICON_X 12
#define PRISM_CORNER_ICON_Y 3

/* The popup: a solid plate; its two buttons are stones on its lower end, PRISM_DLG_SIDE px in from its sides,
 * PRISM_DLG_GAP px apart, PRISM_DLG_TOP and PRISM_DLG_LOW px inside the button band (hits.c: a button's touch area
 * stays the band's half); the panel and its stones PRISM_DLG_GROW px taller below than DLG_H. */
#define PRISM_DLG_SIDE 6
#define PRISM_DLG_GAP 6
#define PRISM_DLG_TOP 4
#define PRISM_DLG_LOW 4
#define PRISM_DLG_GROW 4

/* The keys: the START key's frame has corners cut PRISM_START_CUT px; a theme's picture has its corners cut
 * PRISM_ICON_CUT px at 48 px (the carousel's panes follow). */
#define PRISM_START_CUT 4
#define PRISM_ICON_CUT 6

/* The beam of split light on the top screen (ART_DROP_SUN, DROP_SUN px square): four bands PRISM_BEAM_BAND px wide
 * along x + y / 2, the first PRISM_BEAM_AT px into the image, PRISM_BEAM_ALPHA % strong at the screen's upper edge
 * (PRISM_BEAM_TOP px down the image) and gone PRISM_BEAM_REACH px below it. */
#define PRISM_BEAM_BAND 11
#define PRISM_BEAM_AT 40
#define PRISM_BEAM_TOP 30
#define PRISM_BEAM_ALPHA 22
#define PRISM_BEAM_REACH 66
/* A1: the beam wanders PRISM_BEAM_SWEEP px to the left of DROP_SUN_X and back once per PRISM_BEAM_MS (at most about
 * 10 px a second, the pattern's calm in the other looks). */
#define PRISM_BEAM_SWEEP 370
#define PRISM_BEAM_MS 120000

/* The loader: a cut crystal PRISM_LOADER px across, its corners cut PRISM_LOADER_CUT px, a bevel of
 * PRISM_LOADER_BEVEL px; the light runs round its eight facets once per LOADER_MS from the upper left. */
#define PRISM_LOADER 30
#define PRISM_LOADER_CUT 9
#define PRISM_LOADER_BEVEL 5

#endif
