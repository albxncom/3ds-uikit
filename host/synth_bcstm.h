/* A synthetic BCSTM for the host tests, written from the format documentation (3dbrew "BCSTM"): a small one
 * that plays, and the little-endian writes it is built with. Nothing here comes from a real file. */
#ifndef SYNTH_BCSTM_H
#define SYNTH_BCSTM_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define SYNTH_BCSTM_MAX(samples) (0x100u + 2u * (samples))

static inline void synth_le16(uint8_t *p, unsigned v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static inline void synth_le32(uint8_t *p, uint32_t v) { synth_le16(p, v & 0xFFFF); synth_le16(p + 2, v >> 16); }

/* Sample i of the synthetic music. */
static inline int16_t synth_sample(uint32_t i)
{
    return (int16_t)(i * 37u % 20001u) - 10000;
}

/* A BCSTM that plays: 16-bit PCM, mono, 8000 Hz, `samples` samples in one block, looping from loop_start to its
 * end, without a seek block. out holds SYNTH_BCSTM_MAX(samples) bytes. Returns its size. */
static inline size_t synth_bcstm(uint8_t *out, uint32_t samples, uint32_t loop_start)
{
    const uint32_t info = 0x40, info_size = 0x80, data = info + info_size, bytes = 2 * samples;
    uint32_t data_size = (8 + 0x18 + bytes + 31) / 32 * 32, size = data + data_size;
    memset(out, 0, size);
    memcpy(out, "CSTM", 4);
    synth_le16(out + 4, 0xFEFF);
    synth_le16(out + 6, 0x40);
    synth_le32(out + 8, 0x02000000);
    synth_le32(out + 0xC, size);
    synth_le16(out + 0x10, 2);
    synth_le16(out + 0x14, 0x4000);             /* the INFO block */
    synth_le32(out + 0x18, info);
    synth_le32(out + 0x1C, info_size);
    synth_le16(out + 0x20, 0x4002);             /* the DATA block */
    synth_le32(out + 0x24, data);
    synth_le32(out + 0x28, data_size);
    uint8_t *i = out + info, *st = i + 0x20;
    memcpy(i, "INFO", 4);
    synth_le32(i + 4, info_size);
    synth_le16(i + 8, 0x4100);                  /* the stream's numbers, 0x18 after this reference */
    synth_le32(i + 0xC, 0x18);
    synth_le32(i + 0x14, 0xFFFFFFFFu);          /* no track table */
    synth_le16(i + 0x18, 0x0101);               /* the channel table, 0x50 after the first reference */
    synth_le32(i + 0x1C, 0x50);
    st[0] = 1;                                  /* PCM16 */
    st[1] = 1;                                  /* loops */
    st[2] = 1;                                  /* one channel */
    synth_le32(st + 4, 8000);
    synth_le32(st + 8, loop_start);
    synth_le32(st + 0xC, samples);
    synth_le32(st + 0x10, 1);                   /* one block */
    synth_le32(st + 0x14, bytes);
    synth_le32(st + 0x18, samples);
    synth_le32(st + 0x1C, bytes);
    synth_le32(st + 0x20, samples);
    synth_le32(st + 0x24, bytes);
    synth_le16(st + 0x30, 0x1F00);              /* the samples, 0x18 into the DATA block's data */
    synth_le32(st + 0x34, 0x18);
    uint8_t *table = i + 8 + 0x50;
    synth_le32(table, 1);
    synth_le16(table + 4, 0x4102);              /* the channel's entry, 0xC after the table's count */
    synth_le32(table + 8, 0xC);
    synth_le32(table + 0xC + 4, 0xFFFFFFFFu);   /* PCM: no ADPCM values */
    uint8_t *d = out + data;
    memcpy(d, "DATA", 4);
    synth_le32(d + 4, data_size);
    for (uint32_t k = 0; k < samples; k++)
        synth_le16(d + 8 + 0x18 + 2 * k, (uint16_t)synth_sample(k));
    return size;
}

#endif
