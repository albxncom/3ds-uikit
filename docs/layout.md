# Layout

[`ui_layout.h`](../include/ui_layout.h) is the kit's share of the interface's layout, in console pixels: every colour and each size, place and time the kit's C uses, directly or through another define, and every number the art generator reads to draw one of the kit's sprites (its own, or one the application draws too), and every number a kit function's contract names: `FOOT_DOTS_Y` 195, the `dots_y` `ui_foot` takes on a screen with the header and the card, and `TRANSITION_MS` 200, the time in which `ui_begin`'s `entered` goes from 0 to 1. It holds `#define`s only. The application's own sizes and places stay in a layout header of its own, which includes the kit's file first.

## Who reads it

- The kit's C, directly.
- The art generator ([`art.py`](../art/art.py), in the kit's `art/`), which reads it directly, or with an application's hook through the header the hook names (`LAYOUT`, which includes this one; [part.md](part.md)), and draws the atlas by the same numbers. So a size changed here changes the images and the code that places them together.
- The colour schemes (`schemes.py`) derive every scheme from the colours here ([looks.md](looks.md)).

## The groups, in the file's order

| Group | What it holds | Examples |
| --- | --- | --- |
| Colours, `0xRRGGBB` | the colours as the standard scheme has them: grounds, inks, faces, edges, the selection, the bar; later groups add the colours of their own parts | `COL_INK`, `COL_MUTED`, `COL_CARD`, `COL_FACE_TOP` |
| Screens and text | the screens' sizes and the system font's size and cap height | `TOP_W` 400, `BOTTOM_W` 320, `SCREEN_H` 240, `FONT_PX` 26, `FONT_CAP` 19 |
| Top screen | the dotted lines, the header and the footer, the card, the progress bar and its stripes | `HEAD_Y`, `CARD_Y`, `FOOT_DOTS_Y`, `FOOT_Y`, `PROGRESS_H`, `STRIPE_MS` |
| Bottom screen | the bar and its tabs, the key caps, the tiles, the selection, the corner button, the rows, the touch and scroll numbers, the green button | `BAR_H`, `TAB_W`, `SELECT_MS`, `CORNER_MAX_W`, `DRAG_START`, `TOUCH_HOLD_MS`, `POPUP_GUARD_MS`, `GLIDE_PCT`, `FLING_PCT` |
| Facts, information box, help button | the box of facts, the information box, the help button | `FACT_ROW_H`, `INFO_PX`, `HELP_SIZE`, `HELP_HIT` |
| Popup | the dim, the panel, its text and its buttons; the top screen's depths | `DIM_ALPHA`, `DLG_H`, `DLG_PX`, `DLG_MIN_PX`, `DEPTH_CARD` |
| Glass and pattern | the glass styles' colours, the pattern's tile, drift and depth | `COL_SHARD_A`, `PATTERN_TILE`, `PATTERN_DRIFT`, `DEPTH_PATTERN` |
| Transitions | how long and how a screen comes in, how a popup grows | `TRANSITION_MS`, `TRANSITION_ZOOM_PCT`, `POPUP_ZOOM_PCT` |
| Loader | how long a job runs before the loader shows, one round, its dots and its line | `LOADER_WAIT_MS`, `LOADER_MS`, `LOADER_DOTS` |
| Settings | the setting row's lines and the list of choices | `SET_NAME_LH`, `SET_LIST_PX` |
| Carousel | the panes' slots, their movement and the idle light | `SWARM_SIDE`, `SWARM_SLOTS`, `SWARM_MOVE_MS`, `SWARM_LIGHT_MS` |
| Installing | the loader's beads and the download bar's collars | `LOADER_BEAD`, `LOADER_BEAD_R`, `PROGRESS_COLLAR_OUT` |

Colours that follow the scheme are read as `SC(COL_NAME)` wherever code draws them; the name alone does not compile (the generated `ui_art.h`). The colours that are the same in every scheme may be named either way.

## Times

Everything that moves goes by the clock in ms ([input.md](input.md)). The kit's times, as `ui_layout.h` has them:

| Define | ms | What |
| --- | --- | --- |
| `TOUCH_HOLD_MS` | 80 | a touch on a row shows held after resting this long |
| `LOADER_WAIT_MS` | 150 | a job runs this long before the loader shows; the working loop waits as long (`WORK_DELAY_MS`) |
| `TRANSITION_MS` | 200 | a screen comes in: the time of `ui_begin`'s `entered` |
| `SWARM_MOVE_MS` | 200 | the carousel's panes glide to their new places |
| `POPUP_GUARD_MS` | 350 | a touch counts only this long after a popup's first frame or its close |
| `SWARM_LIGHT_MS` | 500 | the idle light crosses one pane |
| `STRIPE_MS` | 600 | the progress bar's stripes move one period |
| `SELECT_MS` | 1000 | the selection breathes out and back once |
| `LOADER_MS` | 1100 | one round of the loader |
| `SWARM_LIGHT_AFTER_MS` | 3500 | the panes rest this long before the idle light starts |
| `SWARM_LIGHT_EVERY_MS` | 5000 | the idle light comes again |

## What the program chooses

A number that a kit function's own drawing or contract depends on is here; a number that is one program's layout is the program's, in its own layout header, and the kit takes it as an argument. So these stay with the program, and each program picks its own:

- Where a list's rows end and where its first row starts at scroll 0 (the application's `LIST_BOTTOM` 200 and `LIST_INSET` 42): the program hands them to the scrolling in its `scroll_shape` or `scroll_area` (`h`, `inset`; [input.md](input.md)) and stops its rows there.
- Where the scroll bar stands (the application's `SCROLL_X` 304): the program hands `ui_scrollbar` its track as a `rect`; only the track's width `SCROLL_W` and radius `SCROLL_R` are the kit's.
- How high a setting's row is (the application's `SET_ROW_H`, from its bar and the number of rows): the program hands `ui_row_setting` its `rect`; the kit draws any height up to `SET_H_MAX`, stretching the face's middle above `SET_H`.

## Beside it

- [`popup_band.h`](../include/popup_band.h): the popup's panel and button band, measured row by row on screenshots of the System Settings' dialog; numbers only, nothing of the screenshots. `ui_layout.h` includes it.
- [`look_frost.h`](../include/look_frost.h), [`look_night.h`](../include/look_night.h), [`look_prism.h`](../include/look_prism.h): each look's own numbers, read by the look's C and its generator.
- [`work.h`](../include/work.h): the working loop's fades, `WORK_FADE_MS` 350 and `WORK_LOOP_FADE_MS` 120 ([sound.md](sound.md)).
