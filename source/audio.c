// ndsp calls as in libctru 2.7.0 (include/3ds/ndsp/ndsp.h and channel.h, source/ndsp/ndsp.c and ndsp-channel.c in
// devkitpro/devkitarm:20260610): ndspInit loads the DSP firmware from sdmc:/3ds/dspfirm.cdc (or takes the one the
// Homebrew Launcher hands over) and answers "not found" without it; its own thread (priority 0x18) runs the sound
// frames, about 200 a second, and calls the frame callback after each; a channel plays the wave buffers queued on it
// one after the other from linear memory, in the format and at the rate set for it; sleep and the HOME Menu are
// libctru's business (its DSP hook stops and restarts the component).
//
// Channel 0 is the menu music, channel 1 a previewed theme's music, channel 2 the working loop, channels 3 to 6 the
// interface sounds. The three kinds of music are BCSTM streams decoded a buffer at a time (bcstm.c) by a
// thread of this file's own, one step above the main thread, so they go on while the main thread works through an
// install or a file. The same thread steps the working loop's timing (work.h) after every sound frame: when the
// loop starts and stops and how far the menu music has faded.
#include "audio.h"

#include <3ds.h>
#include <string.h>

#include "bcstm.h"
#include "sound_data.h"
#include "tracks.h"
#include "wav.h"
#include "work.h"

#define CHN_MENU 0
#define CHN_THEME 1
#define CHN_WORK 2
#define CHN_SFX 3
#define VOICES 4                /* interface sounds at once */
#define STREAM_BUFS 3
#define STREAM_FRAMES 4096      /* a buffer: 93 ms at 44100 Hz, 186 ms at 22050 Hz */
#define THREAD_STACK 0x4000

typedef struct sound {
    int16_t *pcm;               /* linear memory, NULL: this sound is silent */
    uint32_t frames, rate;
    uint8_t channels;
} sound;

typedef struct stream {
    int chn;
    float level;                /* its kind's level (AUDIO_*_LEVEL) */
    int again;                  /* a file without a loop starts over at its end (the menu music, the working loop) */
    int playing;
    float gain;                 /* the share of its level the channel is set to now; -1: not set since it started */
    int paused;                 /* the channel is paused; -1: not set since it started */
    bcstm_stream source;
    ndspWaveBuf buf[STREAM_BUFS];
    int16_t *pcm;               /* linear memory: STREAM_BUFS buffers of STREAM_FRAMES stereo frames */
} stream;

#define TRACK_BYTES(symbol, file, title, author, licence, page) {track_##symbol, track_##symbol##_end},
static const struct {
    const uint8_t *file, *end;
} TRACK_DATA[TRACK_COUNT] = {TRACKS(TRACK_BYTES)};

void (*audio_log)(const char *fmt, ...);
#define LOG_LINE(...) do { if (audio_log) audio_log(__VA_ARGS__); } while (0)

static int ready;               /* ndsp runs */
static volatile int sounds_on = 1;      /* the interface sounds and the working loop (the settings' "Töne") */
static sound sounds[SFX_COUNT];
static ndspWaveBuf voice[VOICES];
static int next_voice;
static stream menu = {.chn = CHN_MENU, .level = AUDIO_MENU_LEVEL, .again = 1};
static stream theme = {.chn = CHN_THEME, .level = AUDIO_THEME_LEVEL};
static stream working = {.chn = CHN_WORK, .level = AUDIO_WORK_LEVEL, .again = 1};
static work_state work;         /* when the working loop sounds and how far the menu music has faded */
static int no_loop;             /* the working loop could not be started: the menu music then stays as it is */
static int theme_on;            /* a preview holds the menu music back */
static Thread thread;
static LightEvent wake;
static LightLock lock;          /* the streams and the work's state, between the main thread and the audio thread */
static volatile int quit;

/* What a channel plays next: 16-bit PCM, mono or stereo, at rate. Sounds below the DSP's own rate (32728 Hz) are
 * brought up to it along straight lines between their samples: libctru's default, polyphase, leaves them as steps
 * (ndsp-channel.c, ndspiUpdateChn); above it polyphase is the better filter. */
static void set_format(int chn, int channels, uint32_t rate)
{
    ndspChnSetFormat(chn, channels == 2 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);
    ndspChnSetInterp(chn, (float)rate > NDSP_SAMPLE_RATE ? NDSP_INTERP_POLYPHASE : NDSP_INTERP_LINEAR);
    ndspChnSetRate(chn, (float)rate);
}

static void set_channel(int chn, int channels, uint32_t rate, float level)
{
    float mix[12] = {level, level};
    ndspChnReset(chn);
    set_format(chn, channels, rate);
    ndspChnSetMix(chn, mix);
}

/* Queues the stream's next samples on every buffer the DSP is done with. Called with the lock held. */
static void feed(stream *s)
{
    for (int i = 0; s->playing && i < STREAM_BUFS; i++) {
        ndspWaveBuf *b = &s->buf[i];
        if (b->status == NDSP_WBUF_QUEUED || b->status == NDSP_WBUF_PLAYING)
            continue;
        int16_t *pcm = s->pcm + i * STREAM_FRAMES * 2;
        size_t n = bcstm_read(&s->source, pcm, STREAM_FRAMES, 1);
        if (!n && s->again) {
            bcstm_rewind(&s->source);
            n = bcstm_read(&s->source, pcm, STREAM_FRAMES, 1);
        }
        if (!n) {
            s->playing = 0;     /* a theme's music without a loop has played, as in the HOME Menu */
            break;
        }
        memset(b, 0, sizeof *b);
        b->data_pcm16 = pcm;
        b->nsamples = (u32)n;
        DSP_FlushDataCache(pcm, (u32)(n * s->source.channels * sizeof(int16_t)));
        ndspChnWaveBufAdd(s->chn, b);
    }
}

/* A stream's channel at gain times its level, paused or not; only what changed is sent. Lock held. */
static void set_level(stream *s, float gain, int paused)
{
    if (gain != s->gain) {
        float mix[12] = {s->level * gain, s->level * gain};
        ndspChnSetMix(s->chn, mix);
        s->gain = gain;
    }
    if (paused != s->paused) {
        ndspChnSetPaused(s->chn, paused);
        s->paused = paused;
    }
}

/* The menu music and the working loop as the work's state and a preview want them: the menu music at its faded
 * level, paused while a preview shows a theme and while it is faded out (so it goes on later where it was). Lock
 * held. */
static void levels(void)
{
    set_level(&menu, work.menu, theme_on || work.menu <= 0);
    set_level(&working, work.loop, 0);
}

static int start(stream *s, const uint8_t *bcstm, size_t size);
static void stop(stream *s);

static void audio_thread(void *arg)
{
    (void)arg;
    while (!quit) {
        LightEvent_Wait(&wake);
        LightLock_Lock(&lock);
        int event = work_step(&work, osGetTime(), sounds_on && !no_loop);
        if (event == WORK_START)
            no_loop = start(&working, snd_working, (size_t)(snd_working_end - snd_working)) != 0;
        else if (event == WORK_STOP)
            stop(&working);
        levels();
        feed(&menu);
        feed(&theme);
        feed(&working);
        LightLock_Unlock(&lock);
    }
}

/* After every sound frame, on ndsp's thread: the audio thread looks for buffers to fill. */
static void on_frame(void *data)
{
    (void)data;
    LightEvent_Signal(&wake);
}

/* Starts a stream on its channel: the file's header read, the channel set to its format, silent until set_level
 * says how loud. Lock held. */
static int start(stream *s, const uint8_t *bcstm, size_t size)
{
    if (!s->pcm || bcstm_open(&s->source, bcstm, size))
        return -1;
    set_channel(s->chn, s->source.channels, s->source.rate, 0);
    s->gain = 0;
    s->paused = 0;
    memset(s->buf, 0, sizeof s->buf);
    s->playing = 1;
    return 0;
}

static void stop(stream *s)
{
    s->playing = 0;
    ndspChnWaveBufClear(s->chn);
}

static int load_sound(sound *s, const uint8_t *file, size_t size)
{
    wav_info w;
    if (wav_read(file, size, &w))
        return -1;
    size_t bytes = (size_t)w.frames * w.channels * sizeof(int16_t);
    if (!(s->pcm = linearAlloc(bytes)))
        return -1;
    memcpy(s->pcm, w.data, bytes);
    DSP_FlushDataCache(s->pcm, (u32)bytes);
    s->frames = w.frames;
    s->rate = w.rate;
    s->channels = w.channels;
    return 0;
}

void audio_init(void)
{
    static const struct {
        const uint8_t *file, *end;
    } FILES[SFX_COUNT] = {
        [SFX_MOVE] = {snd_move, snd_move_end},       [SFX_CONFIRM] = {snd_confirm, snd_confirm_end},
        [SFX_BACK] = {snd_back, snd_back_end},       [SFX_POPUP] = {snd_popup, snd_popup_end},
        [SFX_ERROR] = {snd_error, snd_error_end},    [SFX_DONE] = {snd_done, snd_done_end},
        [SFX_DROP] = {snd_drop, snd_drop_end},
    };
    Result res = ndspInit();
    if (R_FAILED(res)) {
        LOG_LINE("sound: off, ndspInit answered %08lX%s", (unsigned long)res,
                 res == MAKERESULT(RL_PERMANENT, RS_NOTFOUND, RM_DSP, RD_NOT_FOUND)
                     ? " (the DSP firmware is not on the card: sdmc:/3ds/dspfirm.cdc)" : "");
        return;
    }
    ready = 1;
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    int loaded = 0;
    for (int i = 0; i < SFX_COUNT; i++)
        loaded += !load_sound(&sounds[i], FILES[i].file, (size_t)(FILES[i].end - FILES[i].file));
    for (int v = 0; v < VOICES; v++)
        set_channel(CHN_SFX + v, 1, 32000, AUDIO_SFX_LEVEL);
    LightLock_Init(&lock);
    LightEvent_Init(&wake, RESET_ONESHOT);
    menu.pcm = linearAlloc(STREAM_BUFS * STREAM_FRAMES * 2 * sizeof(int16_t));
    theme.pcm = linearAlloc(STREAM_BUFS * STREAM_FRAMES * 2 * sizeof(int16_t));
    working.pcm = linearAlloc(STREAM_BUFS * STREAM_FRAMES * 2 * sizeof(int16_t));
    work_reset(&work, osGetTime());
    s32 prio = 0x30;
    svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
    thread = threadCreate(audio_thread, NULL, THREAD_STACK, prio > 0x19 ? prio - 1 : prio, -2, false);
    if (thread)
        ndspSetCallback(on_frame, NULL);
    LOG_LINE("sound: on, %d of %d interface sounds, music and working loop %s", loaded, SFX_COUNT,
             thread && menu.pcm && working.pcm ? "ready" : "off (no thread or no memory)");
}

void audio_exit(void)
{
    if (!ready)
        return;
    ready = 0;
    if (thread) {
        quit = 1;
        LightEvent_Signal(&wake);
        threadJoin(thread, U64_MAX);
        threadFree(thread);
        thread = NULL;
    }
    /* the callback stays set: ndsp's thread reads it without a lock, and all it does is signal an event that
     * stays where it is */
    for (int chn = 0; chn < CHN_SFX + VOICES; chn++)
        ndspChnWaveBufClear(chn);
    ndspExit();
    for (int i = 0; i < SFX_COUNT; i++)
        if (sounds[i].pcm)
            linearFree(sounds[i].pcm);
    if (menu.pcm)
        linearFree(menu.pcm);
    if (theme.pcm)
        linearFree(theme.pcm);
    if (working.pcm)
        linearFree(working.pcm);
}

int audio_ready(void)
{
    return ready;
}

void sfx(int id)
{
    if (!ready || !sounds_on || id < 0 || id >= SFX_COUNT || !sounds[id].pcm)
        return;
    int v = next_voice, chn = CHN_SFX + v;
    next_voice = (next_voice + 1) % VOICES;
    ndspChnWaveBufClear(chn);   /* its last sound, if it still plays, ends here */
    set_format(chn, sounds[id].channels, sounds[id].rate);
    memset(&voice[v], 0, sizeof voice[v]);
    voice[v].data_pcm16 = sounds[id].pcm;
    voice[v].nsamples = sounds[id].frames;
    ndspChnWaveBufAdd(chn, &voice[v]);
}

void audio_sounds(int on)
{
    sounds_on = on != 0;        /* the audio thread's next step ends a working loop that sounds */
}

void audio_menu(int which)
{
    if (!ready || !thread)
        return;
    LightLock_Lock(&lock);
    stop(&menu);
    if (which >= 0 && which < TRACK_COUNT &&
        start(&menu, TRACK_DATA[which].file, (size_t)(TRACK_DATA[which].end - TRACK_DATA[which].file)))
        LOG_LINE("sound: %s is not a stream the player reads", TRACK_LIST[which].file);
    levels();
    LightLock_Unlock(&lock);
    LightEvent_Signal(&wake);
}

void audio_work(int on)
{
    if (!ready || !thread)
        return;
    LightLock_Lock(&lock);
    if (on)
        work_begin(&work, osGetTime());
    else
        work_end(&work);
    LightLock_Unlock(&lock);
}

int audio_theme_play(const uint8_t *bcstm, size_t size)
{
    if (!ready || !thread)
        return bcstm ? -1 : 0;
    LightLock_Lock(&lock);
    stop(&theme);
    theme_on = 1;
    int r = bcstm ? start(&theme, bcstm, size) : 0;
    if (!r && bcstm)
        set_level(&theme, 1.0f, 0);
    levels();
    LightLock_Unlock(&lock);
    LightEvent_Signal(&wake);
    return r;
}

void audio_theme_stop(void)
{
    if (!ready || !thread)
        return;
    LightLock_Lock(&lock);
    stop(&theme);               /* under the lock: once this returns the audio thread no longer reads the file */
    theme_on = 0;
    levels();
    LightLock_Unlock(&lock);
}
