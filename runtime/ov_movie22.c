// Written for ocarina-of-time-recomp (MIT, Copyright (c) 2026 Jeff Aigner).
//
// SDK 22's movie library (subsdk0, movie 22.2.8) gives the game movie::VideoDecoder and movie::AudioDecoder (not the
// movie::Decoder of ov_movie.c, which this game never imports). Their Initialize opens the Switch 2's codec services
// through nn::codec (movie::detail::InitializeDecoderByRemoteMemory), which this host does not serve: the SDK aborts.
// The game calls the decoders through imports of main, so every public method it imports is replaced here by name,
// and the library's object is never touched (its constructor, destructor and Finalize are replaced as well).
//
// What stands in for now is a null decoder: the stream is taken and dropped, and the frame the game registered comes
// back once with its end-of-stream state set when the input's end-of-stream buffer arrives. The events the game
// waits on (input ready, output ready, format changed) are real nn::os system events, created and signalled through
// the SDK's own functions on the calling guest thread. Decoding on the host can be added on this protocol: fill the
// registered VideoFrame's planes with SetPixelPlaneData and its time stamp with SetTimeStamp before handing it back.
//
// Measured on OoT 1.1.1 (2026-11-05, build/logs/run-64.log): the title screen is a movie, "video/av01" (AV1)
// 1920x1080 at 60 fps, 321 samples per loop; the game sends the samples through DecodeAsync, an end-of-stream buffer
// (nn::movie::Buffer flag 4, as MediaCodec's BUFFER_FLAG_END_OF_STREAM) after the last, acquires the frames and
// starts over, so with this null decoder the title screen is black and the game waits there for a button. A host
// AV1 decoder (dav1d, BSD-2) is the next step. The game's protocol (main+0x3b5f0c4): C1, Initialize(work memory,
// size, mime type, output format, codec mode), Configure(MediaProperty, codec data, size), the three event getters,
// RegisterVideoFrame(frames, count), Start, then DecodeAsync(nn::movie::Buffer) per extracted sample and
// AcquireVideoFrame / ReleaseVideoFrame per output; Stop, Flush, Finalize, D1 at the end. The track's MediaProperty
// (read by the game before Configure through nn::movie::MediaProperty::Get*, observed below) carries "mime", "width",
// "height", "frame-rate", "color-standard", "color-range", "color-transfer".
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dispatch.h"
#include "host.h"

#define MAX_DECODERS 8
#define MAX_FRAMES 16
#define SYSEVENT_BYTES 0x100             // nn::os::SystemEventType is 0x28 bytes on SDK 22; room to spare
#define FLAG_END_OF_STREAM 4u
#define MOVIE_R_NO_FRAME (342u | (2u << 9))   // module 342 (movie): nothing to acquire

typedef struct {
    uint64_t guest;                      // the game's movie::VideoDecoder / AudioDecoder object (0: free)
    int video;
    int started, eos_pending, eos_done;
    uint8_t *ev_in, *ev_out, *ev_fmt;    // nn::os::SystemEventType objects, host memory the guest addresses directly
    uint64_t frames[MAX_FRAMES];
    int nframes;
    uint64_t decoded, acquired;
} Dec;

static Dec decs[MAX_DECODERS];
static SRWLOCK lock = SRWLOCK_INIT;
static struct { uint64_t create_event, signal_event, buffer_flags, video_eos, audio_eos; } G;
static volatile LONG g_ready;

static uint64_t need(const char *name) {
    uint64_t a = ldr_symbol(name);
    if (!a) rt_fatal("movie22: guest symbol %s is not loaded", name);
    return a;
}

static void guest_init(void) {
    if (g_ready) return;
    AcquireSRWLockExclusive(&lock);
    if (!g_ready) {
        G.create_event = need("_ZN2nn2os17CreateSystemEventEPNS0_15SystemEventTypeENS0_14EventClearModeEb");
        G.signal_event = need("_ZN2nn2os17SignalSystemEventEPNS0_15SystemEventTypeE");
        G.buffer_flags = need("_ZNK2nn5movie6Buffer14GetBufferFlagsEPNS0_11BufferFlagsE");
        G.video_eos = need("_ZN5movie10VideoFrame19SetEndOfStreamStateEb");
        G.audio_eos = need("_ZN5movie10AudioFrame19SetEndOfStreamStateEb");
        InterlockedExchange(&g_ready, 1);
    }
    ReleaseSRWLockExclusive(&lock);
}

#define U(x) ((uint64_t)(uintptr_t)(x))

// nn::os::CreateSystemEvent(event, EventClearMode_AutoClear, false), on the calling guest thread.
static uint8_t *event_new(Cpu *c) {
    uint8_t *e = calloc(1, SYSEVENT_BYTES);
    if (!e) rt_fatal("movie22: out of memory");
    rt_call(c, G.create_event, 3, U(e), 1ull, 0ull);
    return e;
}

static void event_signal(Cpu *c, const uint8_t *e) { if (e) rt_call(c, G.signal_event, 1, U(e)); }

static Dec *find(uint64_t guest) {
    for (int i = 0; i < MAX_DECODERS; i++)
        if (decs[i].guest == guest) return &decs[i];
    return NULL;
}

static Dec *open_dec(Cpu *c, uint64_t guest, int video) {
    guest_init();
    AcquireSRWLockExclusive(&lock);
    Dec *d = find(guest);
    if (!d) d = find(0);
    if (d) {
        uint8_t *in = d->ev_in, *out = d->ev_out, *fmt = d->ev_fmt;
        memset(d, 0, sizeof *d);
        d->guest = guest, d->video = video;
        d->ev_in = in, d->ev_out = out, d->ev_fmt = fmt;     // events of an earlier life stay valid
    }
    ReleaseSRWLockExclusive(&lock);
    if (!d) { rt_log("movie22: more than %d decoders alive", MAX_DECODERS); return NULL; }
    if (!d->ev_in) d->ev_in = event_new(c), d->ev_out = event_new(c), d->ev_fmt = event_new(c);
    return d;
}

static void ov_ctor(Cpu *c) { (void)c; }                       // the object's fields are never read here
static void ov_ok(Cpu *c) { ret_x(c, 0); }

static void init_common(Cpu *c, int video) {
    static volatile LONG said;
    uint64_t self = arg_x(c, 0), work = arg_x(c, 1), size = arg_x(c, 2), mime = arg_x(c, 3), fmt = arg_x(c, 4), mode = arg_x(c, 5);
    Dec *d = open_dec(c, self, video);   // its rt_calls clobber x0-x7: the arguments were read above
    if (InterlockedIncrement(&said) <= 2)
        rt_log("movie22: %s decoder %#llx (work memory %#llx, %llu bytes): a null decoder stands in (mime %llu, output format "
               "%llu, codec mode %llu); the cinematic ends at its first end-of-stream buffer", video ? "video" : "audio",
               (unsigned long long)self, (unsigned long long)work, (unsigned long long)size, (unsigned long long)mime,
               (unsigned long long)fmt, (unsigned long long)mode);
    ret_x(c, d ? 0 : MOVIE_R_NO_FRAME);
}
static void ov_video_init(Cpu *c) { init_common(c, 1); }
static void ov_audio_init(Cpu *c) { init_common(c, 0); }

static void ov_finalize(Cpu *c) {
    Dec *d = find(arg_x(c, 0));
    if (d) {
        rt_log("movie22: %s decoder %p finalized: %llu buffers taken, %llu frames handed back", d->video ? "video" : "audio",
               (void *)(uintptr_t)d->guest, (unsigned long long)d->decoded, (unsigned long long)d->acquired);
        AcquireSRWLockExclusive(&lock);
        d->guest = 0;
        ReleaseSRWLockExclusive(&lock);
    }
    ret_x(c, 0);
}
static void ov_dtor(Cpu *c) {
    Dec *d = find(arg_x(c, 0));
    if (d) { AcquireSRWLockExclusive(&lock); d->guest = 0; ReleaseSRWLockExclusive(&lock); }
}

// The event getters: SystemEventType **out.
static void event_get(Cpu *c, int which) {
    Dec *d = find(arg_x(c, 0));
    uint64_t *out = arg_p(c, 1);
    if (!d || !out) { ret_x(c, MOVIE_R_NO_FRAME); return; }
    *out = U(which == 0 ? d->ev_in : which == 1 ? d->ev_out : d->ev_fmt);
    ret_x(c, 0);
}
static void ov_event_in(Cpu *c) { event_get(c, 0); }
static void ov_event_out(Cpu *c) { event_get(c, 1); }
static void ov_event_fmt(Cpu *c) { event_get(c, 2); }

static void once(const char *what, const Dec *d) {   // DIAG: the first call of each kind, per decoder life
    static volatile LONG said[8];
    int i = what[0] == 'S' ? 0 : what[0] == 'D' ? 1 : what[0] == 'A' ? 2 : what[0] == 'R' ? 3 : 4;
    if (InterlockedIncrement(&said[i]) <= 4)
        rt_log("movie22: %s on %s decoder %#llx (%llu buffers so far)", what, d && d->video ? "video" : "audio",
               (unsigned long long)(d ? d->guest : 0), (unsigned long long)(d ? d->decoded : 0));
}

static void ov_start(Cpu *c) {
    Dec *d = find(arg_x(c, 0));
    once("Start", d);
    if (d) {
        d->started = 1;
        event_signal(c, d->ev_fmt);                          // the output format is known from the start
        event_signal(c, d->ev_in);                           // input is always welcome
    }
    ret_x(c, 0);
}

static void ov_register(Cpu *c) {                            // RegisterVideoFrame / RegisterAudioFrame(frames, count)
    Dec *d = find(arg_x(c, 0));
    const uint64_t *frames = arg_p(c, 1);
    int n = (int)arg_x(c, 2);
    once("Register", d);
    if (d && frames && n > 0) {
        AcquireSRWLockExclusive(&lock);
        d->nframes = 0;
        for (int i = 0; i < n && i < MAX_FRAMES; i++) if (frames[i]) d->frames[d->nframes++] = frames[i];
        ReleaseSRWLockExclusive(&lock);
    }
    ret_x(c, 0);
}

static void ov_decode(Cpu *c) {                              // DecodeAsync(const nn::movie::Buffer *)
    Dec *d = find(arg_x(c, 0));
    uint64_t buffer = arg_x(c, 1);
    if (!d) { ret_x(c, MOVIE_R_NO_FRAME); return; }
    uint32_t flags = 0;
    if (buffer) rt_call(c, G.buffer_flags, 2, buffer, U(&flags));
    if (d->decoded < 2 || (flags & FLAG_END_OF_STREAM)) {
        char what[48];
        snprintf(what, sizeof what, "DecodeAsync(flags %#x)", flags);
        once(what, d);
    }
    d->decoded++;
    if (flags & FLAG_END_OF_STREAM) d->eos_pending = 1;
    event_signal(c, d->ev_in);
    if (d->eos_pending) event_signal(c, d->ev_out);
    ret_x(c, 0);
}

static void acquire(Cpu *c, int with_index) {               // Acquire{Video,Audio}Frame(Frame **out[, int *index])
    Dec *d = find(arg_x(c, 0));
    uint64_t *out = arg_p(c, 1);
    int *index = with_index ? arg_p(c, 2) : NULL;
    once("Acquire", d);
    if (!d || !out || !d->eos_pending || d->eos_done || !d->nframes) { ret_x(c, MOVIE_R_NO_FRAME); return; }
    uint64_t frame = d->frames[0];
    rt_call(c, d->video ? G.video_eos : G.audio_eos, 2, frame, 1ull);
    *out = frame;
    if (index) *index = 0;
    d->eos_done = 1;
    d->acquired++;
    ret_x(c, 0);
}
static void ov_acquire(Cpu *c) { acquire(c, 0); }
static void ov_acquire_index(Cpu *c) { acquire(c, 1); }

// DIAG: every property the game reads, with the key and the answer, and the property calls it makes.
static void ov_props(Cpu *c) {
    char d[128];
    rt_log("movie22: %s property object %#llx", ldr_describe(c->pc, d, sizeof d), (unsigned long long)arg_x(c, 1));
    ret_x(c, 0);
}
static void obs_get_int32(Cpu *c) {
    uint64_t obj = arg_x(c, 0), out = arg_x(c, 1); const char *key = arg_p(c, 2);
    uint64_t r = rt_original(c);
    rt_log("movie22: GetInt32(%#llx, \"%s\") -> %llu, %d", (unsigned long long)obj, key, (unsigned long long)r, out ? *(int *)(uintptr_t)out : 0);
}
static void obs_get_size(Cpu *c) {
    uint64_t obj = arg_x(c, 0), out = arg_x(c, 1); const char *key = arg_p(c, 2);
    uint64_t r = rt_original(c);
    rt_log("movie22: GetSize(%#llx, \"%s\") -> %llu, %llu", (unsigned long long)obj, key, (unsigned long long)r, out ? (unsigned long long)*(uint64_t *)(uintptr_t)out : 0);
}
static void obs_get_float(Cpu *c) {
    uint64_t obj = arg_x(c, 0), out = arg_x(c, 1); const char *key = arg_p(c, 2);
    uint64_t r = rt_original(c);
    rt_log("movie22: GetFloat(%#llx, \"%s\") -> %llu, %g", (unsigned long long)obj, key, (unsigned long long)r, out ? *(float *)(uintptr_t)out : 0.0f);
}
static void obs_get_string(Cpu *c) {
    uint64_t obj = arg_x(c, 0), out = arg_x(c, 1); const char *key = arg_p(c, 3);
    uint64_t r = rt_original(c);
    rt_log("movie22: GetString(%#llx, \"%s\") -> %llu, \"%s\"", (unsigned long long)obj, key, (unsigned long long)r, out && r ? (const char *)(uintptr_t)out : "");
}
static void obs_configure(Cpu *c) {
    rt_log("movie22: Configure(%#llx, property %#llx, codec data %#llx, %llu bytes)", (unsigned long long)arg_x(c, 0),
           (unsigned long long)arg_x(c, 1), (unsigned long long)arg_x(c, 2), (unsigned long long)arg_x(c, 3));
    ret_x(c, 0);
}
HOST_OBSERVER("_ZNK2nn5movie13MediaProperty8GetInt32EPiPKc", obs_get_int32);
HOST_OBSERVER("_ZNK2nn5movie13MediaProperty7GetSizeEPmPKc", obs_get_size);
HOST_OBSERVER("_ZNK2nn5movie13MediaProperty8GetFloatEPfPKc", obs_get_float);
HOST_OBSERVER("_ZNK2nn5movie13MediaProperty9GetStringEPciPKc", obs_get_string);

HOST_OVERRIDE("_ZN5movie12VideoDecoderC1Ev", ov_ctor);
HOST_OVERRIDE("_ZN5movie12VideoDecoderD1Ev", ov_dtor);
HOST_OVERRIDE("_ZN5movie12VideoDecoder10InitializeEPvmN2nn5movie13MediaMimeTypeENS_24VideoDecoderOutputFormatENS_14VideoCodecModeE", ov_video_init);
HOST_OVERRIDE("_ZN5movie12VideoDecoder8FinalizeEv", ov_finalize);
HOST_OVERRIDE("_ZN5movie12VideoDecoder9ConfigureEPKN2nn5movie13MediaPropertyEPKvm", obs_configure);
HOST_OVERRIDE("_ZN5movie12VideoDecoder18GetInputReadyEventEPPN2nn2os15SystemEventTypeE", ov_event_in);
HOST_OVERRIDE("_ZN5movie12VideoDecoder19GetOutputReadyEventEPPN2nn2os15SystemEventTypeE", ov_event_out);
HOST_OVERRIDE("_ZN5movie12VideoDecoder21GetFormatChangedEventEPPN2nn2os15SystemEventTypeE", ov_event_fmt);
HOST_OVERRIDE("_ZN5movie12VideoDecoder5StartEv", ov_start);
HOST_OVERRIDE("_ZN5movie12VideoDecoder4StopEv", ov_ok);
HOST_OVERRIDE("_ZN5movie12VideoDecoder5FlushEv", ov_ok);
HOST_OVERRIDE("_ZN5movie12VideoDecoder11DecodeAsyncEPKN2nn5movie6BufferE", ov_decode);
HOST_OVERRIDE("_ZN5movie12VideoDecoder18RegisterVideoFrameEPPNS_10VideoFrameEi", ov_register);
HOST_OVERRIDE("_ZN5movie12VideoDecoder17AcquireVideoFrameEPPNS_10VideoFrameE", ov_acquire);
HOST_OVERRIDE("_ZN5movie12VideoDecoder17AcquireVideoFrameEPPNS_10VideoFrameEPi", ov_acquire_index);
HOST_OVERRIDE("_ZN5movie12VideoDecoder17ReleaseVideoFrameEPNS_10VideoFrameE", ov_ok);
HOST_OVERRIDE("_ZN5movie12VideoDecoder25GetVideoFrameRequirementsEPN2nn5movie13MediaPropertyE", ov_props);
HOST_OVERRIDE("_ZN5movie12VideoDecoder24GetOutputFramePropertiesEPN2nn5movie13MediaPropertyE", ov_props);

HOST_OVERRIDE("_ZN5movie12AudioDecoderC1Ev", ov_ctor);
HOST_OVERRIDE("_ZN5movie12AudioDecoderD1Ev", ov_dtor);
HOST_OVERRIDE("_ZN5movie12AudioDecoder10InitializeEPvmN2nn5movie13MediaMimeTypeENS_24AudioDecoderOutputFormatENS_14AudioCodecModeE", ov_audio_init);
HOST_OVERRIDE("_ZN5movie12AudioDecoder8FinalizeEv", ov_finalize);
HOST_OVERRIDE("_ZN5movie12AudioDecoder9ConfigureEPKN2nn5movie13MediaPropertyEPKvm", obs_configure);
HOST_OVERRIDE("_ZN5movie12AudioDecoder25GetAudioFrameRequirementsEPN2nn5movie13MediaPropertyE", ov_props);
HOST_OVERRIDE("_ZN5movie12AudioDecoder18GetInputReadyEventEPPN2nn2os15SystemEventTypeE", ov_event_in);
HOST_OVERRIDE("_ZN5movie12AudioDecoder19GetOutputReadyEventEPPN2nn2os15SystemEventTypeE", ov_event_out);
HOST_OVERRIDE("_ZN5movie12AudioDecoder21GetFormatChangedEventEPPN2nn2os15SystemEventTypeE", ov_event_fmt);
HOST_OVERRIDE("_ZN5movie12AudioDecoder5StartEv", ov_start);
HOST_OVERRIDE("_ZN5movie12AudioDecoder4StopEv", ov_ok);
HOST_OVERRIDE("_ZN5movie12AudioDecoder5FlushEv", ov_ok);
HOST_OVERRIDE("_ZN5movie12AudioDecoder11DecodeAsyncEPKN2nn5movie6BufferE", ov_decode);
HOST_OVERRIDE("_ZN5movie12AudioDecoder18RegisterAudioFrameEPPNS_10AudioFrameEi", ov_register);
HOST_OVERRIDE("_ZN5movie12AudioDecoder17AcquireAudioFrameEPPNS_10AudioFrameEPi", ov_acquire_index);
HOST_OVERRIDE("_ZN5movie12AudioDecoder17ReleaseAudioFrameEPNS_10AudioFrameE", ov_ok);
HOST_OVERRIDE("_ZN5movie12AudioDecoder24GetOutputFramePropertiesEPN2nn5movie13MediaPropertyE", ov_props);
