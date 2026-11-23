/* Sound through libctru's ndsp: the interface sounds, the menu music, the working loop and the music of a theme
 * being previewed. ndsp needs the DSP firmware dump on the card (sdmc:/3ds/dspfirm.cdc); without it audio_init says
 * so in the log, nothing sounds and every call here does nothing. How loud it all is on the speakers is the
 * console's volume slider, which acts on everything the DSP puts out; the levels below only set the sounds against
 * each other.
 *
 * The sounds are the files of sound/, built into the application as they are (sound_data.S): seven WAVs,
 * the working loop and the menu music's tracks (tracks.h) as BCSTMs. Swapping one is replacing its file and
 * building again (sound/CREDITS.md). */
#ifndef AUDIO_H
#define AUDIO_H

#include <stddef.h>
#include <stdint.h>

/* The interface sounds, in the order of sound/CREDITS.md's table. */
enum { SFX_MOVE, SFX_CONFIRM, SFX_BACK, SFX_POPUP, SFX_ERROR, SFX_DONE, SFX_DROP, SFX_COUNT };

/* The levels, 0 to 1, of the kinds of sound against each other: the menu music stays under the interface sounds,
 * the working loop between the two, a theme's own music plays as it is. */
#define AUDIO_SFX_LEVEL 0.80f
#define AUDIO_MENU_LEVEL 0.35f
#define AUDIO_WORK_LEVEL 0.60f
#define AUDIO_THEME_LEVEL 0.80f

/* Where the log lines go (what audio_init came to, a track the player does not read): a function of the form of
 * printf, which the application sets before audio_init. NULL at the start: then nothing is logged. */
extern void (*audio_log)(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* Starts ndsp and loads the sounds; one line in the log says what came of it. Nothing sounds yet: the settings
 * say which menu music plays (audio_menu) and whether the interface sounds are on (audio_sounds). */
void audio_init(void);
void audio_exit(void);
/* 1 while ndsp runs (from an audio_init that started it to audio_exit), else 0: whether anything can sound. */
int audio_ready(void);

/* Plays an interface sound (SFX_*), if they are on; up to four sound at once, a fifth cuts off the oldest. */
void sfx(int id);
/* The interface sounds and the working loop on (1) or off (0). */
void audio_sounds(int on);

/* The menu music: the track at place `which` of tracks.h's list plays from its start, looped, at once; -1: none. */
void audio_menu(int which);

/* Work begins (1) or ends (0): an install, or a load that shows the loader. Once it has lasted WORK_DELAY_MS
 * (work.h) the menu music fades out and the working loop comes in; when it ends they change back. Begins and ends
 * are counted, so work inside work is fine as long as each begin has its end. */
void audio_work(int on);

/* A theme's music in place of the menu music, which pauses: bcstm is a whole bgm.bcstm in memory and must stay
 * until audio_theme_stop. NULL: no music, only the menu music pauses (the theme has none, and the preview shows the
 * theme as it is). Returns 0, or -1 when the music does not play: the file is not a stream the reader accepts, or
 * there is no sound at all (silence then). */
int audio_theme_play(const uint8_t *bcstm, size_t size);
/* The theme's music ends and the menu music goes on where it paused. */
void audio_theme_stop(void);

#endif
