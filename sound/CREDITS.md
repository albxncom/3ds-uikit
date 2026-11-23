# Sounds

The interface sounds, the working loop and the menu music the application is built with. Every file taken from elsewhere was taken under CC0 (Creative Commons Zero, a public domain dedication): no credit is required, the sources are named so that each file can be traced. The working loop is our own, and so is the tone of `back.wav`. Nothing here is Nintendo's.

## Which file plays when

| File | When | Source file | Length |
| --- | --- | --- | --- |
| `move.wav` | the cursor shows or moves (D-pad, circle pad, L and R in a list; also in a question) | UI Audio, `rollover2.ogg` | 51 ms |
| `confirm.wav` | a button is pressed that goes on: A, a tap on a tile, a row, a button or the right tab, Y for the preview, OK; a QR code was read | Interface Sounds, `glass_003.ogg` | 120 ms |
| `back.wav` | a button is pressed that goes back: B, a tap on "Zurück", "Abbrechen" in a question | our own tone, shaped by the envelope of Interface Sounds `bong_001.ogg` | 119 ms |
| `popup.wav` | a popup opens (a question, a note, the help) | Interface Sounds, `maximize_008.ogg`, moved up 4 semitones | 216 ms |
| `error.wav` | a problem screen opens, an install ends badly, a preview cannot be shown | Interface Sounds, `error_005.ogg`, moved up 14 semitones | 179 ms |
| `done.wav` | an install ended well ("Installiert") | Interface Sounds, `confirmation_002.ogg` | 534 ms |
| `drop.wav` | a piece of the installing screen's picture lands (a design's install, a badge set's write); pieces that land in the same frame sound once | Interface Sounds, `drop_003.ogg` | 188 ms |
| `working.bcstm` | the working loop: while the application installs, or loads for longer than the loader waits to show (0.15 s); the menu music fades out under it and comes back after it | our own, `make_working.py` | 3.3 s, looped |
| `somewhere-in-the-elevator.bcstm` | the menu music as it comes (the settings choose among the tracks, or none), looped; it pauses while a preview shows a theme | "Somewhere in the Elevator" | 31.5 s, the last 30.7 s loop |
| `urban-shop.bcstm` | the menu music's second track | "Urban Shop" | 83.5 s, all of it loops |

The interface sounds and the working loop follow the settings' "Töne" (sounds), the menu music their "Hintergrundmusik" (background music).

## Swapping a file

Replace the file under the same name and build again (in the devkitPro image); `make test` (or `build/host/test_sound sound`) reads every file with the kit's own readers first and says which one it would not play, and which interface sound is quieter than -34.4 dBFS through an imitation of the console's speakers (a high-pass at 600 Hz, then the loudest 50 ms), too quiet for them.

- An interface sound: a WAV with 16-bit PCM, mono or stereo, 8000 to 48000 Hz, two seconds at most. Every file of the two packs is CC0, so any of them can stand in. They are Ogg Vorbis, which macOS's own tools do not read: convert with ffmpeg (`ffmpeg -i IN.ogg -ac 1 -ar 32000 -c:a pcm_s16le OUT.wav`) or any audio editor.
- A music track or the working loop: a BCSTM. `python3 -I wav2bcstm.py IN.wav OUT.bcstm` makes one from a 16-bit WAV at the rate it is to play at (22050 Hz mono costs about 11 KB a second, 16000 Hz about 8 KB), looping the whole file, or with `--loop START END` (in samples) the part after an intro; `python3 -I wav2bcstm.py --decode IN.bcstm OUT.wav` writes what the console will play, the loop twice, to hear the seam. A theme's own `bgm.bcstm` can be dropped in as it is.
- Which tracks the menu music has is the table in `tracks.h`: a line a track (file, title, author, licence, source page), the first one the default. A new track is its line there, its file here and its section below; `make test` fails while one of the three is missing.
- How loud the kinds are against each other: `AUDIO_SFX_LEVEL`, `AUDIO_MENU_LEVEL`, `AUDIO_WORK_LEVEL` and `AUDIO_THEME_LEVEL` in `audio.h`.

## Interface sounds: Kenney

Both packs are by Kenney (www.kenney.nl). Their asset pages state the licence as "Creative Commons CC0", linked to https://creativecommons.org/publicdomain/zero/1.0/, and https://kenney.nl/support says: "Attribution is not required."

- "Interface Sounds" (1.0), https://kenney.nl/assets/interface-sounds, zip of 834536 bytes, SHA-256 `f2193d072726d6758a5f7871b2dcc54dcce0d5c35c6f0a62f92549b327c81232`. Its `License.txt`: "License: (Creative Commons Zero, CC0) http://creativecommons.org/publicdomain/zero/1.0/ This content is free to use in personal, educational and commercial projects. Support us by crediting Kenney or www.kenney.nl (this is not mandatory)".
- "UI Audio" (its `License.txt` calls it "UI SFX Set"), https://kenney.nl/assets/ui-audio, zip of 411949 bytes, SHA-256 `946fc23a63d535d693eb31b2eabb80c8c28d6351e2186b344ceb71b2cb1d5eb6`. Its `License.txt`: "License (Creative Commons Zero, CC0) http://creativecommons.org/publicdomain/zero/1.0/ You may use these assets in personal and commercial projects. Credit (Kenney or www.kenney.nl) would be nice but is not mandatory."

What was changed: each file mixed to mono and resampled to 32000 Hz, the silence before its start and after its end cut (1 ms and 8 ms kept; the start is the first sample over -45 dBFS, the end the last over -50 dBFS), the last 5 ms faded out, and its level set so that the loudest 50 ms have an RMS of -20 dBFS (`move.wav`: -26 dBFS, because it plays most often).

`back.wav` is made, not converted: a sine of 464 Hz (phase 0) under the amplitude envelope of `bong_001.ogg` as converted above (a centred moving RMS over 139 samples, one period of its 232 Hz, times the square root of 2), then the same trim, fade and level rule, with the loudest 50 ms at -14 dBFS (peak -2.8 dBFS, nothing clipped). `bong_001` itself, a nearly pure tone at 232 Hz, could not be heard on the console's speakers; its octave can. Its generator is a script of 3DStockTheme's (`make_back.py`), as is the conversion of the other sounds (`convert` in its `dsp.py`); the kit holds neither.

`popup.wav` and `error.wav` are the files converted above, moved up in pitch and raised, because the console's speakers played them very quietly and hardly at all: the same sound with every partial moved up under its own envelope (the length, the envelope and the pitch movement kept; nothing resampled), then one gain so that the loudest 50 ms have an RMS of -14 dBFS, as `back.wav` (nothing clipped). `popup.wav`, one tone gliding from 255 to 448 Hz, moved up 4 semitones (now 322 to 564 Hz): its analytic signal multiplied by a phase that runs at 0.26 times its own measured, smoothed frequency, so every moment's frequency f becomes 1.26 f; peak -7.2 dBFS. `error.wav`, a buzz of 50.6 Hz and then a tone of 219.2 Hz, each with its harmonics, moved up 14 semitones (now 113.6 Hz and 492.2 Hz): in the spectrum of each part every harmonic band k moved up by k f0 (2^(14/12) - 1); peak -4.5 dBFS. Each was moved by the fewest semitones that bring it, through an imitation of the speakers (a 6th-order Butterworth high-pass at 600 Hz), within 3 dB under `back.wav` (popup, 2.1 dB under) and from 1 dB under to 2 dB over it (error, 1.0 dB under). Their generator is a script of 3DStockTheme's (`make_popup_error.py`), whose tools' README names the source files and the commands.

`drop.wav` was chosen by the kit's author by ear on 2026-11-18, `back.wav` on 2026-11-19 through an imitation of the console's speakers, after the one chosen the day before could not be heard on the console; `popup.wav` and `error.wav` were made louder on 2026-11-19 by measurement only, nobody has heard the new files yet. Nobody listened when the other five were chosen: they were picked by the packs' own names and by measurements (length, how soft the attack is, pitch, brightness). They are a first set to be replaced by ear.

## Working loop: our own

`working.bcstm` is made by `make_working.py` from plain tones, nothing sampled: its "run". A soft mallet tone (a sine with its octave a third as strong and the octave above that an eighth as strong, in over 12 ms, dying away so that each note rings on under the next ones), in G major pentatonic, 108 quarter notes a minute, six beats (3.333 s): twelve notes in a long-short-short rhythm (an eighth and two sixteenths, four times), B4 A4 G4 E4 D4 E4 G4 A4 D4 G4 B4 D5, then two beats in which they ring out. It is rendered round its own end, so it repeats without a joint, at 22050 Hz mono, peak -14 dBFS.

The script's second loop, "pendulum" (G major, 120 quarter notes a minute, G4 B4 D5 B4, a quarter note each, 2 s), is a calmer stand-in: `python3 -I make_working.py DIR`, then `python3 -I wav2bcstm.py DIR/working-pendulum.wav working.bcstm`.

## Menu music

### "Somewhere in the Elevator" (`somewhere-in-the-elevator.bcstm`, the default)

By Peachtea, published by You're Perfect Studio at https://opengameart.org/content/somewhere-in-the-elevator. The page offers it under "CC-BY 4.0", "CC-BY 3.0", "OGA-BY 3.0" and "CC0"; it is used here under CC0 (https://creativecommons.org/publicdomain/zero/1.0/). The page's notice: "Somewhere in the Elevator by Peachtea@You're Perfect Studio". Source file: `Peachtea - Somewhere in the Elevator.wav`, 10839134 bytes, SHA-256 `52478b76bf603f7e5650bf3e5ff8b1138b5d8f9a147ffc9b318641fa1ee8c80c`.

What was changed: mixed to mono, everything below 120 Hz taken out (the console's speakers do not play it, and it would cost the 4-bit samples their resolution), resampled to 22050 Hz, raised by 2 dB (to about -21 LUFS), cut to 13.44 s to 44.97 s of the original, and written as DSP-ADPCM by `wav2bcstm.py`. The first 0.81 s play once; from 14.25 s of the original on the track repeats itself 30.72 s later, and that part loops.

### "Urban Shop" (`urban-shop.bcstm`)

By MintoDog, at https://opengameart.org/content/urban-shop. The page states "License(s): CC0" (https://creativecommons.org/publicdomain/zero/1.0/) and says "Music for shop theme. This music is loopable." Source file: `urban_shop_bpm92_0.ogg`, 2735750 bytes, SHA-256 `7f7e70a28b65144d70969e008add149ccfb3bdd6e09170bf94e09ed2a56ac66b`.

What was changed: mixed to mono, everything below 120 Hz taken out, resampled to 16000 Hz (99.5 % of its energy lies below 4 kHz, and at this rate its 83.5 s cost 764 KB instead of 1053 KB), lowered by 3.9 dB (to about -21 LUFS), and written as DSP-ADPCM by `wav2bcstm.py`. The whole track loops, as its author made it.
