/* A streaming decoder for a BCSTM held in memory (bcstm.h). */
#include "bcstm.h"

#include <string.h>

static inline uint16_t get_le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static inline uint32_t get_le32(const uint8_t *p) { return get_le16(p) | (uint32_t)get_le16(p + 2) << 16; }

#define REF_INFO 0x4000u
#define REF_SEEK 0x4001u
#define REF_DATA 0x4002u
#define REF_TABLE 0x0101u
#define REF_DSP_INFO 0x0300u
#define REF_SAMPLES 0x1F00u
#define REF_STREAM 0x4100u
#define REF_CHANNEL 0x4102u
#define NULL_OFFSET 0xFFFFFFFFu
#define STREAM_INFO_SIZE 0x38u
#define DSP_INFO_SIZE 0x2Cu      /* coefficients, context, loop context; the padding after them is not needed */
#define FRAME_BYTES 8u
#define FRAME_SAMPLES 14u

static int16_t get_s16(const uint8_t *p)
{
    int v = get_le16(p);
    return (int16_t)(v - (v & 0x8000) * 2);
}

/* The bytes n frames of one channel take. A DSP-ADPCM block may end inside an ADPCM frame: its header and the
 * nibbles up to the last sample are there, nothing after them. */
static uint64_t bytes_for(unsigned encoding, uint64_t n)
{
    if (encoding == BCSTM_PCM8)
        return n;
    if (encoding == BCSTM_PCM16)
        return n * 2;
    uint64_t rest = n % FRAME_SAMPLES;
    return n / FRAME_SAMPLES * FRAME_BYTES + (rest ? 1 + (rest + 1) / 2 : 0);
}

/* A reference (type, padding, offset) at `at` of type `type`, its offset relative to base, pointing to `need` bytes
 * that end by `end`; their position goes to *where. */
static int follow(const uint8_t *f, uint64_t at, unsigned type, uint64_t base, uint64_t need, uint64_t end,
                  uint64_t *where)
{
    if (at + 8 > end || get_le16(f + at) != type)
        return -1;
    uint32_t offset = get_le32(f + at + 4);
    if (offset == NULL_OFFSET || base + offset + need > end)
        return -1;
    *where = base + offset;
    return 0;
}

/* A block that the header's sized reference at `at` names: the reference and the block's own header both inside the
 * file, the magic as its type says; its end (by the block header's size) goes to *end. */
static int block(const uint8_t *f, size_t size, const uint8_t *ref, const char *magic, uint64_t *start, uint64_t *end)
{
    uint32_t offset = get_le32(ref + 4), ref_size = get_le32(ref + 8);
    if ((uint64_t)offset + 8 > size || ref_size < 8 || (uint64_t)offset + ref_size > size)
        return -1;
    uint32_t own = get_le32(f + offset + 4);
    if (memcmp(f + offset, magic, 4) || own < 8 || (uint64_t)offset + own > size)
        return -1;
    *start = offset;
    *end = (uint64_t)offset + own;
    return 0;
}

int bcstm_open(bcstm_stream *s, const uint8_t *f, size_t size)
{
    memset(s, 0, sizeof *s);
    if (size < 0x14 || memcmp(f, "CSTM", 4) || get_le16(f + 4) != 0xFEFF)
        return -1;
    unsigned count = get_le16(f + 0x10);
    if (0x14 + (uint64_t)count * 12 > size)
        return -1;
    /* Found by type, in any order; the seek table may be missing (the decoder does not use it). */
    static const char *const MAGIC[3] = {"INFO", "SEEK", "DATA"};
    uint64_t start[3] = {0, 0, 0}, end[3] = {0, 0, 0};
    int seen[3] = {0, 0, 0};
    for (unsigned i = 0; i < count; i++) {
        const uint8_t *ref = f + 0x14 + 12 * i;
        unsigned type = get_le16(ref);
        if (type < REF_INFO || type > REF_DATA)
            continue;
        unsigned k = type - REF_INFO;
        if (seen[k]++)
            return -1;
        if (type == REF_SEEK && get_le32(ref + 4) == NULL_OFFSET)
            continue;
        if (block(f, size, ref, MAGIC[k], &start[k], &end[k]))
            return -1;
    }
    if (!seen[0] || !seen[2])
        return -1;
    uint64_t info = start[0], info_end = end[0], data = start[2], data_end = end[2];

    uint64_t si, table, samples_at;
    if (follow(f, info + 8, REF_STREAM, info + 8, STREAM_INFO_SIZE, info_end, &si) ||
        follow(f, info + 0x18, REF_TABLE, info + 8, 4, info_end, &table))
        return -1;
    const uint8_t *st = f + si;
    unsigned encoding = st[0], channels = st[2];
    uint32_t rate = get_le32(st + 4), blocks = get_le32(st + 0x10), block_size = get_le32(st + 0x14);
    uint32_t block_samples = get_le32(st + 0x18), last_size = get_le32(st + 0x1C), last_samples = get_le32(st + 0x20);
    uint32_t last_padded = get_le32(st + 0x24);
    if (encoding > BCSTM_ADPCM || channels < 1 || channels > 2 || rate < 1 || rate > 192000 || blocks < 1 ||
        block_size == 0 || block_samples == 0 || last_samples == 0 || last_samples > block_samples ||
        bytes_for(encoding, block_samples) > block_size || bytes_for(encoding, last_samples) > last_size ||
        (channels == 2 && last_padded < last_size))
        return -1;
    /* The parts of the sum below are held against the file before they are multiplied: 32-bit values the file
     * chose (4294836227 blocks of 2147549185 bytes, two channels) would otherwise wrap the 64-bit sum to a few
     * bytes and pass. So bounded, the sum stays below three times the file's size. A stream of one block may name
     * a block size larger than the file: only its last block's sizes count then. */
    uint64_t per_block = (uint64_t)block_size * channels;
    if (blocks - 1 > size / per_block || last_size > size || (channels == 2 && last_padded > size))
        return -1;
    uint64_t samples = (uint64_t)(blocks - 1) * block_samples + last_samples;
    uint64_t need = (uint64_t)(blocks - 1) * block_size * channels + (uint64_t)(channels - 1) * last_padded + last_size;
    if (samples > UINT32_MAX || follow(f, si + 0x30, REF_SAMPLES, data + 8, need, data_end, &samples_at))
        return -1;

    if (get_le32(f + table) < channels)
        return -1;
    for (unsigned c = 0; c < channels; c++) {
        uint64_t ci, dsp;
        if (follow(f, table + 4 + 8 * c, REF_CHANNEL, table, 8, info_end, &ci))
            return -1;
        if (encoding != BCSTM_ADPCM)
            continue;
        if (follow(f, ci, REF_DSP_INFO, ci, DSP_INFO_SIZE, info_end, &dsp))
            return -1;
        for (int k = 0; k < 16; k++)
            s->coef[c][k] = get_s16(f + dsp + 2 * k);
        /* The context's predictor and scale byte repeats the first frame's header; only the history is needed. */
        s->start[c][0] = get_s16(f + dsp + 0x22);
        s->start[c][1] = get_s16(f + dsp + 0x24);
    }

    s->encoding = (uint8_t)encoding;
    s->channels = (uint8_t)channels;
    s->rate = rate;
    s->samples = (uint32_t)samples;
    s->loop_start = get_le32(st + 8);
    s->loop_end = get_le32(st + 0xC);
    s->loops = st[1] != 0;
    if (s->loop_start >= s->loop_end || s->loop_end > s->samples) {
        s->loops = 0;
        s->loop_start = 0;
        s->loop_end = s->samples;
    }
    s->data = f + samples_at;
    s->blocks = blocks;
    s->block_size = block_size;
    s->block_samples = block_samples;
    s->last_padded = last_padded;
    bcstm_rewind(s);
    return 0;
}

void bcstm_rewind(bcstm_stream *s)
{
    s->pos = 0;
    memcpy(s->hist, s->start, sizeof s->hist);
}

/* n samples of one channel from sample `at` of its part of a block, to every stride-th element of out. The frame
 * header is read again for every sample, so a run may start anywhere in an ADPCM frame. */
static void adpcm(const int16_t *coef, int16_t *hist, const uint8_t *p, uint32_t at, uint32_t n, int16_t *out,
                  unsigned stride)
{
    p += at / FRAME_SAMPLES * FRAME_BYTES;
    unsigned k = at % FRAME_SAMPLES;
    int64_t h1 = hist[0], h2 = hist[1];
    for (uint32_t j = 0; j < n; j++) {
        unsigned predictor = p[0] >> 4 & 7, scale = p[0] & 0xF;
        int nibble = p[1 + k / 2] >> (k & 1 ? 0 : 4) & 0xF;
        if (nibble >= 8)
            nibble -= 16;
        /* 64 bits: the sum of two products of 16-bit values can pass 2^31 with hostile coefficients, and
         * multiplying instead of shifting keeps the negative nibble defined. */
        int64_t v = (int64_t)nibble * ((int64_t)2048 << scale) + 1024 + coef[2 * predictor] * h1 +
                    coef[2 * predictor + 1] * h2;
        v >>= 11;
        if (v > 32767)
            v = 32767;
        else if (v < -32768)
            v = -32768;
        h2 = h1;
        h1 = v;
        out[(size_t)j * stride] = (int16_t)v;
        if (++k == FRAME_SAMPLES) {
            k = 0;
            p += FRAME_BYTES;
        }
    }
    hist[0] = (int16_t)h1;
    hist[1] = (int16_t)h2;
}

static void decode(bcstm_stream *s, uint32_t block_no, uint32_t at, uint32_t n, int16_t *out)
{
    unsigned ch = s->channels;
    size_t part = block_no + 1 == s->blocks ? s->last_padded : s->block_size;
    const uint8_t *b = s->data + (size_t)block_no * s->block_size * ch;
    for (unsigned c = 0; c < ch; c++) {
        const uint8_t *p = b + c * part;
        int16_t *o = out + c;
        if (s->encoding == BCSTM_ADPCM)
            adpcm(s->coef[c], s->hist[c], p, at, n, o, ch);
        else if (s->encoding == BCSTM_PCM16)
            for (uint32_t j = 0; j < n; j++)
                o[(size_t)j * ch] = get_s16(p + 2 * ((size_t)at + j));
        else
            for (uint32_t j = 0; j < n; j++)
                o[(size_t)j * ch] = (int16_t)(((p[at + j] ^ 0x80) - 0x80) * 256);
    }
}

size_t bcstm_read(bcstm_stream *s, int16_t *out, size_t frames, int loop)
{
    int looping = loop && s->loops;
    size_t done = 0;
    while (done < frames) {
        /* Past loop_end only when an earlier read did not loop; the history at loop_start is saved by then,
         * because every read stops at loop_start once to save it. */
        if (looping && s->pos >= s->loop_end) {
            s->pos = s->loop_start;
            memcpy(s->hist, s->loop_hist, sizeof s->hist);
        }
        if (s->pos >= s->samples)
            break;
        if (s->loops && !s->loop_saved && s->pos == s->loop_start) {
            memcpy(s->loop_hist, s->hist, sizeof s->hist);
            s->loop_saved = 1;
        }
        uint32_t end = looping ? s->loop_end : s->samples;
        if (s->loops && !s->loop_saved && s->pos < s->loop_start)
            end = s->loop_start;
        uint32_t block_no = s->pos / s->block_samples, at = s->pos % s->block_samples;
        uint32_t n = end - s->pos;
        if (n > s->block_samples - at)
            n = s->block_samples - at;
        if (n > frames - done)
            n = (uint32_t)(frames - done);
        decode(s, block_no, at, n, out + done * s->channels);
        s->pos += n;
        done += n;
    }
    return done;
}
