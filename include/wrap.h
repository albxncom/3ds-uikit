/* Text broken into lines and cut to a width, in plain C so the host tests run it. Widths come from a callback (on
 * the console: the system font through citro2d). Characters are stepped through as citro2d draws them: libctru's
 * decode_utf8 decides how many bytes make one character, and a byte it refuses is one character (citro2d draws
 * U+FFFD for it), so what is measured is what is drawn whatever the bytes. No line grows past its buffer, and every
 * cut is marked with "...". */
#ifndef WRAP_H
#define WRAP_H

#include <stddef.h>

#define UI_LINES 16
#define UI_LINE_BYTES 200

/* Text wrapped to a width: the lines and the size they ended up at. */
typedef struct ui_block {
    int px;
    float lh;
    int count;
    char line[UI_LINES][UI_LINE_BYTES];
} ui_block;

/* The width of the n bytes at s at size px. */
typedef float (*wrap_width)(const char *s, size_t n, int px, void *ctx);

/* How many bytes the character at s takes as citro2d draws it (libctru 2.7.0 source/util/utf/decode_utf8.c):
 * 1 to 4, 1 for a byte decode_utf8 refuses, 0 at the end. */
size_t wrap_unit(const char *s);
size_t wrap_next(const char *s, size_t i);
size_t wrap_prev(const char *s, size_t i);

/* s cut with "..." at its end (or its beginning, from_end) so it is no wider than w and fits size; into out. */
void wrap_cut(char *out, size_t size, const char *s, float w, int px, int from_end, wrap_width width, void *ctx);

/* s wrapped at width w: at px, or smaller down to min_px, until it takes at most max_lines lines (0: any) and at
 * most max_h px (0: any), with lines lh px apart at px. A word wider than w, or longer than a line holds, is broken
 * between characters. Still too long at min_px: the last line ends in "...". Returns 1 when all of s fit. */
int wrap_text(ui_block *b, const char *s, float w, int px, int min_px, int max_lines, float max_h, float lh,
              wrap_width width, void *ctx);

/* count paragraphs, each wrapped at width w into its own block of b, gap px apart (at px; gap and lh scale with the
 * size): at px, or smaller down to min_px, until all of them together take at most max_h px. Still too tall at
 * min_px: the paragraphs are cut with "..." to the height that is left, later ones dropped. Returns 1 when all of
 * them fit. */
int wrap_paragraphs(ui_block *b, int count, const char *const s[], float w, int px, int min_px, float max_h,
                    float lh, float gap, wrap_width width, void *ctx);

/* As wrap_paragraphs, for count paragraphs: the first is the format first with path in its one %s, the others are
 * rest[0] to rest[count - 2]. When they do not all fit, path is cut at its beginning ("...", the last folder kept)
 * as little as makes them fit at min_px, so the path is shortened before any paragraph is cut or dropped. */
#define UI_PARAGRAPHS 8
int wrap_with_path(ui_block *b, int count, const char *first, const char *path, const char *const rest[], float w,
                   int px, int min_px, float max_h, float lh, float gap, wrap_width width, void *ctx);

#endif
