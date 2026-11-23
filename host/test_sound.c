/* Unit tests of the kit's sound: the BCSTM stream decoder (bcstm.c) and the WAV reader (wav.c), every file
 * synthetic, built here; each one is copied into a buffer of exactly its size before it is read, so the asan build
 * catches a read past its end. Both readers also get a mutation run. Then the track table (tracks.c), the working
 * loop's timing (work.c), and, with a folder given, the kit's sound files, held against the track table and
 * CREDITS.md.
 * Run: test_sound [SOUND_DIR]   (the kit's sound files: test_sound sound) */
#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "audio.h"
#include "bcstm.h"
#include "check.h"
#include "mutate.h"
#include "synth_bcstm.h"
#include "tracks.h"
#include "wav.h"
#include "work.h"

/* the core's little-endian helpers, the same lines: the kit's tests build their files with them */
static inline void put_le16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static inline void put_le32(uint8_t *p, uint32_t v) { put_le16(p, (uint16_t)v); put_le16(p + 2, (uint16_t)(v >> 16)); }
static inline uint16_t get_le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

/* The largest stream the kit's player is given today: its user hands audio_theme_play a theme's bgm.bcstm, which
 * HOME caps at 0x337000 bytes (BgmCache.bin); the kit's own streams stay within it. */
#define STREAM_MAX 0x337000u

/* ---------------------------------------------------------------- BCSTM */

#define MAX_SAMPLES 4096u
#define MAX_BLOCKS 128u
#define FILE_CAP 0x10000u

enum { PATTERN_RANDOM, PATTERN_CLAMP, PATTERN_HAND, PATTERN_HOSTILE };

typedef struct stream_spec {
    unsigned encoding, channels;
    uint32_t blocks, block_samples, last_samples;
    uint32_t block_extra, last_extra;   /* bytes after a part's samples (real files' last parts carry 74) */
    unsigned loop;
    uint32_t loop_start, loop_end;
    int seek;
    int pattern;
} stream_spec;

/* Where stream_build put things, for the refusal tests. */
typedef struct layout {
    size_t size, info, si, table, channel[2], dsp[2], seek, data, samples_at, need;
} layout;

static uint8_t stream_buf[FILE_CAP];
static layout L;
static int16_t expect[MAX_SAMPLES * 2];           /* interleaved, from the reference decoder */
static int16_t block_hist[MAX_BLOCKS][2][2];      /* per block and channel: yn1, yn2 at the block's start */
static uint8_t frame_head[2][MAX_SAMPLES];        /* ADPCM: per channel and sample, the header of its frame */
static int8_t nibble_of[2][MAX_SAMPLES];
static int16_t coefs[2][16], context[2][2];

static uint64_t part_bytes(unsigned encoding, uint64_t n)
{
    if (encoding == 0)
        return n;
    if (encoding == 1)
        return 2 * n;
    /* whole frames of 8 bytes; a last, partial frame has its header and the bytes up to its last nibble */
    uint64_t rest = n % 14;
    return n / 14 * 8 + (rest ? 1 + (rest + 1) / 2 : 0);
}

/* floor(a / b) for b > 0 */
static int64_t floor_div(int64_t a, int64_t b)
{
    int64_t q = a / b;
    return a % b && a < 0 ? q - 1 : q;
}

/* The DSP-ADPCM sample as the hardware defines it, written as a sum: the nibble scaled by 2^scale plus the
 * prediction (11 fraction bits) rounded half up, then clamped. */
static int16_t reference_sample(int nibble, unsigned scale, int c1, int c2, int h1, int h2)
{
    int64_t prediction = (int64_t)c1 * h1 + (int64_t)c2 * h2;
    int64_t v = (int64_t)nibble * ((int64_t)1 << scale) + floor_div(prediction + 1024, 2048);
    return (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
}

static uint8_t choose_head(const stream_spec *sp, uint32_t frame)
{
    if (sp->pattern == PATTERN_CLAMP)
        return frame % 2 ? 0x1C : 0x0C;
    if (sp->pattern == PATTERN_HAND)
        return 0x00;
    if (sp->pattern == PATTERN_HOSTILE)
        return (uint8_t)(0x0F | (next_rand() % 16) << 4);
    unsigned r = next_rand();
    unsigned scale = r % 8 == 0 ? 12 + r / 8 % 4 : r / 8 % 12;
    return (uint8_t)((r >> 8) % 16 << 4 | scale);    /* the high bit of the predictor must be ignored */
}

static int choose_nibble(const stream_spec *sp, uint32_t frame)
{
    if (sp->pattern == PATTERN_CLAMP)
        return frame / 2 % 2 ? -8 : 7;
    if (sp->pattern == PATTERN_HAND)
        return 1;
    if (sp->pattern == PATTERN_HOSTILE)
        return -8;
    return (int)(next_rand() % 16) - 8;
}

static void set_nibble(uint8_t *p, unsigned k, int nibble)
{
    uint8_t *b = p + 1 + k / 2;
    unsigned v = (unsigned)nibble & 0xF;
    *b = k % 2 ? (uint8_t)((*b & 0xF0) | v) : (uint8_t)((*b & 0x0F) | v << 4);
}

static size_t align32(size_t x) { return (x + 31) / 32 * 32; }

static void put_ref(uint8_t *p, unsigned type, uint32_t offset)
{
    put_le16(p, (uint16_t)type);
    put_le16(p + 2, 0);
    put_le32(p + 4, offset);
}

/* A BCSTM file as sp says into stream_buf, its expected decoded frames into expect (and block_hist). Unused bytes
 * (padding, the nibbles after a block's last sample) are random, so a decoder reading them would show. */
static size_t stream_build(const stream_spec *sp)
{
    unsigned ch = sp->channels, enc = sp->encoding;
    uint32_t samples = (sp->blocks - 1) * sp->block_samples + sp->last_samples;
    uint32_t block_size = (uint32_t)part_bytes(enc, sp->block_samples) + sp->block_extra;
    uint32_t last_size = (uint32_t)part_bytes(enc, sp->last_samples) + sp->last_extra;
    uint32_t last_padded = (uint32_t)align32(last_size);
    if (samples > MAX_SAMPLES || sp->blocks > MAX_BLOCKS) {
        fprintf(stderr, "stream_spec too big\n");
        exit(2);
    }
    for (size_t i = 0; i < sizeof stream_buf; i++)
        stream_buf[i] = (uint8_t)next_rand();
    memset(&L, 0, sizeof L);

    for (unsigned c = 0; c < ch; c++) {
        for (int k = 0; k < 16; k++)
            coefs[c][k] = (int16_t)((int)(next_rand() % 8192) - 4096);
        context[c][0] = (int16_t)((int)(next_rand() % 2000) - 1000);
        context[c][1] = (int16_t)((int)(next_rand() % 2000) - 1000);
        if (sp->pattern == PATTERN_CLAMP) {
            memset(coefs[c], 0, sizeof coefs[c]);
            coefs[c][2] = 4096;         /* predictor 1: twice the last sample */
            context[c][0] = context[c][1] = 0;
        } else if (sp->pattern == PATTERN_HAND) {
            memset(coefs[c], 0, sizeof coefs[c]);
            coefs[c][0] = 2048;         /* predictor 0: the last sample */
            context[c][0] = 100;
            context[c][1] = 0;
        } else if (sp->pattern == PATTERN_HOSTILE) {
            for (int k = 0; k < 16; k++)
                coefs[c][k] = k % 4 < 2 ? -32768 : 32767;
            context[c][0] = context[c][1] = -32768;
        }
    }

    /* The header, then INFO at 0x40: stream info, an empty track table, the channel table, the channel infos and
     * (ADPCM) the DSP infos. */
    L.info = 0x40;
    size_t at = L.info + 0x20 + 0x38;
    size_t tracks = at;
    at += 4;
    L.table = at;
    at += 4 + 8 * ch;
    for (unsigned c = 0; c < ch; c++, at += 8)
        L.channel[c] = at;
    for (unsigned c = 0; c < ch; c++, at += 0x30)
        L.dsp[c] = at;
    if (enc != 2)
        at = L.dsp[0];
    size_t info_size = align32(at - L.info);
    L.seek = L.info + info_size;
    size_t seek_size = sp->seek ? align32(8 + (size_t)sp->blocks * ch * 4) : 0;
    size_t data = L.seek + seek_size;
    L.data = data;
    L.samples_at = data + 0x20;
    L.need = (size_t)(sp->blocks - 1) * block_size * ch + (ch - 1) * last_padded + last_size;
    size_t data_size = align32(0x20 + L.need);
    L.size = data + data_size;
    if (L.size > FILE_CAP) {
        fprintf(stderr, "stream too big for the buffer\n");
        exit(2);
    }

    uint8_t *f = stream_buf;
    memcpy(f, "CSTM", 4);
    put_le16(f + 4, 0xFEFF);
    put_le16(f + 6, 0x40);
    put_le32(f + 8, 0x02000000);
    put_le32(f + 0xC, (uint32_t)L.size);
    put_le16(f + 0x10, sp->seek ? 3 : 2);
    put_le16(f + 0x12, 0);
    put_ref(f + 0x14, 0x4000, (uint32_t)L.info);
    put_le32(f + 0x1C, (uint32_t)info_size);
    if (sp->seek) {
        put_ref(f + 0x20, 0x4001, (uint32_t)L.seek);
        put_le32(f + 0x28, (uint32_t)seek_size);
        put_ref(f + 0x2C, 0x4002, (uint32_t)data);
        put_le32(f + 0x34, (uint32_t)data_size);
    } else {
        put_ref(f + 0x20, 0x4002, (uint32_t)data);
        put_le32(f + 0x28, (uint32_t)data_size);
    }
    memset(f + 0x38, 0, 8);

    uint8_t *info = f + L.info;
    memcpy(info, "INFO", 4);
    put_le32(info + 4, (uint32_t)info_size);
    put_ref(info + 8, 0x4100, 0x18);
    put_ref(info + 0x10, 0x0101, (uint32_t)(tracks - (L.info + 8)));
    put_ref(info + 0x18, 0x0101, (uint32_t)(L.table - (L.info + 8)));
    L.si = L.info + 0x20;
    uint8_t *st = f + L.si;
    st[0] = (uint8_t)enc;
    st[1] = (uint8_t)sp->loop;
    st[2] = (uint8_t)ch;
    st[3] = 0;
    put_le32(st + 4, 32000);
    put_le32(st + 8, sp->loop_start);
    put_le32(st + 0xC, sp->loop_end);
    put_le32(st + 0x10, sp->blocks);
    put_le32(st + 0x14, block_size);
    put_le32(st + 0x18, sp->block_samples);
    put_le32(st + 0x1C, last_size);
    put_le32(st + 0x20, sp->last_samples);
    put_le32(st + 0x24, last_padded);
    put_le32(st + 0x28, 4);
    put_le32(st + 0x2C, sp->block_samples);
    put_ref(st + 0x30, 0x1F00, (uint32_t)(L.samples_at - (data + 8)));
    put_le32(f + tracks, 0);
    put_le32(f + L.table, ch);
    for (unsigned c = 0; c < ch; c++) {
        put_ref(f + L.table + 4 + 8 * c, 0x4102, (uint32_t)(L.channel[c] - L.table));
        if (enc == 2) {
            put_ref(f + L.channel[c], 0x0300, (uint32_t)(L.dsp[c] - L.channel[c]));
            for (int k = 0; k < 16; k++)
                put_le16(f + L.dsp[c] + 2 * k, (uint16_t)coefs[c][k]);
            f[L.dsp[c] + 0x20] = 0x55;              /* not the first frame's header: must not matter */
            put_le16(f + L.dsp[c] + 0x22, (uint16_t)context[c][0]);
            put_le16(f + L.dsp[c] + 0x24, (uint16_t)context[c][1]);
            memset(f + L.dsp[c] + 0x26, 0x77, 6);  /* a loop context that is wrong on purpose */
        } else {
            put_ref(f + L.channel[c], 0, 0xFFFFFFFF);
        }
    }

    memcpy(f + data, "DATA", 4);
    put_le32(f + data + 4, (uint32_t)data_size);
    for (uint32_t b = 0; b < sp->blocks; b++) {
        int last = b + 1 == sp->blocks;
        uint32_t n = last ? sp->last_samples : sp->block_samples;
        for (unsigned c = 0; c < ch; c++) {
            uint8_t *p = f + L.samples_at + (size_t)b * block_size * ch + c * (size_t)(last ? last_padded : block_size);
            for (uint32_t j = 0; j < n; j++) {
                uint32_t i = b * sp->block_samples + j;
                if (enc == 0) {
                    expect[i * ch + c] = (int16_t)((int)(int8_t)p[j] * 256);
                } else if (enc == 1) {
                    if (next_rand() % 16 == 0)
                        put_le16(p + 2 * j, next_rand() % 2 ? 0x8000 : 0x7FFF);
                    expect[i * ch + c] = (int16_t)get_le16(p + 2 * j);
                } else {
                    uint32_t frame = (b * 1000 + j / 14);
                    if (j % 14 == 0)
                        p[j / 14 * 8] = choose_head(sp, frame);
                    frame_head[c][i] = p[j / 14 * 8];
                    nibble_of[c][i] = (int8_t)choose_nibble(sp, frame);
                    set_nibble(p + j / 14 * 8, j % 14, nibble_of[c][i]);
                }
            }
        }
    }
    if (enc == 2)
        for (unsigned c = 0; c < ch; c++) {
            int h1 = context[c][0], h2 = context[c][1];
            for (uint32_t i = 0; i < samples; i++) {
                if (i % sp->block_samples == 0) {
                    block_hist[i / sp->block_samples][c][0] = (int16_t)h1;
                    block_hist[i / sp->block_samples][c][1] = (int16_t)h2;
                }
                unsigned head = frame_head[c][i], predictor = head >> 4 & 7;
                int v = reference_sample(nibble_of[c][i], head & 0xF, coefs[c][2 * predictor],
                                         coefs[c][2 * predictor + 1], h1, h2);
                expect[i * ch + c] = (int16_t)v;
                h2 = h1;
                h1 = v;
            }
        }
    if (sp->seek) {
        memcpy(f + L.seek, "SEEK", 4);
        put_le32(f + L.seek + 4, (uint32_t)seek_size);
        for (uint32_t b = 0; b < sp->blocks; b++)
            for (unsigned c = 0; c < ch; c++) {
                put_le16(f + L.seek + 8 + (b * ch + c) * 4, (uint16_t)block_hist[b][c][0]);
                put_le16(f + L.seek + 8 + (b * ch + c) * 4 + 2, (uint16_t)block_hist[b][c][1]);
            }
    }
    return L.size;
}

/* The frame a looping (or not) read should give as its k-th. */
static uint32_t frame_at(const bcstm_stream *s, uint64_t k, int looping)
{
    if (!looping || k < s->loop_end)
        return (uint32_t)k;
    return s->loop_start + (uint32_t)((k - s->loop_end) % (s->loop_end - s->loop_start));
}

static int16_t got[MAX_SAMPLES * 8 * 2];

static size_t mismatches(const bcstm_stream *s, const int16_t *g, size_t frames, int looping)
{
    size_t wrong = 0;
    for (size_t k = 0; k < frames; k++)
        for (unsigned c = 0; c < s->channels; c++)
            wrong += g[k * s->channels + c] != expect[frame_at(s, k, looping) * s->channels + c];
    return wrong;
}

static void stream_case(const stream_spec *sp)
{
    size_t n = stream_build(sp);
    uint8_t *file = exact_copy(stream_buf, n);
    uint32_t samples = (sp->blocks - 1) * sp->block_samples + sp->last_samples;
    bcstm_stream s;
    CHECK_EQ(bcstm_open(&s, file, n), 0);
    CHECK_EQ(s.encoding, sp->encoding);
    CHECK_EQ(s.channels, sp->channels);
    CHECK_EQ(s.rate, 32000);
    CHECK_EQ(s.samples, samples);
    CHECK_EQ(s.loops, sp->loop);
    CHECK_EQ(s.pos, 0);
    if (sp->loop) {
        CHECK_EQ(s.loop_start, sp->loop_start);
        CHECK_EQ(s.loop_end, sp->loop_end);
    }
    unsigned ch = s.channels;

    /* in one call; the end; again after a rewind */
    CHECK_EQ(bcstm_read(&s, got, samples + 100, 0), samples);
    CHECK_EQ(mismatches(&s, got, samples, 0), 0);
    CHECK_EQ(bcstm_read(&s, got, 10, 0), 0);
    CHECK_EQ(bcstm_read(&s, got, 10, 0), 0);
    CHECK_EQ(s.pos, samples);
    bcstm_rewind(&s);
    CHECK_EQ(s.pos, 0);
    CHECK_EQ(bcstm_read(&s, got, 5, 0), 5);
    CHECK_EQ(mismatches(&s, got, 5, 0), 0);

    /* in pieces */
    static const size_t pieces[] = {1, 7, 1000, 13, 14, 15};
    for (size_t p = 0; p < sizeof pieces / sizeof *pieces; p++) {
        bcstm_rewind(&s);
        size_t total = 0, r, short_reads = 0;
        while (total <= samples && (r = bcstm_read(&s, got + total * ch, pieces[p], 0)) > 0) {
            short_reads += r != pieces[p] && total + r != samples;
            total += r;
        }
        CHECK_EQ(total, samples);
        CHECK_EQ(short_reads, 0);
        CHECK_EQ(mismatches(&s, got, total, 0), 0);
    }

    /* looping: three turns and a bit, in pieces; never short */
    if (sp->loop) {
        size_t want = s.loop_end + 3 * (size_t)(s.loop_end - s.loop_start) + 17;
        for (size_t p = 0; p < sizeof pieces / sizeof *pieces; p++) {
            bcstm_rewind(&s);
            size_t total = 0, short_reads = 0;
            while (total < want) {
                size_t ask = pieces[p] < want - total ? pieces[p] : want - total;
                size_t r = bcstm_read(&s, got + total * ch, ask, 1);
                short_reads += r != ask;
                total += r;
                if (r == 0)
                    break;
            }
            CHECK_EQ(total, want);
            CHECK_EQ(short_reads, 0);
            CHECK_EQ(mismatches(&s, got, total, 1), 0);
        }
        /* a loop read stopped, then the rest without looping ends at samples */
        bcstm_rewind(&s);
        CHECK_EQ(bcstm_read(&s, got, s.loop_end + 5, 1), s.loop_end + 5);
        size_t rest = bcstm_read(&s, got, MAX_SAMPLES, 0);
        CHECK_EQ(rest, samples - (frame_at(&s, s.loop_end + 4, 1) + 1));
        CHECK_EQ(bcstm_read(&s, got, 5, 0), 0);
        /* rewind after looping */
        bcstm_rewind(&s);
        CHECK_EQ(bcstm_read(&s, got, samples, 0), samples);
        CHECK_EQ(mismatches(&s, got, samples, 0), 0);
    } else {
        bcstm_rewind(&s);
        CHECK_EQ(bcstm_read(&s, got, samples + 50, 1), samples);
        CHECK_EQ(bcstm_read(&s, got, 50, 1), 0);
    }

    /* the history at every block start, as a SEEK table holds it */
    if (sp->encoding == 2) {
        bcstm_rewind(&s);
        size_t wrong = 0;
        for (uint32_t b = 0; b < sp->blocks; b++) {
            for (unsigned c = 0; c < ch; c++)
                wrong += s.hist[c][0] != block_hist[b][c][0] || s.hist[c][1] != block_hist[b][c][1];
            bcstm_read(&s, got, sp->block_samples, 0);
        }
        CHECK_EQ(wrong, 0);
    }
    free(file);
}

static void test_bcstm_streams(void)
{
    static const stream_spec specs[] = {
        /* enc ch blocks bsamples last  bextra lextra loop start end seek pattern */
        {1, 1, 3, 100, 37, 0, 0, 0, 0, 0, 1, PATTERN_RANDOM},
        {1, 2, 4, 64, 10, 0, 3, 1, 50, 201, 1, PATTERN_RANDOM},
        {1, 2, 1, 300, 300, 0, 0, 1, 0, 300, 0, PATTERN_RANDOM},
        {0, 2, 2, 128, 128, 0, 0, 1, 0, 256, 0, PATTERN_RANDOM},
        {0, 1, 5, 33, 1, 5, 0, 1, 32, 33, 1, PATTERN_RANDOM},
        {2, 1, 5, 280, 47, 0, 0, 1, 140, 1167, 1, PATTERN_RANDOM},
        {2, 2, 6, 280, 99, 0, 74, 1, 563, 1301, 1, PATTERN_RANDOM},
        {2, 2, 7, 20, 3, 0, 0, 1, 40, 120, 1, PATTERN_RANDOM},
        {2, 2, 7, 20, 3, 8, 0, 1, 41, 123, 0, PATTERN_RANDOM},
        {2, 1, 1, 1000, 500, 0, 0, 1, 0, 500, 0, PATTERN_RANDOM},
        {2, 2, 9, 14, 14, 0, 0, 1, 13, 15, 1, PATTERN_RANDOM},
        {2, 2, 4, 140, 1, 0, 0, 0, 0, 0, 1, PATTERN_RANDOM},
        {2, 1, 3, 56, 56, 0, 0, 1, 1, 168, 0, PATTERN_CLAMP},
        {2, 1, 2, 28, 28, 0, 0, 0, 0, 0, 0, PATTERN_HAND},
        {2, 2, 3, 42, 30, 0, 0, 1, 5, 100, 1, PATTERN_HOSTILE},
    };
    for (size_t i = 0; i < sizeof specs / sizeof *specs; i++)
        stream_case(&specs[i]);

    /* The reference itself against values worked out by hand. Predictor 0 = 1.0 * the last sample, nibble 1,
     * scale 0, history 100: 100 + 1 + 0.5 rounded down each time (floor of x.5 after adding 1024). */
    stream_spec hand = specs[13];
    size_t n = stream_build(&hand);
    bcstm_stream s;
    uint8_t *file = exact_copy(stream_buf, n);
    CHECK_EQ(bcstm_open(&s, file, n), 0);
    CHECK_EQ(bcstm_read(&s, got, 3, 0), 3);
    CHECK(got[0] == 101 && got[1] == 102 && got[2] == 103);
    free(file);
    /* Clamping: predictor 0 (zero) with scale 12 and nibble 7 gives 28672; predictor 1 (twice the last) then
     * clamps at 32767; nibble -8 gives -32768 exactly, and predictor 1 clamps below it. */
    stream_spec clamp = specs[12];
    n = stream_build(&clamp);
    file = exact_copy(stream_buf, n);
    CHECK_EQ(bcstm_open(&s, file, n), 0);
    CHECK_EQ(bcstm_read(&s, got, 56, 0), 56);
    CHECK(got[0] == 28672 && got[13] == 28672);
    CHECK(got[14] == 32767 && got[27] == 32767);
    CHECK(got[28] == -32768 && got[41] == -32768);
    CHECK(got[42] == -32768 && got[55] == -32768);
    free(file);
}

static int open_buf(size_t n)
{
    uint8_t *file = exact_copy(stream_buf, n);
    bcstm_stream s;
    int r = bcstm_open(&s, file, n);
    free(file);
    return r;
}

static const stream_spec BASE = {2, 2, 6, 280, 99, 0, 74, 1, 563, 1301, 1, PATTERN_RANDOM};

static uint8_t base_copy[FILE_CAP];

static void base(void)
{
    memcpy(stream_buf, base_copy, L.size);
}

static void set32(size_t at, uint32_t v) { put_le32(stream_buf + at, v); }

static void test_bcstm_refusals(void)
{
    size_t n = stream_build(&BASE);
    memcpy(base_copy, stream_buf, n);
    layout b = L;
    CHECK_EQ(open_buf(n), 0);

    size_t opened = 0;
    for (size_t k = 0; k < n; k++)
        opened += open_buf(k) == 0;
    CHECK_EQ(opened, 0);
    /* named places to cut: in the header, the stream info, the tables, SEEK, DATA, one byte short of the data */
    size_t cuts[] = {0, 4, 0x13, 0x14, 0x37, b.info + 4, b.si + 0x10, b.table + 4, b.dsp[1] + 0x20, b.seek + 8,
                     b.data + 4, b.samples_at + b.need - 1};
    for (size_t k = 0; k < sizeof cuts / sizeof *cuts; k++)
        CHECK_EQ(open_buf(cuts[k]), -1);

    struct { size_t at; uint32_t value; int width; } bad[] = {
        {0, 0x5853544D, 4},                 /* "MTSX" */
        {4, 0xFFFE, 2},                     /* the big-endian mark: FE FF */
        {b.info, 0x4F464E4A, 4},            /* "JNFO" */
        {b.seek, 0x4B454553 ^ 0x20, 4},
        {b.data, 0x41544144 ^ 0x2000, 4},
        {b.si, 3, 1},                       /* IMA-ADPCM */
        {b.si, 4, 1},
        {b.si + 2, 3, 1},
        {b.si + 2, 0, 1},
        {b.si + 4, 0, 4},                   /* rate */
        {b.si + 4, 192001, 4},
        {b.si + 0x10, 0, 4},                /* block count */
        {b.si + 0x10, 7, 4},                /* one block more than the data holds */
        {b.si + 0x10, 0xFFFFFFFF, 4},
        {b.si + 0x14, 0, 4},                /* block size */
        {b.si + 0x14, 0x9F, 4},             /* 280 samples need 160 bytes */
        {b.si + 0x14, 0xFFFFFFFF, 4},
        {b.si + 0x18, 0, 4},                /* block samples */
        {b.si + 0x18, 281, 4},
        {b.si + 0x1C, 57, 4},               /* last size below what its 99 samples take (58 bytes) */
        {b.si + 0x1C, 0x1000, 4},
        {b.si + 0x20, 0, 4},                /* last samples */
        {b.si + 0x20, 281, 4},
        {b.si + 0x24, 0x7F, 4},             /* last padded below the last size (131) with two channels */
        {b.si + 0x24, 0x1000, 4},
        {b.si + 0x34, 0x1000, 4},           /* sample data offset */
        {b.si + 0x34, 0xFFFFFFFF, 4},
        {b.si + 0x30, 0x1F01, 2},
        {b.info + 8, 0x4101, 2},            /* stream info reference type */
        {b.info + 0xC, 0x1000, 4},
        {b.info + 0xC, 0xFFFFFFFF, 4},
        {b.info + 0x18, 0x0100, 2},         /* channel table */
        {b.info + 0x1C, 0xFFFFFFF0, 4},
        {b.table, 1, 4},                    /* fewer channels than the stream */
        {b.table + 4 + 8, 0x0300, 2},
        {b.table + 4 + 8 + 4, 0x7FFFFFFF, 4},
        {b.channel[1], 0x0301, 2},          /* IMA info for a DSP stream */
        {b.channel[1] + 4, 0xFFFFFFFF, 4},
        {b.channel[1] + 4, (uint32_t)(b.seek - b.channel[1] - 0x2B), 4},   /* DSP info past the INFO block */
        {0x14, 0x4005, 2},                  /* no INFO */
        {0x2C, 0x4000, 2},                  /* INFO twice, no DATA */
        {0x20, 0x4002, 2},                  /* DATA twice */
        {0x18, 0xFFFFFFFF, 4},              /* INFO at null */
        {0x18, (uint32_t)n - 4, 4},
        {0x1C, 4, 4},                       /* a reference size below a block header */
        {0x1C, 0xFFFFFFFF, 4},
        {0x30, (uint32_t)n, 4},             /* DATA starts at the end */
        {b.info + 4, 0x10000, 4},           /* INFO's own size past the file */
        {b.info + 4, 0x1C, 4},              /* INFO too small for its references */
        {b.data + 4, 0x18, 4},              /* DATA too small for its samples */
        {b.seek + 4, 0x10000, 4},
        {0x10, 0x1000, 2},                  /* more references than the file holds */
    };
    for (size_t k = 0; k < sizeof bad / sizeof *bad; k++) {
        base();
        if (bad[k].width == 1)
            stream_buf[bad[k].at] = (uint8_t)bad[k].value;
        else if (bad[k].width == 2)
            put_le16(stream_buf + bad[k].at, (uint16_t)bad[k].value);
        else
            set32(bad[k].at, bad[k].value);
        if (open_buf(n) != -1)
            fprintf(stderr, "refusal %zu (at %#zx) opened\n", k, bad[k].at);
        CHECK_EQ(open_buf(n), -1);
    }

    /* accepted variations: rate 192000 and 1, no SEEK (null, or of an unknown type), a loop flag of 2, any
     * context header; then INFO and DATA swapped in the header */
    struct { size_t at; uint32_t value; int width; } fine[] = {
        {b.si + 4, 192000, 4}, {b.si + 4, 1, 4}, {0x24, 0xFFFFFFFF, 4}, {0x20, 0x5000, 2},
        {b.si + 1, 2, 1}, {b.dsp[0] + 0x20, 0xFF, 1},
    };
    for (size_t k = 0; k < sizeof fine / sizeof *fine; k++) {
        base();
        if (fine[k].width == 1)
            stream_buf[fine[k].at] = (uint8_t)fine[k].value;
        else if (fine[k].width == 2)
            put_le16(stream_buf + fine[k].at, (uint16_t)fine[k].value);
        else
            set32(fine[k].at, fine[k].value);
        CHECK_EQ(open_buf(n), 0);
    }
    base();
    uint8_t swap[12];
    memcpy(swap, stream_buf + 0x14, 12);
    memmove(stream_buf + 0x14, stream_buf + 0x2C, 12);
    memcpy(stream_buf + 0x2C, swap, 12);
    CHECK_EQ(open_buf(n), 0);

    /* unusable loop points clear the flag instead of refusing; the stream then ends at samples */
    uint32_t samples = 5 * 280 + 99;
    static const uint32_t loops[][2] = {{100, 100}, {200, 100}, {0, 1500}, {1499, 1499}, {0xFFFFFFFF, 0},
                                        {5, 0xFFFFFFFF}};
    for (size_t k = 0; k < sizeof loops / sizeof *loops; k++) {
        base();
        set32(b.si + 8, loops[k][0]);
        set32(b.si + 0xC, loops[k][1]);
        uint8_t *file = exact_copy(stream_buf, n);
        bcstm_stream s;
        CHECK_EQ(bcstm_open(&s, file, n), 0);
        CHECK_EQ(s.loops, 0);
        CHECK(s.loop_start < s.loop_end && s.loop_end <= s.samples);
        CHECK_EQ(bcstm_read(&s, got, MAX_SAMPLES, 1), samples);
        CHECK_EQ(bcstm_read(&s, got, 1, 1), 0);
        free(file);
    }
    /* the last frame and the whole stream are fine loop points */
    base();
    set32(b.si + 8, 1498);
    set32(b.si + 0xC, 1499);
    uint8_t *file = exact_copy(stream_buf, n);
    bcstm_stream s;
    CHECK_EQ(bcstm_open(&s, file, n), 0);
    CHECK_EQ(s.loops, 1);
    free(file);
    base();
}

static void bcstm_mutations(const char *label, const stream_spec *sp, int rounds)
{
    size_t size = stream_build(sp);
    size_t headers = L.samples_at;
    static const uint32_t interesting[] = {0, 1, 2, 3, 0x7F, 0x80, 0xFF, 0xFFFF, 0x10000, 0x7FFFFFFF, 0xFFFFFFFF,
                                           0xFFFFFFFE, 14, 15, 0x2000};
    size_t opened = 0, short_reads = 0, bad_fields = 0;
    for (int round = 0; round < rounds; round++) {
        uint8_t *buf = exact_copy(stream_buf, size);
        size_t n = size;
        for (int k = 1 + (int)(next_rand() % 4); k > 0 && n > 0; k--) {
            uint32_t what = next_rand() % 4;
            /* mostly in the headers and tables: the sample bytes cannot be refused */
            uint32_t span = (uint32_t)(next_rand() % 4 && headers < n ? headers : n), at = next_rand() % span;
            if (what == 0)
                buf[at] ^= (uint8_t)(1 << (next_rand() % 8));
            else if (what == 1)
                buf[at] = (uint8_t)next_rand();
            else if (what == 2 && at + 4 <= n)
                put_le32(buf + at, interesting[next_rand() % (sizeof interesting / sizeof *interesting)]);
            else if (what == 3 && round % 7 == 0)
                n = at;
        }
        /* the shortened file in a buffer of its own size, so ASan sees a read past it */
        uint8_t *file = exact_copy(buf, n);
        free(buf);
        bcstm_stream s;
        if (bcstm_open(&s, file, n) == 0) {
            opened++;
            bad_fields += s.channels < 1 || s.channels > 2 || s.encoding > 2 || s.rate < 1 || s.rate > 192000 ||
                          s.loop_start >= s.loop_end || s.loop_end > s.samples;
            uint64_t want = s.loops ? s.loop_end + 3 * (uint64_t)(s.loop_end - s.loop_start) : s.samples;
            uint64_t total = 0;
            for (int pass = 0; pass < 2; pass++) {
                while (total < want) {
                    size_t ask = 777;
                    if (want - total < ask)
                        ask = (size_t)(want - total);
                    size_t r = bcstm_read(&s, got, ask, s.loops);
                    if (r == 0)
                        break;
                    total += r;
                }
                if (pass == 0) {
                    short_reads += total != want;
                    bcstm_rewind(&s);
                    want = total + 1000;
                }
            }
        }
        free(file);
    }
    CHECK(opened > 0);
    CHECK_EQ(bad_fields, 0);
    CHECK_EQ(short_reads, 0);
    printf("mutations of %s: %d rounds, %zu opened\n", label, rounds, opened);
}

static void test_bcstm_mutations(void)
{
    bcstm_mutations("an ADPCM stream", &BASE, 6000);
    static const stream_spec pcm = {1, 2, 4, 64, 10, 0, 3, 1, 50, 201, 1, PATTERN_RANDOM};
    bcstm_mutations("a PCM16 stream", &pcm, 2000);
    static const stream_spec pcm8 = {0, 1, 3, 100, 5, 0, 0, 1, 7, 205, 0, PATTERN_RANDOM};
    bcstm_mutations("a PCM8 stream", &pcm8, 2000);
}

/* ---------------------------------------------------------------- WAV */

static uint8_t wav_buf[0x4000];

static size_t wav_start(void)
{
    memset(wav_buf, 0, sizeof wav_buf);
    memcpy(wav_buf, "RIFF", 4);
    memcpy(wav_buf + 8, "WAVE", 4);
    return 12;
}

/* A chunk of len bytes (the declared size may say otherwise), padded to an even size. */
static size_t wav_chunk(size_t at, const char *id, const uint8_t *data, uint32_t len, uint32_t declared)
{
    memcpy(wav_buf + at, id, 4);
    put_le32(wav_buf + at + 4, declared);
    if (data)
        memcpy(wav_buf + at + 8, data, len);
    else
        for (uint32_t i = 0; i < len; i++)
            wav_buf[at + 8 + i] = (uint8_t)next_rand();
    at += 8 + len;
    if (len & 1)
        wav_buf[at++] = 0;
    put_le32(wav_buf + 4, (uint32_t)(at - 8));
    return at;
}

static void fmt_of(uint8_t *f, unsigned tag, unsigned ch, uint32_t rate, unsigned bits)
{
    memset(f, 0, 40);
    put_le16(f, (uint16_t)tag);
    put_le16(f + 2, (uint16_t)ch);
    put_le32(f + 4, rate);
    put_le32(f + 8, rate * ch * bits / 8);
    put_le16(f + 12, (uint16_t)(ch * bits / 8));
    put_le16(f + 14, (uint16_t)bits);
}

/* wav_read on a copy of exactly n bytes; *data_at: where the samples start in the file. */
static int wav_of(size_t n, wav_info *w, size_t *data_at)
{
    uint8_t *file = exact_copy(wav_buf, n);
    int r = wav_read(file, n, w);
    *data_at = r == 0 ? (size_t)(w->data - file) : 0;
    free(file);
    return r;
}

static size_t plain_wav(unsigned ch, uint32_t rate, uint32_t frames)
{
    uint8_t fmt[40];
    fmt_of(fmt, 1, ch, rate, 16);
    size_t at = wav_start();
    at = wav_chunk(at, "fmt ", fmt, 16, 16);
    return wav_chunk(at, "data", NULL, frames * ch * 2, frames * ch * 2);
}

static void test_wav(void)
{
    wav_info w;
    size_t at, n, data_at;
    uint8_t fmt[40];

    n = plain_wav(1, 22050, 100);
    CHECK_EQ(wav_of(n, &w, &data_at), 0);
    CHECK_EQ(w.channels, 1);
    CHECK_EQ(w.rate, 22050);
    CHECK_EQ(w.frames, 100);
    CHECK_EQ(data_at, 12 + 8 + 16 + 8);
    n = plain_wav(2, 48000, 333);
    CHECK_EQ(wav_of(n, &w, &data_at), 0);
    CHECK_EQ(w.channels, 2);
    CHECK_EQ(w.rate, 48000);
    CHECK_EQ(w.frames, 333);
    n = plain_wav(2, 8000, 1);
    CHECK_EQ(wav_of(n, &w, &data_at), 0);
    CHECK_EQ(w.frames, 1);

    /* extensible: 40 bytes, sub-format 1 */
    fmt_of(fmt, 0xFFFE, 2, 44100, 16);
    put_le16(fmt + 16, 22);
    put_le16(fmt + 18, 16);
    put_le32(fmt + 20, 3);
    put_le16(fmt + 24, 1);
    at = wav_chunk(wav_start(), "fmt ", fmt, 40, 40);
    n = wav_chunk(at, "data", NULL, 400, 400);
    CHECK_EQ(wav_of(n, &w, &data_at), 0);
    CHECK_EQ(w.frames, 100);
    CHECK_EQ(w.rate, 44100);
    CHECK_EQ(data_at, 12 + 8 + 40 + 8);
    put_le16(fmt + 24, 3);                          /* float inside extensible */
    at = wav_chunk(wav_start(), "fmt ", fmt, 40, 40);
    n = wav_chunk(at, "data", NULL, 400, 400);
    CHECK_EQ(wav_of(n, &w, &data_at), -1);
    put_le16(fmt + 24, 1);
    at = wav_chunk(wav_start(), "fmt ", fmt, 18, 18);  /* extensible tag with too short a chunk */
    n = wav_chunk(at, "data", NULL, 400, 400);
    CHECK_EQ(wav_of(n, &w, &data_at), -1);

    /* LIST before and after fmt, an odd-sized chunk padded, a longer fmt (18 bytes) */
    fmt_of(fmt, 1, 2, 32000, 16);
    at = wav_chunk(wav_start(), "LIST", NULL, 26, 26);
    at = wav_chunk(at, "fmt ", fmt, 18, 18);
    at = wav_chunk(at, "LIST", NULL, 5, 5);
    at = wav_chunk(at, "junk", NULL, 1, 1);
    size_t expect_at = at + 8;
    n = wav_chunk(at, "data", NULL, 401, 401);
    CHECK_EQ(wav_of(n, &w, &data_at), 0);
    CHECK_EQ(data_at, expect_at);
    CHECK_EQ(w.frames, 100);
    CHECK_EQ(w.rate, 32000);
    /* data after which more chunks follow */
    n = wav_chunk(n, "LIST", NULL, 9, 9);
    CHECK_EQ(wav_of(n, &w, &data_at), 0);
    CHECK_EQ(w.frames, 100);

    /* a data size past the end of the file: cut to it */
    fmt_of(fmt, 1, 2, 44100, 16);
    at = wav_chunk(wav_start(), "fmt ", fmt, 16, 16);
    n = wav_chunk(at, "data", NULL, 400, 0xFFFFFFF0);
    CHECK_EQ(wav_of(n, &w, &data_at), 0);
    CHECK_EQ(w.frames, 100);
    CHECK_EQ(wav_of(n - 3, &w, &data_at), 0);
    CHECK_EQ(w.frames, 99);
    n = wav_chunk(at, "data", NULL, 400, 1000);
    CHECK_EQ(wav_of(n, &w, &data_at), 0);
    CHECK_EQ(w.frames, 100);

    /* refusals */
    static const struct { unsigned tag, ch; uint32_t rate; unsigned bits; } bad[] = {
        {1, 1, 22050, 8}, {3, 1, 22050, 32}, {3, 1, 22050, 16}, {1, 3, 22050, 16}, {1, 0, 22050, 16},
        {1, 1, 7999, 16}, {1, 2, 48001, 16}, {1, 2, 0, 16}, {1, 1, 22050, 24}, {2, 1, 22050, 16},
    };
    for (size_t k = 0; k < sizeof bad / sizeof *bad; k++) {
        fmt_of(fmt, bad[k].tag, bad[k].ch, bad[k].rate, bad[k].bits);
        at = wav_chunk(wav_start(), "fmt ", fmt, 16, 16);
        n = wav_chunk(at, "data", NULL, 400, 400);
        CHECK_EQ(wav_of(n, &w, &data_at), -1);
    }
    fmt_of(fmt, 1, 2, 22050, 16);
    put_le16(fmt + 12, 2);                          /* block align not channels * 2 */
    at = wav_chunk(wav_start(), "fmt ", fmt, 16, 16);
    n = wav_chunk(at, "data", NULL, 400, 400);
    CHECK_EQ(wav_of(n, &w, &data_at), -1);
    fmt_of(fmt, 1, 2, 22050, 16);
    at = wav_chunk(wav_start(), "fmt ", fmt, 14, 14);  /* fmt too short */
    n = wav_chunk(at, "data", NULL, 400, 400);
    CHECK_EQ(wav_of(n, &w, &data_at), -1);
    at = wav_chunk(wav_start(), "LIST", NULL, 10, 10);  /* no fmt */
    n = wav_chunk(at, "data", NULL, 400, 400);
    CHECK_EQ(wav_of(n, &w, &data_at), -1);
    at = wav_chunk(wav_start(), "data", NULL, 400, 400);  /* data before fmt */
    n = wav_chunk(at, "fmt ", fmt, 16, 16);
    CHECK_EQ(wav_of(n, &w, &data_at), -1);
    at = wav_chunk(wav_start(), "fmt ", fmt, 16, 16);  /* no data */
    CHECK_EQ(wav_of(at, &w, &data_at), -1);
    n = wav_chunk(at, "data", NULL, 0, 0);          /* no frame */
    CHECK_EQ(wav_of(n, &w, &data_at), -1);
    n = wav_chunk(at, "data", NULL, 3, 3);
    CHECK_EQ(wav_of(n, &w, &data_at), -1);
    n = wav_chunk(at, "LIST", NULL, 10, 0x1000);    /* an unknown chunk past the end before data */
    CHECK_EQ(wav_of(n, &w, &data_at), -1);
    at = wav_chunk(wav_start(), "fmt ", fmt, 16, 0x1000);  /* fmt past the end */
    CHECK_EQ(wav_of(at, &w, &data_at), -1);
    n = plain_wav(2, 22050, 100);
    for (size_t k = 0; k < 12 + 8 + 16 + 8 + 4; k++)
        CHECK_EQ(wav_of(k, &w, &data_at), -1);     /* truncated: header, fmt, data header, less than a frame */
    CHECK_EQ(wav_of(12 + 8 + 16 + 8 + 4, &w, &data_at), 0);
    wav_buf[0] = 'r';
    CHECK_EQ(wav_of(n, &w, &data_at), -1);
    wav_buf[0] = 'R';
    wav_buf[11] = 'w';
    CHECK_EQ(wav_of(n, &w, &data_at), -1);
}

static void test_wav_mutations(int rounds)
{
    uint8_t fmt[40];
    fmt_of(fmt, 1, 2, 32000, 16);
    size_t at = wav_chunk(wav_start(), "LIST", NULL, 7, 7);
    at = wav_chunk(at, "fmt ", fmt, 16, 16);
    size_t size = wav_chunk(at, "data", NULL, 200, 200);
    static uint8_t base_wav[sizeof wav_buf];
    memcpy(base_wav, wav_buf, size);
    static const uint32_t interesting[] = {0, 1, 2, 16, 40, 0xFFFE, 0x7FFFFFFF, 0xFFFFFFFF, 0xFFFFFFFE, 8000, 48000};
    size_t opened = 0, outside = 0;
    for (int round = 0; round < rounds; round++) {
        uint8_t *buf = exact_copy(base_wav, size);
        size_t n = size;
        for (int k = 1 + (int)(next_rand() % 4); k > 0 && n > 0; k--) {
            uint32_t what = next_rand() % 4, span = (uint32_t)(next_rand() % 3 && n > 64 ? 64 : n);
            uint32_t at_ = next_rand() % span;
            if (what == 0)
                buf[at_] ^= (uint8_t)(1 << (next_rand() % 8));
            else if (what == 1)
                buf[at_] = (uint8_t)next_rand();
            else if (what == 2 && at_ + 4 <= n)
                put_le32(buf + at_, interesting[next_rand() % (sizeof interesting / sizeof *interesting)]);
            else if (what == 3 && round % 7 == 0)
                n = at_;
        }
        uint8_t *file = exact_copy(buf, n);
        free(buf);
        wav_info w;
        if (wav_read(file, n, &w) == 0) {
            opened++;
            size_t start = (size_t)(w.data - file);
            outside += w.channels < 1 || w.channels > 2 || w.rate < 8000 || w.rate > 48000 || w.frames == 0 ||
                       start > n || (uint64_t)w.frames * w.channels * 2 > n - start;
            /* touch every sample byte: ASan reports any outside the file */
            unsigned sum = 0;
            for (uint64_t i = 0; i < (uint64_t)w.frames * w.channels * 2; i++)
                sum += w.data[i];
            (void)sum;
        }
        free(file);
    }
    CHECK(opened > 0);
    CHECK_EQ(outside, 0);
    printf("mutations of a WAV: %d rounds, %zu opened\n", rounds, opened);
}

/* Counts and sizes that wrap the 64-bit sum of the bytes a stream needs (found in review: 4294836227
 * blocks of 2147549185 bytes in two channels come to 6 bytes modulo 2^64, and the decoder then read 128 KiB past
 * the file per sample), and their neighbours: every one is refused, in each encoding. */
static void test_bcstm_wrap(void)
{
    static const struct { uint32_t blocks, block_size, last_size, last_padded; } cases[] = {
        {4294836227u, 2147549185u, 1, 1},       /* the sum wraps to a few bytes */
        {0xFFFFFFFFu, 0x80000001u, 2, 2},
        {0xFFFFFFFFu, 0xFFFFFFFFu, 2, 2},
        {2, 0xFFFFFFFFu, 2, 2},                 /* no wrap, only far too large */
        {0xFFFFFFFFu, 2, 2, 2},
        {1, 2, 0xFFFFFFFFu, 0xFFFFFFFFu},
        {1, 2, 2, 0xFFFFFFFFu},
    };
    for (unsigned encoding = 0; encoding < 3; encoding++)
        for (size_t i = 0; i < sizeof cases / sizeof *cases; i++) {
            static uint8_t f[SYNTH_BCSTM_MAX(16)];
            size_t size = synth_bcstm(f, 16, 0);
            uint8_t *st = f + 0x40 + 0x20, *table = f + 0x40 + 8 + 0x50;
            st[0] = (uint8_t)encoding;
            st[2] = 2;                              /* two channels: a second entry in the channel table */
            synth_le32(st + 0x10, cases[i].blocks);
            synth_le32(st + 0x14, cases[i].block_size);
            synth_le32(st + 0x18, 1);               /* one sample a block: 1 or 2 bytes, or part of an ADPCM frame */
            synth_le32(st + 0x1C, cases[i].last_size);
            synth_le32(st + 0x20, 1);
            synth_le32(st + 0x24, cases[i].last_padded);
            synth_le32(table, 2);
            synth_le16(table + 0xC, 0x4102);
            synth_le32(table + 0x10, 0x14);
            uint8_t *copy = exact_copy(f, size);
            bcstm_stream s;
            CHECK_EQ(bcstm_open(&s, copy, size), -1);
            free(copy);
        }
    /* the same file with counts that fit is read, so it is the counts that were refused */
    static uint8_t ok[SYNTH_BCSTM_MAX(16)];
    size_t size = synth_bcstm(ok, 16, 0);
    bcstm_stream s;
    CHECK_EQ(bcstm_open(&s, ok, size), 0);
    CHECK(s.samples == 16 && s.loops);
}

/* ---------------------------------------------------------------- the kit's own sound files (sound/) */

/* The whole file, or NULL. */
static uint8_t *slurp(const char *dir, const char *name, size_t *size)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    uint8_t *buf = NULL;
    long end = -1;
    if (!fseek(f, 0, SEEK_END))
        end = ftell(f);
    if (end > 0 && !fseek(f, 0, SEEK_SET) && (buf = malloc((size_t)end)) && fread(buf, 1, (size_t)end, f) != (size_t)end) {
        free(buf);
        buf = NULL;
    }
    fclose(f);
    *size = buf ? (size_t)end : 0;
    return buf;
}

/* A BCSTM of the kit's own as the player takes it: opens, at a rate it plays, and decodes through its whole
 * length and two more turns of its loop, a buffer of the player's size at a time. Returns its size, 0 when it fails. */
static size_t check_stream(const char *dir, const char *name, int must_loop, double most_seconds)
{
    size_t size = 0;
    uint8_t *file = slurp(dir, name, &size);
    bcstm_stream s;
    int ok = file && size <= STREAM_MAX && !bcstm_open(&s, file, size);
    CHECK(ok);
    if (!ok) {
        fprintf(stderr, "%s/%s: missing, or not a BCSTM the player reads\n", dir, name);
        free(file);
        return 0;
    }
    CHECK(s.rate >= 8000 && s.rate <= 48000);
    CHECK(!must_loop || s.loops);
    CHECK((double)s.samples / s.rate <= most_seconds);
    static int16_t pcm[4096 * 2];
    uint64_t want = (uint64_t)s.samples + (s.loops ? 2 * (uint64_t)(s.loop_end - s.loop_start) : 0), got_frames = 0;
    while (got_frames < want) {
        size_t n = bcstm_read(&s, pcm, 4096, 1);
        if (!n)
            break;
        got_frames += n;
    }
    CHECK(got_frames >= want);
    printf("%s/%s: %u Hz, %u channel(s), %.1f s, %s %.1f s, %zu bytes\n", dir, name, (unsigned)s.rate,
           (unsigned)s.channels, (double)s.samples / s.rate,
           s.loops ? "loop from" : "no loop of its own, the player starts it over from", (double)s.loop_start / s.rate, size);
    free(file);
    return size;
}

/* How loud a sound is on the console's speakers, as far as an imitation of them can say: the sound (a stereo file
 * mixed to mono) through a 6th-order Butterworth high-pass at SPEAKER_CUT Hz, three biquad high-passes in series
 * (Audio EQ Cookbook, zero start state, 100 ms of silence after it for the tail), then the level rule's measure: the
 * RMS of the loudest 50 ms, in dBFS. The filter was fitted to what the console played, never measured on it. */
#define SPEAKER_CUT 600.0
static double speaker_level(const wav_info *w)
{
    size_t n = w->frames + w->rate / 10, win = w->rate / 20;
    double *y = calloc(n, sizeof *y);
    if (!y)
        return -INFINITY;
    for (size_t i = 0; i < w->frames; i++) {
        double v = 0;
        for (unsigned c = 0; c < w->channels; c++) {
            const uint8_t *b = w->data + 2 * (i * w->channels + c);
            v += (int16_t)(b[0] | b[1] << 8);
        }
        y[i] = v / w->channels;
    }
    const double pi = 3.14159265358979323846, w0 = 2 * pi * SPEAKER_CUT / w->rate, c = cos(w0);
    for (int k = 1; k <= 3; k++) {
        double q = 1 / (2 * cos(pi * (2 * k - 1) / 12)), al = sin(w0) / (2 * q), a0 = 1 + al;
        double b0 = (1 + c) / 2 / a0, b1 = -(1 + c) / a0, a1 = -2 * c / a0, a2 = (1 - al) / a0;
        double in1 = 0, in2 = 0, out1 = 0, out2 = 0;
        for (size_t i = 0; i < n; i++) {
            double o = b0 * y[i] + b1 * in1 + b0 * in2 - a1 * out1 - a2 * out2;
            in2 = in1; in1 = y[i]; out2 = out1; out1 = o;
            y[i] = o;
        }
    }
    if (win > n)
        win = n;
    double acc = 0, best;
    for (size_t i = 0; i < win; i++)
        acc += y[i] * y[i];
    best = acc;
    for (size_t i = win; i < n; i++) {
        acc += y[i] * y[i] - y[i - win] * y[i - win];
        if (acc > best)
            best = acc;
    }
    free(y);
    double rms = sqrt(best / win) / 32768;
    return 20 * log10(rms > 1e-12 ? rms : 1e-12);
}

/* The files as audio.c takes them at start: seven interface sounds its WAV reader accepts, short enough for a
 * click; the working loop; and every track of tracks.h's table, each with its file, title, author and page named
 * in CREDITS.md. A swapped file that fails here would be silent on the console, a track without its credits would
 * be built in unnamed. Each interface sound must also be at least SPEAKER_LEVEL loud through the imitation of the
 * console's speakers (speaker_level), which hardly play low tones. The line lies in the middle of the gap between
 * what the console played: the back sound bong_001 was silent (-54.2 dBFS), the popup
 * maximize_008 as converted was heard but very quiet (-46.0) and error_005 as converted barely heard (-40.5); the back
 * sound that was heard well is at -26.2, and the seven as built are at -28.3 (the popup) and above. */
#define SPEAKER_LEVEL -34.4
static void test_sounds(const char *dir)
{
    static const char *const names[SFX_COUNT] = {"move.wav", "confirm.wav", "back.wav", "popup.wav", "error.wav",
                                                 "done.wav", "drop.wav"};
    size_t total = 0;
    for (int i = 0; i < SFX_COUNT; i++) {
        size_t size = 0;
        uint8_t *file = slurp(dir, names[i], &size);
        wav_info w;
        int ok = file && !wav_read(file, size, &w);
        CHECK(ok);
        if (ok) {
            CHECK(w.frames <= 2 * w.rate);          /* an interface sound, not a tune: two seconds at most */
            double loud = speaker_level(&w);
            printf("%s/%s: %u Hz, %u channel(s), %u ms, %zu bytes, %.2f dBFS through the speakers' imitation\n", dir,
                   names[i], (unsigned)w.rate, (unsigned)w.channels, (unsigned)((uint64_t)w.frames * 1000 / w.rate),
                   size, loud);
            CHECK(loud >= SPEAKER_LEVEL);
            if (loud < SPEAKER_LEVEL)
                fprintf(stderr, "%s/%s: %.2f dBFS through the speakers' imitation, under %.1f dBFS: too quiet for the "
                        "console's speakers\n", dir, names[i], loud, SPEAKER_LEVEL);
        } else {
            fprintf(stderr, "%s/%s: missing, or not 16-bit PCM, mono or stereo, 8000 to 48000 Hz\n", dir, names[i]);
        }
        total += size;
        free(file);
    }
    /* the working loop: it must loop by itself and stay a short pattern */
    total += check_stream(dir, "working.bcstm", 1, 8.0);

    size_t credits_size = 0;
    char *credits = (char *)slurp(dir, "CREDITS.md", &credits_size);
    CHECK(credits != NULL);
    if (credits && (credits = realloc(credits, credits_size + 1)))
        credits[credits_size] = 0;
    for (int i = 0; i < TRACK_COUNT; i++) {
        const track *t = &TRACK_LIST[i];
        total += check_stream(dir, t->file, 0, 600.0);
        const char *const facts[4] = {t->file, t->title, t->author, t->page};
        for (int k = 0; k < 4; k++) {
            int named = credits && strstr(credits, facts[k]) != NULL;
            CHECK(named);
            if (!named)
                fprintf(stderr, "%s/CREDITS.md does not name \"%s\" of the track %s\n", dir, facts[k], t->file);
        }
    }
    free(credits);
    printf("%s: %zu bytes of sound in the application (%d tracks)\n", dir, total, TRACK_COUNT);
    CHECK(total <= 1700u * 1024);                   /* sounds, loop and all tracks stay under 1700 KiB */
}

/* ---------------------------------------------------------------- the track table (tracks.c) */

static void test_tracks(void)
{
    CHECK(TRACK_COUNT >= 1);
    CHECK_EQ(track_find("somewhere-in-the-elevator.bcstm"), TRACK_DEFAULT);     /* the default the user chose */
    CHECK_EQ(track_find(""), -1);
    CHECK_EQ(track_find("somewhere-in-the-elevator"), -1);
    CHECK_EQ(track_find("no-such-track.bcstm"), -1);
    for (int i = 0; i < TRACK_COUNT; i++) {
        const track *t = &TRACK_LIST[i];
        CHECK(t->file[0] && t->title[0] && t->author[0] && t->licence[0] && !strncmp(t->page, "https://", 8));
        CHECK(strstr(t->licence, "CC0") != NULL);           /* the only licence a track is taken under */
        CHECK_EQ(track_find(t->file), i);                   /* no file twice */
    }
}

/* ---------------------------------------------------------------- the working loop's timing (work.c) */

/* steps of 5 ms from *now to until, as the audio thread makes them; returns the events seen, START before STOP */
static int steps(work_state *w, uint64_t *now, uint64_t until, int enabled, int *starts, int *stops)
{
    int seen = 0;
    for (; *now <= until; *now += 5) {
        int e = work_step(w, *now, enabled);
        if (e == WORK_START)
            (*starts)++, seen = seen * 10 + 1;
        else if (e == WORK_STOP)
            (*stops)++, seen = seen * 10 + 2;
        CHECK(w->menu >= 0 && w->menu <= 1 && w->loop >= 0 && w->loop <= 1);
    }
    return seen;
}

static void test_work(void)
{
    work_state w;
    uint64_t now = 1000000;
    int starts = 0, stops = 0;
    work_reset(&w, now);
    CHECK(w.menu == 1.0f && w.loop == 0 && !w.sounding);
    steps(&w, &now, now + 1000, 1, &starts, &stops);
    CHECK(starts == 0 && stops == 0 && w.menu == 1.0f && w.loop == 0);     /* nothing under way: the menu music */

    /* a load shorter than the delay: nothing changes, nothing sounds */
    work_begin(&w, now);
    steps(&w, &now, now + WORK_DELAY_MS - 10, 1, &starts, &stops);
    CHECK(starts == 0 && w.menu == 1.0f && w.loop == 0);
    work_end(&w);
    steps(&w, &now, now + 2000, 1, &starts, &stops);
    CHECK(starts == 0 && stops == 0 && w.menu == 1.0f && w.loop == 0 && !w.sounding);

    /* music to loop to music: the loop starts once the delay has passed, comes in while the music goes out */
    uint64_t began = now;
    work_begin(&w, now);
    steps(&w, &now, began + WORK_DELAY_MS - 5, 1, &starts, &stops);
    CHECK(starts == 0 && w.menu == 1.0f);
    steps(&w, &now, began + WORK_DELAY_MS + 5, 1, &starts, &stops);
    CHECK(starts == 1 && w.sounding && w.loop < 0.2f && w.menu > 0.9f);
    steps(&w, &now, began + WORK_DELAY_MS + WORK_LOOP_FADE_MS + 10, 1, &starts, &stops);
    CHECK(w.loop == 1.0f && w.menu > 0 && w.menu < 1);                     /* the loop is in, the music on its way out */
    steps(&w, &now, began + WORK_DELAY_MS + WORK_FADE_MS + 10, 1, &starts, &stops);
    CHECK(w.loop == 1.0f && w.menu == 0);                                  /* faded out: audio.c pauses it here */
    steps(&w, &now, now + 60000, 1, &starts, &stops);
    CHECK(starts == 1 && stops == 0 && w.loop == 1.0f && w.menu == 0);     /* a long install: it stays */
    uint64_t ended = now;
    work_end(&w);
    steps(&w, &now, ended + WORK_LOOP_FADE_MS / 2, 1, &starts, &stops);
    CHECK(stops == 0 && w.loop > 0 && w.loop < 1 && w.menu > 0 && w.menu < 1);
    steps(&w, &now, ended + WORK_LOOP_FADE_MS + 10, 1, &starts, &stops);
    CHECK(stops == 1 && !w.sounding && w.loop == 0);                       /* the loop is out and stopped */
    steps(&w, &now, ended + WORK_FADE_MS + 10, 1, &starts, &stops);
    CHECK(starts == 1 && stops == 1 && w.menu == 1.0f);                    /* the music is back */

    /* an install that fails ends the same way: the end is the end (the error sound is main.c's) */
    work_begin(&w, now);
    steps(&w, &now, now + 3000, 1, &starts, &stops);
    work_end(&w);
    steps(&w, &now, now + 1000, 1, &starts, &stops);
    CHECK(starts == 2 && stops == 2 && w.menu == 1.0f && w.loop == 0 && w.depth == 0);

    /* work that follows work while the loop still sounds keeps it: a check right after a download */
    work_begin(&w, now);
    steps(&w, &now, now + 2000, 1, &starts, &stops);
    work_end(&w);
    work_begin(&w, now);
    steps(&w, &now, now + 2000, 1, &starts, &stops);
    CHECK(starts == 3 && stops == 2 && w.loop == 1.0f && w.menu == 0);
    work_end(&w);
    steps(&w, &now, now + 40, 1, &starts, &stops);                         /* 40 ms into its fade out */
    work_begin(&w, now);
    steps(&w, &now, now + 1000, 1, &starts, &stops);
    CHECK(starts == 3 && stops == 2 && w.loop == 1.0f);                    /* no stop, no second start */
    /* work inside work: the loop lasts until the outer one ends */
    work_begin(&w, now);
    work_end(&w);
    steps(&w, &now, now + 1000, 1, &starts, &stops);
    CHECK(stops == 2 && w.loop == 1.0f && w.depth == 1);
    work_end(&w);
    work_end(&w);                                                          /* an end too many is not counted */
    CHECK_EQ(w.depth, 0);
    steps(&w, &now, now + 1000, 1, &starts, &stops);
    CHECK(starts == 3 && stops == 3 && w.menu == 1.0f);

    /* the interface sounds switched off: no loop, the menu music stays as it is */
    work_begin(&w, now);
    steps(&w, &now, now + 5000, 0, &starts, &stops);
    CHECK(starts == 3 && stops == 3 && w.menu == 1.0f && w.loop == 0 && !w.sounding);
    /* switched on while the work is under way: the loop comes in; switched off again: it goes, the music returns */
    steps(&w, &now, now + 600, 1, &starts, &stops);
    CHECK(starts == 4 && w.loop == 1.0f && w.menu == 0);
    steps(&w, &now, now + 600, 0, &starts, &stops);
    CHECK(stops == 4 && w.loop == 0 && w.menu == 1.0f && w.depth == 1);
    work_end(&w);

    /* a clock that stands still or jumps: levels stay in their range, one start and one stop */
    work_reset(&w, 50);
    starts = stops = 0;
    work_begin(&w, 50);
    CHECK_EQ(work_step(&w, 50, 1), WORK_NONE);
    CHECK_EQ(work_step(&w, 40, 1), WORK_NONE);                             /* a step back in time */
    CHECK_EQ(work_step(&w, 50 + WORK_DELAY_MS, 1), WORK_START);
    CHECK_EQ(work_step(&w, UINT64_MAX / 2, 1), WORK_NONE);
    CHECK(w.loop == 1.0f && w.menu == 0);
    work_end(&w);
    CHECK_EQ(work_step(&w, UINT64_MAX / 2 + 5, 1), WORK_NONE);
    CHECK_EQ(work_step(&w, UINT64_MAX - 1, 1), WORK_STOP);
    CHECK(w.loop == 0 && w.menu == 1.0f);
}

int main(int argc, char **argv)
{
    test_bcstm_streams();
    test_bcstm_refusals();
    test_bcstm_mutations();
    test_bcstm_wrap();
    test_wav();
    test_wav_mutations(6000);
    test_tracks();
    test_work();
    if (argc > 1)
        test_sounds(argv[1]);
    return check_summary("test_sound");
}
