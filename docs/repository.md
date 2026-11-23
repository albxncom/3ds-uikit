# The repository: build, boundary and files

How the kit is built and tested, how a program takes it, what holds its boundary, and what each file is. The short overview is [the kit's README](../README.md); to start writing a program, read [example.md](example.md).

## Build and test

Build and test on the Mac with `make test` (the unit tests in a plain and a sanitizer build, the kit's art made alone, the Python tests of `host/`); the example with `make example` in the devkitPro image (`tools/build/image.sh` names it); the pictures of the docs with `make docs-pictures`. `tools/build/alone.sh REV OUT` proves that a commit, exported by itself, passes its tests and builds its example; where the kit is an application's submodule, `REV` may be the application's commit, and the kit's commit it pins is exported. In the devkitPro image, `make lib` builds the kit as a static library and `make install` puts it into devkitPro's 3DS portlibs folder (`DESTDIR` for a staging folder); `tools/build/installed.sh REV OUT` proves that the example builds against the installed kit alone.

On the host, `make test` and `make docs-pictures` need, besides make and a C compiler: zlib (`test_probe` links it), Python 3 with Pillow (the flow renderer of `tools/flows/` draws with it: the example's host walk in `host/test_example.py`, the renderer's compile in `host/test_kit_bounds.py` and the pictures; the art generator itself needs no more than Python's standard library), and no font of the host's: the renderer sets its text in Nunito, which is in the repository (`tools/flows/font/Nunito.ttf`, under the SIL Open Font License beside it; `tools/flows/fontdef.py` names it).

## Taking the kit

This is the kit's own repository. An application takes it as a git submodule pinned to one commit of it: `git submodule add ../3ds-uikit <folder>`, a URL relative to the application's own repository, so that the two stand side by side on any host (3DStockTheme holds it as the folder `uikit` beside its own sources, and its Makefile builds the kit's files from there). A clone of such an application gets the kit with `git clone --recurse-submodules`, or afterwards with `git submodule update --init`; git asks for `-c protocol.file.allow=always` on both when the repositories are folders on the same disk. The pin moves by checking out another commit in the folder (`git -C <folder> checkout <commit>`) and committing the folder in the application.

The second way, for programs that do not hold the kit's sources: the kit installed as a library into devkitPro's 3DS portlibs folder, by `make lib` and `make install` in the devkitPro image, or by the pacman package [`pacman/PKGBUILD`](../pacman/PKGBUILD) describes (`dkp-makepkg` in that folder; the package file is a release's download, not part of the repository). It installs, under the prefix (`/opt/devkitpro/portlibs/3ds`, 3ds_rules' `PORTLIBS`): `lib/lib3ds-uikit.a` (every source of the kit, its art for the four looks, its sounds), `include/3ds-uikit/` (its headers and the two generated ones), `share/3ds-uikit/` (the art generator with what it reads, for a part of the program's own, and the icons, in the repository's layout: `art/`, `include/`, `icons/`) and `licenses/3ds-uikit/` (the kit's `LICENSE`, the icons' and the sounds' terms); the docs stay in this repository. `prefix` and `DESTDIR` must be absolute paths without a space, else `make install` stops with one line before it builds anything. A program compiles with `-I$(PORTLIBS)/include/3ds-uikit` and links `-l3ds-uikit -lcitro2d -lcitro3d -lz -lctru -lm`; the example's Makefile does that with `make UIKIT=$(PORTLIBS)`. How each way builds, and what a part of the program's own needs then, is in [`example.md`](example.md#taking-the-kit).

## Boundary

The kit's files include only each other, the C library's and the SDK's headers, and two headers the host build generates into `build/art/`: `ui_art.h` (the atlas's sprites) and `looks_built.h` (the looks a build holds). Every `*.c` of the kit (`source/`) compiles with only its `include/`, the generated headers' folder and the host's fake SDK headers on the include path. `host/test_kit_bounds.py` checks both, and that no code, script or Makefile of this folder names a path outside it; `make test` runs it. An application holds its own side with tests of its own: 3DStockTheme's check that no kit file names its probe build's flag.

The art generator is the kit's too: `art/art.py` makes the kit's art with no application at all (`host/test_art_alone.py`), and an application's own images come in through its one hook, `--app FILE.py`, as the application's part (`part_art.h`, `part_art.c`, `part_art_<look>.c`), which the application hands to `ui_part` and the kit knows by its type alone. `host/test_art_part.py` holds that small parts build and a faulty hook stops with one line; 3DStockTheme's own tests hold that no kit file names anything of its part but what the hook offers. Each of the kit's tests is named with what it holds in [`part.md`](part.md#the-tests-that-hold-the-boundary).

## Files

| File | What it holds |
| --- | --- |
| `include/ui.h` | the drawing layer's interface: sprites from the atlas, the system font for every word, a theme's own icon |
| `source/ui.c` | the drawing layer itself: the frame's state, the images, the elements every screen draws |
| `include/kit.h` | what a look may use from `ui.c` besides `ui.h`: the frame's state, the images, the drawing order, shared pieces |
| `include/widget.h` | what an application's own widget may use from `ui.c` besides `ui.h` and `kit.h`, under the `ui_` prefix, and the part's pictures |
| `include/ui_layout.h` | the kit's share of the layout in console pixels: every colour (`COL_*`) and each size, place and time the kit uses |
| `include/look.h` | a look: the table of drawing functions and metrics each design fills; the looks by number |
| `source/look.c` | the looks a build holds, by their number; Stock for any other |
| `source/look_stock.c` | Stock, the look of the console's own menus rebuilt from measurements; the fallback |
| `source/look_frost.c`, `include/look_frost.h` | Frost, frosted glass over the colour schemes; its own numbers |
| `source/look_night.c`, `include/look_night.h` | Nachtglas, a dark look of smoked glass; its own numbers |
| `source/look_prism.c`, `include/look_prism.h` | Prisma, light and angular glass with cut corners; its own numbers |
| `art/art.py` | the art generator: every image of the kit at build time, per look and colour scheme, into `ui_art.h`, `ui_art.c` and `art_<look>.c`; an application's part through its hook (`--app`) |
| `art/look_stock.py`, `art/look_frost.py`, `art/look_night.py`, `art/look_prism.py` | each look's generator: its images in its own material, through the generator's look hooks |
| `art/schemes.py` | the colour schemes, derived from the colours of `ui_layout.h` and `popup_band.h` |
| `art/liquid.py` | the liquid's strength: the one rule by which a look's tone becomes a liquid's (the kit's download bar) |
| `icons/` | the 18 Tabler icons the generator strokes, with the pack's `LICENSE` |
| `include/order.h` | the order in which pictures and texts reach the GPU (the rule against the white flicker of text) |
| `include/popup.h`, `source/popup.c` | what a popup and the screen under it take: the touch guard when a popup opens and closes |
| `include/popup_band.h` | the popup's panel and button band, as measured numbers |
| `include/touch.h`, `source/touch.c` | what a touch presses: held, dragged into a scroll, cancelled by sliding off |
| `source/hits.c` | where the buttons drawn by `ui.c` take touches |
| `include/scroll.h`, `source/scroll.c` | every list's scrolling |
| `include/swarm.h` | the list's carousel: where each pane stands and which icon slot it draws from (header only) |
| `include/wrap.h`, `source/wrap.c` | text broken into lines and cut to a width, with "..." on every cut |
| `include/ease.h` | one clock and one family of curves for what moves (header only) |
| `include/drift.h` | the pattern's drift behind the screens, by the clock (header only) |
| `include/probe.h` | the flicker probe's frame comparison, for a probe build that is never delivered |
| `include/audio.h`, `source/audio.c` | the sound through `ndsp`: the interface sounds, the menu music, a theme's music and the working loop, on a thread of its own; its log lines go to `audio_log`, which the application sets |
| `include/wav.h`, `source/wav.c` | the WAV reader for the interface sounds |
| `include/bcstm.h`, `source/bcstm.c` | the BCSTM stream decoder for the music |
| `include/work.h`, `source/work.c` | the working loop's timing: while work runs, the menu music gives way to a short loop |
| `include/tracks.h`, `source/tracks.c` | the menu music's tracks, one table |
| `source/sound_data.S`, `include/sound_data.h` | the files of `sound/` inside the application, linked in as they are |
| `sound/` | the interface sounds, the music tracks and the working loop; `CREDITS.md` with every source and licence; `wav2bcstm.py`, `make_working.py` |
| `docs/` | the kit's documentation |
| `host/` | the kit's host tests: `test_kit.c` (popup, selection and stylus, wrap, order, ease) and `test_sound.c` (BCSTM, WAV, tracks, working loop, the files of `sound/`), built from the kit alone, with the lines they share with an application's tests (`check.h`, `ui_fakes.h`, `mutate.h`, `synth_bcstm.h`); the Python checks of the boundary, the art and the docs; the fake SDK headers (`gpu/`) |
| `pacman/PKGBUILD` | the description of the kit's pacman package for devkitPro, after the image's 3DS port packages (`3ds-` name, `arch` any, the group `3ds-portlibs`): its `build` is `make lib`, its `package` `make install` |

## How a look is written

In [`looks.md`](looks.md#how-a-look-is-written), with the look table and what `kit.h` gives a look.

## Known limits

The headers are C without `extern "C"` guards, so a C++ program wraps its includes of them itself, and some of the kit's global names carry no prefix of its own (`sfx`, `focus_open`, `select_next`, `popup_key`, `track_find`, `wav_read`, the `guard_*`, `touch_*`, `wrap_*`, `work_*`, `scroll_*`, `audio_*` and `bcstm_*` functions, the sounds' `snd_*` and `track_*` data), so a program must not define those names itself.
