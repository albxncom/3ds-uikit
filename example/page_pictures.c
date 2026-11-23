/* Pictures, every one made by the program itself (it reads no file): a wide picture on each screen (the wallpaper of
 * a theme as the preview shows it, gliding), a colour from top to bottom, a camera frame, theme icons, and sprites at
 * their own size, stretched and in part. The tiles choose what the top screen shows. */
#include "gallery.h"
#include "widget.h"

#define WALL_WIDTH 512
#define WALL_HIGH 256
#define ICON_PX 48
#define ICONS 4
#define CAM_W 400
#define CAM_H 240
#define TILES_Y 110
enum { WALLPAPER, COLOURS, CAMERA, MODES };
static const char *const NAMES[MODES] = {"Wallpaper", "Colours", "Camera"};

static uint8_t wall[WALL_WIDTH * WALL_HIGH * 2], icons[ICONS][ICON_PX * ICON_PX * 2];
static uint16_t camera[CAM_W * CAM_H];
static int mode, made;
static u32 camera_from;         /* the first frame (ui_frame) that shows the camera's picture */

static uint16_t rgb(unsigned r, unsigned g, unsigned b)
{
    return (uint16_t)(((r & 255) >> 3) << 11 | ((g & 255) >> 2) << 5 | (b & 255) >> 3);
}

/* a texel into a picture of 8x8 tiles, row by row, each in the GPU's Z order (a theme's body, an SMDH icon) */
static void put(uint8_t *tiled, unsigned width, unsigned x, unsigned y, uint16_t c)
{
    size_t at = ((size_t)(y / 8) * (width / 8) + x / 8) * 64 + ui_morton(x & 7, y & 7);
    tiled[2 * at] = c & 255;
    tiled[2 * at + 1] = c >> 8;
}

static void make(void)
{
    for (unsigned y = 0; y < WALL_HIGH; y++)
        for (unsigned x = 0; x < WALL_WIDTH; x++) {
            unsigned band = (x + y) / 24 % 2 ? 24 : 0;
            put(wall, WALL_WIDTH, x, y, rgb(90 + x / 4 + band, 150 + y / 4, 220 - x / 4 + band));
        }
    static const unsigned TONES[ICONS][3] = {{22, 135, 124}, {181, 90, 20}, {143, 108, 242}, {196, 40, 28}};
    for (int i = 0; i < ICONS; i++)
        for (unsigned y = 0; y < ICON_PX; y++)
            for (unsigned x = 0; x < ICON_PX; x++) {
                int dx = (int)x - 24, dy = (int)y - 24, in = dx * dx + dy * dy < 15 * 15;
                const unsigned *t = TONES[i];
                put(icons[i], ICON_PX, x, y, in ? rgb(t[0], t[1], t[2]) : rgb(240, 236, 220));
            }
    made = 1;
}

static void open(void)
{
    if (!made)
        make();
    ui_wall_set(0, wall, WALL_WIDTH);
    ui_wall_set(1, wall, WALL_WIDTH);
}

/* the camera's frame, made anew in each frame's input (both eyes draw the same): bars that move by the clock */
static void camera_frame(u64 now)
{
    unsigned shift = (unsigned)(now / 8 % 80);
    for (unsigned y = 0; y < CAM_H; y++)
        for (unsigned x = 0; x < CAM_W; x++) {
            unsigned v = (x + shift) / 40 % 2 ? 200 : 60;
            camera[y * CAM_W + x] = rgb(v, v - y / 4, 60 + y / 2);
        }
}

static int own(const input *in)
{
    if (mode == CAMERA)
        camera_frame(in->now);
    return 0;
}

static rect target(int i)
{
    float w = (BOTTOM_W - 2 * PAD - 2 * GAP) / 3;
    return (rect){PAD + i * (w + GAP), TILES_Y, w, WIDE_H};
}

static void press(int i)
{
    sfx(SFX_CONFIRM);
    mode = i;
    camera_from = ui_frame() + 2;   /* the frame drawn now (ui_begin counts it) shows the camera's place */
}

static void top(void)
{
    static const uint8_t UPPER[3] = {250, 220, 140}, LOWER[3] = {40, 120, 160};
    if (mode == WALLPAPER)
        ui_wall(0, (float)(frame_now / 40 % (WALL_WIDTH - TOP_W)));
    else if (mode == COLOURS)
        ui_wall_color(UPPER, LOWER);
    else
        ui_camera(ui_frame() >= camera_from ? camera : NULL);   /* its place until the first frame */
    ui_foot(FOOT_DOTS_Y, -1, NAMES[mode], MUTED, 0, NULL);
}

static void bottom(int held)
{
    if (mode == WALLPAPER)
        ui_wall(1, 0);
    for (int i = 0; i < ICONS; i++)
        ui_icon(icons[i], PAD + i * (ICON_PX + GAP), 8, ICON_PX);
    ui_sprite(ART_WARN, PAD + ICONS * (ICON_PX + GAP), 12);
    ui_sprite_box(ART_CARD, (rect){PAD, 64, 140, 36});
    ui_sprite_part(ART_CARD, (rect){PAD + 140 + GAP, 64, 150, 36}, PAD + 140 + GAP, PAD + 140 + GAP + 100);
    for (int i = 0; i < MODES; i++)
        ui_tile(FACE_WIDE, target(i), held == i ? UI_DOWN : 0, i == CAMERA ? ART_QR : ART_SD, NAMES[i], LABEL_PX,
                LABEL_LH, LABEL_GAP, TILE_INK);
}

const page page_pictures = {.name = "Pictures", .symbol = ART_FOLDER_UP, .count = MODES, .columns = 3,
                            .target = target, .press = press, .top = top, .bottom = bottom, .open = open,
                            .own = own};
