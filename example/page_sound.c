/* Sound: a tile for each of the seven interface sounds, the working loop on and off, the menu music (off or a track
 * of tracks.h, looped) and the interface sounds on and off; the bar's right tab (or Y) plays a theme's music in the
 * menu music's place and stops it (audio_theme_play with a track linked in, as a theme's bgm.bcstm would lie in
 * memory: no file is read). Without the DSP firmware on the card the kit plays nothing (audio_ready answers 0), which
 * this page says, and its log line (audio_log) says why; the page shows it. */
#include <stdio.h>
#include "gallery.h"
#include "sound_data.h"
#include "widget.h"

#define ROW_HIGH SET_H_MAX     /* the setting row's face, which grows: three rows above the bar */
enum { WORK = SFX_COUNT, MUSIC, SOUNDS, TARGETS, THEME = TARGETS };    /* THEME: the right tab */
static const char *const SOUND_NAMES[SFX_COUNT] = {[SFX_MOVE] = "Move", [SFX_CONFIRM] = "Confirm", [SFX_BACK] = "Back",
                                                   [SFX_POPUP] = "Popup", [SFX_ERROR] = "Error", [SFX_DONE] = "Done",
                                                   [SFX_DROP] = "Drop"};
static int working, music = -1, sounds_on = 1, theme, theme_plays;

/* the theme's music: the last track of tracks.h, as the application links them in (sound_data.S) */
#define TRACK_START(symbol, file, title, author, licence, page) track_##symbol,
#define TRACK_END(symbol, file, title, author, licence, page) track_##symbol##_end,
static const uint8_t *const STARTS[TRACK_COUNT] = {TRACKS(TRACK_START)}, *const ENDS[TRACK_COUNT] = {TRACKS(TRACK_END)};

static rect target(int i)
{
    float w = (BOTTOM_W - 2 * PAD - 3 * GAP) / 4;
    int col = i < MUSIC ? i % 4 : (i - MUSIC) * 2, span = i < MUSIC ? 1 : 2;
    return (rect){PAD + col * (w + GAP), 8 + (i < MUSIC ? i / 4 : 2) * (ROW_HIGH + GAP), span * w + (span - 1) * GAP,
                  ROW_HIGH};
}

static void press(int i)
{
    if (i < SFX_COUNT) {
        sfx(i);
        return;
    }
    sfx(SFX_CONFIRM);
    if (i == WORK)
        audio_work(working = !working);
    else if (i == MUSIC)
        audio_menu(music = music + 1 < TRACK_COUNT ? music + 1 : -1);
    else if (i == THEME && (theme = !theme))
        theme_plays = audio_theme_play(STARTS[TRACK_COUNT - 1],
                                       (size_t)(ENDS[TRACK_COUNT - 1] - STARTS[TRACK_COUNT - 1])) == 0;
    else if (i == THEME)
        audio_theme_stop();     /* the menu music goes on where it paused */
    else
        audio_sounds(sounds_on = !sounds_on);
}

static const char *label(int i)
{
    static char s[64];
    if (i < SFX_COUNT)
        return SOUND_NAMES[i];
    if (i == WORK)
        return working ? "Job: on" : "Job: off";
    if (i == SOUNDS)
        return sounds_on ? "Sounds: on" : "Sounds: off";
    snprintf(s, sizeof s, "Music: %s", music < 0 ? "off" : TRACK_LIST[music].title);
    return s;
}

static void top(void)
{
    int silent = !audio_ready();
    ui_head("Sound", music < 0 ? "No music" : TRACK_LIST[music].author);
    card_text(silent ? "This console has no DSP firmware on its card (sdmc:/3ds/dspfirm.cdc), so the example is "
                       "silent: the kit plays nothing and every sound call does nothing."
                     : "Each tile plays its sound. Job starts the working loop, which takes over from the music after "
                       "a moment; Music plays a track from its start, looped. The right tab plays one as a theme's "
                       "music, the menu music paused until it stops.");
    ui_foot(FOOT_DOTS_Y, -1, sound_line[0] ? sound_line : "No log line yet", MUTED, 0, NULL);
}

static void bottom(int held)
{
    for (int i = 0; i < TARGETS; i++)
        ui_wide(FACE_SET, target(i), held == i ? UI_DOWN : 0, -1, label(i), LABEL_PX, 0, TILE_INK);
}

static void bar(int held)
{
    ui_bar2("Back", !theme ? "Theme: off" : theme_plays ? "Theme: on" : "Theme: silent", held);
}

const page page_sound = {.name = "Sound", .symbol = ART_NO_MUSIC, .count = TARGETS, .columns = 4, .target = target,
                         .press = press, .top = top, .bottom = bottom, .right = "Theme", .bar = bar};
