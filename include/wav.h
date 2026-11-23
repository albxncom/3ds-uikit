/* A WAV reader for the interface sounds, in plain C so the host tests run it: RIFF/WAVE with 16-bit PCM, mono or
 * stereo, 8 to 48 kHz. No allocation; the file is hostile bytes, the result points into it. */
#ifndef WAV_H
#define WAV_H

#include <stddef.h>
#include <stdint.h>

typedef struct wav_info {
    const uint8_t *data;        /* the samples inside the file: signed 16-bit little-endian, interleaved */
    uint32_t frames;            /* sample frames */
    uint32_t rate;              /* Hz */
    uint8_t channels;           /* 1 or 2 */
} wav_info;

/* 0, or -1 when the file is not RIFF/WAVE with plain 16-bit PCM this reader accepts. Chunks come in any order, but
 * "fmt " before "data"; unknown ones are skipped. A data chunk whose size runs past the file is cut to the file. */
int wav_read(const uint8_t *file, size_t size, wav_info *out);

#endif
