#include "wav.h"

#include <string.h>

#define FORMAT_PCM 1u
#define FORMAT_EXTENSIBLE 0xFFFEu

static uint32_t le16(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }
static uint32_t le32(const uint8_t *p) { return le16(p) | le16(p + 2) << 16; }

/* The format chunk's fields this reader needs; 0 when it describes 16-bit PCM in its range. */
static int format(const uint8_t *p, uint32_t size, wav_info *out)
{
    if (size < 16)
        return -1;
    uint32_t tag = le16(p), channels = le16(p + 2), rate = le32(p + 4), align = le16(p + 12), bits = le16(p + 14);
    /* Extensible: the sub-format GUID at 24 starts with the plain format tag. */
    if (tag == FORMAT_EXTENSIBLE && size >= 40)
        tag = le16(p + 24);
    if (tag != FORMAT_PCM || channels < 1 || channels > 2 || rate < 8000 || rate > 48000 || bits != 16 ||
        align != channels * 2)
        return -1;
    out->channels = (uint8_t)channels;
    out->rate = rate;
    return 0;
}

int wav_read(const uint8_t *file, size_t size, wav_info *out)
{
    memset(out, 0, sizeof *out);
    if (size < 12 || memcmp(file, "RIFF", 4) || memcmp(file + 8, "WAVE", 4))
        return -1;
    int have_format = 0;
    uint64_t at = 12;
    while (at + 8 <= size) {
        const uint8_t *chunk = file + at;
        uint64_t length = le32(chunk + 4), body = at + 8;
        if (!memcmp(chunk, "data", 4)) {
            if (!have_format)
                return -1;
            if (length > size - body)
                length = size - body;
            uint64_t frames = length / (out->channels * 2u);
            if (frames == 0)
                return -1;
            if (frames > UINT32_MAX)
                frames = UINT32_MAX;
            out->data = file + body;
            out->frames = (uint32_t)frames;
            return 0;
        }
        if (length > size - body)
            return -1;
        if (!memcmp(chunk, "fmt ", 4)) {
            if (format(file + body, (uint32_t)length, out))
                return -1;
            have_format = 1;
        }
        at = body + length + (length & 1);
    }
    return -1;
}
