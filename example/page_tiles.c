/* Tiles: a tile in each state (plain, which A or a touch makes amber and back; greyed out), a stack without a face,
 * and a tile that shows each kind of face in turn. Any tile shows held while the stylus is on it. A face is drawn at
 * its own height whatever the rectangle says, so the left column holds the row faces (ROW_H) and the stack, the right
 * one the changing face, as tall as its kind (at most TILE_H). */
#include "gallery.h"
#include "widget.h"

#define TOP_Y 12
#define STACK_H 64

static const char *const FACES[] = {[FACE_TILE] = "Tile face", [FACE_CHOICE] = "Choice face", [FACE_ROW] = "Row face",
                                    [FACE_FOLDER] = "Folder face", [FACE_GO] = "Go face", [FACE_ALT] = "Alt face",
                                    [FACE_DTILE] = "Design face", [FACE_WIDE] = "Wide face", [FACE_SET] = "Setting face"};
#define KINDS ((int)(sizeof FACES / sizeof *FACES))
static int amber, kind = FACE_WIDE;

/* each kind's own height (ui_layout.h); the setting row's face grows, it gets a row's largest */
static float face_high(int k)
{
    static const float HIGH[] = {[FACE_TILE] = TILE_H, [FACE_CHOICE] = CHOICE_H, [FACE_ROW] = ROW_H,
                                 [FACE_FOLDER] = FOLDER_H, [FACE_GO] = GO_H, [FACE_ALT] = GO_H,
                                 [FACE_DTILE] = DTILE_H, [FACE_WIDE] = WIDE_H, [FACE_SET] = SET_H_MAX};
    return HIGH[k];
}

/* 0 the plain tile (top left), 1 the changing face (right), 2 the greyed tile (under the plain one); the stack under
 * them is no target */
static rect target(int i)
{
    float w = (BOTTOM_W - 2 * PAD - GAP) / 2;
    if (i == 1)
        return (rect){PAD + w + GAP, TOP_Y, w, face_high(kind)};
    return (rect){PAD, TOP_Y + (i ? ROW_H + GAP : 0), w, ROW_H};
}

static void press(int i)
{
    if (i == 2) {
        sfx(SFX_ERROR);         /* greyed out: it does nothing */
        return;
    }
    sfx(SFX_CONFIRM);
    if (i == 0)
        amber = !amber;
    else
        kind = (kind + 1) % KINDS;
}

static void top(void)
{
    ui_head("Tiles", FACES[kind]);
    card_text("A tile shows held while the stylus is on it. The first is plain, and pressing it makes its light amber "
              "and back; the second is greyed out; under them a stack, a symbol and a label "
              "without a face. Pressing the right tile shows the next kind of face.");
    ui_foot(FOOT_DOTS_Y, -1, amber ? "Amber light" : "Plain light", MUTED, 0, of(kind + 1, KINDS));
}

static void bottom(int held)
{
    rect left = target(0);
    ui_wide(FACE_ROW, left, (held == 0 ? UI_DOWN : 0) | (amber ? UI_AMBER : 0), ART_SD, "Plain", LABEL_PX, LABEL_GAP,
            TILE_INK);
    ui_wide(FACE_ROW, target(2), UI_OFF | (held == 2 ? UI_DOWN : 0), ART_QR, "Greyed out", LABEL_PX, LABEL_GAP,
            UI_COLOR(SC(COL_OFF_INK)));
    ui_stack((rect){left.x, TOP_Y + 2 * (ROW_H + GAP), left.w, STACK_H}, ART_KEEP, "A stack without a face", LABEL_PX,
             LABEL_LH, LABEL_GAP, TILE_INK);
    ui_wide(kind, target(1), held == 1 ? UI_DOWN : 0, ART_DOWNLOAD, FACES[kind], LABEL_PX, LABEL_GAP, TILE_INK);
}

const page page_tiles = {.name = "Tiles", .symbol = ART_INSTALLED, .count = 3, .columns = 2, .target = target,
                         .press = press, .top = top, .bottom = bottom};
