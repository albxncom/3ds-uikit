/* Popup: a plain one with one button, one whose text holds a path (cut at its beginning when it must be), a
 * question with the warning sign, and one whose right button deletes (its tone). A popup takes a touch only once it
 * has stood POPUP_GUARD_MS, and the page under it only that long after it closed (popup.h); its keys: the first
 * press shows the cursor on Cancel, left and right move it, A presses it, B cancels. */
#include <stdio.h>
#include "gallery.h"
#include "ease.h"
#include "popup.h"
#include "touch.h"
#include "widget.h"

#define POP_MS 200              /* a popup opens, and closes, in this time */
#define TILE_HIGH WIDE_H
enum { PLAIN, PATH, WARNING, DELETE, KINDS };
static const char *const NAMES[KINDS] = {"Plain", "With a path", "Warning", "Delete"};
static const int SYMBOLS[KINDS] = {ART_HELP_MARK, ART_FOLDER, ART_INFO_WARN, ART_TRASH};
static const char *const RIGHT[KINDS] = {"OK", "Make it", "Go on", "Delete"};

static ui_popup pop;
static popup_guard guard;
static popup_focus focus;
static touch_state touch;
static int kind = -1, closing, signed_ok;
static u64 opened_at, closed_at;
static const char *answer = "none yet";

static rect target(int i)
{
    float w = (BOTTOM_W - 2 * PAD - GAP) / 2;
    return (rect){PAD + (i % 2) * (w + GAP), PAD + (i / 2) * (TILE_HIGH + GAP), w, TILE_HIGH};
}

static void press(int i)
{
    static const char *const plain[] = {"A popup with one button.", "A or B closes it, as a touch on its button does."};
    static const char *const rest[] = {"The path is cut at its beginning when the text would not fit otherwise."};
    static const char *const warning[] = {"A question with the warning sign above it.", "Cancel is on the left."};
    static const char *const del[] = {"This popup's right button deletes, so the look may show it in its own tone."};
    kind = i;
    closing = 0;
    if (i == PLAIN)
        ui_popup_text(&pop, plain, 2);
    else if (i == PATH)
        ui_popup_fit(&pop, "The folder %s is not there.", "sdmc:/gallery/a/folder/whose/path/is/long/pictures", rest,
                     2);
    else if (i == WARNING)
        signed_ok = ui_popup_text_signed(&pop, warning, 2);
    else
        ui_popup_text(&pop, del, 1);
    guard_arm(&guard, (hidKeysHeld() & KEY_TOUCH) != 0);  /* a stylus that is down takes nothing until lifted */
    focus_open(&focus);
    touch = (touch_state){.target = -1};
    forget_touch();             /* the touch under it: its lift presses nothing on the page */
    opened_at = frame_now;
    sfx(SFX_POPUP);
}

static int two(void)
{
    return kind != PLAIN;
}

static void close_with(int button)
{
    answer = button == 1 || !two() ? RIGHT[kind] : "Cancel";
    sfx(button == 1 && kind == DELETE ? SFX_DROP : button == 1 || !two() ? SFX_CONFIRM : SFX_BACK);
    closing = 1;
    closed_at = frame_now;
    guard_arm(&guard, (hidKeysHeld() & KEY_TOUCH) != 0);
    forget_touch();
    guard_from(&guard, closed_at);
}

static int button_at(int x, int y)
{
    if (!two())
        return ui_hit(ui_popup_button(0, 1), x, y) ? 1 : -1;
    return ui_hit(ui_popup_button(1, 0), x, y) ? 0 : ui_hit(ui_popup_button(1, 1), x, y) ? 1 : -1;
}

static int own(const input *in)
{
    if (in->up & KEY_TOUCH)
        guard_lift(&guard);
    if (kind < 0 || closing) {
        if (closing && in->now - closed_at >= POP_MS)
            kind = -1;
        return (in->down & KEY_TOUCH) && !guard_takes(&guard, in->now);   /* too soon after the close: nothing */
    }
    guard_from(&guard, opened_at);
    if (in->down & KEY_TOUCH) {
        focus.ring = 0;
        touch_down(&touch, guard_takes(&guard, in->now) ? button_at(in->x, in->y) : -1, 0, 0, in->x, in->y, in->now);
    } else if (in->held & KEY_TOUCH) {
        touch_move(&touch, in->x, in->y, touch.target >= 0 && button_at(in->x, in->y) == touch.target);
    } else if (in->up & KEY_TOUCH) {
        int b = touch_up(&touch, touch.target >= 0 && button_at(in->x, in->y) == touch.target);
        if (b >= 0)
            close_with(b);
    } else {
        int key = in->down & KEY_B ? POPUP_KEY_B : in->down & KEY_A ? POPUP_KEY_A : in->down & KEY_LEFT ? POPUP_KEY_LEFT
                : in->down & KEY_RIGHT ? POPUP_KEY_RIGHT : -1;
        int b = key < 0 ? -1 : popup_key(&focus, two(), key);
        if (key >= 0 && b < 0)
            sfx(SFX_MOVE);
        if (b >= 0)
            close_with(b);
    }
    return 1;
}

static void top(void)
{
    char foot[64];
    ui_head("Popup", kind >= 0 && !closing ? NAMES[kind] : NULL);
    card_text("A popup dims the screen behind it and grows out of its middle. It takes no touch in its first moments, "
              "and the page under it none just after it closed, so a double tap presses nothing by mistake.");
    snprintf(foot, sizeof foot, "Last answer: %s", answer);
    ui_foot(FOOT_DOTS_Y, -1, foot, MUTED, 0, NULL);
}

static void bottom(int held)
{
    for (int i = 0; i < KINDS; i++)
        ui_tile(FACE_WIDE, target(i), held == i && kind < 0 ? UI_DOWN : 0, SYMBOLS[i], NAMES[i], LABEL_PX, LABEL_LH,
                LABEL_GAP, TILE_INK);
    if (cursor_shown && kind < 0)
        ui_select(target(cursor), TILE_R);
    ui_bar(UI_KEY_B, "Back", 0);
    if (kind < 0)
        return;
    u64 now = frame_now;
    float opened = closing ? 1 - ease_span(now, closed_at, POP_MS) : ease_out(ease_span(now, opened_at, POP_MS));
    ui_popup_tone(kind == DELETE);
    ui_popup_sign(kind == WARNING && signed_ok ? ui_sign_once(ART_INFO_WARN) : -1);
    ui_popup_draw(&pop, two() ? "Cancel" : NULL, RIGHT[kind], closing ? -1 : touch_held(&touch, now),
                  two() && focus.ring ? focus.at : -1, opened);
}

static void no_bar(int held)
{
    (void)held;                 /* drawn under the popup by bottom */
}

static void no_select(int i)
{
    (void)i;                    /* drawn under the popup by bottom */
}

const page page_popups = {.name = "Popup", .symbol = ART_INFO_WARN, .count = KINDS, .columns = 2, .target = target,
                          .press = press, .top = top, .bottom = bottom, .own = own, .bar = no_bar,
                          .select = no_select};
