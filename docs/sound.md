# Sound

Sound goes through libctru's `ndsp` on a thread of its own: the interface sounds, the menu music, the working loop and the music of a theme being previewed. `ndsp` needs the DSP firmware dump on the card (`sdmc:/3ds/dspfirm.cdc`); without it `audio_init` says so in the log, nothing sounds and every call does nothing. How loud it all is on the speakers is the console's volume slider; the kit's levels only set the sounds against each other.

The sounds are the files of [`sound/`](../sound/), built into the application as they are by [`sound_data.S`](../source/sound_data.S) ([`sound_data.h`](../include/sound_data.h) names their bytes). Swapping one is replacing its file and building again. Their sources and licences are in [credits.md](credits.md) and in [`sound/CREDITS.md`](../sound/CREDITS.md).

## The interface

[`audio.h`](../include/audio.h):

- `void audio_init(void);` and `void audio_exit(void);`: starts `ndsp` and loads the sounds, with one log line on what came of it. Nothing sounds yet.
- `int audio_ready(void);`: 1 while `ndsp` runs (after an `audio_init` that started it, until `audio_exit`), else 0: whether anything can sound, for a program that says so on its screen; the log line says why not.
- `void sfx(int id);`: plays an interface sound (`SFX_*`) if they are on; up to four sound at once, a fifth cuts off the oldest.
- `void audio_sounds(int on);`: the interface sounds and the working loop on or off.
- `void audio_menu(int which);`: the menu music, the track at place `which` of the music table, from its start, looped; -1: none.
- `void audio_work(int on);`: work begins (1) or ends (0); see the working loop below.
- `int audio_theme_play(const uint8_t *bcstm, size_t size);`: a theme's `bgm.bcstm` in place of the menu music, which pauses; `NULL`: no music, only the menu music pauses. The file must stay in memory until `audio_theme_stop`. Returns -1 when it does not play.
- `void audio_theme_stop(void);`: the theme's music ends and the menu music goes on where it paused.

The levels, 0 to 1: `AUDIO_SFX_LEVEL` 0.80, `AUDIO_MENU_LEVEL` 0.35, `AUDIO_WORK_LEVEL` 0.60, `AUDIO_THEME_LEVEL` 0.80. The menu music stays under the interface sounds and the working loop between the two.

The log hook: `audio_log`, a function of the form of `printf` that the application sets before `audio_init`. While it is `NULL`, nothing is logged.

## The seven interface sounds

The ids follow the order of the table in `sound/CREDITS.md`. When 3DStockTheme plays each, from its calls of `sfx`:

| Id | File | When 3DStockTheme plays it |
| --- | --- | --- |
| `SFX_MOVE` | `move.wav` | moving the selection in a list; the cursor showing or moving among a screen's buttons, among a popup's two buttons, in the settings (also after a choice changes there) and in the Badges part's screens |
| `SFX_CONFIRM` | `confirm.wav` | pressing a button that goes on: a tile, a row, a button, the bar's right action, a popup's only or right button, and the bar's left action on the done screen and on a problem screen that ended fine; a QR code was read |
| `SFX_BACK` | `back.wav` | pressing the bar's left action elsewhere, and a question's left button ("Abbrechen") |
| `SFX_POPUP` | `popup.wav` | a popup opens |
| `SFX_ERROR` | `error.wav` | a problem screen opens; an install ends badly; a preview cannot be shown; a badge set's write ends badly |
| `SFX_DONE` | `done.wav` | an install ends well; a badge set was written or removed |
| `SFX_DROP` | `drop.wav` | a piece of the installing screen's picture lands |

The interface sounds are 16-bit PCM WAVs, read by [`wav.h`](../include/wav.h):

- `int wav_read(const uint8_t *file, size_t size, wav_info *out);`: RIFF/WAVE with 16-bit PCM, mono or stereo, 8 to 48 kHz; -1 for anything else. No allocation; the result points into the file.

## The working loop

While the application works (an install, a load that shows the loader) the menu music gives way to a short loop, `working.bcstm`, and comes back after it. 3DStockTheme says when work begins and ends with `audio_work`; the kit's audio thread steps [`work.h`](../include/work.h) about 200 times a second.

Work that ends within `WORK_DELAY_MS` (as long as the loader waits, `LOADER_WAIT_MS`) changes nothing. Then the loop comes in over `WORK_LOOP_FADE_MS` while the menu music goes out over `WORK_FADE_MS` and pauses; when the work ends they change back the same way. Begins and ends are counted, so work inside work is fine. With the interface sounds off there is no loop.

- `void work_reset(work_state *w, uint64_t now);`
- `void work_begin(work_state *w, uint64_t now);`
- `void work_end(work_state *w);`
- `int work_step(work_state *w, uint64_t now, int enabled);`: sets the two levels and returns `WORK_START`, `WORK_STOP` or `WORK_NONE` for the loop.

## The music table

[`tracks.h`](../include/tracks.h) holds the menu music in one table, a line a track, the first one the default:

```c
X(symbol, file in sound/, title, author, licence as taken, source page)
```

Today it has two tracks: `somewhere-in-the-elevator.bcstm` (the default) and `urban-shop.bcstm`. The settings screen lists them in this order; the settings file names the chosen one by its file, so a line may move or go without another track's choice changing.

- `int track_find(const char *file);`: the track with this file name, its place in the list, or -1. `TRACK_LIST` holds the tracks.

Adding a track: its line in `tracks.h`, its file in `sound/` and its section in `sound/CREDITS.md`. `make test` fails while one of the three is missing.

## The music format

Music and the working loop are BCSTM streams, decoded by [`bcstm.h`](../include/bcstm.h) (PCM8, PCM16 and DSP-ADPCM, one or two channels, with its loop; after 3dbrew's "BCSTM"). The file is checked once when it is opened, so no sample byte can make a read leave it.

- `int bcstm_open(bcstm_stream *s, const uint8_t *file, size_t size);`: -1 when it is not a BCSTM the decoder accepts.
- `size_t bcstm_read(bcstm_stream *s, int16_t *out, size_t frames, int loop);`: interleaved signed 16-bit frames, looping when asked and the file loops.
- `void bcstm_rewind(bcstm_stream *s);`: back to frame 0.

## The files of `sound/`

| File | What it is |
| --- | --- |
| `move.wav`, `confirm.wav`, `back.wav`, `popup.wav`, `error.wav`, `done.wav`, `drop.wav` | the seven interface sounds |
| `working.bcstm` | the working loop |
| `somewhere-in-the-elevator.bcstm`, `urban-shop.bcstm` | the menu music |
| `CREDITS.md` | which file plays when, every source and licence, what was changed, how to swap a file |
| `wav2bcstm.py` | makes a BCSTM from a 16-bit WAV (`--loop START END` for an intro), and with `--decode` a WAV of what the console will play |
| `make_working.py` | makes the working loop from plain tones |

`make test` (or `build/host/test_sound sound`, the kit's `host/test_sound.c`) reads every file with the kit's own readers and says which one it would not play, and which interface sound is too quiet for the console's speakers.
