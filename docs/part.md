# The art generator and the application's part

Every image the kit draws is made at build time by the art generator, [`art.py`](../art/art.py), with the looks' generators beside it (`look_<name>.py`), the colour schemes (`schemes.py`), the liquid glass (`liquid.py`) and the Tabler icons (`icons/`). It is Python 3's standard library only, and the same input gives the same bytes. An application that draws images of its own hands the generator a module of its own, the hook; what the generator makes from it is the application's part, which the kit draws as its own. Without a hook the generator makes the kit's art alone.

## The kit's art

- `python3 -I art/art.py [--app APP.py] --look NAME --part OUT.part`: one look's share in every colour scheme, drawn by `look_NAME.py`. A make target per look.
- `python3 -I art/art.py [--app APP.py] --join OUT_DIR NAME...`: the looks' parts `OUT_DIR/NAME.part`, Stock first and every look once, joined into the C the kit compiles: `ui_art.h` (the ids, the sizes, the types), `ui_art.c` (the selection's corners, which no look owns), `art_NAME.c` per look (its blobs and tables as a `const look_art ART_NAME`) and `banner.wav` (the CIA banner's chime).
- `python3 -I art/art.py --app APP.py [--look NAME] ARGS...`: the application's own commands (the hook's `main`).

The kit's textures, per look and scheme: the atlas, `ART_ATLAS_W` (512) wide and `ART_ATLAS_H` high (the tallest look's: 1024 with the four looks), and the sheet of what is glass, `ART_GLASS_SHEET_W` x `ART_GLASS_SHEET_H` (512 x 64); the corners have a texture of their own. Each blob is zlib-compressed RGBA8 in the GPU's tile order; a blob equal to an earlier one is named once. Every packed sprite keeps one transparent texel (`BORDER`) to each edge of its texture.

The generator runs with no `--app` too: then it writes no file of a part, and `ui_art.h`, `ui_art.c` and `art_<look>.c` are the same bytes as with the hook. The kit then compiles and runs with no part ([below](#the-part-in-the-kit)).

## The hook

`--app FILE.py` names the module; the generator imports it before anything is drawn and names it nowhere else. Its own modules import the kit's helpers as `art` (`Image`, `Sprite` and the drawing functions of `art.py`, whole by the time a hook function is called). The module must have:

| Entry | What it is |
| --- | --- |
| `LAYOUT` | the header the numbers are read from: the application's, which includes the kit's `ui_layout.h` |
| `STOCK_ONLY` | a set of its sprite names only Stock fills; every other look's atlas leaves them out (empty for none) |
| `sprites(index)` | its sprites of the atlas in scheme `index`, drawn in the look (`art.LOOK`, `art.L`) with the kit's helpers; a list, empty for none |
| `sheet()` | its sprites of the sheet in the scheme drawn (`art.SCHEME`); a list, empty for none |
| `main(args)` | its own commands; `True` if `args` are one |

and may have `pictures(index)`: the part's pictures in scheme `index`, one `Image` whose sides are powers of two from 8 to 1024, of one size in every look and scheme, or `None`.

A module that lacks one of the five ends the run at once with one line, `--app: FILE.py lacks NAME (the hook needs ...)`. A sprite of the module's (atlas or sheet) with a name the kit draws too in the look ends the run with one line, `--app: FILE.py names sprites the kit draws too: NAME`, before any file is written. Pictures that are missing in some schemes, of two sizes, or with a side that is no power of two end the run the same way.

The one hook in use is 3DStockTheme's: it draws nothing itself, it hands over the images its other modules draw.

## What the generator writes for the part

With `--app`, the join writes beside the kit's files:

- `part_art.h`: `enum part_art_id`, the part's ids from `ART_ALL` on, its atlas's first and then its sheet's, each in Stock's order (no enum when the part has no sprite); `ART_APP_COUNT` (the atlas's sprites) and `ART_APP_GLASS_COUNT` (the sheet's); `extern const art_part ART_APP_PART`.
- `part_art.c`: per look its two tables (none where the part has no sprite there) and `ART_APP_PART`.
- `part_art_<look>.c` per look: the blobs of its atlas, sheet and pictures per scheme, `ART_PART_<LOOK>_ATLAS_Z`, `ART_PART_<LOOK>_SHEET_Z`, `ART_PART_<LOOK>_PICTURES_Z` with their `_Z_SIZE`; a blob equal to another scheme's named once.

The types are in the generated `ui_art.h`. `art_part_look`, one per look built:

| Field | What it holds |
| --- | --- |
| `look` | the look's `look_art` the tables are for |
| `count`, `sheet_count` | the part's sprites of the atlas and of the sheet (`ART_APP_COUNT`, `ART_APP_GLASS_COUNT`) |
| `sprites` | `[count]` places in the part's atlas; `NULL` for none; a name the look does not draw has `w` 0 |
| `sheet_sprites` | `[sheet_count]` places in the part's sheet; `NULL` for none |
| `atlas_z`, `atlas_z_size` | `[ART_SCHEMES]` the atlas's blobs; `NULL` where the look has no sprite of the part's (all in `STOCK_ONLY`, or none at all) |
| `sheet_z`, `sheet_z_size` | `[ART_SCHEMES]` the sheet's blobs; `NULL`: no sheet |
| `pictures_z`, `pictures_z_size` | `[ART_SCHEMES]` the pictures' blobs; `NULL`: none |

`art_part`, the one the application hands over:

| Field | What it holds |
| --- | --- |
| `looks`, `count` | the `art_part_look` of every look built |
| `atlas_w`, `atlas_h` | the part's atlas: 512 wide, as high as the tallest look's; 0 and 0 when it has no sprite there |
| `sheet_w`, `sheet_h` | the part's sheet: 256 wide, as high as the tallest look's; 0 and 0 when it has none |
| `pictures_w`, `pictures_h` | the pictures' size, the same in every look; 0 and 0 when it has none |

## The part in the kit

- `void ui_part(const art_part *part);` hands the part over, before `ui_init`. A part handed over later is looked up, but its sprites have no texture. Without a part, the part's ids draw nothing and `ui_part_pictures` returns `NULL`.
- Made: `ui_init` makes the part's atlas and sheet at the part's sizes, each with a smoothed twin of the same memory as the kit's, where the part brings sprites there, and the pictures' texture (whole pixels) where it brings pictures. Each is freed once by `ui_exit`; a change of look makes none and frees none.
- Unpacked: the part's atlas with the kit's atlas (the first frame, a change of scheme, a change of look), the part's sheet with the kit's sheet, the pictures when the application asks for them. A blob of the atlas or sheet that does not unpack counts as a failed draw (`ui_stats`), as the kit's.
- Ids: two ranges in one. Below `ART_ALL` the kit's (the atlas's below `ART_COUNT`, then the sheet's); from `ART_ALL` on the part's (`ART_APP_COUNT` of its atlas, then its sheet's). `ui_sprite_info` and `ui_sprite_tex` give the entry and the texture for either range, from the tables of the look drawn; `NULL` where the look has no entry or the texture holds nothing. The functions that draw a sprite by its id (`ui_sprite`, `ui_sprite_box`, `ui_sprite_part`, and `kit.h`'s `ui_sprite_draw`, `ui_sprite_tinted`, `ui_sprite_scaled`, `ui_sprite_turned`) read both ranges through these two, so a part's id is drawn as the kit's.
- Drawn: the part's atlas is drawn at once, as the kit's atlas; the part's sheet waits for the drawing rule as the kit's sheet does ([looks.md](looks.md#the-drawing-rule)).
- Pictures: `C3D_Tex *ui_part_pictures(void);` (in [`widget.h`](../include/widget.h)) unpacks them into their texture when they are not the ones in it (the first time asked, after a change of scheme or look) and returns the texture, or `NULL` where the part brings none, the texture could not be made or they did not unpack. `ui_start_textures` asks for them with the pattern's tile. The application draws from it as a picture (`ui_picture`).

## The tests that hold the boundary

All run by the kit's `make test`, each after the kit's art is made but `test_kit_docs`:

| Test | What it holds |
| --- | --- |
| `host/test_kit_bounds.py` | the kit's files include only each other and the two generated headers (`ui_art.h`, `looks_built.h`); every `*.c` compiles with only the kit's `include/` on the include path, the generated art and the kit's fake SDK headers (`host/gpu/`), and so do the kit's host tests (`host/*.c`); no file of the folder names a path outside it or its retired wording |
| `host/test_art_alone.py` | the generator without `--app`: every look and the join run, no file of a part is written, every `*.c` of the kit compiles against them |
| `host/test_art_part.py` | the hook on small parts written by the test: pictures only, and sprites only Stock fills with a second look, each generating and compiling beside the kit; a module that lacks an entry and one that takes a name of the kit's, each ending in its one line |
| `host/test_example.py` | the example: every element and sound once or left out by name, nothing named from outside the kit, each source compiling against the kit alone, and its host walk through every page |
| `host/test_kit_docs.py` | every function a kit header declares is named in these docs, every sound and icon is named, every link of the folder's pages leads to a file inside the folder |

An application holds its side with tests of its own. 3DStockTheme's: no kit file names a sprite, function, constant, module, file or number of its part but what the hook offers, nor its probe flag or its installing screen's cut; the kit's art is the same bytes as its build's with the hook; every sprite of every look, the part's too, lies inside its texture.
