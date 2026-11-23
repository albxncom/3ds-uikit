/* A streaming decoder for a BCSTM (CTR stream, a theme's bgm.bcstm) held in memory: PCM8, PCM16 and DSP-ADPCM, one
 * or two channels, with its loop. Format: 3dbrew, "BCSTM". No allocation; the file is hostile bytes, checked once
 * by bcstm_open so that no sample byte can make bcstm_read leave it.
 *
 * Data layout: the stream is cut into blocks; block b holds, channel after channel, block size bytes per channel
 * with block samples frames. The last block holds last samples frames in last size bytes per channel, each
 * channel's part padded to last padded size (the very last part may end at its unpadded size). */
#ifndef BCSTM_H
#define BCSTM_H

#include <stddef.h>
#include <stdint.h>

enum { BCSTM_PCM8, BCSTM_PCM16, BCSTM_ADPCM };

typedef struct bcstm_stream {
    uint8_t encoding;           /* 0 PCM8, 1 PCM16, 2 DSP-ADPCM */
    uint8_t loops;              /* the file's loop flag, cleared when its loop points are unusable */
    uint8_t channels;           /* 1 or 2 */
    uint32_t rate;              /* Hz */
    uint32_t samples;           /* frames in the stream (a frame: one sample per channel) */
    uint32_t loop_start, loop_end;  /* loop_start < loop_end <= samples */
    uint32_t pos;               /* the next frame bcstm_read decodes */
    /* private */
    const uint8_t *data;        /* block 0, channel 0 */
    uint32_t blocks, block_size, block_samples, last_padded;
    int16_t coef[2][16];        /* per channel: 8 pairs, the pair a frame's predictor index picks */
    int16_t start[2][2], hist[2][2], loop_hist[2][2];  /* per channel the last two samples (newest first): before
                                                          frame 0, now, before loop_start */
    uint8_t loop_saved;         /* loop_hist holds the history at loop_start */
} bcstm_stream;

/* 0, or -1 when this is not a BCSTM the decoder accepts. Keeps the pointer: the file must stay while the stream is
 * used. */
int bcstm_open(bcstm_stream *s, const uint8_t *file, size_t size);
/* Decodes up to `frames` frames from pos as interleaved signed 16-bit samples. With loop != 0 and a file that loops,
 * loop_end is followed by loop_start for ever; otherwise the stream ends at `samples`. Returns the frames written
 * (fewer than asked only at the end; 0 from then on). */
size_t bcstm_read(bcstm_stream *s, int16_t *out, size_t frames, int loop);
void bcstm_rewind(bcstm_stream *s);    /* back to frame 0 */

#endif
