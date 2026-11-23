/* Scratch renderer: the recorder behind fake/citro2d.h (see there). Text is measured with the advance widths of the
 * font raster.py draws with (font_adv.h, made by fontgen.py), at the cap height ui_layout.h states for the console's
 * font, so lines break and are cut by the application's own code. */
#include <citro2d.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "font_adv.h"

enum { CMD_CLEAR, CMD_RECT, CMD_IMAGE, CMD_TEXT };
typedef struct {
    int kind, target, tex, align, linear, repeat, life, dead;
    float x, y, w, h, u0, v0, u1, v1, alpha, scale, blend[4], cx, cy, angle, z;
    u32 c[4], fade;
    char *text;
} cmd;
#define MAX_CMDS 20000
static cmd cmds[MAX_CMDS];
static int n_cmds, target_now;
static C3D_Mtx view_now = {1, 0, 0};
static u32 fade_now;
static C3D_Tex *texes[32];
static int n_texes;

/* The second form (REC_NAMED): every texture made, by the number of its making (a struct made twice is two), with
 * its pixels as they were at its last flush, and what happened to the textures since the last list was written. */
typedef struct { void *data; int w, h, fmt, alive; unsigned char *flushed; } life;
#define MAX_LIVES 256
static life lives[MAX_LIVES];
static int n_lives;
#define MAX_EVENTS 4096
static struct { u32 frame; int life; char what[64]; } events[MAX_EVENTS];   /* life: the texture's, -1 none */
static int n_events, events_lost;
static u32 frame_no;
static void event(int life, const char *fmt, ...)
{
    if (n_events == MAX_EVENTS) {
        events_lost++;
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    events[n_events].frame = frame_no, events[n_events].life = life;
    vsnprintf(events[n_events++].what, sizeof events[0].what, fmt, ap);
    va_end(ap);
}
static int life_of(const void *data)
{
    for (int i = n_lives - 1; i >= 0; i--)
        if (lives[i].data == data)
            return i;
    return -1;
}
static const char *fmt_name(int fmt) { return fmt == GPU_RGBA8 ? "rgba8" : fmt == GPU_RGB565 ? "rgb565" : "other"; }
/* the sprite (the names table) whose place in the texture holds the texels x0..x1, y0..y1 (from the top), or "-" */
const char *rec_sprite_name(const void *data, int w, int h, int x0, int y0, int x1, int y1);
const char *rec_texture_word(const void *data);
static C3D_RenderTarget targets[3] = {{0}, {1}, {2}};
struct rec_textbuf { char *mem; size_t used, cap, glyphs; };
void (*rec_frame_done)(void);

void gfxInitDefault(void) {} void gfxExit(void) {} void gfxSet3D(bool on) { (void)on; }
float rec_slider;              /* the 3D slider (sheet.c raises it) */
float osGet3DSliderState(void) { return rec_slider; }
Result GSPGPU_FlushDataCache(const void *adr, u32 size)
{
    const unsigned char *p = adr;
    for (int i = n_lives - 1; i >= 0; i--) {
        life *l = &lives[i];
        size_t all = (size_t)l->w * l->h * 4, off = (size_t)(p - (const unsigned char *)l->data);
        if (p < (const unsigned char *)l->data || off >= all)
            continue;
        size_t n = size < all - off ? size : all - off;
        memcpy(l->flushed + off, p, n);
        event(i, "cacheflush %d at %ld bytes %ld", i, (long)off, (long)size);
        return 0;
    }
    event(-1, "cacheflush %d at %ld bytes %ld", -1, 0, (long)size);
    return 0;
}
bool C3D_Init(size_t size) { (void)size; return true; } void C3D_Fini(void) {}
bool C2D_Init(size_t n) { (void)n; return true; } void C2D_Fini(void) {} void C2D_Prepare(void) {}
C3D_RenderTarget *C2D_CreateScreenTarget(gfxScreen_t screen, gfx3dSide_t side)
{ return &targets[screen == GFX_BOTTOM ? 2 : side == GFX_RIGHT ? 1 : 0]; }
bool C3D_TexInit(C3D_Tex *tex, u16 w, u16 h, int fmt)
{
    tex->data = calloc((size_t)w * h, 4);
    tex->width = w, tex->height = h, tex->fmt = fmt, tex->linear = tex->repeat = 0;
    tex->id = n_texes;
    if (n_texes == 32 || n_lives == MAX_LIVES) {
        fprintf(stderr, "rec: too many textures\n");
        exit(3);
    }
    texes[n_texes++] = tex;
    /* the memory is never freed (a deleted texture's calls stay readable): a new one never shares an old address */
    lives[n_lives] = (life){tex->data, w, h, fmt, 1, calloc((size_t)w * h, 4)};
    event(n_lives, "create %d %s", n_lives, fmt_name(fmt));
    n_lives++;
    return tex->data != NULL;
}
void C3D_TexDelete(C3D_Tex *tex)
{
    int i = life_of(tex->data);
    if (i >= 0)
        lives[i].alive = 0;
    event(i, "delete %d", i);
}
void C3D_TexSetFilter(C3D_Tex *tex, int mag, int min) { (void)min; tex->linear = mag == GPU_LINEAR; }
void C3D_TexSetWrap(C3D_Tex *tex, int s, int t) { (void)t; tex->repeat = s == GPU_REPEAT; }
void C3D_TexFlush(C3D_Tex *tex)
{
    int i = life_of(tex->data);
    if (i >= 0)
        memcpy(lives[i].flushed, tex->data, (size_t)lives[i].w * lives[i].h * 4);
    event(i, "flush %d", i);
}
bool C3D_FrameBegin(int flags)
{
    (void)flags;
    for (int i = 0; i < n_cmds; i++)
        free(cmds[i].text);
    n_cmds = 0;
    frame_no++;
    return true;
}
void C3D_FrameEnd(int flags) { (void)flags; if (rec_frame_done) rec_frame_done(); }
float C3D_GetCmdBufUsage(void) { return 0; }

static cmd *add(int kind)
{
    if (n_cmds == MAX_CMDS) {
        fprintf(stderr, "rec: too many draw calls\n");
        exit(3);
    }
    cmd *c = &cmds[n_cmds++];
    memset(c, 0, sizeof *c);
    c->kind = kind, c->target = target_now, c->fade = fade_now, c->alpha = 1;
    return c;
}
void C2D_TargetClear(C3D_RenderTarget *t, u32 color)
{
    int was = target_now;
    target_now = t->id;
    add(CMD_CLEAR)->c[0] = color;
    target_now = was;
}
void C2D_SceneBegin(C3D_RenderTarget *t) { target_now = t->id; }
void C2D_ViewReset(void) { view_now = (C3D_Mtx){1, 0, 0}; }
void C2D_ViewTranslate(float x, float y) { view_now.tx += view_now.k * x; view_now.ty += view_now.k * y; }
void C2D_ViewScale(float x, float y) { (void)y; view_now.k *= x; }
void C2D_ViewSave(C3D_Mtx *m) { *m = view_now; }
void C2D_ViewRestore(const C3D_Mtx *m) { view_now = *m; }
void C2D_Fade(u32 color) { fade_now = color; }
static void place(cmd *c, float x, float y, float w, float h)
{
    c->x = view_now.k * x + view_now.tx, c->y = view_now.k * y + view_now.ty;
    c->w = view_now.k * w, c->h = view_now.k * h;
}
bool C2D_DrawRectangle(float x, float y, float z, float w, float h, u32 c0, u32 c1, u32 c2, u32 c3)
{
    cmd *c = add(CMD_RECT);
    place(c, x, y, w, h);
    c->z = z;
    c->c[0] = c0, c->c[1] = c1, c->c[2] = c2, c->c[3] = c3;
    return true;
}
bool C2D_DrawRectSolid(float x, float y, float z, float w, float h, u32 col)
{ return C2D_DrawRectangle(x, y, z, w, h, col, col, col, col); }
bool C2D_DrawImage(C2D_Image img, const C2D_DrawParams *p, const C2D_ImageTint *tint)
{
    cmd *c = add(CMD_IMAGE);
    place(c, p->pos.x, p->pos.y, p->pos.w, p->pos.h);
    c->tex = img.tex->id;
    c->life = life_of(img.tex->data), c->z = p->depth;
    c->dead = c->life < 0 || !lives[c->life].alive;
    c->linear = img.tex->linear, c->repeat = img.tex->repeat;  /* this struct's: atlas_smooth shares atlas's memory */
    c->u0 = img.subtex->left, c->v0 = img.subtex->top, c->u1 = img.subtex->right, c->v1 = img.subtex->bottom;
    for (int k = 0; k < 4; k++) {   /* per corner: its colour (alpha: how solid) and how far towards it */
        c->c[k] = tint ? tint->corners[k].color : 0xFF000000u;
        c->blend[k] = tint ? tint->corners[k].blend : 0;
    }
    c->alpha = 1;
    c->cx = view_now.k * p->center.x, c->cy = view_now.k * p->center.y, c->angle = p->angle;
    return true;
}
bool C2D_DrawImageAt(C2D_Image img, float x, float y, float z, const C2D_ImageTint *tint, float sx, float sy)
{
    C2D_DrawParams p = {{x, y, img.subtex->width * sx, img.subtex->height * sy}, {0, 0}, z, 0};
    return C2D_DrawImage(img, &p, tint);
}
void C2D_SetImageTint(C2D_ImageTint *tint, C2D_Corner corner, u32 color, float blend)
{ tint->corners[corner].color = color; tint->corners[corner].blend = blend; }
void C2D_PlainImageTint(C2D_ImageTint *tint, u32 color, float blend)
{ for (int k = 0; k < 4; k++) C2D_SetImageTint(tint, (C2D_Corner)k, color, blend); }
void C2D_AlphaImageTint(C2D_ImageTint *tint, float alpha)
{ C2D_PlainImageTint(tint, C2D_Color32(0, 0, 0, (u8)(alpha * 255.0f + 0.5f)), 0); }

C2D_TextBuf C2D_TextBufNew(size_t glyphs)
{
    C2D_TextBuf b = calloc(1, sizeof *b);
    b->cap = glyphs * 8;
    b->mem = malloc(b->cap);
    return b;
}
void C2D_TextBufDelete(C2D_TextBuf b) { free(b->mem); free(b); }
void C2D_TextBufClear(C2D_TextBuf b) { b->used = b->glyphs = 0; }
size_t C2D_TextBufGetNumGlyphs(C2D_TextBuf b) { return b->glyphs; }
const char *C2D_TextParse(C2D_Text *t, C2D_TextBuf b, const char *s)
{
    size_t n = strlen(s) + 1;
    if (b->used + n > b->cap) {
        fprintf(stderr, "rec: text buffer full\n");
        exit(3);
    }
    memcpy(b->mem + b->used, s, n);
    t->s = b->mem + b->used;
    b->used += n;
    for (const char *p = s; *p; p++)
        b->glyphs += ((unsigned char)*p & 0xC0) != 0x80 && *p != ' ';
    return s + n - 1;
}
void C2D_TextOptimize(const C2D_Text *t) { (void)t; }
static float advance(const char *s)
{
    float w = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        u32 cp = *p;
        int more = cp >= 0xF0 ? 3 : cp >= 0xE0 ? 2 : cp >= 0xC0 ? 1 : 0;
        cp &= more == 3 ? 0x07 : more == 2 ? 0x0F : more == 1 ? 0x1F : 0x7F;
        p++;
        for (int i = 0; i < more && *p; i++)
            cp = cp << 6 | (*p++ & 0x3F);
        float a = FONT_ADV_OTHER;
        for (int i = 0; i < FONT_ADV_COUNT; i++)
            if (FONT_ADV_CP[i] == cp) {
                a = FONT_ADV[i];
                break;
            }
        w += a;
    }
    return w;
}
void C2D_TextGetDimensions(const C2D_Text *t, float sx, float sy, float *w, float *h)
{
    if (w)
        *w = advance(t->s) * sx;
    if (h)
        *h = 30 * sy;
}
void rec_text(const C2D_Text *t, u32 flags, float x, float y, float z, float sx, float sy, u32 color)
{
    (void)sy;
    if (!(flags & C2D_WithColor))
        color = 0xFF000000;
    cmd *c = add(CMD_TEXT);
    place(c, x, y, 0, 0);
    c->z = z;
    c->scale = sx * view_now.k;
    c->c[0] = color;
    c->align = flags & C2D_AlignCenter ? 1 : flags & C2D_AlignRight ? 2 : 0;
    c->text = strdup(t->s);
}

static void hex(FILE *f, u32 c) { fprintf(f, " %02x%02x%02x%02x", c & 255, (c >> 8) & 255, (c >> 16) & 255, c >> 24); }

/* the texels x0..x1, y0..y1 (from the top; outside the texture the edge's, as the GPU clamps) of memory laid out
 * as the texture l, in the GPU's order: a hash of their bytes row by row */
static unsigned long long texels(const life *l, const unsigned char *mem, int x0, int y0, int x1, int y1)
{
    unsigned long long h = 1469598103934665603ull;
    int bpp = l->fmt == GPU_RGB565 ? 2 : 4;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            int cx = x < 0 ? 0 : x >= l->w ? l->w - 1 : x, cy = y < 0 ? 0 : y >= l->h ? l->h - 1 : y;
            size_t k = (size_t)((cy >> 3) * (l->w >> 3) + (cx >> 3)) << 6 | (cx & 1) | (cy & 1) << 1 | (cx & 2) << 1 |
                       (cy & 2) << 2 | (cx & 4) << 2 | (cy & 4) << 3;
            for (int b = 0; b < bpp; b++)
                h = (h ^ mem[k * bpp + b]) * 1099511628211ull;
        }
    return h;
}
/* a texel count as printed: what float rounding leaves of a whole number is that number, and never "-0.000" */
static double tidy(double v) { return fabs(v - round(v)) < 5e-4 ? round(v) + 0.0 : v; }
/* Along one axis of an unturned call drawn over q..q+n on the screen from the texels s0..s1 (s1 < s0: mirrored),
 * which hold whole texels lo..hi: whether the linear filter reaches the texel before lo and the one at hi. Each
 * pixel whose centre lies in [q, q+n) samples the texel coordinate its centre maps to and mixes the two texels
 * round it; a texel past the edge counts where its weight is at least 1/256 (under that a neighbour moves no 8-bit
 * channel by a whole step). */
static void reach(double q, double n, double s0, double s1, int lo, int hi, int *before, int *after)
{
    *before = *after = 0;
    if (n < 0) {
        double t = s0;
        q += n, n = -n, s0 = s1, s1 = t;
    }
    double first = ceil(q - 0.5) + 0.5, last = ceil(q + n - 0.5) - 0.5;
    if (n <= 0 || last < first)
        return;
    double a = s0 + (first - q) * (s1 - s0) / n, b = s0 + (last - q) * (s1 - s0) / n;
    double smin = a < b ? a : b, smax = a < b ? b : a;
    *before = lo - (smin - 0.5) >= 1.0 / 256;
    *after = smax + 0.5 - hi >= 1.0 / 256;
}
/* An image call in the second form: in place of the texture's number and its coordinates, the sprite's name, the
 * texture's format, the region sampled (its size in texels, where in its first texel it starts, mirrored on x and
 * on y) and the hash of its texels, with the texels past its left, top, right and bottom edge the linear filter
 * reaches ("ring L T R B", reach); a repeating texture as it is (its size, its coordinates, the hash of all of
 * it). "unflushed": the texels differ from those of the texture's last flush; "deleted": drawn after its texture
 * was deleted. */
static void named_image(FILE *f, const cmd *c)
{
    if (c->life < 0) {
        fprintf(f, "I %d ? ?", c->target);
        return;
    }
    const life *l = &lives[c->life];
    double sx0 = (double)c->u0 * l->w, sx1 = (double)c->u1 * l->w, sy0 = (1 - (double)c->v0) * l->h,
           sy1 = (1 - (double)c->v1) * l->h;
    unsigned long long now, then;
    if (c->repeat) {
        now = texels(l, l->data, 0, 0, l->w, l->h), then = texels(l, l->flushed, 0, 0, l->w, l->h);
        fprintf(f, "I %d %s %s repeat %d %d %.6f %.6f %.6f %.6f %016llx", c->target,
                rec_sprite_name(l->data, l->w, l->h, 0, 0, l->w, l->h), fmt_name(l->fmt), l->w, l->h, c->u0, c->v0,
                c->u1, c->v1, now);
    } else {
        double lx = sx0 < sx1 ? sx0 : sx1, hx = sx0 < sx1 ? sx1 : sx0, ly = sy0 < sy1 ? sy0 : sy1,
               hy = sy0 < sy1 ? sy1 : sy0;
        int x0 = (int)floor(lx + 1e-3), x1 = (int)ceil(hx - 1e-3), y0 = (int)floor(ly + 1e-3), y1 = (int)ceil(hy - 1e-3);
        /* a quad of no area, or one whose tint makes every corner fully transparent, draws nothing: it samples no
         * texel (a name the look does not draw has an empty place at the atlas's corner, and ui_text's transparent
         * piece is the atlas's first 8 x 8 texels: whatever sprite stands there would otherwise name them) */
        int none = c->w == 0 || c->h == 0, clear = 1;
        for (int k = 0; k < 4; k++)
            clear &= c->c[k] >> 24 == 0;
        if (x1 <= x0)
            x1 = x0 + 1;
        if (y1 <= y0)
            y1 = y0 + 1;
        if (none || clear)
            x1 = x0, y1 = y0;
        int rl = 0, rt = 0, rr = 0, rb = 0;
        if (c->linear && !none && !clear && c->angle != 0)
            rl = rt = rr = rb = 1;      /* turned: every side, unweighed */
        else if (c->linear && !none && !clear) {
            reach(c->x - c->cx, c->w, sx0, sx1, x0, x1, &rl, &rr);
            reach(c->y - c->cy, c->h, sy0, sy1, y0, y1, &rt, &rb);
        }
        now = texels(l, l->data, x0 - rl, y0 - rt, x1 + rr, y1 + rb);
        then = texels(l, l->flushed, x0 - rl, y0 - rt, x1 + rr, y1 + rb);
        fprintf(f, "I %d %s %s region %.3f %.3f %.3f %.3f %d %d ring %d %d %d %d %016llx", c->target,
                none ? "-none" : clear ? "-clear" : rec_sprite_name(l->data, l->w, l->h, x0, y0, x1, y1),
                fmt_name(l->fmt),
                tidy(hx - lx), tidy(hy - ly), tidy(lx - x0), tidy(ly - y0), sx0 > sx1, sy0 > sy1, rl, rt, rr, rb, now);
    }
    fprintf(f, " %.3f %.3f %.3f %.3f %.4f", c->x, c->y, c->w, c->h, c->alpha);
    hex(f, c->fade);
    fprintf(f, " %d %d", c->linear, c->repeat);
    for (int k = 0; k < 4; k++) {
        hex(f, c->c[k]);
        fprintf(f, " %.4f", c->blend[k]);
    }
    fprintf(f, " %.3f %.3f %.6f z %.3f%s%s", c->cx, c->cy, c->angle, c->z, now != then ? " unflushed" : "",
            c->dead ? " deleted" : "");
}
/* REC_NAMED=DIR: the second form of the list into DIR/<the prefix's last part>.dl: the textures' events since the
 * last list (E: the frames before this one, from and to, how many, what; an event of the application's part's
 * atlas, sheet or pictures ends in its word, part-atlas, part-sheet or part-pictures, its create with the size before
 * it), then the calls, free of the atlas's places (named_image), each with its depth */
static void named_dump(const char *dir, const char *prefix)
{
    char path[1024];
    const char *base = strrchr(prefix, '/');
    snprintf(path, sizeof path, "%s/%s.dl", dir, base ? base + 1 : prefix);
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "rec: cannot write %s\n", path);
        exit(3);
    }
    for (int i = 0; i < n_events;) {
        int j = i + 1;
        while (j < n_events && !strcmp(events[j].what, events[i].what))
            j++;
        const life *l = events[i].life >= 0 ? &lives[events[i].life] : NULL;
        const char *word = l ? rec_texture_word(l->data) : NULL;
        fprintf(f, "E %u %u %d %s", frame_no - events[i].frame, frame_no - events[j - 1].frame, j - i, events[i].what);
        if (word && !strncmp(events[i].what, "create", 6))
            fprintf(f, " %dx%d", l->w, l->h);
        fprintf(f, "%s%s\n", word ? " " : "", word ? word : "");
        i = j;
    }
    if (events_lost)
        fprintf(f, "E lost %d\n", events_lost);
    n_events = events_lost = 0;
    for (int i = 0; i < n_cmds; i++) {
        const cmd *c = &cmds[i];
        switch (c->kind) {
        case CMD_CLEAR: fprintf(f, "C %d", c->target); hex(f, c->c[0]); break;
        case CMD_RECT:
            fprintf(f, "R %d %.3f %.3f %.3f %.3f", c->target, c->x, c->y, c->w, c->h);
            for (int k = 0; k < 4; k++) hex(f, c->c[k]);
            hex(f, c->fade);
            fprintf(f, " z %.3f", c->z);
            break;
        case CMD_IMAGE: named_image(f, c); break;
        case CMD_TEXT:
            fprintf(f, "T %d %.3f %.3f %.5f %d", c->target, c->x, c->y, c->scale, c->align);
            hex(f, c->c[0]); hex(f, c->fade);
            fprintf(f, " z %.3f %s", c->z, c->text);
            break;
        }
        fputc('\n', f);
    }
    fclose(f);
}

void rec_dump(const char *prefix)
{
    char path[1024];
    snprintf(path, sizeof path, "%s.dl", prefix);
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "rec: cannot write %s\n", path);
        exit(3);
    }
    /* REC_HASH=1 (g1 scratch): each texture's line ends with a hash of its pixels, and its pixels are written only
     * for captures whose name holds one of REC_TEX's comma-separated words */
    const char *hash = getenv("REC_HASH"), *keep = getenv("REC_TEX"), *base = strrchr(prefix, '/');
    int write_tex = !hash;
    for (const char *k = keep; hash && k && *k; k = strchr(k, ',') ? strchr(k, ',') + 1 : "") {
        char word[64];
        size_t n = strcspn(k, ",");
        snprintf(word, sizeof word, "%.*s", (int)(n < 63 ? n : 63), k);
        if (n && strstr(base ? base : prefix, word))
            write_tex = 1;
    }
    for (int i = 0; i < n_texes; i++) {
        C3D_Tex *t = texes[i];
        if (t->fmt != GPU_RGBA8)
            continue;
        if (hash) {
            unsigned long long h = 1469598103934665603ull;
            const unsigned char *d = t->data;
            for (size_t b = 0; b < (size_t)t->width * t->height * 4; b++)
                h = (h ^ d[b]) * 1099511628211ull;
            fprintf(f, "X %d %d %d %d %d %016llx\n", t->id, t->width, t->height, t->linear, t->repeat, h);
        } else
            fprintf(f, "X %d %d %d %d %d\n", t->id, t->width, t->height, t->linear, t->repeat);
        if (!write_tex)
            continue;
        snprintf(path, sizeof path, "%s.tex%d", prefix, t->id);
        FILE *g = fopen(path, "wb");
        fwrite(t->data, 4, (size_t)t->width * t->height, g);
        fclose(g);
    }
    for (int i = 0; i < n_cmds; i++) {
        cmd *c = &cmds[i];
        switch (c->kind) {
        case CMD_CLEAR: fprintf(f, "C %d", c->target); hex(f, c->c[0]); break;
        case CMD_RECT:
            fprintf(f, "R %d %.3f %.3f %.3f %.3f", c->target, c->x, c->y, c->w, c->h);
            for (int k = 0; k < 4; k++) hex(f, c->c[k]);
            hex(f, c->fade);
            break;
        case CMD_IMAGE:
            fprintf(f, "I %d %d %.3f %.3f %.3f %.3f %.6f %.6f %.6f %.6f %.4f", c->target, c->tex, c->x, c->y, c->w, c->h,
                    c->u0, c->v0, c->u1, c->v1, c->alpha);
            hex(f, c->fade);
            fprintf(f, " %d %d", c->linear, c->repeat);
            for (int k = 0; k < 4; k++) {
                hex(f, c->c[k]);
                fprintf(f, " %.4f", c->blend[k]);
            }
            fprintf(f, " %.3f %.3f %.6f", c->cx, c->cy, c->angle);
            break;
        case CMD_TEXT:
            fprintf(f, "T %d %.3f %.3f %.5f %d", c->target, c->x, c->y, c->scale, c->align);
            hex(f, c->c[0]); hex(f, c->fade);
            fprintf(f, " %s", c->text);
            break;
        }
        fputc('\n', f);
    }
    fclose(f);
    if (getenv("REC_NAMED"))
        named_dump(getenv("REC_NAMED"), prefix);
}
