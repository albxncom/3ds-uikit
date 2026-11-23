/* What the wrap and order checks of the host tests share (test_kit.c, and the application's own tests of the
 * kit's code with its texts and shapes): a font's widths without a font, a block's lines held to its box, the
 * words of a text, and a log of what reaches the GPU, a letter each. */
#ifndef UI_FAKES_H
#define UI_FAKES_H

#include <stddef.h>
#include <string.h>

#include "check.h"
#include "order.h"
#include "wrap.h"

/* ---------------------------------------------------------------- an address with bytes the font would not draw */

/* As the font draws: one character per wrap_unit, 9 px each at 22 px. */
static float fake_width(const char *s, size_t n, int px, void *ctx)
{
    (void)ctx;
    float w = 0;
    for (size_t i = 0; i < n && s[i]; i += wrap_unit(s + i))
        w += 9.0f * (float)px / 22.0f;
    return w;
}

static int valid_utf8_shown(const char *s)
{
    for (size_t i = 0; s[i]; i += wrap_unit(s + i)) {
        unsigned char c = (unsigned char)s[i];
        if (wrap_unit(s + i) == 1 && (c < 0x20 || c >= 0x7F))
            return 0;
    }
    return 1;
}

/* Every line no wider than w and inside its buffer; a cut text ends in "...". */
static void check_block(const ui_block *b, int all, float w)
{
    for (int i = 0; i < b->count; i++) {
        CHECK(fake_width(b->line[i], strlen(b->line[i]), b->px, NULL) <= w);
        CHECK(strlen(b->line[i]) < UI_LINE_BYTES);
    }
    if (!all) {
        const char *last = b->line[b->count - 1];
        size_t n = strlen(last);
        CHECK(n >= 3 && !strcmp(last + n - 3, "..."));
    }
}

/* The words of s (runs between spaces and line ends), each followed by one space. */
static void words_of(const char *s, char *out, size_t size)
{
    size_t n = 0;
    for (const char *p = s; *p;) {
        while (*p == ' ' || *p == '\n')
            p++;
        const char *w = p;
        while (*p && *p != ' ' && *p != '\n')
            p++;
        if (p > w && n + (size_t)(p - w) + 2 < size) {
            memcpy(out + n, w, (size_t)(p - w));
            n += (size_t)(p - w);
            out[n++] = ' ';
        }
    }
    out[n] = 0;
}

static void block_words(const ui_block *b, char *out, size_t size)
{
    char all[UI_LINES * (UI_LINE_BYTES + 1)] = "";
    for (int i = 0; i < b->count; i++) {
        strcat(all, b->line[i]);
        strcat(all, "\n");
    }
    words_of(all, out, size);
}

/* The white flicker (order.h): what reaches the GPU, a letter each: P a picture, I an image, T a text. The
 * screens ask as ui.c's elements do: a picture where it stands, then its texts. */
static char order_log[512];
static order_rect order_last;

static void order_put(char c)
{
    size_t n = strlen(order_log);
    if (n + 1 < sizeof order_log)
        order_log[n] = c, order_log[n + 1] = 0;
}

static void order_drawn(void *ctx, int k, order_rect at)
{
    (void)ctx, (void)k;
    order_last = at;
    order_put('P');
}

static void sim_image(order *o, float x, float y, float w, float h)
{
    order_image(o, (order_rect){x, y, w, h}, 4);
    order_put('I');
}

static int order_count(char c)
{
    int n = 0;
    for (const char *p = order_log; *p; p++)
        n += *p == c;
    return n;
}

#endif
