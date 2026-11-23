/* The kit's example (example/) on the Mac: its main.c under rec.c's recorder (fake/3ds.h, fake/citro2d.h),
 * with scripted keys, touches and clock, and the sound calls counted instead of played (audio.c is not
 * linked). The walk opens every page, uses every control once, goes back; then, for every look the build holds
 * (chosen on the settings page as a user would), it opens every page again; it leaves through START. It writes the
 * draw list of still frames into OUT_DIR: of each page and control in Stock, of the start page in every look, and of
 * every page in every look (look<N>-<page>), each as OUT_DIR/<name>.dl with the shown page's targets beside it
 * (<name>.targets). Exit 0 when the walk held: every page reached, every sound asked for, the music, the theme's
 * music, the working loop and the sounds' switch used both ways, a touch held while a popup or a page opened and
 * closed pressed nothing when lifted, a tab the bar does not draw took no touch, the clock read once a frame by the
 * example (the pages draw with its frame_now) and once by ui_begin, the New 3DS's speed asked for, START left. With 3d
 * the 3D slider is up for the whole walk: every frame draws the top screen for both eyes. Built by example.sh.
 * Run: driver OUT_DIR [3d] */
#define _DARWIN_C_SOURCE
#include <3ds.h>
#define main example_main
#include "main.c"
#undef main
#include <stdlib.h>
#include <string.h>
#include "looks_built.h"
#include "tracks.h"

void rec_dump(const char *prefix);
extern float rec_slider;

/* ---- libctru: the keys, the stylus and the clock as the script sets them */
static u64 sim_now = 1000000, sim_ticks;
static u32 sim_keys, keys_now, keys_was;
static int sim_x, sim_y, clock_reads, speedup;
void hidScanInput(void) { keys_was = keys_now; keys_now = sim_keys; }
u32 hidKeysDown(void) { return keys_now & ~keys_was; }
u32 hidKeysHeld(void) { return keys_now; }
u32 hidKeysUp(void) { return keys_was & ~keys_now; }
u32 hidKeysDownRepeat(void) { return keys_now & ~keys_was; }
void hidTouchRead(touchPosition *t) { t->px = (u16)sim_x; t->py = (u16)sim_y; }
void hidSetRepeatParameters(u32 a, u32 b) { (void)a; (void)b; }
u64 osGetTime(void) { clock_reads++; return sim_now; }
void osSetSpeedupEnable(bool enable) { speedup = enable; }
u64 svcGetSystemTick(void) { return sim_ticks += 268112; }
bool aptMainLoop(void) { return true; }

/* ---- the sound: counted */
void (*audio_log)(const char *fmt, ...);
static int n_sfx[SFX_COUNT], n_menu[TRACK_COUNT + 1], n_sounds[2], n_work[2], n_theme[2];
void audio_init(void) { if (audio_log) audio_log("sound: off (the host walk plays nothing)"); }
void audio_exit(void) {}
int audio_ready(void) { return 0; }
void sfx(int id) { if (id >= 0 && id < SFX_COUNT) n_sfx[id]++; }
void audio_sounds(int on) { n_sounds[on != 0]++; }
void audio_menu(int which) { if (which >= -1 && which < TRACK_COUNT) n_menu[which + 1]++; }
void audio_work(int on) { n_work[on != 0]++; }
int audio_theme_play(const uint8_t *bcstm, size_t size) { n_theme[bcstm && size]++; return 0; }
void audio_theme_stop(void) { n_theme[0]++; }
/* the tracks as sound_data.S links them in: a byte each here, which audio_theme_play only counts */
#define TRACK_BYTE(symbol, file, title, author, licence, page) const uint8_t track_##symbol[1], track_##symbol##_end[1];
TRACKS(TRACK_BYTE)

/* ---- the script */
static const page *const ALL[] = {&page_start, &page_tiles, &page_buttons, &page_list, &page_settings, &page_text,
                                  &page_popups, &page_loader, &page_top, &page_pictures, &page_sound};
#define PAGES ((int)(sizeof ALL / sizeof *ALL))
static int seen[PAGES], frames, shots, left, clock_frames, held_pressed, tab_pressed;
static const char *out_dir;

static void step(void)
{
    if (left)
        return;
    clock_reads = 0;
    left = !frame();
    clock_frames += clock_reads != 1 + !left;   /* frame() once, ui_begin once (ui.c); the pages use frame_now */
    frames++;
    for (int i = 0; i < PAGES; i++)
        seen[i] |= ALL[i] == shown;
    sim_now += 16;
}
static void wait_ms(unsigned ms) { for (unsigned t = 0; t < ms; t += 16) step(); }
static void key(u32 k) { sim_keys |= k; step(); sim_keys &= ~k; step(); }
static void stylus(int x, int y) { sim_x = x, sim_y = y; sim_keys |= KEY_TOUCH; }
static void lift(void) { sim_keys &= ~KEY_TOUCH; step(); }
/* a tap rests 7 frames (past TOUCH_HOLD_MS, so a row in the list presses too) */
static void tap_at(int x, int y) { stylus(x, y); for (int k = 0; k < 7; k++) step(); lift(); }
static void tap(rect r) { tap_at((int)(r.x + r.w / 2), (int)(r.y + r.h / 2)); }
static void tap_target(int i) { tap(shown->target(i)); }
/* the stylus from y0 to y1 at x in n frames; lifted at once (a fling: the rows go on) */
static void drag(int x, int y0, int y1, int n)
{
    stylus(x, y0);
    step();
    for (int k = 1; k <= n; k++) {
        sim_y = y0 + (y1 - y0) * k / n;
        step();
    }
    lift();
}
/* the cursor's corners at rest: the frame drawn at a whole SELECT_MS */
static void still(void)
{
    while (!left && sim_now % SELECT_MS >= 16)   /* a walk that left too early ends: no clock runs then */
        step();
    step();
}
/* the frame just drawn, still or not; beside its list the page's targets, a line each (x y w h) */
static void shot_now(const char *name)
{
    char prefix[1024], path[1100];
    snprintf(prefix, sizeof prefix, "%s/%s", out_dir, name);
    rec_dump(prefix);
    snprintf(path, sizeof path, "%s.targets", prefix);
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "walk: cannot write %s\n", path);
        exit(3);
    }
    for (int i = 0; i < shown->count; i++) {
        rect r = shown->target(i);
        fprintf(f, "%.3f %.3f %.3f %.3f\n", (double)r.x, (double)r.y, (double)r.w, (double)r.h);
    }
    fclose(f);
    shots++;
    fprintf(stderr, "captured %s: page %s, look %d, scheme %d, glass %d, pattern %d\n", name, shown->name, look_now,
            scheme_now, glass_now, pattern_now);
}
static void shot(const char *name)
{
    still();
    shot_now(name);
}
static void shotf(const char *fmt, int n)
{
    char name[64];
    snprintf(name, sizeof name, fmt, n);
    shot(name);
}
static void open_tile(int i) { tap(page_start.target(i)); wait_ms(300); }
/* look<N>-<page in small letters><what> */
static void shot_look(const char *what)
{
    char name[64];
    int n = snprintf(name, sizeof name, "look%d-%s", look_now, shown->name);
    for (int i = 5; i < n; i++)
        name[i] = (char)(name[i] >= 'A' && name[i] <= 'Z' ? name[i] - 'A' + 'a' : name[i]);
    snprintf(name + n, sizeof name - (size_t)n, "%s", what);
    shot(name);
}

/* every page once in the look in use, from the start page and back: each kind of the Top page, the Text page's two
 * widths, the Pictures page's three, a popup open and the loader at work */
static void tour(void)
{
    shot_look("");
    for (int i = 1; i <= 9; i++) {
        open_tile(i);
        if (shown == &page_top || shown == &page_pictures || shown == &page_text) {
            for (int k = 0; k < (shown == &page_text ? 2 : 3); k++) {
                if (shown == &page_top)
                    key(KEY_A);
                else
                    tap_target(shown == &page_text ? 0 : k);
                char what[8];
                snprintf(what, sizeof what, "%d", k);
                shot_look(what);
            }
        } else {
            shot_look("");
        }
        if (shown == &page_popups) {
            tap_target(2);
            wait_ms(400);
            shot_look("-warning");
            key(KEY_B);
            wait_ms(400);
        } else if (shown == &page_loader) {
            tap_target(0);
            wait_ms(900);
            shot_look("-known");
            wait_ms(2200);
        }
        key(KEY_B);
        wait_ms(300);
    }
    open_tile(0);
    shot_look("");
    key(KEY_B);
    wait_ms(300);
}

static void walk(void)
{
    wait_ms(FADE_MS + 100);
    shotf("start-look%d", LOOK_STOCK);
    key(KEY_DRIGHT);                    /* the first press shows the cursor on the settings button */
    shot("start-cursor");
    key(KEY_DRIGHT);
    key(KEY_A);                         /* the cursor's tile: Tiles */
    wait_ms(300);
    shot("tiles");
    tap_target(0);                      /* amber */
    tap_target(2);                      /* greyed out: the error sound */
    stylus(PAD + 20, 40);
    step(), step();
    shot("tiles-held");
    lift();
    for (int k = 0; k < 9; k++) {
        tap_target(1);
        if (k == 0 || k == 8)
            shotf("tiles-face%d", k);
    }
    key(KEY_B);

    open_tile(2);                       /* Buttons */
    shot("buttons");
    int confirms = n_sfx[SFX_CONFIRM];
    tap_at(BOTTOM_W - 20, 228);         /* one tab: the right tab's place takes nothing */
    tab_pressed += n_sfx[SFX_CONFIRM] != confirms;
    for (int k = 1; k < 4; k++) {
        tap_target(1);
        shotf("buttons-bar%d", k);
    }
    tap_at(20, 228);                    /* the right tab alone: the left tab's place does not go back */
    tab_pressed += shown != &page_buttons;
    tap_at(BOTTOM_W - 20, 228);         /* the right tab */
    key(KEY_Y);
    tap_target(1);                      /* back to one tab */
    tap_target(2);                      /* the green button greyed out */
    tap_target(1);                      /* ... does nothing */
    shot("buttons-off");
    tap_target(2);
    tap_target(0);                      /* help */
    shot("buttons-help");
    tap_at(20, 228);                    /* the left tab: back */

    open_tile(3);                       /* List */
    shot("list");
    key(KEY_DDOWN);                     /* selects the first row that shows whole */
    key(KEY_DDOWN), key(KEY_DDOWN), key(KEY_DRIGHT);
    wait_ms(300);
    shot("list-selected");
    key(KEY_A);
    drag(150, 180, 80, 12);             /* a slow drag */
    wait_ms(200);
    shot("list-dragged");
    drag(150, 190, 70, 3);              /* a fling */
    wait_ms(1200);
    shot("list-flung");
    stylus(316, 60);                    /* beside the scroll bar, where it still takes touches: the thumb to the end */
    step();
    sim_y = 199;
    step(), step();
    shot("list-thumb");
    lift();
    tap_at(150, 120);                   /* a row chosen by a tap */
    shot("list-chosen");
    tap_at(20, 16);                     /* the corner button: back */

    open_tile(4);                       /* Text */
    shot("text");
    tap_target(0);
    shot("text-narrow");
    key(KEY_B);

    open_tile(5);                       /* Popups */
    shot("popups");
    for (int k = 0; k < 4; k++) {
        tap_target(k);
        if (k == 0)
            tap(ui_popup_button(0, 1)); /* too soon after it opened: nothing */
        wait_ms(400);
        shotf("popup%d", k);
        if (k == 0)
            tap(ui_popup_button(0, 1));
        else if (k == 1)
            key(KEY_DRIGHT), key(KEY_DRIGHT), key(KEY_A);   /* shows the cursor, moves it, presses */
        else if (k == 2)
            key(KEY_B);
        else
            tap(ui_popup_button(1, 1));
        wait_ms(400);
    }
    /* the stylus held on Warning while A shows the cursor and A opens Plain (B does nothing while it is down): the
     * lift presses nothing, and once B has closed Plain no tile shows held */
    int popups = n_sfx[SFX_POPUP];
    stylus(PAD + 20, (int)(shown->target(2).y + 20));
    step();
    key(KEY_A), key(KEY_A);
    wait_ms(400);
    key(KEY_B);
    lift();
    wait_ms(400);
    key(KEY_B);
    wait_ms(400);
    held_pressed += n_sfx[SFX_POPUP] != popups + 1 || touch_held(&touch, sim_now) >= 0;
    key(KEY_B);

    open_tile(6);                       /* Loader */
    tap_target(0);
    wait_ms(900);
    shot("loader-known");
    wait_ms(2200);
    tap_target(1);
    wait_ms(900);
    shot("loader-unknown");
    wait_ms(2200);
    key(KEY_B);

    open_tile(7);                       /* Top screen */
    shot("top0");
    key(KEY_A);
    shot("top1");
    tap_at(BOTTOM_W - 20, 228);
    shot("top2");
    key(KEY_B);

    open_tile(8);                       /* Pictures */
    shot("pictures-wall");
    tap_target(1);
    shot("pictures-colours");
    tap_target(2);
    shot_now("pictures-camera-first");  /* the frame of the choice: its place on both eyes */
    shot("pictures-camera");
    key(KEY_B);

    open_tile(9);                       /* Sound */
    for (int i = 0; i < SFX_COUNT + 3; i++)
        tap_target(i);
    tap_target(SFX_COUNT + 1), tap_target(SFX_COUNT + 1);   /* the second track, then no music */
    tap_at(BOTTOM_W - 20, 228);                             /* the right tab: the theme's music */
    shot("sound");
    tap_at(BOTTOM_W - 20, 228);                             /* ... stopped */
    tap_target(SFX_COUNT), tap_target(SFX_COUNT + 2);       /* the working loop off, the sounds on */
    key(KEY_B);

    open_tile(0);                       /* Settings, by the settings button */
    shot("settings");
    for (int k = 1; k < LOOKS_HELD; k++) {
        tap_target(0);
        key(KEY_B);
        wait_ms(300);
        shotf("start-look%d", look_now);
        open_tile(0);
    }
    tap_target(0);                      /* back to Stock */
    for (int k = 1; k <= ART_SCHEMES; k++) {
        tap_target(1);
        shotf("settings-scheme%d", scheme_now);
    }
    for (int k = 0; k < GLASS_COUNT; k++)
        tap_target(2);
    for (int k = 0; k < PATTERN_COUNT; k++) {
        tap_target(3);
        shotf("settings-pattern%d", pattern_now);
    }
    key(KEY_DDOWN), key(KEY_DDOWN);     /* the cursor on the scheme's row, then left and right */
    key(KEY_DLEFT), key(KEY_DRIGHT);
    shot("settings-cursor");
    /* the stylus held on the scheme's row while B goes back: lifted on the Tiles tile, it opens nothing */
    stylus(40, 78);
    step(), step();
    key(KEY_B);
    wait_ms(300);
    lift();
    wait_ms(300);
    held_pressed += shown != &page_start;

    tour();
    for (int k = 1; k < LOOKS_HELD; k++) {
        open_tile(0);
        tap_target(0);
        key(KEY_B);
        wait_ms(300);
        tour();
    }
    key(KEY_START);
}

static int check(int ok, const char *what)
{
    if (!ok)
        fprintf(stderr, "walk: FAILED: %s\n", what);
    return ok;
}

int main(int argc, char **argv)
{
    if (argc < 2 || argc > 3 || (argc == 3 && strcmp(argv[2], "3d"))) {
        fprintf(stderr, "usage: driver OUT_DIR [3d]\n");
        return 2;
    }
    out_dir = argv[1];
    rec_slider = argc == 3;
    start();
    walk();
    finish();
    int ok = check(left, "START left the gallery"), reached = 0;
    for (int i = 0; i < PAGES; i++) {
        reached += seen[i];
        if (!seen[i])
            fprintf(stderr, "walk: page %s not reached\n", ALL[i]->name);
    }
    ok &= check(reached == PAGES, "every page reached");
    for (int i = 0; i < SFX_COUNT; i++)
        ok &= check(n_sfx[i] > 0, "every interface sound asked for");
    for (int i = 0; i <= TRACK_COUNT; i++)
        ok &= check(n_menu[i] > 0, "the music off and every track");
    ok &= check(n_sounds[0] && n_sounds[1], "the sounds off and on");
    ok &= check(n_work[0] && n_work[1], "the working loop on and off");
    ok &= check(n_theme[0] && n_theme[1], "the theme's music played and stopped");
    ok &= check(!held_pressed, "a touch held while a popup or a page opened and closed pressed nothing when lifted");
    ok &= check(!tab_pressed, "a tab the bar does not draw took no touch");
    ok &= check(!clock_frames, "the clock read once a frame");
    ok &= check(speedup, "the New 3DS's speed asked for");
    fprintf(stderr, "walk: %d frames, %d of %d pages reached, %d looks, %d captures%s, sounds", frames, reached, PAGES,
            LOOKS_HELD, shots, rec_slider > 0 ? " in 3D" : "");
    for (int i = 0; i < SFX_COUNT; i++)
        fprintf(stderr, " %d", n_sfx[i]);
    fprintf(stderr, ", %s\n", ok ? "held" : "FAILED");
    return ok ? 0 : 1;
}
