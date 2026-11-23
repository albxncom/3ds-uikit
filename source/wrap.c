#include "wrap.h"

#include <stdio.h>
#include <string.h>

size_t wrap_unit(const char *str)
{
    const unsigned char *s = (const unsigned char *)str;
    unsigned c = s[0];
    if (!c)
        return 0;
    if (c < 0xC2)
        return 1;               /* ASCII, or a byte decode_utf8 refuses (a stray continuation byte, C0, C1) */
    if (c < 0xE0)
        return (s[1] & 0xC0) == 0x80 ? 2 : 1;
    if (c < 0xF0) {
        if ((s[1] & 0xC0) != 0x80 || (c == 0xE0 && s[1] < 0xA0))
            return 1;
        return (s[2] & 0xC0) == 0x80 ? 3 : 1;
    }
    if (c < 0xF5) {
        if ((s[1] & 0xC0) != 0x80 || (c == 0xF0 && s[1] < 0x90) || (c == 0xF4 && s[1] >= 0x90))
            return 1;
        if ((s[2] & 0xC0) != 0x80)
            return 1;
        return (s[3] & 0xC0) == 0x80 ? 4 : 1;
    }
    return 1;
}

size_t wrap_next(const char *s, size_t i)
{
    return i + wrap_unit(s + i);
}

/* The start of the character that ends at or before i (found from the start: a refused byte has no way back). */
size_t wrap_prev(const char *s, size_t i)
{
    size_t at = 0, j = 0;
    while (j < i && s[j]) {
        at = j;
        j = wrap_next(s, j);
    }
    return at;
}

void wrap_cut(char *out, size_t size, const char *s, float w, int px, int from_end, wrap_width width, void *ctx)
{
    size_t n = strlen(s);
    if (size < 4) {
        if (size)
            out[0] = 0;
        return;
    }
    if (n < size && width(s, n, px, ctx) <= w) {
        memcpy(out, s, n + 1);
        return;
    }
    if (from_end) {
        for (size_t start = wrap_next(s, 0); s[start]; start = wrap_next(s, start)) {
            if (n - start + 4 > size)
                continue;
            snprintf(out, size, "...%s", s + start);
            if (width(out, strlen(out), px, ctx) <= w)
                return;
        }
        snprintf(out, size, "...");
        return;
    }
    size_t best = 0;
    for (size_t j = wrap_next(s, 0); j <= n && j + 4 <= size; j = wrap_next(s, j)) {
        memcpy(out, s, j);
        memcpy(out + j, "...", 4);
        if (width(out, j + 3, px, ctx) > w)
            break;
        best = j;
        if (!s[j])
            break;
    }
    memcpy(out, s, best);
    memcpy(out + best, "...", 4);
}

/* Fills b with the lines of s at px; 1 when all of s fit in max lines. Each line is put together from its words
 * with one space between them: a run of spaces, and spaces at a line's start, are drawn as they are measured,
 * collapsed. "\n" ends a line. */
static int wrap_lines(ui_block *b, const char *s, float w, int px, int max, wrap_width width, void *ctx)
{
    float space = width(" ", 1, px, ctx);
    const char *p = s;
    b->count = 0;
    while (*p == ' ')
        p++;
    while (*p) {
        if (b->count == max)
            return 0;
        char *line = b->line[b->count++];
        size_t n = 0;
        float used = 0;
        const char *q = p;
        while (*q && *q != '\n') {
            const char *word = q;
            while (*q && *q != ' ' && *q != '\n')
                q += wrap_unit(q);
            size_t len = (size_t)(q - word);
            float ww = width(word, len, px, ctx);
            if (!n && (ww > w || len >= UI_LINE_BYTES)) {
                /* a word wider than the line or longer than its buffer: as many of its characters as fit */
                size_t fit = 0;
                for (size_t i = 0; i < len;) {
                    size_t nx = i + wrap_unit(word + i);
                    if (nx >= UI_LINE_BYTES || width(word, nx, px, ctx) > w)
                        break;
                    fit = i = nx;
                }
                n = fit ? fit : wrap_unit(word);
                memcpy(line, word, n);
                q = word + n;
                break;
            }
            float add = (n ? space : 0) + ww;
            if (n && (used + add > w || n + 1 + len >= UI_LINE_BYTES)) {
                q = word;
                break;
            }
            if (n)
                line[n++] = ' ';
            memcpy(line + n, word, len);
            n += len;
            used += add;
            while (*q == ' ')
                q++;
        }
        line[n] = 0;
        p = q;
        while (*p == ' ')
            p++;
        if (*p == '\n')
            for (p++; *p == ' '; p++)
                ;
    }
    return 1;
}

int wrap_text(ui_block *b, const char *s, float w, int px, int min_px, int max_lines, float max_h, float lh,
              wrap_width width, void *ctx)
{
    if (min_px > px)
        min_px = px;
    for (int at = px;; at--) {
        float l = lh * (float)at / (float)px;
        int allowed = UI_LINES;
        if (max_lines > 0 && max_lines < allowed)
            allowed = max_lines;
        if (max_h > 0 && (int)(max_h / l) < allowed)
            allowed = (int)(max_h / l);
        if (allowed < 1)
            allowed = 1;
        b->px = at;
        b->lh = l;
        int all = wrap_lines(b, s, w, at, allowed, width, ctx);
        if (all || at <= min_px) {
            if (!all && b->count) {
                char last[UI_LINE_BYTES + 4];
                snprintf(last, sizeof last, "%s ...", b->line[b->count - 1]);
                wrap_cut(b->line[b->count - 1], UI_LINE_BYTES, last, w, at, 0, width, ctx);
            }
            return all;
        }
    }
}

int wrap_with_path(ui_block *b, int count, const char *first, const char *path, const char *const rest[], float w,
                   int px, int min_px, float max_h, float lh, float gap, wrap_width width, void *ctx)
{
    char text[1024], cut_path[UI_LINE_BYTES + 4];
    const char *s[UI_PARAGRAPHS];
    if (count > UI_PARAGRAPHS)
        count = UI_PARAGRAPHS;
    s[0] = text;
    for (int i = 1; i < count; i++)
        s[i] = rest[i - 1];
    snprintf(text, sizeof text, first, path);
    if (wrap_paragraphs(b, count, s, w, px, min_px, max_h, lh, gap, width, ctx))
        return 1;
    /* the widest cut of the path at which everything fits, found by halving */
    float lo = 0, hi = width(path, strlen(path), min_px, ctx);
    for (int k = 0; k < 16 && hi - lo > 0.5f; k++) {
        float mid = (lo + hi) / 2;
        wrap_cut(cut_path, sizeof cut_path, path, mid, min_px, 1, width, ctx);
        snprintf(text, sizeof text, first, cut_path);
        if (wrap_paragraphs(b, count, s, w, px, min_px, max_h, lh, gap, width, ctx))
            lo = mid;
        else
            hi = mid;
    }
    wrap_cut(cut_path, sizeof cut_path, path, lo, min_px, 1, width, ctx);
    snprintf(text, sizeof text, first, cut_path);
    return wrap_paragraphs(b, count, s, w, px, min_px, max_h, lh, gap, width, ctx);
}

int wrap_paragraphs(ui_block *b, int count, const char *const s[], float w, int px, int min_px, float max_h,
                    float lh, float gap, wrap_width width, void *ctx)
{
    if (min_px > px)
        min_px = px;
    for (int at = px;; at--) {
        float k = (float)at / (float)px, h = gap * k * (float)(count - 1);
        int all = 1;
        for (int i = 0; i < count; i++) {
            all &= wrap_text(&b[i], s[i], w, at, at, 0, 0, lh * k, width, ctx);
            h += b[i].count * b[i].lh;
        }
        if (all && h <= max_h)
            return 1;
        if (at > min_px)
            continue;
        /* still too tall at min_px: the paragraphs in turn, each in what height is left, cut with "..." */
        float left = max_h;
        for (int i = 0; i < count; i++) {
            if (left < lh * k) {
                b[i].count = 0;
                continue;
            }
            wrap_text(&b[i], s[i], w, at, at, 0, left, lh * k, width, ctx);
            left -= b[i].count * b[i].lh + gap * k;
        }
        return 0;
    }
}
