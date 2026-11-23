# Icons

The symbols on the screens are 18 outline icons of Tabler Icons, as SVG files in `icons/`. The art generator draws them into the atlas at build time: `tabler(name)` in `art.py` reads `icons/<name>.svg` and strokes its paths with round caps and joins; every `<path>` is drawn except the 24x24 frame (`stroke="none"`), and any other shape stops the build. Another symbol is its file copied there and its name in the generator. Source, version and licence: [credits.md](credits.md).

Each icon becomes one or more sprites (`ART_*`), drawn in a size, a colour and a stroke width of their own. The sprites marked "Stock" are drawn by Stock's generator (`look_stock.py`); the others by `art.py`.

| File | Sprites | Where they are drawn |
| --- | --- | --- |
| `alert-triangle.svg` | `ART_WARN`, `ART_WARN_ROW`, `ART_INFO_WARN` | the warning on top screens (the folder view, the check screen, the preview, a problem screen, the Badges part); in a design's row; as a note's sign in a popup |
| `award.svg` | `ART_BADGES` (Stock) | the start screen's Badges tile and the Badges menu |
| `brush.svg` | `ART_DESIGNS` (Stock) | the start screen's Designs tile and the Designs menu |
| `check.svg` | `ART_DONE_BADGE`, `ART_STEP_DONE_GLASS_SHARD`, `ART_CHECK_ROW`, `ART_FACT_CHECK` | the check on the done card; the installing screen's finished steps; before the chosen line of `ui_choices` or `ui_choices_top` and in a design's row; a row of `ui_facts`. Nachtglas's generator draws a tick from it too |
| `chevron-right.svg` | `ART_CHEVRON` | the way to an entry in the HOME Menu (house, name, chevron, name) |
| `device-sd-card.svg` | `ART_SD_OFF`, `ART_INFO_SD`, `ART_SD` (Stock), `ART_DELETE_FILE` (Stock) | crossed out on the installing screen; a note's sign; the tile for the card in the Designs and Badges menus; the tile that deletes the file on the check screen |
| `download.svg` | `ART_STEPS_TITLE`, `ART_DOWNLOAD` (Stock) | the installing screen's title; the green install button (`ui_go`) |
| `file-check.svg` | `ART_KEEP` (Stock) | the tile that keeps the file on the done screen |
| `file.svg` | `ART_FACT_FILE` | the file's row in `ui_facts` |
| `folder-up.svg` | `ART_FOLDER_UP` (Stock), `ART_FOLDER_UP_OFF` | the corner button (`ui_corner`), and the same greyed out |
| `folder.svg` | `ART_FOLDER` (Stock), `ART_FOLDER_HERO`, `ART_FOLDER_BIG`, `ART_FOLDER_FOOT` | a folder's row (`ui_row_folder`); a folder's picture on the top screen; the folder view's top; a footer line |
| `help.svg` | `ART_HELP_MARK` (Stock) | the help button (`ui_help`) |
| `music.svg` | `ART_MUSIC_BADGE`, `ART_MUSIC_ROW`, `ART_NO_MUSIC` (Stock) | the badge on a design's picture that has music; in a design's row; crossed out on "Ohne Musik installieren" (`ui_alt`) |
| `power.svg` | `ART_POWER_OFF` | crossed out on the installing screen |
| `qrcode.svg` | `ART_QR_SCAN`, `ART_QR` (Stock) | the QR code scanner; the tile for a QR code in the Designs and Badges menus |
| `tool.svg` | `ART_SETTINGS` (Stock) | the settings button (`ui_settings_button`) |
| `trash.svg` | `ART_TRASH` (Stock) | the tile that deletes the file on the done screen |
| `world.svg` | `ART_GLOBE`, `ART_GLOBE_SMALL` | the address card of a download from a QR code; the top screen of a running job |

The generator's own drawings, the house for example, are not icons of the pack ([credits.md](credits.md)).
