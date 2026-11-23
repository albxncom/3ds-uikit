# The kit's example

A small gallery of the kit's elements for the 3DS, a page each, built on the kit alone: it includes only the kit's headers, libctru, citro2d and citro3d, registers no part (`ui_part` is never called) and reads no file. Every picture it shows it makes from its own pixels. All its text is its own and English; the kit holds none.

How to start with the kit from this example, with pictures of its pages: [`../docs/example.md`](../docs/example.md).

## What it shows

- Start: a tile a page and the settings button in the top left corner; the top screen says how to use the gallery, its footer shows the drawing's statistics.
- Tiles: a tile plain (pressing it makes its light amber and back), one greyed out, a stack without a face, and a tile that shows each kind of face in turn, each at its own height.
- Buttons: the green button and the second one, the round help button, and the bar in each of its kinds (one tab, two, B and Y, the right tab alone).
- List: forty rows to scroll with the D-pad, by dragging and flinging them with the stylus, and by the scroll bar's thumb; the selection, and the corner button that goes back.
- Settings (the settings button): the look (every look the build holds), the colour scheme (eight), the glass style and the pattern, switched at once through `ui_look`, with the setting's choices on the top screen.
- Text: sizes, the three alignments, a measured width, lines cut at their end or at their beginning, a paragraph wrapped at two widths.
- Popup: plain, with a path in its text, with the warning sign, and one whose right button deletes.
- Loader: a pretended job of known and of unknown length, with the progress bar on the top screen and the loader on the bottom screen.
- Top: the header with its dotted line, a screen without a header with the line alone, the card plain and in glass, the footer with a symbol, a path and a warning; a box of facts and the information box on the bottom screen.
- Pictures: a wide picture on each screen, a colour from top to bottom, a camera frame, theme icons, sprites at their size, stretched and in part.
- Sound: each of the seven interface sounds, the working loop, the menu music (off or a track of `tracks.h`), a theme's music in its place (the bar's right tab: `audio_theme_play` with a track linked in, so no file is read, and `audio_theme_stop`) and the sounds on and off. Without the DSP firmware on the card (`sdmc:/3ds/dspfirm.cdc`) the example is silent and its sound page says so.

Touch and keys work on every page: a touch presses what the stylus is lifted on; the first D-pad press shows the cursor and the next ones move it, A presses; B goes back to the start page, START leaves.

## Build

In the devkitPro image, from the kit's folder (this folder's parent):

```sh
docker run --rm --network none --platform linux/amd64 -v "$PWD":/src -w /src/example devkitpro/devkitarm:20260610 make
```

The result is `build/3ds-uikit-example.3dsx`, for the Homebrew Launcher, with libctru's own icon. The build draws the kit's art with the kit's generator (`art/art.py` without `--app`) for every look whose `source/look_<name>.c` is there, links every one of the kit's `.c` files (`source/`, with its `include/` on the include path) and its sounds as they are (`source/sound_data.S`), and nothing else: `-lcitro2d -lcitro3d -lz -lctru -lm`.

Against the kit as installed (`make install` in the kit's folder, or its pacman package) the same Makefile takes one variable: `make UIKIT=<prefix>`, the folder the kit was installed into. Then only this folder's `.c` files are compiled, against `<prefix>/include/3ds-uikit`, and linked with `<prefix>/lib/lib3ds-uikit.a`, into a folder of its own, `build/installed/3ds-uikit-example.3dsx`, so that a build from the sources in `build/` never stands in for it (`make clean UIKIT=<prefix>` removes that folder alone); the program is the same bytes either way (the kit's docs say how that is checked).

A host build of the example's `main.c` walks every page in every look the build holds with scripted keys and touches under the kit's flow recorder, once with the 3D slider down and once up; the kit's `make test` runs it.

## What is left out

| Element | Why |
| --- | --- |
| `ui_part` | The example brings no images of its own, so it registers no part: it is the first program that runs the kit without one. |
| `ui_motif` | Called by `ui_sign_once`, which the popups page uses; a program asks that one. |
| `ui_probe_arm` | Declared only in a probe build (`UI_PROBE`), a build for measuring that is never delivered. |
| `ui_probe_slider` | Declared only in a probe build (`UI_PROBE`), as `ui_probe_arm`. |
| `ui_probe` | Declared only in a probe build (`UI_PROBE`), as `ui_probe_arm`. |
| `ui_info_probe` | Declared only in a probe or fit probe build (`UI_PROBE`, `UI_FIT_PROBE`), never delivered. |
| `ui_note_probe` | Declared only in a probe or fit probe build, as `ui_info_probe`. |
| `ui_choices_top` | The settings page puts nothing under a setting's choices, so it centres them with `ui_choices`; this one draws the same list at the card's top for a program that adds lines of its own below it. |
