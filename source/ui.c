#include "ui.h"
#include "kit.h"
#include "look.h"
#include "widget.h"

#include "drift.h"
#include "ease.h"
#include "order.h"
#include "swarm.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <zlib.h>

#define TEXT_GLYPHS 8192        /* the frame's text buffer */
#define WHITE UI_COLOR(COL_WHITE)

static C3D_RenderTarget *top_target, *top_right, *bottom_target;
static C2D_TextBuf text_buf, measure_buf;
static C3D_Tex atlas, icon_tex, corner_tex, pattern_tex;
static C3D_Tex ui_camera_tex;   /* the camera picture (ui_camera) */
/* the application's part's pictures (ui_part_pictures); not static: a flows recorder knows it by its memory */
C3D_Tex ui_part_pictures_tex;
static C3D_Tex atlas_smooth;    /* the atlas's texels once more, read with smoothing: for a sprite in motion (step_mark) */
static int corners_ok, atlas_made, atlas_ok, icon_ok;
static int ui_camera_ok;
static icon_slots slots;       /* which of the icon texture's slots the next picture takes (swarm.h) */
/* the icon each slot holds converted, with the look's corner mask (ui_icon_put); a look swap empties it */
static uint8_t icon_held_bytes[ICON_SLOTS][48 * 48 * 2], icon_converted[ICON_SLOTS];
/* the glass sheet (ui_art.h): the chosen glass's sprites, whole pixels and smoothed as the atlas; made (0: no memory
 * for it), which glass is in it (-1 none yet), whether that one unpacked */
static C3D_Tex glass_tex, glass_smooth;
static int glass_made, glass_sheet_in = -1, glass_ok;
/* the application's part (ui_part), NULL until it is handed over; its own atlas and sheet, each with its smoothed
 * twin as the kit's, made by ui_init at the part's sizes where it brings sprites, unpacked and freed with the kit's */
static const art_part *app_part;
static const art_part_look *part_look(void);
static C3D_Tex part_atlas, part_atlas_smooth, part_sheet, part_sheet_smooth;
static int part_atlas_made, part_atlas_ok, part_sheet_made, part_sheet_ok;
/* its pictures' texture, made by ui_init at their size where it brings them; which scheme's are in it (-1 none yet)
 * and whether they unpacked */
static int pictures_made, pictures_in = -1, pictures_ok;
/* the colour scheme: which one is wanted (which one's atlas is in the texture: ui_f.scheme), and its colours */
static int scheme_want = ART_SCHEME_WEISS;
const uint32_t *ui_colours = ART_STOCK_COLOUR[ART_SCHEME_WEISS];
unsigned ui_top_signs;          /* the motifs of the signs the top screen draws this frame (ui.h) */
/* the look drawn (look.h; its number: ui_f.look), Stock until the first frame of another one told (ui_look) */
const look *ui_lk = &look_stock;
static int look_want = LOOK_STOCK;
/* the start screen's pattern: made (0: no memory for it), which one is wanted (the one in it: ui_f.pattern), and
 * whether that one unpacked */
static int pattern_made, pattern_want = PATTERN_HOUSES, pattern_good;
int ui_glass_want = GLASS_SHARD;
static int ground_flat;         /* ui_ground_flat was told this frame: ui_f.ground_flat on the next */
_Static_assert(GLASS_COUNT == ART_GLASSES && PATTERN_COUNT == ART_PATTERNS, "ui.h and art.py name the same choices");
_Static_assert(ART_PROGRESS_PLATIN == ART_PROGRESS + GLASS_PLATIN &&
               ART_PROGRESS_FILL_PLATIN == ART_PROGRESS_FILL + GLASS_PLATIN &&
               ART_CARD_GLASS_PLATIN == ART_CARD_GLASS + GLASS_PLATIN, "a bar's and a card's styles stand in a row");
ui_block ui_scratch;            /* for the cards' wrapped text; one frame draws one at a time */
static float enter = 1.0f;      /* how far the screen's transition is: 0 just changed, 1 done */
float ui_zoom = 1.0f;           /* the transition's scale this frame: the screen's content about its middle */
static u32 veil, fade_now;      /* ... and its fade: the background's colour and how far the content is mixed into
                                   it (citro2d's fade colour); the one set last */
static float black;             /* the start's black over both screens this frame (ui_black), 0 to 1 */
struct ui_frame ui_f = {.solid = 1.0f, .scheme = -1, .pattern = -1};

/* How close the frames come to the limits (a white flicker of text seen on the console, cause not
 * found by reading): citro2d 1.7.0 draws text without checking its vertex buffer (4 vertices for each of
 * C2D_DEFAULT_MAX_OBJECTS quads a frame, source/text.c), and libctru 2.7.0 drops GPU commands silently once the
 * command buffer (C3D_DEFAULT_CMDBUF_SIZE) is full (source/gpu/gpu.c). A few additions a frame; the application logs them
 * when a limit comes near and on leaving. */
static struct {
    unsigned frames, quads, quads_peak, glyphs_peak, failed, slow, switches;
    float cmd_peak;
    u64 slowest, last_begin;
    int was_3d, alarmed;
} stats;
static unsigned frame_quads;
static void wall_free(void);

#ifdef UI_PROBE
static void probe_read(void);
static u8 *probe_fb[2];         /* the framebuffers the last frame's transfer wrote: the bottom's, the top's left eye */
#endif

/* One draw for ui_stats: drawn 0, it failed. */
static void ui_count(bool drawn)
{
    frame_quads++;
    if (!drawn)
        stats.failed++;
}

/* The white flicker (order.h): every picture, that is every image from a texture other than the atlas, waits when it is
 * asked for (ui_picture, ui_icon_put) and is drawn, with the view it was asked under, before the first image that could
 * cover it or when the screen is done; the texts after it come first. So a tile's, a button's or a row's symbol from
 * the glass sheet is drawn after its label wherever it is drawn (a menu's letters flickered on the
 * console, the label drawn directly after the sheet's symbol), and so are a theme's icon, a pane, the selection's
 * corners and the house. Every image drawn at once says so before (ui_under, or ui_flush when its place is not known):
 * the atlas's pieces and the plain rectangles; the grounds over the whole screen (the pattern, the wallpaper, the
 * camera picture), which text is laid over, are drawn at once. Should text follow a picture directly all the same (text
 * on a ground, the next screen begins with text), one transparent piece of the atlas goes between (ui_text). */
#define ICON_REACH 4            /* more than the top screen's layers shift against each other (layout.h, DEPTH_*) */
static struct {
    int slot, tinted;
    C3D_Mtx view;
    C2D_ImageTint tint;         /* a carousel's pane: its veil */
    C3D_Tex *tex;               /* a picture (ui_picture): its texture, part and place; NULL: a theme's icon in slot */
    Tex3DS_SubTexture sub;
    C2D_DrawParams params;
} icon_held[ORDER_MAX];
static int ui_after_picture;    /* the last texture drawn from was not the kit's atlas: a picture's, the part's atlas */

static void icon_draw(void *ctx, int k, order_rect at)
{
    (void)ctx;
    float v0 = 64.0f * icon_held[k].slot;
    Tex3DS_SubTexture sub = {48, 48, 0.0f, 1.0f - v0 / icon_tex.height, 48.0f / 64.0f,
                             1.0f - (v0 + 48.0f) / icon_tex.height};
    C2D_DrawParams params = {{at.x, at.y, at.w, at.h}, {0, 0}, 0, 0};
    C2D_Image image = {&icon_tex, &sub};
    if (icon_held[k].tex)
        image = (C2D_Image){icon_held[k].tex, &icon_held[k].sub}, params = icon_held[k].params;
    C3D_Mtx now;
    C2D_ViewSave(&now);
    C2D_ViewRestore(&icon_held[k].view);
    ui_count(C2D_DrawImage(image, &params, icon_held[k].tinted ? &icon_held[k].tint : NULL));
    C2D_ViewRestore(&now);
    ui_after_picture = 1;
}

static order icons = {.draw = icon_draw};

void ui_flush(void)
{
    order_flush(&icons);
}

void ui_under(float x, float y, float w, float h)
{
    order_image(&icons, (order_rect){x, y, w, h}, ICON_REACH);
}

/* A picture: drawn from tex as params say, with tint (NULL: none), when order.h says; at covers what it draws on. */
void ui_picture(C3D_Tex *tex, const Tex3DS_SubTexture *sub, const C2D_DrawParams *params,
                const C2D_ImageTint *tint, order_rect at)
{
    int k = order_picture(&icons, at);
    icon_held[k].tex = tex;
    icon_held[k].sub = *sub;
    icon_held[k].params = *params;
    icon_held[k].tinted = tint != NULL;
    if (tint)
        icon_held[k].tint = *tint;
    C2D_ViewSave(&icon_held[k].view);
}

void ui_ground_image(C3D_Tex *tex, const Tex3DS_SubTexture *sub, const C2D_DrawParams *params,
                     const C2D_ImageTint *tint)
{
    if (ui_f.discard)
        return;
    ui_flush();                 /* a picture over the whole screen (order.h): the icons waiting come first ... */
    ui_count(C2D_DrawImage((C2D_Image){tex, sub}, params, tint));
    ui_after_picture = 1;       /* ... and letters after it get the atlas's transparent piece first */
}

void ui_rect(float x, float y, float w, float h, uint32_t c0, uint32_t c1, uint32_t c2, uint32_t c3)
{
    if (ui_f.discard)
        return;
    ui_under(x, y, w, h);
    ui_count(C2D_DrawRectangle(x, y, 0, w, h, c0, c1, c2, c3));
}

/* A texture of sprites (the kit's atlas and sheet, the part's): nearest texels, and a twin of the same memory read
 * with smoothing, never freed through that one; 0 for no memory or no size. */
static int sprite_tex_make(C3D_Tex *tex, C3D_Tex *smooth, int w, int h)
{
    if (!w || !h || !C3D_TexInit(tex, w, h, GPU_RGBA8))
        return 0;
    C3D_TexSetFilter(tex, GPU_NEAREST, GPU_NEAREST);
    *smooth = *tex;
    C3D_TexSetFilter(smooth, GPU_LINEAR, GPU_LINEAR);
    return 1;
}

int ui_init(void)
{
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    top_target = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    top_right = C2D_CreateScreenTarget(GFX_TOP, GFX_RIGHT);
    bottom_target = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    text_buf = C2D_TextBufNew(TEXT_GLYPHS);
    measure_buf = C2D_TextBufNew(512);
    icon_ok = C3D_TexInit(&icon_tex, 64, 64 * ICON_SLOTS, GPU_RGBA8);
    if (icon_ok)
        C3D_TexSetFilter(&icon_tex, GPU_LINEAR, GPU_LINEAR);
    /* nearest texels: sprites are drawn at whole pixels and stretch only where they are uniform; the first frame
     * unpacks the scheme's images into it (scheme_load) */
    atlas_made = sprite_tex_make(&atlas, &atlas_smooth, ART_ATLAS_W, ART_ATLAS_H);
    glass_made = sprite_tex_make(&glass_tex, &glass_smooth, ART_GLASS_SHEET_W, ART_GLASS_SHEET_H);
    /* the application's part's, when it brings sprites there (ui_part before ui_init) */
    part_atlas_made = app_part && sprite_tex_make(&part_atlas, &part_atlas_smooth, app_part->atlas_w, app_part->atlas_h);
    part_sheet_made = app_part && sprite_tex_make(&part_sheet, &part_sheet_smooth, app_part->sheet_w, app_part->sheet_h);
    /* the selection's corners: linear filtering, so a corner drawn at a fractional place moves by less than a pixel */
    if (C3D_TexInit(&corner_tex, ART_CORNERS_W, ART_CORNERS_H, GPU_RGBA8)) {
        C3D_TexSetFilter(&corner_tex, GPU_LINEAR, GPU_LINEAR);
        uLongf size = (uLongf)ART_CORNERS_W * ART_CORNERS_H * 4;
        corners_ok = uncompress(corner_tex.data, &size, ART_CORNERS_Z, ART_CORNERS_Z_SIZE) == Z_OK &&
                     size == (uLongf)ART_CORNERS_W * ART_CORNERS_H * 4;
        C3D_TexFlush(&corner_tex);
    }
    /* the part's pictures, when it brings them: whole pixels, as the atlas; the pattern: a fraction of a pixel
     * further each frame, and repeated by the texture's own wrap (citro2d passes a sub-texture's coordinates on as
     * they are) */
    pictures_made = app_part && app_part->pictures_w &&
                    C3D_TexInit(&ui_part_pictures_tex, app_part->pictures_w, app_part->pictures_h, GPU_RGBA8);
    if (pictures_made)
        C3D_TexSetFilter(&ui_part_pictures_tex, GPU_NEAREST, GPU_NEAREST);
    pattern_made = C3D_TexInit(&pattern_tex, PATTERN_TILE, PATTERN_TILE, GPU_RGBA8);
    if (pattern_made) {
        C3D_TexSetFilter(&pattern_tex, GPU_LINEAR, GPU_LINEAR);
        C3D_TexSetWrap(&pattern_tex, GPU_REPEAT, GPU_REPEAT);
    }
    ui_lk->draw.load();
    return atlas_made ? 0 : -1;
}

void ui_exit(void)
{
    ui_lk->draw.unload();
    if (pictures_made)
        C3D_TexDelete(&ui_part_pictures_tex);
    if (pattern_made)
        C3D_TexDelete(&pattern_tex);
    if (ui_camera_ok)
        C3D_TexDelete(&ui_camera_tex);
    if (icon_ok)
        C3D_TexDelete(&icon_tex);
    if (glass_made)
        C3D_TexDelete(&glass_tex);
    if (part_sheet_made)
        C3D_TexDelete(&part_sheet);
    if (part_atlas_made)
        C3D_TexDelete(&part_atlas);
    C3D_TexDelete(&atlas);
    C3D_TexDelete(&corner_tex);
    wall_free();
    C2D_TextBufDelete(measure_buf);
    C2D_TextBufDelete(text_buf);
    C2D_Fini();
    C3D_Fini();
    gfxExit();
}

int ui_unpack(C3D_Tex *tex, const uint8_t *z, uint32_t z_size)
{
    uLongf want = (uLongf)tex->width * tex->height * 4, size = want;
    int ok = uncompress(tex->data, &size, z, z_size) == Z_OK && size == want;
    C3D_TexFlush(tex);
    return ok;
}

/* One of the part's textures from the look's blob of the scheme, at the moment of the kit's own (scheme_load,
 * glass_load): 0 where it is not made or the part has no blob in the look drawn; one that does not unpack counts as
 * a failed draw, as the kit's. */
static int part_unpack(int made, C3D_Tex *tex, const uint8_t *const *z, const uint32_t *z_size, int scheme)
{
    if (!made || !z)
        return 0;
    int ok = ui_unpack(tex, z[scheme], z_size[scheme]);
    if (!ok)
        stats.failed++;
    return ok;
}

/* The wanted scheme's images and colours, when they are not the ones in use: the first frame, and a change in the
 * settings. Its atlas is unpacked over the one in the texture (no second texture, no buffer between: zlib writes
 * into the texture's memory), which takes the frame it happens in a moment longer; ui_begin calls this when the
 * last frame is drawn and nothing of the new one is, so no frame is drawn from half an atlas or with the colours
 * of one scheme on the images of another. The pattern's tile is the scheme's too and follows when the pattern is
 * drawn next (ui_start_textures). An atlas that does not unpack counts as a failed draw (ui_stats). */
static void scheme_load(void)
{
    if (ui_f.scheme == scheme_want)
        return;
    ui_f.scheme = scheme_want;
    ui_colours = ui_lk->art->colour[ui_f.scheme];
    ui_f.pattern = -1;
    atlas_ok = atlas_made && ui_unpack(&atlas, ui_lk->art->atlas_z[ui_f.scheme], ui_lk->art->atlas_z_size[ui_f.scheme]);
    if (!atlas_ok)
        stats.failed++;
    const art_part_look *p = part_look();
    part_atlas_ok = part_unpack(part_atlas_made, &part_atlas, p ? p->atlas_z : NULL, p ? p->atlas_z_size : NULL,
                                ui_f.scheme);
    ui_lk->draw.refresh(ui_f.scheme, ui_f.pattern);
}

/* The look told (ui_look), when it is not the one drawn: the old one puts its own textures away, the new one makes
 * its own, and every image of the look's art (atlas, sheet, pattern) and of the part's (atlas, sheet, pictures) is
 * unpacked anew by scheme_load, glass_load, ui_start_textures and ui_part_pictures, at the same moment of the frame as
 * scheme_load and for the same reason. */
static void look_swap(void)
{
    const look *want = look_get(look_want);
    if (want == ui_lk)
        return;
    ui_lk->draw.unload();
    ui_lk = want;
    ui_f.look = look_want;
    ui_lk->draw.load();
    ui_f.scheme = ui_f.pattern = glass_sheet_in = pictures_in = -1;
    memset(icon_converted, 0, sizeof icon_converted);      /* the icons' corners are the look's mask */
}

/* The scheme's sheet (the look's, one per scheme) and the part's, when they are not the ones in the textures: the
 * first frame and a change in the settings, at the same moment of the frame as scheme_load and for the same
 * reason. */
static void glass_load(void)
{
    if (glass_sheet_in == scheme_want)
        return;
    glass_sheet_in = scheme_want;
    if (glass_made) {
        glass_ok = ui_unpack(&glass_tex, ui_lk->art->sheet_z[glass_sheet_in], ui_lk->art->sheet_z_size[glass_sheet_in]);
        if (!glass_ok)
            stats.failed++;
    }
    const art_part_look *p = part_look();
    part_sheet_ok = part_unpack(part_sheet_made, &part_sheet, p ? p->sheet_z : NULL, p ? p->sheet_z_size : NULL,
                                glass_sheet_in);
}

uint32_t ui_mix(uint32_t a, uint32_t b, float t, float alpha)
{
    int r = (int)(((a >> 16) & 255) + ((int)((b >> 16) & 255) - (int)((a >> 16) & 255)) * t + 0.5f);
    int g = (int)(((a >> 8) & 255) + ((int)((b >> 8) & 255) - (int)((a >> 8) & 255)) * t + 0.5f);
    int bl = (int)((a & 255) + ((int)(b & 255) - (int)(a & 255)) * t + 0.5f);
    return C2D_Color32((u8)r, (u8)g, (u8)bl, (u8)(alpha * 255.0f + 0.5f));
}

/* citro2d mixes everything drawn from here on into color by its alpha (0: as it is). */
static void fade(u32 color)
{
    if (color != fade_now)
        C2D_Fade(color);
    fade_now = color;
}

/* The view of a screen's content: scaled by k about the point cx, cy (citro2d's view functions each multiply from
 * the right, so the last one named is applied to a point first), then moved shift px sideways. */
void ui_view(float k, float cx, float cy, float shift)
{
    C2D_ViewReset();
    if (k != 1.0f) {
        C2D_ViewTranslate(cx, cy);
        C2D_ViewScale(k, k);
        C2D_ViewTranslate(-cx, -cy);
    }
    if (shift)
        C2D_ViewTranslate(shift, 0);
    ui_f.shift = shift;
}

/* Where the next things are drawn on the top screen: depth px of parallax at the slider's top (layout.h, DEPTH_*),
 * half of it for each eye in opposite directions, inside the transition's zoom. In front of the screen, the left
 * eye sees a thing further right. */
void ui_layer(float depth)
{
    if (ui_f.top != 1)
        return;
    float shift = roundf(depth * ui_f.slider / 2);
    ui_view(ui_zoom, TOP_W / 2.0f, SCREEN_H / 2.0f, ui_f.eye ? -shift : shift);
}

/* A screen's content grows to its size about the screen's middle and comes out of the background while enter goes
 * from 0 to 1 (layout.h, TRANSITION_*), easing out: what is left of the way falls with the square of the time left.
 * The background itself stays as it is, and so does what stands on the screen's edge (ui_edge_begin). The first frame
 * already shows the content, TRANSITION_FADE_PCT % faded: a full cover drawn over the first frame of every
 * screen would leave an empty frame between two screens, and half of the fade would be over two frames later. */
static void scene(C3D_RenderTarget *target, int top)
{
    C2D_SceneBegin(target);
    ui_f.top = top;
    ui_f.shift = 0;
    float left = 1.0f - ease_out(enter);        /* a screen's content coming in (ease.h) */
    ui_zoom = 1.0f - (100 - TRANSITION_ZOOM_PCT) / 100.0f * left;
    veil = left > 0 ? ui_mix(top ? SC(COL_TOP_BG_A) : SC(COL_BOT_BG_A), top ? SC(COL_TOP_BG_B) : SC(COL_BOT_BG_B), 0.5f,
                          TRANSITION_FADE_PCT / 100.0f * left) : 0;
    C2D_ViewReset();
    fade(0);
    ui_lk->draw.ground(0, SCREEN_H, 1.0f);  /* the look's background; behind every top screen it moves (A1) */
    ui_view(ui_zoom, (top ? TOP_W : BOTTOM_W) / 2.0f, SCREEN_H / 2.0f, 0);
    fade(veil);
}

static void scene_done(void)
{
    ui_flush();
    C2D_ViewReset();
    ui_f.shift = 0;
    fade(0);
    if (black > 0)              /* last on every screen and eye, over a popup too, at the screen's plane; no texture */
        ui_count(C2D_DrawRectSolid(0, 0, 0, ui_f.top == 1 ? TOP_W : BOTTOM_W, SCREEN_H,
                                C2D_Color32(0, 0, 0, (u8)(black * 255.0f + 0.5f))));
}

void ui_black(float amount)
{
    black = amount;
}

void ui_ground_flat(void)
{
    ground_flat = 1;
}

/* The 3D slider (libctru: osGet3DSliderState, the shared configuration page) decides each frame whether the top
 * screen is drawn for both eyes: gfxSet3D switches the top screen's mode, and citro3d presents it in stereo only in
 * a frame that drew on the right eye's target. Without that target (it could not be made) the slider is ignored. */
void ui_begin(float entered)
{
    enter = entered;
    ui_f.ms = osGetTime();
    ui_f.moving_ms = MOVING_MS(ui_f.ms);
    ui_f.no++;
    ui_f.ground_flat = ground_flat;
    ground_flat = 0;
    ui_f.slider = top_right ? osGet3DSliderState() : 0;
    if (ui_f.slider < 0)
        ui_f.slider = 0;
    if (ui_f.slider > 1)
        ui_f.slider = 1;
    if (stats.last_begin && ui_f.ms - stats.last_begin > 20) {
        stats.slow++;
        if (ui_f.ms - stats.last_begin > stats.slowest)
            stats.slowest = ui_f.ms - stats.last_begin;
    }
    stats.last_begin = ui_f.ms;
    float usage = C3D_GetCmdBufUsage();     /* of the last frame's command list */
    if (usage > stats.cmd_peak)
        stats.cmd_peak = usage;
    if ((ui_f.slider > 0) != stats.was_3d && stats.frames)
        stats.switches++;
    stats.was_3d = ui_f.slider > 0;
    stats.frames++;
    frame_quads = 0;
    ui_f.eye = ui_f.discard = 0;
    black = 0;
    ui_top_signs = 0;
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);   /* the last frame is presented by now, in its own mode */
    look_swap();
    scheme_load();
    glass_load();
#ifdef UI_PROBE
    probe_read();
#endif
    gfxSet3D(ui_f.slider > 0);
    C2D_TextBufClear(text_buf);
    icon_slots_frame(&slots);
    icons.count = 0;
    C2D_TargetClear(top_target, UI_COLOR(SC(COL_TOP_BG_A)));
    if (ui_f.slider > 0)
        C2D_TargetClear(top_right, UI_COLOR(SC(COL_TOP_BG_A)));
    C2D_TargetClear(bottom_target, UI_COLOR(SC(COL_BOT_BG_A)));
    ui_f.top = -1;
}

void ui_top(void)
{
    icon_slots_top(&slots, ui_f.eye);
    scene(ui_f.eye ? top_right : top_target, 1);
}

void ui_bottom(void)
{
    if (ui_f.top == 1)
        scene_done();
    if (ui_f.eye) {
        ui_f.top = -1;
        ui_f.discard = 1;
        return;
    }
    scene(bottom_target, 0);
}

int ui_next_eye(void)
{
    if (ui_f.slider <= 0 || ui_f.eye)
        return 0;
    if (ui_f.top >= 0)
        scene_done();
    ui_f.top = -1;
    ui_f.eye = 1;
    return 1;
}

void ui_end(void)
{
    if (ui_f.top >= 0 && !ui_f.discard)
        scene_done();
    ui_f.discard = 0;
    ui_f.top = -1;
    unsigned glyphs = (unsigned)C2D_TextBufGetNumGlyphs(text_buf), quads = frame_quads + glyphs;
    if (quads > stats.quads_peak)
        stats.quads_peak = quads;
    if (glyphs > stats.glyphs_peak)
        stats.glyphs_peak = glyphs;
#ifdef UI_PROBE
    probe_fb[0] = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);     /* the ones C3D_FrameEnd transfers to */
    probe_fb[1] = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
#endif
    C3D_FrameEnd(0);
}

int ui_stats_alarm(void)
{
    if (stats.alarmed)
        return 0;
    stats.alarmed = stats.quads_peak * 10 > C2D_DEFAULT_MAX_OBJECTS * 9 || stats.glyphs_peak * 10 > TEXT_GLYPHS * 9 ||
                    stats.cmd_peak > 0.9f || stats.failed;
    return stats.alarmed;
}

void ui_stats(char *out, size_t size)
{
    snprintf(out, size, "%u frames; quads a frame at most %u of %u, glyphs %u of %u; command buffer at most %.0f %%; "
             "failed draws %u; frames over 20 ms %u (slowest %u ms); 2D/3D switches %u", stats.frames,
             stats.quads_peak, (unsigned)C2D_DEFAULT_MAX_OBJECTS, stats.glyphs_peak, (unsigned)TEXT_GLYPHS,
             (double)(stats.cmd_peak * 100), stats.failed, stats.slow, (unsigned)stats.slowest, stats.switches);
}

u32 ui_frame(void)
{
    return ui_f.no;
}


/* ---------------------------------------------------------------- text */

/* s's first n bytes in out (n + 1 bytes or more), each control character (U+0000 to U+001F, U+007F, U+0085, U+2028,
 * U+2029) as one space: a name reaches the screen as its file holds it, and citro2d starts a new line at "\n". */
const char *ui_plain(char *out, const char *s, size_t n)
{
    size_t k = 0;
    for (size_t i = 0; i < n;) {
        const unsigned char *c = (const unsigned char *)s + i;
        size_t len = c[0] < 0x20 || c[0] == 0x7F ? 1
                     : n - i >= 2 && c[0] == 0xC2 && c[1] == 0x85 ? 2
                     : n - i >= 3 && c[0] == 0xE2 && c[1] == 0x80 && (c[2] == 0xA8 || c[2] == 0xA9) ? 3 : 0;
        if (len)
            out[k++] = ' ', i += len;
        else
            out[k++] = s[i++];
    }
    out[k] = 0;
    return out;
}

static float measure(const char *s, size_t n, int px)
{
    char tmp[2 * UI_LINE_BYTES];
    if (n >= sizeof tmp)
        return 1e9f;            /* longer than any line can be: too wide, so it is broken or cut */
    C2D_Text t;
    C2D_TextBufClear(measure_buf);
    C2D_TextParse(&t, measure_buf, ui_plain(tmp, s, n));
    float w;
    C2D_TextGetDimensions(&t, ui_scale_of(px), ui_scale_of(px), &w, NULL);
    return w;
}

static float measured(const char *s, size_t n, int px, void *ctx)
{
    (void)ctx;
    return measure(s, n, px);
}

static void cut(char *out, size_t size, const char *s, float w, int px, int from_end)
{
    wrap_cut(out, size, s, w, px, from_end, measured, NULL);
}

float ui_baseline(float top, float lh, int px)
{
    return top + lh / 2 + FONT_CAP * ui_scale_of(px) / 2;
}

float ui_text_width(const char *s, int px)
{
    return measure(s, strlen(s), px);
}


void ui_text(const char *s, float x, float baseline, int px, uint32_t color, int align)
{
    if (ui_f.discard)
        return;
    if (ui_after_picture) {     /* never letters directly after a picture's texture (order.h) */
        C2D_ImageTint clear;
        C2D_AlphaImageTint(&clear, 0);
        ui_piece(&atlas, 0, 0, 8, 8, 0, 0, 8, 8, &clear);
    }
    if (ui_f.solid < 1.0f)
        color = (color & 0xFFFFFF) | (uint32_t)((color >> 24) * ui_f.solid + 0.5f) << 24;
    char tmp[2 * UI_LINE_BYTES];
    size_t n = strlen(s);
    C2D_Text t;
    C2D_TextParse(&t, text_buf, n < sizeof tmp ? ui_plain(tmp, s, n) : s);
    C2D_TextOptimize(&t);
    u32 flags = C2D_WithColor | C2D_AtBaseline |
                (align == UI_CENTER ? C2D_AlignCenter : align == UI_RIGHT ? C2D_AlignRight : C2D_AlignLeft);
    C2D_DrawText(&t, flags, x, baseline, 0, ui_scale_of(px), ui_scale_of(px), color);
}

void ui_text_fit(const char *s, float x, float baseline, float w, int px, uint32_t color, int align, int from_end)
{
    char out[UI_LINE_BYTES + 4];
    cut(out, sizeof out, s, w, px, from_end);
    ui_text(out, x, baseline, px, color, align);
}

int ui_wrap(ui_block *b, const char *s, float w, int px, int min_px, int max_lines, float max_h, float lh)
{
    return wrap_text(b, s, w, px, min_px, max_lines, max_h, lh, measured, NULL);
}

float ui_block_height(const ui_block *b)
{
    return b->count * b->lh;
}

void ui_block_draw(const ui_block *b, float x, float top, uint32_t color, int align)
{
    for (int i = 0; i < b->count; i++)
        ui_text(b->line[i], x, ui_baseline(top + i * b->lh, b->lh, b->px), b->px, color, align);
}

/* ---------------------------------------------------------------- images */

void ui_part(const art_part *part)
{
    app_part = part;
}

/* The application's part's tables in the look drawn, NULL for none. */
static const art_part_look *part_look(void)
{
    for (int k = 0; app_part && k < app_part->count; k++)
        if (app_part->looks[k].look == ui_lk->art)
            return &app_part->looks[k];
    return NULL;
}

/* A sprite's entry: the atlas's (ids below ART_COUNT), the glass sheet's (ART_COUNT up to ART_ALL) or the
 * application's part's (from ART_ALL on, its atlas's, then its sheet's); NULL for none. */
const art_sprite *ui_sprite_info(int id)
{
    if (id >= 0 && id < ART_COUNT)
        return &ui_lk->art->sprites[id];
    if (id >= ART_COUNT && id < ART_ALL)
        return &ui_lk->art->sheet_sprites[id - ART_COUNT];
    const art_part_look *p = id >= ART_ALL ? part_look() : NULL;
    if (p && id - ART_ALL < p->count)
        return &p->sprites[id - ART_ALL];
    if (p && id - ART_ALL - p->count < p->sheet_count)
        return &p->sheet_sprites[id - ART_ALL - p->count];
    return NULL;
}

/* The texture a sprite is read from, on whole pixels or smoothed; NULL while it holds nothing to draw: the kit's
 * atlas or sheet, or for the part's ids the part's own. */
C3D_Tex *ui_sprite_tex(int id, int smooth)
{
    const art_part_look *p = id >= ART_ALL ? part_look() : NULL;
    if (id >= 0 && id < ART_COUNT)
        return !atlas_ok ? NULL : smooth ? &atlas_smooth : &atlas;
    if (id >= ART_COUNT && id < ART_ALL)
        return !glass_ok ? NULL : smooth ? &glass_smooth : &glass_tex;
    if (p && id - ART_ALL < p->count)
        return !part_atlas_ok ? NULL : smooth ? &part_atlas_smooth : &part_atlas;
    if (p && id - ART_ALL - p->count < p->sheet_count)
        return !part_sheet_ok ? NULL : smooth ? &part_sheet_smooth : &part_sheet;
    return NULL;
}

/* An image's element size: without the shadow, ring or light it reaches past it with. */
float ui_ew(int id)
{
    const art_sprite *s = ui_sprite_info(id);
    return s ? s->w - s->pad[0] - s->pad[2] : 0;
}

float ui_eh(int id)
{
    const art_sprite *s = ui_sprite_info(id);
    return s ? s->h - s->pad[1] - s->pad[3] : 0;
}

void ui_piece(C3D_Tex *tex, float sx, float sy, float sw, float sh, float dx, float dy, float dw, float dh,
                  const C2D_ImageTint *tint)
{
    if (ui_f.discard || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0)
        return;
    /* the texture's first tile row is the top of the image, at v = 1 */
    float tw = tex->width, th = tex->height;
    Tex3DS_SubTexture sub = {(u16)(sw + 0.5f), (u16)(sh + 0.5f), sx / tw, 1.0f - sy / th, (sx + sw) / tw,
                             1.0f - (sy + sh) / th};
    C2D_DrawParams params = {{dx, dy, dw, dh}, {0, 0}, 0, 0};
    C2D_ImageTint faded;
    if (ui_f.solid < 1.0f && !tint) {
        C2D_AlphaImageTint(&faded, ui_f.solid);
        tint = &faded;
    }
    if (tex != &atlas && tex != &part_atlas) {   /* the glass sheets are a picture's texture for order.h, the
                                                 * part's atlas is drawn at once as the kit's */
        ui_picture(tex, &sub, &params, tint, (order_rect){dx, dy, dw, dh});
        return;
    }
    ui_under(dx, dy, dw, dh);
    ui_count(C2D_DrawImage((C2D_Image){tex, &sub}, &params, tint));
    ui_after_picture = tex != &atlas;   /* letters after the part's atlas get the kit's transparent piece first */
}

/* Source and destination spans along one axis: [src0, src1) to [dst0, dst1). */
static int spans(int size, int a, int b, float total, float out[3][4])
{
    if (!a && !b) {
        out[0][0] = 0, out[0][1] = (float)size, out[0][2] = 0, out[0][3] = (float)size;
        return 1;
    }
    float s[3][4] = {{0, (float)a, 0, (float)a},
                     {(float)a, (float)(size - b), (float)a, total - b},
                     {(float)(size - b), (float)size, total - b, total}};
    memcpy(out, s, sizeof s);
    return 3;
}

void ui_sprite_draw(int id, rect r, float from, float to, const C2D_ImageTint *tint)
{
    const art_sprite *s = ui_sprite_info(id);
    C3D_Tex *tex = ui_sprite_tex(id, 0);
    if (!s || !tex)
        return;
    float x = roundf(r.x) - s->pad[0], y = roundf(r.y) - s->pad[1];
    float w = s->cap[0] || s->cap[2] ? roundf(r.w) + s->pad[0] + s->pad[2] : s->w;
    float h = s->cap[1] || s->cap[3] ? roundf(r.h) + s->pad[1] + s->pad[3] : s->h;
    float cols[3][4], rows[3][4];
    int nc = spans(s->w, s->cap[0], s->cap[2], w, cols), nr = spans(s->h, s->cap[1], s->cap[3], h, rows);
    for (int i = 0; i < nc; i++) {
        float s0 = cols[i][0], s1 = cols[i][1], d0 = x + cols[i][2], d1 = x + cols[i][3];
        if (d1 <= d0)
            continue;
        float k = (s1 - s0) / (d1 - d0);
        if (d0 < from)
            s0 += (from - d0) * k, d0 = from;
        if (d1 > to)
            s1 -= (d1 - to) * k, d1 = to;
        for (int j = 0; j < nr; j++)
            ui_piece(tex, s->x + s0, s->y + rows[j][0], s1 - s0, rows[j][1] - rows[j][0], d0, y + rows[j][2], d1 - d0,
                  rows[j][3] - rows[j][2], tint);
    }
}

void ui_sprite(int id, float x, float y)
{
    if (!ui_sprite_info(id))
        return;
    ui_sprite_draw(id, (rect){x, y, ui_ew(id), ui_eh(id)}, -1e9f, 1e9f, NULL);
}

void ui_sprite_box(int id, rect r)
{
    ui_sprite_draw(id, r, -1e9f, 1e9f, NULL);
}

void ui_sprite_part(int id, rect r, float from, float to)
{
    ui_sprite_draw(id, r, from, to, NULL);
}

/* An SMDH icon is 48x48 RGB565 in the GPU's own order (8x8 tiles, Z order inside), six tiles to a row. It goes
 * into an RGBA8 slot (swarm.h, icon_slots), its corners rounded by the mask art.py drew, and waits there (order.h) to
 * be drawn into at, with tint when not NULL. */
void ui_icon_put(const uint8_t *smdh_icon, int slot, order_rect at, const C2D_ImageTint *tint)
{
    if (ui_f.discard || !smdh_icon || !icon_ok || slot < 0)
        return;
    /* a slot is converted and flushed only when its icon or the look changes (the slots go to the same pictures
     * every frame) */
    uint32_t *dst = (uint32_t *)icon_tex.data + slot * 64 * 64;
    if (!icon_converted[slot] || memcmp(icon_held_bytes[slot], smdh_icon, sizeof icon_held_bytes[slot])) {
        memcpy(icon_held_bytes[slot], smdh_icon, sizeof icon_held_bytes[slot]);
        icon_converted[slot] = 1;
        for (unsigned py = 0; py < ART_MASK_SIZE; py++) {
            for (unsigned px = 0; px < ART_MASK_SIZE; px++) {
                unsigned m = ui_morton(px & 7, py & 7);
                unsigned si = ((py >> 3) * 6 + (px >> 3)) * 64 + m, di = ((py >> 3) * 8 + (px >> 3)) * 64 + m;
                unsigned v = smdh_icon[2 * si] | (unsigned)smdh_icon[2 * si + 1] << 8;
                uint32_t r = ((v >> 11) & 31) * 255 / 31, g = ((v >> 5) & 63) * 255 / 63, b = (v & 31) * 255 / 31;
                dst[di] = r << 24 | g << 16 | b << 8 | ui_lk->art->mask[py * ART_MASK_SIZE + px];
            }
        }
        GSPGPU_FlushDataCache(dst, 64 * 64 * 4);      /* this slot only (C3D_TexFlush flushes all 256 KiB) */
    }
    int k = order_picture(&icons, at);
    icon_held[k].tex = NULL;
    icon_held[k].slot = slot;
    icon_held[k].tinted = tint != NULL;
    if (tint)
        icon_held[k].tint = *tint;
    C2D_ViewSave(&icon_held[k].view);
}

void ui_icon(const uint8_t *smdh_icon, float x, float y, float size)
{
    if (ui_f.discard || !smdh_icon || !icon_ok)
        return;
    ui_icon_put(smdh_icon, icon_slot_next(&slots), (order_rect){roundf(x), roundf(y), size, size}, NULL);
}

/* The frame goes into a 512x256 texture in the GPU's order: 8x8 tiles, rows of tiles from the top, and inside a
 * tile the pixels in Z order of their x and y bits. */
void ui_camera(const uint16_t *frame)
{
    static const Tex3DS_SubTexture sub = {400, 240, 0.0f, 1.0f, 400.0f / 512.0f, 1.0f - 240.0f / 256.0f};
    static u32 uploaded;        /* the frame it was put into the texture in: the right eye reuses it */
    if (ui_f.discard)
        return;
    ui_flush();
    ui_after_picture = 1;
    ui_layer(DEPTH_CAMERA);
    if (!frame) {
        C2D_DrawRectSolid(0, 0, 0, TOP_W, SCREEN_H, UI_COLOR(COL_CAMERA));
        return;
    }
    if (ui_camera_ok && uploaded == ui_f.no) {
        C2D_DrawImageAt((C2D_Image){&ui_camera_tex, &sub}, 0, 0, 0, NULL, 1.0f, 1.0f);
        return;
    }
    uploaded = ui_f.no;
    if (!ui_camera_ok) {
        if (!C3D_TexInit(&ui_camera_tex, 512, 256, GPU_RGB565))
            return;
        C3D_TexSetFilter(&ui_camera_tex, GPU_LINEAR, GPU_LINEAR);
        ui_camera_ok = 1;
    }
    uint16_t *dst = ui_camera_tex.data;
    for (unsigned y = 0; y < 240; y++) {
        const uint16_t *src = frame + y * 400;
        unsigned row = (y >> 3) * (512 >> 3);
        for (unsigned x = 0; x < 400; x++)
            dst[((row + (x >> 3)) << 6) | ui_morton(x & 7, y & 7)] = src[x];
    }
    C3D_TexFlush(&ui_camera_tex);
    C2D_DrawImageAt((C2D_Image){&ui_camera_tex, &sub}, 0, 0, 0, NULL, 1.0f, 1.0f);
}

/* The bar by glass style: the track and the fill are the style's (art.py liq_bars), Platin's
 * with a collar over each end. */
_Static_assert(ART_STRIPES_SHARD == ART_STRIPES + GLASS_SHARD && ART_STRIPES_PLATIN == ART_STRIPES + GLASS_PLATIN,
               "the stripes' styles stand in a row");

/* The fill from x to x + w (at least its two fixed ends), with its stripes moving to the right, cut by the fill's
 * outline (art.py stripe_ends): over the left end the frame of the stripes' phase, over the straight middle
 * the period repeated, over the right end the frame of the phase as it stands there. */
static void fill(rect r, float x, float w)
{
    const art_sprite *s = ui_sprite_info(ART_PROGRESS_FILL + ui_glass_want), *o = ui_sprite_info(ART_STRIPES + ui_glass_want);
    C3D_Tex *tex = ui_sprite_tex(ART_STRIPES + ui_glass_want, 0);
    int left = s->cap[0], right = s->cap[2];
    x = roundf(x);
    w = roundf(w) < left + right ? left + right : roundf(w);
    ui_sprite_box(ART_PROGRESS_FILL + ui_glass_want, (rect){x, r.y, w, r.h});
    if (!tex)
        return;
    int phase = (int)roundf((float)(ui_f.ms % STRIPE_MS) / STRIPE_MS * STRIPE_W) % STRIPE_W;
    int back = ((phase - ((int)w - right)) % STRIPE_W + STRIPE_W) % STRIPE_W;
    float mid = x + left, end = x + w - right;
    ui_piece(tex, o->x + phase * left, o->y, left, PROGRESS_H, x, r.y, left, r.h, NULL);
    for (float sx = x + phase - STRIPE_W; sx < end; sx += STRIPE_W) {
        float d0 = sx > mid ? sx : mid, d1 = sx + STRIPE_W < end ? sx + STRIPE_W : end;
        if (d1 > d0)
            ui_piece(tex, o->x + STRIPE_W * left + (d0 - sx), o->y, d1 - d0, PROGRESS_H, d0, r.y, d1 - d0, r.h, NULL);
    }
    ui_piece(tex, o->x + back * right, o->y + PROGRESS_H, right, PROGRESS_H, end, r.y, right, r.h, NULL);
}

void ui_progress(rect r, float fraction)
{
    ui_sprite_box(ART_PROGRESS + ui_glass_want, r);
    if (fraction < 0) {
        /* no size known: a quarter of the bar sliding to and fro */
        float w = roundf(r.w / 4), t = (float)(ui_f.ms % 1600) / 1600.0f;
        float k = 0.5f - 0.5f * cosf(TAU * t);
        fill(r, roundf(r.x + (r.w - w) * k), w);
    } else if (fraction > 0) {
        fill(r, r.x, roundf(r.w * (fraction > 1 ? 1 : fraction)));
    }
    if (ui_glass_want == GLASS_PLATIN) {
        ui_sprite(ART_PROGRESS_COLLAR, r.x - PROGRESS_COLLAR_OUT, r.y);
        ui_sprite(ART_PROGRESS_COLLAR, r.x + r.w - PROGRESS_COLLAR_IN, r.y);
    }
}

/* ---------------------------------------------------------------- top screen */

void ui_dots(float y)
{
    ui_lk->draw.rule(y);
}

rect ui_card_rect(void)
{
    return (rect){TOP_PAD, CARD_Y, TOP_W - 2 * TOP_PAD, CARD_H};
}

void ui_card(rect r)
{
    ui_lk->draw.card(r, -1);
}

void ui_card_glass(rect r)
{
    ui_lk->draw.card(r, ui_glass_want);
}


/* A symbol that stands in a line of text (art.py, glyph): its ink's bottom on the baseline, its top at the cap
 * height. */
void ui_inline_symbol(int id, float x, float baseline)
{
    ui_sprite(id, x, roundf(baseline) - ui_eh(id));
}

void ui_head(const char *left, const char *right)
{
    ui_dots(HEAD_DOTS_Y);       /* first: atlas images between a picture behind (the pattern) and the letters */
    ui_layer(DEPTH_HEADER);
    float b = ui_baseline(HEAD_Y, LINE_LH, LINE_PX), rw = right ? ui_text_width(right, LINE_PX) + 12 : 0;
    if (left)
        ui_text_fit(left, TOP_PAD, b, TOP_W - 2 * TOP_PAD - rw, LINE_PX, MUTED, UI_LEFT, 0);
    if (right)
        ui_text(right, TOP_W - TOP_PAD, b, LINE_PX, MUTED, UI_RIGHT);
}

void ui_foot(float dots_y, int symbol, const char *left, uint32_t left_color, int from_end, const char *right)
{
    ui_dots(dots_y);
    ui_layer(DEPTH_FOOTER);
    float b = ui_baseline(FOOT_Y, LINE_LH, LINE_PX), x = TOP_PAD;
    float rw = right ? ui_text_width(right, LINE_PX) + 12 : 0;
    if (symbol >= 0) {
        ui_inline_symbol(symbol, x, b);
        ui_top_signs |= ui_motif(symbol);
        x += ui_ew(symbol) + FOOT_ICON_GAP;
    }
    if (left)
        ui_text_fit(left, x, b, TOP_W - TOP_PAD - x - rw, LINE_PX, left_color, UI_LEFT, from_end);
    if (right)
        ui_text(right, TOP_W - TOP_PAD, b, LINE_PX, MUTED, UI_RIGHT);
}

void ui_foot_warn(const char *right)
{
    ui_layer(DEPTH_FOOTER);
    float b = ui_baseline(FOOT_Y, LINE_LH, LINE_PX), x = roundf(TOP_W - TOP_PAD - ui_text_width(right, LINE_PX));
    ui_text(right, x, b, LINE_PX, UI_COLOR(SC(COL_ALERT)), UI_LEFT);
    ui_inline_symbol(ART_WARN_FOOT, x - FOOT_WARN_GAP - ui_ew(ART_WARN_FOOT), b);
    ui_top_signs |= ui_motif(ART_WARN_FOOT);
}

void ui_look(int id, int scheme, int glass, int pattern)
{
    look_want = look_built(id) ? id : LOOK_STOCK;
    scheme_want = scheme >= 0 && scheme < ART_SCHEMES ? scheme : ART_SCHEME_WEISS;
    ui_glass_want = glass >= 0 && glass < GLASS_COUNT ? glass : GLASS_SHARD;
    pattern_want = pattern >= 0 && pattern < PATTERN_COUNT ? pattern : PATTERN_HOUSES;
}

C3D_Tex *ui_part_pictures(void)
{
    const art_part_look *p = part_look();
    if (!pictures_made || !p || !p->pictures_z)
        return NULL;
    if (pictures_in != scheme_want) {
        pictures_ok = ui_unpack(&ui_part_pictures_tex, p->pictures_z[scheme_want], p->pictures_z_size[scheme_want]);
        pictures_in = scheme_want;
    }
    return pictures_ok ? &ui_part_pictures_tex : NULL;
}

/* The application's part's pictures (ui_part_pictures) and the chosen pattern's tile in their textures: unpacked when
 * the choice is not the one in the texture (the first time the pattern or the start screen is drawn, and after a
 * change in the settings). */
void ui_start_textures(void)
{
    ui_part_pictures();
    if (pattern_made && ui_f.pattern != pattern_want) {
        pattern_good = ui_unpack(&pattern_tex, ui_lk->art->pattern_z[ui_f.scheme < 0 ? 0 : ui_f.scheme][pattern_want],
                              ui_lk->art->pattern_z_size[ui_f.scheme < 0 ? 0 : ui_f.scheme][pattern_want]);
        ui_f.pattern = pattern_want;
        ui_lk->draw.refresh(ui_f.scheme, ui_f.pattern);
    }
}

/* Where the pattern is at this moment, by the grounds' clock: the pixel at x, y of its quad shows the tile's x - *sx,
 * y - *sy; returned, how far its quad is moved sideways for this eye (behind the screen's plane in 3D, so on the top
 * screen only: the bottom screen has no depth). */
static float pattern_place(float *sx, float *sy)
{
    static const signed char move[PATTERN_COUNT][2] = {{PATTERN_PLUS_DX, PATTERN_PLUS_DY},
                                                       {PATTERN_HOUSES_DX, PATTERN_HOUSES_DY},
                                                       {PATTERN_BUBBLES_DX, PATTERN_BUBBLES_DY}};
    float drift = turn_drift(ui_f.moving_ms);
    *sx = move[ui_f.pattern][0] * drift, *sy = move[ui_f.pattern][1] * drift;
    float shift = ui_f.top == 1 ? roundf(DEPTH_PATTERN * ui_f.slider / 2) : 0;
    return ui_f.eye ? -shift : shift;
}

/* The pattern behind everything from y0 to y1, the screen's width: one quad whose texture repeats, read PATTERN_DRIFT
 * px a second further on along the pattern's direction, by one clock for every screen, so it moves on unbroken from
 * one screen to the next. It belongs to the background: not zoomed with a screen's content, a little behind the
 * screen's plane in 3D on the top screen, on the bottom screen in it. */
void ui_pattern(float y0, float y1, float alpha)
{
    ui_start_textures();
    if (ui_f.discard || !pattern_good)
        return;
    float sx, sy, shift = pattern_place(&sx, &sy), n = PATTERN_TILE, was = ui_f.shift;
    float x0 = -BLEED, x1 = TOP_W + BLEED;
    /* the pixel at x, y shows the tile's x - sx, y - sy; the texture's v is 1 at its first row; whole tiles more
     * (2 and 6) keep every coordinate above 0 and change nothing */
    Tex3DS_SubTexture sub = {(u16)(x1 - x0), (u16)(y1 - y0), 2 + (x0 - sx) / n, 6 - (y0 - sy) / n,
                             2 + (x1 - sx) / n, 6 - (y1 - sy) / n};
    C2D_DrawParams params = {{x0, y0, x1 - x0, y1 - y0}, {0, 0}, 0, 0};
    C3D_Mtx content;
    C2D_ViewSave(&content);
    C2D_ImageTint tint;
    C2D_AlphaImageTint(&tint, alpha);
    ui_view(1.0f, 0, 0, shift);
    ui_ground_image(&pattern_tex, &sub, &params, alpha < 1.0f ? &tint : NULL);
    C2D_ViewRestore(&content);
    ui_f.shift = was;
}

void ui_pattern_over(C3D_Tex *tex, float x, float y, float w, float h, float alpha)
{
    if (ui_f.discard || !pattern_good || ui_f.pattern < 0 || !tex || w <= 0 || h <= 0)
        return;
    float sx, sy, depth = pattern_place(&sx, &sy), n = PATTERN_TILE;
    sx += depth - ui_f.shift;   /* on the screen the pattern is moved by depth, what is drawn now by ui_f.shift */
    /* as ui_pattern's quad: whole tiles more (2 and 6) keep every coordinate above 0 */
    Tex3DS_SubTexture sub = {(u16)w, (u16)h, 2 + (x - sx) / n, 6 - (y - sy) / n, 2 + (x + w - sx) / n,
                             6 - (y + h - sy) / n};
    C2D_DrawParams params = {{x, y, w, h}, {0, 0}, 0, 0};
    float a = alpha * ui_f.solid;
    C2D_ImageTint tint;
    C2D_AlphaImageTint(&tint, a);
    ui_ground_image(tex, &sub, &params, a < 1.0f ? &tint : NULL);
}

/* A sprite whose element is drawn at x, y, kx and ky times its size, with tint (NULL: none): on whole pixels at its
 * own size and a whole place, smoothed otherwise. */
void ui_sprite_tinted(int id, float x, float y, float kx, float ky, const C2D_ImageTint *tint)
{
    const art_sprite *s = ui_sprite_info(id);
    int whole = kx == 1 && ky == 1 && x == roundf(x) && y == roundf(y);
    C3D_Tex *tex = ui_sprite_tex(id, !whole);
    if (ui_f.discard || !s || !tex)
        return;
    float dx = x - s->pad[0] * kx, dy = y - s->pad[1] * ky, dw = s->w * kx, dh = s->h * ky;
    float tw = tex->width, th = tex->height;
    Tex3DS_SubTexture sub = {s->w, s->h, s->x / tw, 1.0f - s->y / th, (s->x + s->w) / tw, 1.0f - (s->y + s->h) / th};
    C2D_DrawParams params = {{dx, dy, dw, dh}, {0, 0}, 0, 0};
    if (tex != &atlas && tex != &part_atlas) {
        ui_picture(tex, &sub, &params, tint, (order_rect){dx, dy, dw, dh});
        return;
    }
    ui_under(dx, dy, dw, dh);
    ui_count(C2D_DrawImage((C2D_Image){tex, &sub}, &params, tint));
    ui_after_picture = tex != &atlas;   /* letters after the part's atlas get the kit's transparent piece first */
}

/* A sprite about its middle at cx, cy, k times its size and alpha opaque: for a step's mark on its way from one
 * state to the next, and the start screen's word. Read with smoothing (its texture's smoothed view), so every size
 * and place between two pixels comes out round. For the order of pictures and text (order.h) this counts as a
 * texture of its own: a picture, it waits. */
void ui_sprite_scaled(int id, float cx, float cy, float k, float alpha)
{
    const art_sprite *s = ui_sprite_info(id);
    C3D_Tex *tex = ui_sprite_tex(id, 1);
    if (ui_f.discard || !s || !tex)
        return;
    /* the element's middle at cx, cy; what the image holds round it (a shadow) scaled with it */
    float w = s->w * k, h = s->h * k, x = cx - (s->pad[0] + ui_ew(id) / 2) * k,
          y = cy - (s->pad[1] + ui_eh(id) / 2) * k;
    float tw = tex->width, th = tex->height;
    Tex3DS_SubTexture sub = {s->w, s->h, s->x / tw, 1.0f - s->y / th, (s->x + s->w) / tw, 1.0f - (s->y + s->h) / th};
    C2D_DrawParams params = {{x, y, w, h}, {0, 0}, 0, 0};
    C2D_ImageTint tint;
    C2D_AlphaImageTint(&tint, alpha);
    ui_picture(tex, &sub, &params, &tint, (order_rect){x, y, w, h});
}

/* A sprite turned by angle about its image's middle at cx, cy, read smoothed and drawn as ui_sprite_scaled (a
 * picture, it waits). citro2d places the params' centre at pos and turns x towards y (C2Di_CalcQuad: x cos - y sin,
 * x sin + y cos, libcitro2d of devkitARM 20260610), so with y down a growing angle turns clockwise on the screen. */
void ui_sprite_turned(int id, float cx, float cy, float angle, const C2D_ImageTint *tint)
{
    const art_sprite *s = ui_sprite_info(id);
    C3D_Tex *tex = ui_sprite_tex(id, 1);
    if (ui_f.discard || !s || !tex)
        return;
    float tw = tex->width, th = tex->height;
    Tex3DS_SubTexture sub = {s->w, s->h, s->x / tw, 1.0f - s->y / th, (s->x + s->w) / tw, 1.0f - (s->y + s->h) / th};
    C2D_DrawParams params = {{cx, cy, s->w, s->h}, {s->w / 2.0f, s->h / 2.0f}, 0, angle};
    ui_picture(tex, &sub, &params, tint, (order_rect){cx - s->w / 2.0f, cy - s->h / 2.0f, s->w, s->h});
}

/* ---------------------------------------------------------------- bottom screen */

/* The corner image (art.py select_corners, in their order from ART_CORNER_R0_T3_TL_IN) for an item corner of
 * radius radius whose arms may run tail px past the curve, at position pos (top left, top right, bottom left,
 * bottom right), at rest or out. The shapes drawn: radius 0 with 3; a row's with 4 or 8; a tile's with 3 or 8. */
static int corner_id(int radius, float tail, int pos, int out)
{
    int shape = radius >= TILE_R ? (tail >= SELECT_TAIL_MAX ? 4 : 3) : radius >= ROW_R ? (tail >= SELECT_TAIL_MAX ? 2 : 1)
                                                                                         : 0;
    return ART_CORNER_R0_T3_TL_IN + (shape * 2 + out) * 4 + pos;
}

static void corner_draw(int id, float x, float y, const C2D_ImageTint *tint)
{
    const art_sprite *c = &ART_CORNER_SPRITES[id];
    Tex3DS_SubTexture sub = {c->w, c->h, (float)c->x / ART_CORNERS_W, 1.0f - (float)c->y / ART_CORNERS_H,
                             (float)(c->x + c->w) / ART_CORNERS_W, 1.0f - (float)(c->y + c->h) / ART_CORNERS_H};
    C2D_DrawParams params = {{x, y, c->w, c->h}, {0, 0}, 0, 0};
    C2D_ImageTint faded;
    if (ui_f.solid < 1.0f && !tint) {
        C2D_AlphaImageTint(&faded, ui_f.solid);
        tint = &faded;
    }
    ui_picture(&corner_tex, &sub, &params, tint, (order_rect){x, y, c->w, c->h});
}

/* The selection's four corners around r, each concentric with r's corner of that radius (top left, top right,
 * bottom left, bottom right), over what is drawn: out and back once per SELECT_MS (a sine), lighter outwards, at
 * fractional places (their texture is read with linear filtering, so the breathing glides). The arms' length comes
 * from r's shorter side (ui_layout.h SELECT_TAIL_*). first >= 0: a look's own corners in its atlas, in the corner
 * texture's order from that id. */
void ui_select_brackets(rect r, const int radius[4], int first)
{
    if (ui_f.discard || (first < 0 && !corners_ok))
        return;
    float t = (float)(ui_f.ms % SELECT_MS) / SELECT_MS, out = 0.5f - 0.5f * cosf(TAU * t);
    float d = SELECT_PAD + out * SELECT_AMP;
    float side = r.w < r.h ? r.w : r.h, most = 0;
    for (int k = 0; k < 4; k++)
        most = radius[k] > most ? radius[k] : most;
    float tail = side * SELECT_TAIL_PCT / 100.0f, room = side / 2 - most - SELECT_W / 2.0f - SELECT_GAP_MIN;
    tail = tail < room ? tail : room;
    tail = tail < SELECT_TAIL_MIN ? SELECT_TAIL_MIN : tail > SELECT_TAIL_MAX ? SELECT_TAIL_MAX : tail;
    /* the inward colour, and over it the outward one as far as the corners are out: the colours mix as the alpha
     * says (a colour tint's blend did not show in the emulator; the alpha tint did) */
    C2D_ImageTint tint, rest;
    C2D_AlphaImageTint(&tint, out * ui_f.solid);
    C2D_AlphaImageTint(&rest, ui_f.solid);
    for (int k = 0; k < 4; k++) {
        int id = corner_id(radius[k], tail, k, 0), lit = corner_id(radius[k], tail, k, 1);
        if (first >= 0) {
            id += first - ART_CORNER_R0_T3_TL_IN, lit += first - ART_CORNER_R0_T3_TL_IN;
            const art_sprite *s = ui_sprite_info(id);
            float n = s ? s->w : 0;
            float x = k & 1 ? r.x + r.w + d - n : r.x - d, y = k & 2 ? r.y + r.h + d - n : r.y - d;
            ui_sprite_tinted(id, x, y, 1, 1, ui_f.solid < 1.0f ? &rest : NULL);
            ui_sprite_tinted(lit, x, y, 1, 1, &tint);
            continue;
        }
        float n = ART_CORNER_SPRITES[id].w;
        float x = k & 1 ? r.x + r.w + d - n : r.x - d, y = k & 2 ? r.y + r.h + d - n : r.y - d;
        corner_draw(id, x, y, NULL);
        corner_draw(lit, x, y, &tint);
    }
}

void ui_select_corners(rect r, const int radius[4])
{
    ui_lk->draw.select(r, radius);
}

void ui_select(rect r, int radius)
{
    const int all[4] = {radius, radius, radius, radius};
    ui_select_corners(r, all);
}

static void face_tall(int id, rect r);

void ui_face_plain(int kind, rect r, int state)
{
    static const int faces[][3] = {     /* plain, held, greyed; the selection is drawn over the element after it */
        [FACE_TILE] = {ART_TILE, ART_TILE_DOWN, ART_TILE_OFF},
        [FACE_CHOICE] = {ART_CHOICE, ART_CHOICE_DOWN, ART_CHOICE},
        [FACE_ROW] = {ART_ROW, ART_ROW_DOWN, ART_ROW_OFF},
        [FACE_FOLDER] = {ART_FOLDER_ROW, ART_FOLDER_ROW_DOWN, ART_FOLDER_ROW},
        [FACE_GO] = {ART_GO, ART_GO_DOWN, ART_GO},
        [FACE_ALT] = {ART_ALT, ART_ALT_DOWN, ART_ALT},
        [FACE_DTILE] = {ART_DTILE, ART_DTILE_DOWN, ART_DTILE},
        [FACE_WIDE] = {ART_WIDE, ART_WIDE_DOWN, ART_WIDE},
    };
    if (kind == FACE_SET) {
        face_tall(state & UI_DOWN ? ART_SET_ROW_DOWN : ART_SET_ROW, r);
        return;
    }
    const int *f = faces[kind];
    ui_sprite_box(state & UI_DOWN ? f[1] : state & UI_OFF ? f[2] : f[0], r);
}

void ui_stack(rect r, int symbol, const char *label, int px, int lh, int gap, uint32_t ink)
{
    ui_wrap(&ui_scratch, label, r.w, px, px - 3, 2, 0, lh);
    float top = roundf(r.y + (r.h - (ui_eh(symbol) + gap + ui_block_height(&ui_scratch))) / 2);
    ui_sprite(symbol, roundf(r.x + (r.w - ui_ew(symbol)) / 2), top);
    ui_block_draw(&ui_scratch, r.x + r.w / 2, top + ui_eh(symbol) + gap, ink, UI_CENTER);
}

void ui_tile(int kind, rect r, int state, int symbol, const char *label, int px, int lh, int gap, uint32_t ink)
{
    r = ui_lk->draw.face(kind, r, state, symbol);
    int down = state & UI_DOWN ? 1 : 0;
    ui_stack((rect){r.x + TILE_PAD, r.y + down, r.w - 2 * TILE_PAD, r.h}, symbol, label, px, lh, gap,
             down ? PRESS_INK : ink);
}

void ui_wide(int kind, rect r, int state, int symbol, const char *label, int px, int gap, uint32_t ink)
{
    r = ui_lk->draw.face(kind, r, state, symbol);
    int down = state & UI_DOWN ? 1 : 0;
    char shown[UI_LINE_BYTES + 4];
    cut(shown, sizeof shown, label, r.w - 2 * TILE_PAD - ui_ew(symbol) - gap, px, 0);
    float x = roundf(r.x + (r.w - (ui_ew(symbol) + gap + ui_text_width(shown, px))) / 2);
    ui_sprite(symbol, x, roundf(r.y + (r.h - ui_eh(symbol)) / 2) + down);
    ui_text(shown, x + ui_ew(symbol) + gap, ui_baseline(r.y, r.h, px) + down, px, down ? PRESS_INK : ink, UI_LEFT);
}

void ui_facts(const ui_fact *rows, int count, float y)
{
    static char shown[UI_FACTS][UI_LINE_BYTES + 4];
    float most = BOTTOM_W - 2 * TILE_X1 - 2 * FACT_PAD_X - FACT_ICON - FACT_GAP, widest = 0;
    if (count > UI_FACTS)
        count = UI_FACTS;
    for (int k = 0; k < count; k++) {
        cut(shown[k], sizeof shown[k], rows[k].text, most, FACT_PX, rows[k].from_end);
        float w = ui_text_width(shown[k], FACT_PX);
        widest = w > widest ? w : widest;
    }
    float w = roundf(widest) + FACT_ICON + FACT_GAP + 2 * FACT_PAD_X, x = roundf((BOTTOM_W - w) / 2);
    ui_lk->draw.card((rect){x, y, w, 2 * FACT_PAD_Y + count * FACT_ROW_H}, -1);
    for (int k = 0; k < count; k++) {
        float top = y + FACT_PAD_Y + k * FACT_ROW_H;
        ui_text(shown[k], x + FACT_PAD_X + FACT_ICON + FACT_GAP, ui_baseline(top, FACT_ROW_H, FACT_PX), FACT_PX, INK,
                UI_LEFT);
        ui_sprite(rows[k].symbol, x + FACT_PAD_X, roundf(top + (FACT_ROW_H - FACT_ICON) / 2.0f));
    }
}

void ui_go(rect r, int state, int icon, const char *label)
{
    r = ui_lk->draw.face(FACE_GO, r, state, icon);
    float lead = GO_ICON + GO_GAP;
    float down = state & UI_DOWN ? 1 : 0, most = r.w - 2 * TILE_PAD - lead;
    char shown[UI_LINE_BYTES + 4];
    cut(shown, sizeof shown, label, most, GO_PX, 0);
    float x = roundf(r.x + (r.w - (lead + ui_text_width(shown, GO_PX))) / 2);
    ui_sprite(icon, x, roundf(r.y + (r.h - GO_ICON) / 2) + down);
    ui_text(shown, x + lead, ui_baseline(r.y, r.h, GO_PX) + down, GO_PX, WHITE, UI_LEFT);
}

void ui_alt(rect r, int state, int icon, const char *label)
{
    r = ui_lk->draw.face(FACE_ALT, r, state, icon);
    float down = state & UI_DOWN ? 1 : 0, most = r.w - 2 * TILE_PAD - ui_ew(icon) - GO_GAP;
    char shown[UI_LINE_BYTES + 4];
    cut(shown, sizeof shown, label, most, ALT_PX, 0);
    float x = roundf(r.x + (r.w - (ui_ew(icon) + GO_GAP + ui_text_width(shown, ALT_PX))) / 2);
    ui_sprite(icon, x, roundf(r.y + (r.h - ui_eh(icon)) / 2) + down);
    ui_text(shown, x + ui_ew(icon) + GO_GAP, ui_baseline(r.y, r.h, ALT_PX) + down, ALT_PX, down ? PRESS_INK : TILE_INK,
            UI_LEFT);
}

/* Engraved, as the System Settings' bar: a dark copy LABEL_SHADOW_DY px above, then the text. down: held, as measured
 * on the console's held bar: the copy white, the text in the held ink. */
void ui_engraved(const char *s, float x, float baseline, int px, int align, int down)
{
    uint32_t copy = down ? SC(COL_LABEL_HELD_SHADOW) : COL_LABEL_SHADOW;
    int alpha = down ? LABEL_HELD_SHADOW_ALPHA : LABEL_SHADOW_ALPHA;
    ui_text(s, x, baseline - LABEL_SHADOW_DY, px,
            C2D_Color32((copy >> 16) & 255, (copy >> 8) & 255, copy & 255, alpha * 255 / 100), align);
    ui_text(s, x, baseline, px, down ? PRESS_INK : UI_COLOR(COL_LABEL), align);
}

/* A key and its action, centred on cx. The A and B keys are our own engraved images (art.py, keycap), so the
 * label's copy above never doubles their thin ring. down: the bar or tab is held, as on the console: key and label
 * in the held ink, in their place, the copy above them white. */
static void key_label(const char *key, const char *label, float cx, int down, float most)
{
    int start = !strcmp(key, UI_KEY_START), a = !strcmp(key, UI_KEY_A);
    int id = down ? (a ? ART_KEY_A_DOWN : ART_KEY_B_DOWN) : (a ? ART_KEY_A : ART_KEY_B);
    if (!strcmp(key, UI_KEY_Y))
        id = down ? ART_KEY_Y_DOWN : ART_KEY_Y;
    float b = ui_baseline(BAR_Y, BAR_H, BAR_PX) + BAR_LABEL_DY;
    float kw = start ? roundf(ui_text_width(UI_KEY_START, START_PX) + 2 * START_PAD) : KEYCAP;
    if (kw + KEYCAP_GAP + ui_text_width(label, BAR_PX) > most) {     /* a tab too narrow for both: the label alone,
                                                                         as on the console's own tabs */
        ui_lk->draw.bar_label(label, roundf(cx - ui_text_width(label, BAR_PX) / 2), b, BAR_PX, UI_LEFT, down);
        return;
    }
    float x = roundf(cx - (kw + KEYCAP_GAP + ui_text_width(label, BAR_PX)) / 2);
    if (start) {
        rect pill = {x, KEYCAP_Y, kw, START_H};
        float kb = ui_baseline(KEYCAP_Y, START_H, START_PX);
        ui_sprite_box(down ? ART_START_DOWN_SHADOW : ART_START_SHADOW,
                      (rect){x, KEYCAP_Y - LABEL_SHADOW_DY, kw, START_H});
        ui_sprite_box(down ? ART_START_DOWN : ART_START, pill);
        ui_lk->draw.bar_label(UI_KEY_START, x + kw / 2, kb, START_PX, UI_CENTER, down);
    } else {
        ui_sprite(id, x, KEYCAP_Y);
    }
    ui_lk->draw.bar_label(label, x + kw + KEYCAP_GAP, b, BAR_PX, UI_LEFT, down);
}

/* What stands on the screen's edge (the bar, the tabs, the corner button) stays as it is while a screen's content
 * grows in: drawn between these two it is neither zoomed nor faded. A popup is drawn so too, in a view of its own. */
static float edge_shift;        /* ui_f.shift before ui_edge_begin (the edges are not nested) */
void ui_edge_begin(C3D_Mtx *content)
{
    C2D_ViewSave(content);
    C2D_ViewReset();
    edge_shift = ui_f.shift;
    ui_f.shift = 0;
    fade(0);
}

void ui_edge_end(const C3D_Mtx *content)
{
    C2D_ViewRestore(content);
    ui_f.shift = edge_shift;
    fade(veil);
}

void ui_bar_wide(const char *key, const char *label, int down)
{
    C3D_Mtx content;
    ui_edge_begin(&content);
    ui_lk->draw.bar(BAR_WIDE, down ? 2 : -1);
    key_label(key, label, BOTTOM_W / 2.0f, down ? 1 : 0, BOTTOM_W);
    ui_edge_end(&content);
}

void ui_bar(const char *key, const char *label, int down)
{
    C3D_Mtx content;
    ui_edge_begin(&content);
    ui_lk->draw.bar(BAR_LEFT, down ? 0 : -1);
    key_label(key, label, TAB_LABEL_X1, down ? 1 : 0, TAB_TEXT_W);
    ui_edge_end(&content);
}

void ui_bar_right(const char *key, const char *label, int down)
{
    C3D_Mtx content;
    ui_edge_begin(&content);
    ui_lk->draw.bar(BAR_RIGHT, down ? 1 : -1);
    key_label(key, label, TAB_LABEL_X2, down ? 1 : 0, TAB_TEXT_W);
    ui_edge_end(&content);
}

void ui_bar2(const char *left, const char *right, int down)
{
    C3D_Mtx content;
    ui_edge_begin(&content);
    ui_lk->draw.bar(BAR_LEFT | BAR_RIGHT, down);
    key_label(UI_KEY_B, left, TAB_LABEL_X1, down == 0, TAB_TEXT_W);
    key_label(UI_KEY_A, right, TAB_LABEL_X2, down == 1, TAB_TEXT_W);
    ui_edge_end(&content);
}

/* The corner button's width for path, and the path as it shows: measured once for each path, not every frame. */
static float corner_width(const char *path, const char **shown)
{
    static char key[768], cut_path[UI_LINE_BYTES + 4];
    static float width;
    if (strcmp(key, path)) {
        snprintf(key, sizeof key, "%s", path);
        cut(cut_path, sizeof cut_path, path, CORNER_MAX_W - CORNER_PAD - CORNER_ICON - CORNER_GAP - CORNER_PAD_R,
            CORNER_PX, 1);
        width = roundf(CORNER_PAD + CORNER_ICON + CORNER_GAP + ui_text_width(cut_path, CORNER_PX) + CORNER_PAD_R);
        if (width < CORNER_W)
            width = CORNER_W;
    }
    if (shown)
        *shown = cut_path;
    return width;
}

rect ui_corner_rect(const char *path)
{
    return (rect){0, 0, corner_width(path, NULL), CORNER_H};
}

void ui_corner(const char *path, int enabled, int down)
{
    const char *shown;
    float w = corner_width(path, &shown), d = enabled && down ? 1 : 0;
    C3D_Mtx content;
    ui_edge_begin(&content);
    const look_metrics *m = &ui_lk->m;
    ui_sprite_box(enabled ? (down ? ART_CORNER_DOWN : ART_CORNER) : ART_CORNER_OFF, (rect){0, 0, w, m->corner_h});
    ui_sprite(enabled ? ART_FOLDER_UP : ART_FOLDER_UP_OFF, m->corner_icon_x, m->corner_icon_y + d);
    ui_text(shown, m->corner_icon_x + CORNER_ICON + CORNER_GAP,
            ui_baseline(0, 2 * m->corner_icon_y + CORNER_ICON, CORNER_PX) + d, CORNER_PX,
            !enabled ? UI_COLOR(SC(COL_OFF_INK)) : down ? PRESS_INK : TILE_INK, UI_LEFT);
    ui_edge_end(&content);
}

void ui_help(int down)
{
    const look_metrics *m = &ui_lk->m;
    int x = HELP_X + (HELP_SIZE - m->help_size) / 2, y = HELP_Y + (HELP_SIZE - m->help_size) / 2;
    ui_sprite(down ? ART_HELP_DOWN : ART_HELP, x, y);
    ui_sprite(ART_HELP_MARK, x + (m->help_size - HELP_ICON) / 2, y + m->help_mark_y + (down ? m->help_mark_down : 0));
}

float ui_help_low(void)
{
    return HELP_Y + (HELP_SIZE + ui_lk->m.help_size) / 2;
}

int ui_popup_fit(ui_popup *p, const char *first, const char *path, const char *const rest[], int count)
{
    p->count = count < UI_POPUP_PARAS ? count : UI_POPUP_PARAS;
    return wrap_with_path(p->para, p->count, first, path, rest, DLG_TEXT_W, DLG_PX, DLG_MIN_PX,
                          DLG_H - DLG_BTN_H - 2 * DLG_PAD_Y, DLG_LH, DLG_PARA, measured, NULL);
}

int ui_popup_text(ui_popup *p, const char *const paras[], int count)
{
    p->count = count < UI_POPUP_PARAS ? count : UI_POPUP_PARAS;
    return wrap_paragraphs(p->para, p->count, paras, DLG_TEXT_W, DLG_PX, DLG_MIN_PX,
                           DLG_H - DLG_BTN_H - 2 * DLG_PAD_Y, DLG_LH, DLG_PARA, measured, NULL);
}

static int popup_removes;       /* the next popup's right button deletes (ui_popup_tone) */
static int popup_sign = -1;     /* the next popup's sign above its text (ui_popup_sign) */

void ui_popup_tone(int removes)
{
    popup_removes = removes;
}

void ui_popup_draw(const ui_popup *p, const char *left, const char *right, int held, int ring, float opened)
{
    if (ui_f.discard)
        return;
    /* its own view, whatever the screen under it does: the dim over the whole screen, then the panel and all on it
     * grown about the panel's middle and as solid as it has opened */
    /* changing in place, no speed at either end (ease.h): its largest step is 1.5 times the even one, so at 60
     * frames a second no frame of POPUP_IN_MS moves more than a seventh of the way */
    float open = ease_both(opened);
    C3D_Mtx screen_view;
    ui_flush();               /* the pictures under the dim first (order.h) */
    ui_edge_begin(&screen_view);
    ui_lk->draw.ground(BAR_Y - BAR_SHADOW, SCREEN_H, open);  /* the bar goes as the dim comes: none of it beside the panel */
    C2D_DrawRectSolid(-BLEED, 0, 0, BOTTOM_W + 2 * BLEED, SCREEN_H,
                      C2D_Color32((COL_DIM >> 16) & 255, (COL_DIM >> 8) & 255, COL_DIM & 255,
                                  (u8)(ui_lk->m.dim_pct * 255 / 100 * open + 0.5f)));
    ui_view((POPUP_ZOOM_PCT + (100 - POPUP_ZOOM_PCT) * open) / 100.0f, DLG_X + DLG_W / 2.0f,
            DLG_Y + ui_lk->m.dlg_h / 2.0f, 0);
    ui_f.solid = open;
    uint32_t text_ink = UI_COLOR(SC(COL_DLG_INK));
    ui_lk->draw.popup_panel((rect){DLG_X, DLG_Y, DLG_W, ui_lk->m.dlg_h});
    float k = p->count ? (float)p->para[0].px / DLG_PX : 1, h = 0;
    int shown = 0;
    for (int i = 0; i < p->count; i++)
        if (p->para[i].count)
            h += ui_block_height(&p->para[i]) + (shown++ ? DLG_PARA * k : 0);
    int sign = ui_sign_once(popup_sign);
    if (sign >= 0)
        h += INFO_ICON + INFO_POP_GAP;
    float top = roundf(DLG_Y + (DLG_H - DLG_BTN_H - h) / 2);    /* above the band (hits.c), whatever the panel's height */
    if (sign >= 0) {
        ui_sprite(sign, roundf((BOTTOM_W - INFO_ICON) / 2), top);
        top += INFO_ICON + INFO_POP_GAP;
    }
    for (int i = 0; i < p->count; i++) {
        if (!p->para[i].count)
            continue;
        ui_block_draw(&p->para[i], BOTTOM_W / 2.0f, top, text_ink, UI_CENTER);
        top += ui_block_height(&p->para[i]) + DLG_PARA * k;
    }
    ui_lk->draw.popup_buttons(left, right, held, ring, popup_removes);
    popup_removes = 0;
    popup_sign = -1;
    ui_f.solid = 1.0f;
    ui_edge_end(&screen_view);
}

void ui_row_folder(rect r, int state, const char *name)
{
    r = ui_lk->draw.face(FACE_FOLDER, r, state, -1);
    float down = state & UI_DOWN ? 1 : 0;
    ui_sprite(ART_FOLDER, r.x + ROW_PAD, r.y + (r.h - FOLDER_ICON) / 2 + down);
    ui_text_fit(name, r.x + FOLDER_TEXT_X, ui_baseline(r.y, r.h, ROW_NAME_PX) + down,
                r.w - FOLDER_TEXT_X - ROW_PAD, ROW_NAME_PX, down ? PRESS_INK : TILE_INK, UI_LEFT, 0);
}

void ui_scrollbar(rect r, float at, float size, int held)
{
    ui_sprite_box(ART_TRACK, r);
    if (size >= 1)
        size = 1;
    float h = roundf(r.h * size);
    if (h < THUMB_MIN)
        h = THUMB_MIN;
    if (h > r.h)
        h = r.h;
    float k = size < 1 ? at / (1 - size) : 0;
    if (k > 1)
        k = 1;
    ui_sprite_box(held ? ART_THUMB_DOWN : ART_THUMB, (rect){r.x, roundf(r.y + (r.h - h) * k), r.w, h});
}

/* ---------------------------------------------------------------- the loader */

void ui_loader(const char *line, const char *notice, float cover, float shown)
{
    if (ui_f.discard)
        return;
    cover = cover < 0 ? 0 : cover > 1 ? 1 : cover;
    shown = shown < 0 ? 0 : shown > 1 ? 1 : shown;
    if (cover > 0)
        ui_lk->draw.ground(0, SCREEN_H, cover); /* what the screen shows goes back into its background */
    if (shown <= 0)
        return;
    if (!line)
        notice = NULL;
    float ring = LOADER_BOX, h = ring + (line ? LOADER_GAP + LOADER_LH : 0) + (notice ? LOADER_LH : 0);
    float w = ui_f.top == 1 ? TOP_W : BOTTOM_W, cx = w / 2.0f, cy = roundf((SCREEN_H - h) / 2 + ring / 2);
    ui_layer(DEPTH_PROGRESS);   /* on the top screen: the ring stands as a running bar does, its line at 0 */
    ui_lk->draw.loader(cx, cy, ui_f.ms, shown);
    ui_layer(DEPTH_CARD_TEXT);
    if (!line)
        return;
    float base = ui_baseline(cy + ring / 2 + LOADER_GAP, LOADER_LH, LOADER_PX);
    u32 muted = C2D_Color32((SC(COL_MUTED) >> 16) & 255, (SC(COL_MUTED) >> 8) & 255, SC(COL_MUTED) & 255,
                            (u8)(255 * shown + 0.5f));
    ui_text_fit(line, cx, base, w - 2 * BODY_PAD, LOADER_PX, muted, UI_CENTER, 0);
    if (notice)
        ui_text_fit(notice, cx, base + LOADER_LH, w - 2 * BODY_PAD, LOADER_PX, muted, UI_CENTER, 0);
}

#ifdef UI_PROBE
/* ---------------------------------------------------------------- the flicker probe (ui.h; a UI_PROBE build) */

static int probe_on, probe_reads;
static uint8_t probe_bottom[2][UI_PROBE_TARGET_BYTES + UI_PROBE_FB_BYTES],
    probe_top[2][UI_PROBE_TOP_TARGET_BYTES + UI_PROBE_TOP_FB_BYTES];
/* Per screen of the console (0 the bottom, 1 the top): the last two frames read, each its render target and then
 * its framebuffer, and their CRCs. */
static struct {
    uint8_t *copy[2];
    size_t target_bytes, fb_bytes;
    uint32_t target_crc[2], fb_crc[2];
} probe_side[2] = {
    {{probe_bottom[0], probe_bottom[1]}, UI_PROBE_TARGET_BYTES, UI_PROBE_FB_BYTES, {0}, {0}},
    {{probe_top[0], probe_top[1]}, UI_PROBE_TOP_TARGET_BYTES, UI_PROBE_TOP_FB_BYTES, {0}, {0}},
};

/* After C3D_FrameBegin: the last frame is drawn and transferred, nothing writes these buffers until C3D_FrameEnd. */
static void probe_read(void)
{
    if (!probe_on || !probe_fb[0] || !probe_fb[1]) {
        probe_reads = 0;
        return;
    }
    int k = probe_reads++ & 1;
    C3D_RenderTarget *const target[2] = {bottom_target, top_target};
    for (int i = 0; i < 2; i++) {
        uint8_t *copy = probe_side[i].copy[k];
        size_t tb = probe_side[i].target_bytes, fb = probe_side[i].fb_bytes;
        memcpy(copy, target[i]->frameBuf.colorBuf, tb);
        GSPGPU_InvalidateDataCache(probe_fb[i], fb);
        memcpy(copy + tb, probe_fb[i], fb);
        probe_side[i].target_crc[k] = (uint32_t)crc32(0L, copy, tb);
        probe_side[i].fb_crc[k] = (uint32_t)crc32(0L, copy + tb, fb);
    }
}

void ui_probe_arm(int on)
{
    probe_on = on;
}

float ui_probe_slider(void)
{
    return ui_f.slider;
}

int ui_probe(int top, ui_probe_frame *now, ui_probe_frame *before)
{
    if (probe_reads < 2)
        return 0;
    int k = (probe_reads - 1) & 1, j = k ^ 1;
    const uint8_t *const *c = (const uint8_t *const *)probe_side[top].copy;
    size_t tb = probe_side[top].target_bytes, fb = probe_side[top].fb_bytes;
    *now = (ui_probe_frame){c[k], c[k] + tb, tb, fb, probe_side[top].target_crc[k], probe_side[top].fb_crc[k]};
    *before = (ui_probe_frame){c[j], c[j] + tb, tb, fb, probe_side[top].target_crc[j], probe_side[top].fb_crc[j]};
    return 1;
}
#endif
// ---------------------------------------------------------------- the preview

void ui_bar_y(const char *left, const char *right, int down)
{
    C3D_Mtx content;
    ui_edge_begin(&content);       /* as ui_bar2: neither zoomed nor faded with the screen's content */
    ui_lk->draw.bar(BAR_LEFT | BAR_RIGHT, down);
    key_label(UI_KEY_B, left, TAB_LABEL_X1, down == 0, TAB_TEXT_W);
    key_label(UI_KEY_Y, right, TAB_LABEL_X2, down == 1, TAB_TEXT_W);
    ui_edge_end(&content);
}

/* The wallpapers' textures, one per screen, 1024x256 RGB565, made when a preview first needs them and kept until
 * the application ends: a texture deleted right after its last frame could still be read by the GPU. */
#define WALL_W 1024
#define WALL_H 256
static C3D_Tex ui_wall_tex[2];
static int ui_wall_ok[2];

/* A body's texels are in the GPU's own order (8x8 tiles, row of tiles after row from the top, as ui_camera fills
 * its texture), so a row of tiles is copied as it is; a 512 wide wallpaper fills the left half of each row. */
int ui_wall_set(int bottom, const uint8_t *tiled, int width)
{
    C3D_Tex *t = &ui_wall_tex[bottom];
    if (width != 512 && width != WALL_W)
        return -1;
    if (!ui_wall_ok[bottom]) {
        if (!C3D_TexInit(t, WALL_W, WALL_H, GPU_RGB565))
            return -1;
        C3D_TexSetFilter(t, GPU_LINEAR, GPU_LINEAR);   /* a gliding wallpaper stands between two texels */
        ui_wall_ok[bottom] = 1;
    }
    size_t row = (size_t)(width / 8) * 64 * 2, pitch = (WALL_W / 8) * 64 * 2;
    for (int r = 0; r < WALL_H / 8; r++)
        memcpy((uint8_t *)t->data + r * pitch, tiled + r * row, row);
    C3D_TexFlush(t);
    return 0;
}

void ui_wall(int bottom, float x)
{
    if (ui_f.discard || !ui_wall_ok[bottom])
        return;
    float w = ui_f.top ? TOP_W : BOTTOM_W;
    Tex3DS_SubTexture sub = {(u16)w, SCREEN_H, x / WALL_W, 1.0f, (x + w) / WALL_W, 1.0f - (float)SCREEN_H / WALL_H};
    ui_flush();               /* a picture over the whole screen (order.h) */
    ui_count(C2D_DrawImageAt((C2D_Image){&ui_wall_tex[bottom], &sub}, 0, 0, 0, NULL, 1.0f, 1.0f));
    ui_after_picture = 1;       /* the footer's letters after it get the atlas's transparent piece first */
}

void ui_wall_color(const uint8_t upper[3], const uint8_t lower[3])
{
    if (ui_f.discard)
        return;
    u32 a = C2D_Color32(upper[0], upper[1], upper[2], 0xFF), b = C2D_Color32(lower[0], lower[1], lower[2], 0xFF);
    ui_flush();               /* it covers the whole screen: no icon waits to be drawn under it */
    ui_count(C2D_DrawRectangle(0, 0, 0, ui_f.top ? TOP_W : BOTTOM_W, SCREEN_H, a, a, b, b));
}

static void wall_free(void)
{
    for (int i = 0; i < 2; i++)
        if (ui_wall_ok[i])
            C3D_TexDelete(&ui_wall_tex[i]);
}

// ---------------------------------------------------------------- the settings

void ui_settings_button(int down)
{
    rect r = ui_settings_rect();
    C3D_Mtx content;
    ui_edge_begin(&content);       /* as ui_corner: it stands on the screen's edge */
    const look_metrics *m = &ui_lk->m;
    ui_sprite_box(down ? m->tab_face_down : m->tab_face, (rect){r.x, r.y, r.w, m->tab_h});
    ui_sprite(ART_SETTINGS, r.x + m->tab_icon_x, r.y + m->tab_icon_y + (down ? 1 : 0));
    ui_edge_end(&content);
}

/* A settings row's face r.h tall (layout.h SET_ROW_H) from its image (SET_H tall; Stock's 1 px less than its
 * lowest row's height, look_stock.py set_row_face_h), which stretches only across: its upper and lower halves as they
 * are, the row between them stretched. */
static void face_tall(int id, rect r)
{
    const art_sprite *s = ui_sprite_info(id);
    C3D_Tex *tex = ui_sprite_tex(id, 0);
    if (!s || !tex)
        return;
    if (roundf(r.h) <= ui_eh(id)) {
        ui_sprite_box(id, r);
        return;
    }
    int upper = s->pad[1] + (s->h - s->pad[1] - s->pad[3]) / 2;
    float x = roundf(r.x) - s->pad[0], y = roundf(r.y) - s->pad[1];
    float cols[3][4], rows[3][4];
    int nc = spans(s->w, s->cap[0], s->cap[2], roundf(r.w) + s->pad[0] + s->pad[2], cols);
    int nr = spans(s->h, upper, s->h - upper - 1, roundf(r.h) + s->pad[1] + s->pad[3], rows);
    for (int i = 0; i < nc; i++)
        for (int j = 0; j < nr; j++)
            ui_piece(tex, s->x + cols[i][0], s->y + rows[j][0], cols[i][1] - cols[i][0], rows[j][1] - rows[j][0],
                  x + cols[i][2], y + rows[j][2], cols[i][3] - cols[i][2], rows[j][3] - rows[j][2], NULL);
}

void ui_row_setting(rect r, int state, const char *label, const char *value, const char *note)
{
    r = ui_lk->draw.face(FACE_SET, r, state, -1);
    float down = state & UI_DOWN ? 1 : 0, x = r.x + ROW_PAD;
    float top = r.y + (r.h - (SET_NAME_LH + SET_SUB_GAP + SET_SUB_LH)) / 2 + down;
    float note_w = ui_text_width(note, SET_NOTE_PX), w = r.w - 2 * ROW_PAD - note_w - ROW_PAD;
    ui_text(note, r.x + r.w - ROW_PAD, ui_baseline(r.y, r.h, SET_NOTE_PX) + down, SET_NOTE_PX,
            down ? PRESS_INK : MUTED, UI_RIGHT);
    ui_text_fit(label, x, ui_baseline(top, SET_NAME_LH, ROW_NAME_PX), w, ROW_NAME_PX, down ? PRESS_INK : TILE_INK,
                UI_LEFT, 0);
    ui_text_fit(value, x, ui_baseline(top + SET_NAME_LH + SET_SUB_GAP, SET_SUB_LH, ROW_SUB_PX), w, ROW_SUB_PX,
                down ? PRESS_INK : MUTED, UI_LEFT, 0);
}

/* How many lines a column of choices takes in room px: more choices than it holds below each other stand in two
 * columns, the first half on the left. */
static int choice_rows(int count, float room)
{
    int cols = count * SET_LIST_LH > room ? 2 : 1;
    return (count + cols - 1) / cols;
}

/* The card and its choices in rows lines a column from top down; returns the y under the last line. */
static float choices_from(int count, int chosen, const char *(*name)(int choice), int rows, float top)
{
    rect c = ui_card_rect();
    ui_layer(DEPTH_CARD);
    ui_lk->draw.card(c, -1);
    ui_layer(DEPTH_CARD_TEXT);
    int cols = rows < count ? 2 : 1;
    float x = c.x + CARD_PAD, col_w = (c.w - 2 * CARD_PAD) / cols;
    for (int k = 0; k < count; k++) {
        float b = ui_baseline(top + k % rows * SET_LIST_LH, SET_LIST_LH, SET_LIST_PX);
        ui_text_fit(name(k), x + k / rows * col_w + SET_LIST_INDENT, b, col_w - SET_LIST_INDENT, SET_LIST_PX,
                    k == chosen ? INK : MUTED, UI_LEFT, 0);
    }
    /* the check after the texts, as every symbol beside text is drawn here */
    if (chosen >= 0 && chosen < count)
        ui_inline_symbol(ART_CHECK_ROW, x + chosen / rows * col_w,
                      ui_baseline(top + chosen % rows * SET_LIST_LH, SET_LIST_LH, SET_LIST_PX));
    return top + rows * SET_LIST_LH;
}

void ui_choices(int count, int chosen, const char *(*name)(int choice))
{
    rect c = ui_card_rect();
    int rows = choice_rows(count, c.h);
    choices_from(count, chosen, name, rows, roundf(c.y + (c.h - rows * SET_LIST_LH) / 2));
}

float ui_choices_top(int count, int chosen, const char *(*name)(int choice))
{
    rect c = ui_card_rect();
    /* CARD_PAD above the first line's box and at least as much below the last, as at the card's sides */
    return choices_from(count, chosen, name, choice_rows(count, c.h - 2 * CARD_PAD), c.y + CARD_PAD);
}

void ui_backdrop(void)
{
    /* every top screen shows it already (scene); gone with the join's cleanup (G1 plan) */
}

/* ---------------------------------------------------------------- problems, refusals and empty states */

static ui_block info_text;
static char info_path[UI_LINE_BYTES + 4];
static float info_w, info_pad_x, info_pad_y;
static int info_wide;           /* the wide box's pass (INFO_TIGHT_*) */

static float info_path_lh(void)
{
    return info_path[0] ? INFO_PATH_GAP + roundf(INFO_PATH_PX * INFO_LH_PCT / 100.0f) : 0;
}

/* One pass of the box's layout in room px (0: all the height above the bar, which the wide box may take whole): the
 * text and the path's line in the box's whole inner width, the width and the padding; 1 when the text fits. Passes
 * (INFO_LONG), each at the largest size that fits down to INFO_MIN_PX: 0 the box; 1 the wide box. */
static int info_pass(int wide, const char *text, const char *path, float room)
{
    float full = BAR_Y - BAR_SHADOW - 2 * INFO_AIR, tight = BAR_Y - BAR_SHADOW - 2 * INFO_TIGHT_AIR;
    info_wide = wide;
    info_w = wide ? BOTTOM_W - 2 * INFO_TIGHT_AIR : INFO_WIDE_W;
    info_pad_x = wide ? INFO_TIGHT_PAD : INFO_PAD_X;
    info_pad_y = wide ? INFO_TIGHT_PAD : INFO_PAD_Y;
    float most = (room > 0 ? room : wide ? tight : full) - 2 * info_pad_y, text_w = info_w - 2 * info_pad_x;
    info_path[0] = 0;
    if (path && path[0])
        cut(info_path, sizeof info_path, path, text_w, INFO_PATH_PX, 1);
    return ui_wrap(&info_text, text ? text : "", text_w, INFO_PX, INFO_MIN_PX, 0, most - info_path_lh(),
                   INFO_PX * INFO_LH_PCT / 100.0f);
}

/* The box laid out and its height: the first pass that fits, else the wide box. Every sentence stays: only the wide
 * box can cut, and an application tests that the texts it shows fit in it. */
static float info_lay(const char *text, const char *path, float room)
{
    if (!info_pass(0, text, path, room))
        info_pass(1, text, path, room);
    /* up: laid out again in this room, the box fits as it did */
    return ceilf(ui_block_height(&info_text) + info_path_lh() + 2 * info_pad_y);
}

float ui_info_height(const char *head, const char *text, const char *path, float room)
{
    (void)head;
    return info_lay(text, path, room);
}

/* The text and the path's line from top, at x as align says. */
static void info_text_draw(float x, float top, int align)
{
    ui_block_draw(&info_text, x, top, INK, align);
    if (info_path[0])
        ui_text(info_path, x, ui_baseline(top + ui_block_height(&info_text) + INFO_PATH_GAP,
                                          roundf(INFO_PATH_PX * INFO_LH_PCT / 100.0f), INFO_PATH_PX),
                INFO_PATH_PX, MUTED, align);
}

float ui_info_box(const char *head, const char *text, const char *path, float top, float bottom)
{
    (void)head;
    float low = BAR_Y - BAR_SHADOW - INFO_AIR;     /* the lowest the box reaches: INFO_AIR clear of the bar's shadow */
    int whole = bottom <= 0;
    if (whole)
        bottom = low;
    float room = whole ? 0 : (bottom < low ? bottom : low) - (top > INFO_AIR ? top : INFO_AIR);
    float h = info_lay(text, path, room);
    float air = info_wide && whole ? INFO_TIGHT_AIR : INFO_AIR;
    if (info_wide && whole)
        bottom = BAR_Y - BAR_SHADOW - INFO_TIGHT_AIR;
    float y = roundf(top + (bottom - top - h) / 2), x = roundf((BOTTOM_W - info_w) / 2);
    if (y + h > bottom)
        y = bottom - h;
    if (y < air)
        y = air;
    ui_lk->draw.card((rect){x, y, info_w, h}, -1);
    /* text only, from the inner left edge, the block centred in the box's height */
    float th = ui_block_height(&info_text) + info_path_lh();
    info_text_draw(x + info_pad_x, roundf(y + (h - th) / 2), UI_LEFT);
    return y;
}

void ui_popup_sign(int symbol)
{
    popup_sign = symbol;
}

int ui_popup_text_signed(ui_popup *p, const char *const paras[], int count)
{
    p->count = count < UI_POPUP_PARAS ? count : UI_POPUP_PARAS;
    if (wrap_paragraphs(p->para, p->count, paras, DLG_TEXT_W, DLG_PX, DLG_MIN_PX,
                        DLG_H - DLG_BTN_H - 2 * DLG_PAD_Y - INFO_ICON - INFO_POP_GAP, DLG_LH, DLG_PARA, measured, NULL))
        return 1;
    ui_popup_text(p, paras, count);     /* the sign never costs a sentence: the note goes without it */
    return 0;
}

#if defined UI_PROBE || defined UI_FIT_PROBE
/* ---------------------------------------------------------------- the fit probe (ui.h; the probe builds) */

static int probe_dots(const char *s)
{
    int n = 0;
    for (; s && (s = strstr(s, "...")); s += 3)
        n++;
    return n;
}

static int probe_cut(const ui_block *b, const char *src)
{
    int n = 0;
    for (int i = 0; i < b->count; i++)
        n += probe_dots(b->line[i]);
    return n > probe_dots(src);
}

void ui_info_probe(const char *head, const char *text, ui_fit *f)
{
    (void)head;
    info_lay(text, NULL, 0);
    f->layout = info_wide ? 2 : 1;
    f->px = info_text.px;
    f->lines = info_text.count;
    f->cut = probe_cut(&info_text, text);
}

void ui_note_probe(const char *head, const char *text, ui_fit *f)
{
    static ui_popup p;
    const char *paras[2] = {head, text};
    f->layout = ui_popup_text_signed(&p, paras, text && text[0] ? 2 : 1) ? 1 : 2;
    f->px = p.para[p.count - 1].px;
    f->lines = f->cut = 0;
    for (int k = 0; k < p.count; k++) {
        f->lines += p.para[k].count;
        f->cut |= probe_cut(&p.para[k], paras[k]);
    }
}
#endif
