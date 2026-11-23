# The UI kit's documentation

The kit builds on citro2d (drawing images, tints, views, text), citro3d (textures, the frame) and libctru. It is a layer above them, made of elements and looks; it does not replace them. These files describe what the kit has today, for someone who wants to use it in another 3DS application; start with [example.md](example.md). These pages describe version 0.1.0 (the kit's [`VERSION`](../VERSION)). The kit's headers are the source of truth: every function here is declared there with its comment.

| File | What it holds |
| --- | --- |
| [example.md](example.md) | how to start: what the kit is, the smallest program, the one rule that breaks silently, how the kit's example is built, the two ways a program takes the kit (a submodule, or the library installed into devkitPro's portlibs by `make install` or the pacman package), what it shows page by page with its pictures, and what a program brings itself |
| [elements.md](elements.md) | every element the drawing layer offers, by group as `ui.h` orders them: what it is, its function, what the look draws of it, one use |
| [looks.md](looks.md) | the four looks, the colour schemes, glass styles and patterns, the `look` table, how a look is written, the drawing rule, what `kit.h` gives a look and `widget.h` an application's own widget |
| [input.md](input.md) | touch, where buttons take touches, scrolling, the popup's guard and keys, the carousel, text wrapping, the curves and the clock |
| [layout.md](layout.md) | the groups of `ui_layout.h`: colours, sizes, places and times, and who reads it |
| [sound.md](sound.md) | the audio interface, the seven interface sounds and when 3DStockTheme plays each, the working loop, the music table, the log hook |
| [icons.md](icons.md) | the 18 icons by file name and where each is drawn |
| [part.md](part.md) | the art generator, its hook for an application's own images (`--app`), what it writes for the application's part, `art_part` field by field, how the kit makes, unpacks, draws and frees the part's textures, and the tests that hold the boundary |
| [credits.md](credits.md) | every third-party thing with its source, version and licence as the repository records them, and what is the project's own |
| [repository.md](repository.md) | the repository itself: how it is built and tested, the two ways a program takes the kit, the boundary and the tests that hold it, every file with what it holds, the known limits |

The short overview with pictures: [the kit's README](../README.md). `host/test_kit_docs.py`, run by `make test`, checks that every function a kit header declares is named here, that every sound and icon is named in `sound.md`, `icons.md` or `credits.md`, that every link here leads to a file inside the kit's folder, and that every picture in `img/` is linked from a page, is a PNG of 400 by 480 and stays small. The pictures are drawn by `make docs-pictures` from the example's host walk, with the text in Nunito, the renderer's font ([credits.md](credits.md)).
