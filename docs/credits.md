# Credits

Every third-party thing the kit uses, with its source, version and licence exactly as a file of this repository records it, and the file that records it. Where the repository records nothing, the entry says so. Nothing of Nintendo's is inside the kit: the looks are rebuilt from measurements of the console's screens (`popup_band.h`: numbers only, nothing of the screenshots themselves), `sound/CREDITS.md` says "Nothing here is Nintendo's" of the sounds, and the application's README says the same of the libraries.

## Icons: Tabler Icons

| What | As recorded |
| --- | --- |
| Files | the 18 outline icons in use, in `icons/` ([icons.md](icons.md)), unmodified |
| Version | 3.51.0 |
| Licence | MIT, the pack's licence file beside the icons: `icons/LICENSE`, "Copyright (c) 2020-2026 Paweł Kuna" |
| Source | <https://github.com/tabler/tabler-icons>, its release `v3.51.0`; how the 18 files were taken from it is not recorded |
| Recorded in | `icons/LICENSE`; 3DStockTheme's README, the row for the icons in its table of files ("Tabler Icons 3.51.0, the 18 outline icons in use, unmodified, with the pack's MIT licence"); the comment on `tabler()` in `art.py` |

The icons: `alert-triangle.svg`, `award.svg`, `brush.svg`, `check.svg`, `chevron-right.svg`, `device-sd-card.svg`, `download.svg`, `file-check.svg`, `file.svg`, `folder-up.svg`, `folder.svg`, `help.svg`, `music.svg`, `power.svg`, `qrcode.svg`, `tool.svg`, `trash.svg`, `world.svg`. The licence file says: "The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software."

## Interface sounds: Kenney

Recorded in [`sound/CREDITS.md`](../sound/CREDITS.md), which also says what was changed in each file. Both packs are by Kenney (www.kenney.nl); their asset pages state the licence as "Creative Commons CC0", and the CREDITS file quotes each pack's `License.txt`.

| File | Pack | Source file | Version | Licence |
| --- | --- | --- | --- | --- |
| `move.wav` | "UI Audio" (its `License.txt`: "UI SFX Set"), https://kenney.nl/assets/ui-audio | `rollover2.ogg` | none recorded | CC0 |
| `confirm.wav` | "Interface Sounds", https://kenney.nl/assets/interface-sounds | `glass_003.ogg` | 1.0 | CC0 |
| `popup.wav` | "Interface Sounds" | `maximize_008.ogg`, moved up 4 semitones | 1.0 | CC0 |
| `error.wav` | "Interface Sounds" | `error_005.ogg`, moved up 14 semitones | 1.0 | CC0 |
| `done.wav` | "Interface Sounds" | `confirmation_002.ogg` | 1.0 | CC0 |
| `drop.wav` | "Interface Sounds" | `drop_003.ogg` | 1.0 | CC0 |
| `back.wav` | the project's own tone, shaped by the envelope of "Interface Sounds" `bong_001.ogg` | `bong_001.ogg` (its envelope only) | 1.0 | CC0 for the envelope's source |

The CREDITS file records each pack's zip with its size and SHA-256.

## Menu music

Recorded in [`sound/CREDITS.md`](../sound/CREDITS.md) and in the table of [`tracks.h`](../include/tracks.h).

| File | Title | Author | Source | Licence | Version |
| --- | --- | --- | --- | --- | --- |
| `somewhere-in-the-elevator.bcstm` | "Somewhere in the Elevator" | Peachtea, published by You're Perfect Studio | https://opengameart.org/content/somewhere-in-the-elevator | CC0, one of the four licences its page offers ("CC-BY 4.0", "CC-BY 3.0", "OGA-BY 3.0" and "CC0") | none recorded; the source file's name, size and SHA-256 are |
| `urban-shop.bcstm` | "Urban Shop" | MintoDog | https://opengameart.org/content/urban-shop | CC0 ("License(s): CC0" on its page) | none recorded; the source file's name, size and SHA-256 are |

The page of "Somewhere in the Elevator" gives the notice "Somewhere in the Elevator by Peachtea@You're Perfect Studio".

## Libraries the kit links

The kit's C includes `3ds.h` (libctru), `citro2d.h` (citro2d, which builds on citro3d) and `zlib.h`. Versions and licences as 3DStockTheme's README records them, in its "Dependencies" section, for the build image the kit was cut in:

| Library | Version | Licence | Where it comes from | Recorded in |
| --- | --- | --- | --- | --- |
| libctru | 2.7.0-1 | zlib licence | the devkitPro image | the paragraph "The application adds (third party, ...)" |
| citro2d | 1.7.0-1 | zlib licence | the devkitPro image | the same |
| citro3d | 1.7.1-2 | zlib licence | the devkitPro image | the same |
| zlib | `3ds-zlib` 1.3.1-1 (on the host: the macOS SDK's `zlib.h` 1.2.12) | zlib licence | the devkitPro image's portlibs | the paragraph on zlib |

The image is `devkitpro/devkitarm:20260610`, named in [`tools/build/image.sh`](../tools/build/image.sh).

## Formats

The BCSTM decoder ([`bcstm.h`](../include/bcstm.h)) and the WAV reader ([`wav.h`](../include/wav.h)) are part of the kit's C. The decoder follows 3dbrew's "BCSTM" page, as its header records.

## The project's own

- `working.bcstm`, the working loop: made by `make_working.py` from plain tones, nothing sampled ([`sound/CREDITS.md`](../sound/CREDITS.md), "Working loop: our own").
- `back.wav`'s tone: a sine of 464 Hz under the envelope of `bong_001.ogg`, made by a script of 3DStockTheme's (`make_back.py`), which the kit does not hold.
- The tools beside the sounds: `wav2bcstm.py` and `make_working.py`.
- The art: every image of the atlas is drawn at build time by the art generator and the looks' generators from the numbers in [`ui_layout.h`](../include/ui_layout.h), with the icons above as the only third-party drawings.
- All of the kit's C.

## The renderer's font: Nunito

The flow renderer (`tools/flows/`) sets its text in Nunito Medium: the text in the pictures of `docs/img/` (`make docs-pictures`) and the widths the example's host walk measures its lines with (`tools/flows/fontgen.py`). The font is a tool's file, not a part of the library: no built program holds it, and the console's own font is not used.

| What | As recorded |
| --- | --- |
| File | `tools/flows/font/Nunito.ttf`: the variable font `Nunito[wght].ttf`, unmodified but for the file's name; 276932 bytes, SHA-256 `bb55a5ca5c2042335b3991af27c4d0705d0ef41cac6164ac737fd8f2a1e85207` |
| Version | "Version 3.602" (the font's own name record) |
| Licence | SIL Open Font License 1.1, the licence file beside the font: `tools/flows/font/OFL.txt`, "Copyright 2014 The Nunito Project Authors (https://github.com/googlefonts/nunito)" |
| Source | <https://github.com/google/fonts>, its folder `ofl/nunito/` (the font file as of commit `604936664fd62c14271209b51f98e7f495dd1a3e`) |
| Recorded in | `tools/flows/font/OFL.txt`; the comment at the top of `tools/flows/fontdef.py` |

The licence says of what is drawn with the font: "The requirement for fonts to remain under this license does not apply to any document created using the fonts or their derivatives."

## The kit's own licence

The kit's own files (its C, the generators, the tools, the docs and the sounds named above as the project's own) are under the zlib licence, in [`LICENSE`](../LICENSE): "Copyright (c) 2026 the 3ds-uikit contributors". The third parties' files above keep their own terms.
