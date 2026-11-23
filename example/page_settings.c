/* Settings, opened by the start page's settings button: a row each for the look (those this build holds), the colour
 * scheme, the glass style and the pattern, told to ui_look at once. A touch or A on a row shows its choices on the top
 * screen and takes the next one; left and right on the cursor's row take the one before or after. */
#include "gallery.h"
#include "widget.h"

#define ROW_TOP 6
#define ROW_HIGH 44

static const char *const LOOKS[LOOK_COUNT] = {[LOOK_STOCK] = "Stock", [LOOK_FROST] = "Frost", [LOOK_NIGHT] = "Night",
                                              [LOOK_PRISM] = "Prism"};
static const char *const SCHEMES[ART_SCHEMES] = {
    [ART_SCHEME_WEISS] = "White", [ART_SCHEME_GELB] = "Yellow", [ART_SCHEME_SAND] = "Sand",
    [ART_SCHEME_MINZE] = "Mint",  [ART_SCHEME_HIMMEL] = "Sky",  [ART_SCHEME_FLIEDER] = "Lilac",
    [ART_SCHEME_ROSE] = "Rose",   [ART_SCHEME_NACHT] = "Night"};
static const char *const GLASSES[GLASS_COUNT] = {[GLASS_CLEAR] = "Clear", [GLASS_SHARD] = "Shard",
                                                 [GLASS_PLATIN] = "Platinum"};
static const char *const PATTERNS[PATTERN_COUNT] = {[PATTERN_PLUS] = "Plus", [PATTERN_HOUSES] = "Houses",
                                                    [PATTERN_BUBBLES] = "Bubbles"};

int look_now = LOOK_STOCK, scheme_now = 0, glass_now = GLASS_SHARD, pattern_now = PATTERN_HOUSES;
static int looks[LOOK_COUNT], n_looks, row_shown;

enum { ROW_LOOK, ROW_SCHEME, ROW_GLASS, ROW_PATTERN, ROWS };
static const char *const LABELS[ROWS] = {"Look", "Colour scheme", "Glass", "Pattern"};

static int count(int row)
{
    return row == ROW_LOOK ? n_looks : row == ROW_SCHEME ? ART_SCHEMES : row == ROW_GLASS ? GLASS_COUNT : PATTERN_COUNT;
}

static int chosen(int row)
{
    if (row != ROW_LOOK)
        return row == ROW_SCHEME ? scheme_now : row == ROW_GLASS ? glass_now : pattern_now;
    for (int i = 0; i < n_looks; i++)
        if (looks[i] == look_now)
            return i;
    return 0;
}

static const char *choice(int row, int c)
{
    return row == ROW_LOOK ? LOOKS[looks[c]] : row == ROW_SCHEME ? SCHEMES[c] : row == ROW_GLASS ? GLASSES[c]
                                                                                                   : PATTERNS[c];
}

static const char *shown_choice(int c)
{
    return choice(row_shown, c);
}

static void open(void)
{
    n_looks = 0;
    for (int id = 0; id < LOOK_COUNT; id++)
        if (look_built(id))
            looks[n_looks++] = id;
}

/* the row's choice step places on (+1, -1), told to the drawing from the next frame */
static void step(int row, int by)
{
    int n = count(row), c = (chosen(row) + by + n) % n;
    if (row == ROW_LOOK)
        look_now = looks[c];
    else if (row == ROW_SCHEME)
        scheme_now = c;
    else if (row == ROW_GLASS)
        glass_now = c;
    else
        pattern_now = c;
    row_shown = row;
    ui_look(look_now, scheme_now, glass_now, pattern_now);
    sfx(SFX_MOVE);
}

static rect target(int i)
{
    return (rect){PAD, ROW_TOP + i * (ROW_HIGH + GAP), BOTTOM_W - 2 * PAD, ROW_HIGH};
}

static void press(int i)
{
    step(i, 1);
}

static int own(const input *in)
{
    if (!cursor_shown || !(in->down & (KEY_LEFT | KEY_RIGHT)))
        return 0;
    step(cursor, in->down & KEY_LEFT ? -1 : 1);
    return 1;
}

static void top(void)
{
    ui_backdrop();
    ui_head("Settings", LABELS[row_shown]);
    ui_choices(count(row_shown), chosen(row_shown), shown_choice);
    ui_foot(FOOT_DOTS_Y, -1, "A or a touch: the next choice", MUTED, 0, NULL);
}

static void bottom(int held)
{
    for (int i = 0; i < ROWS; i++)
        ui_row_setting(target(i), held == i ? UI_DOWN : 0, LABELS[i], choice(i, chosen(i)), of(chosen(i) + 1, count(i)));
}

static void select_one(int i)
{
    ui_select(target(i), ROW_R);
}

const page page_settings = {.name = "Settings", .symbol = ART_SETTINGS, .count = ROWS, .columns = 1, .target = target,
                            .press = press, .top = top, .bottom = bottom, .open = open, .own = own,
                            .select = select_one};
