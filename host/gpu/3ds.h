/* For the kit's host tests (host/test_kit_bounds.py and the others that compile the kit alone) and an application's
 * UI tests: the flows' stand-in for libctru (tools/flows/fake/3ds.h: the keys, the touch screen, the clock, the
 * threads and the graphics calls) and what ui.c and an application's interface take besides. */
#ifndef GPU_3DS_H
#define GPU_3DS_H
#include "../../tools/flows/fake/3ds.h"
/* What audio.c takes besides (ndsp, the lock and the event, linear memory), declared as audio.c uses it, for
 * the kit's compile-alone check (host/test_kit_bounds.py): nothing links against these, and the constants' values
 * are placeholders, not libctru's. */
typedef s32 LightLock;
typedef struct { s32 state; LightLock lock; } LightEvent;
typedef enum { RESET_ONESHOT, RESET_STICKY, RESET_PULSE } ResetType;
void LightLock_Init(LightLock *lock); void LightLock_Lock(LightLock *lock); void LightLock_Unlock(LightLock *lock);
void LightEvent_Init(LightEvent *event, ResetType reset); void LightEvent_Signal(LightEvent *event);
void LightEvent_Wait(LightEvent *event);
#define CUR_THREAD_HANDLE 0xFFFF8000
#define R_FAILED(res) ((Result)(res) < 0)
#define MAKERESULT(level, summary, module, description) \
    ((Result)((level) << 27 | (summary) << 21 | (module) << 10 | (description)))
enum { RL_PERMANENT = 1, RS_NOTFOUND, RM_DSP, RD_NOT_FOUND };
void *linearAlloc(size_t size); void linearFree(void *mem);
Result DSP_FlushDataCache(const void *adr, u32 size);
enum { NDSP_FORMAT_MONO_PCM16 = 1, NDSP_FORMAT_STEREO_PCM16 };
enum { NDSP_WBUF_FREE, NDSP_WBUF_QUEUED, NDSP_WBUF_PLAYING, NDSP_WBUF_DONE };
typedef enum { NDSP_INTERP_POLYPHASE, NDSP_INTERP_LINEAR, NDSP_INTERP_NONE } ndspInterpType;
typedef enum { NDSP_OUTPUT_MONO, NDSP_OUTPUT_STEREO, NDSP_OUTPUT_SURROUND } ndspOutputMode;
#define NDSP_SAMPLE_RATE 32728.0
typedef struct { int16_t *data_pcm16; u32 nsamples; u8 status; } ndspWaveBuf;
typedef void (*ndspCallback)(void *data);
Result ndspInit(void); void ndspExit(void); void ndspSetOutputMode(ndspOutputMode mode);
void ndspSetCallback(ndspCallback callback, void *data);
void ndspChnReset(int id); void ndspChnSetFormat(int id, u16 format); void ndspChnSetInterp(int id, ndspInterpType type);
void ndspChnSetRate(int id, float rate); void ndspChnSetMix(int id, float mix[12]);
void ndspChnSetPaused(int id, bool paused);
void ndspChnWaveBufAdd(int id, ndspWaveBuf *buf); void ndspChnWaveBufClear(int id);
#endif
