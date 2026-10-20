/*
 * SPDX-License-Identifier: MIT
 * Experimental Miles 7.2g ASI adapter, authored from interface declarations
 * and observed ABI behavior. No RAD implementation code is included.
 * Decoder: upstream minimp3, separately licensed CC0 (see vendor/minimp3/LICENSE).
 */
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ONLY_MP3
#define MINIMP3_ONLY_SIMD
#include "minimp3.h"

typedef int32_t S32;
typedef uint32_t U32;
typedef int32_t HSTREAM;
typedef S32 (__stdcall *FETCH)(U32, void *, S32, S32);
typedef struct {
    U32 type;
    const char *name;
    U32 token;
    U32 subtype;
} ENTRY;
typedef S32 (__stdcall *REGISTER)(U32, const char *, S32, const ENTRY *);
_Static_assert(sizeof(void *) == 4 && sizeof(ENTRY) == 16, "Win32 ABI required");

enum { FUNCTION = 0, PROPERTY = 1, NONE = 0, CUSTOM = 1, DEC = 2,
       HEX = 3, FLOAT_VALUE = 4, PERCENT = 5, BOOL_VALUE = 6, STRING = 7 };
#define READONLY UINT32_C(0x80000000)
#define MAGIC UINT32_C(0x4d503341)
#define INPUT_CAP 4096
#define SCAN_LIMIT (4u * 1024u * 1024u)

typedef struct {
    U32 magic, user, total;
    FETCH fetch;
    mp3dec_t decoder;
    mp3dec_frame_info_t info;
    unsigned char input[INPUT_CAP];
    size_t used, filled;
    mp3d_sample_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
    size_t pcm_used, pcm_bytes;
    uint64_t fetched, output_bytes;
    S32 next_offset;
    S32 requested_rate, requested_bits, requested_channels;
    int eof, failed, mpeg_version;
} STREAM;

static LONG started;
static void *volatile error_text;
static const char provider_name[] = "Settlers 7 minimp3 SSE2 adapter";
static const char input_types[] = "MPEG Layer 3 audio files\0*.MP3\0";
static const char output_types[] = "Raw PCM files\0*.RAW\0";
static int trace_enabled, trace_count;
#define TRACE(...) do { if (trace_enabled && trace_count++ < 80) \
    fprintf(stderr, __VA_ARGS__); } while (0)

static void error(const char *message) {
    InterlockedExchangePointer(&error_text, (void *)message);
}
static STREAM *stream_ptr(HSTREAM handle) {
    STREAM *s = (STREAM *)(uintptr_t)(U32)handle;
    return s && s->magic == MAGIC ? s : NULL;
}
static S32 fail(STREAM *s, const char *message) {
    error(message);
    if (s) s->failed = 1;
    return 0;
}

/* Miles reports playback position from the source callback's cursor.
   Fetch exactly one compressed frame, never hidden multi-frame read-ahead. */
static int refill(STREAM *s, size_t wanted) {
    if (wanted > INPUT_CAP) return fail(s, "MP3 frame exceeds input capacity");
    if (s->used) {
        memmove(s->input, s->input + s->used, s->filled - s->used);
        s->filled -= s->used;
        s->used = 0;
    }
    while (!s->eof && s->filled < wanted) {
        /* Miles passes the current input-buffer size as total for high-level
           streams. It is a percentage hint, never an absolute read boundary. */
        size_t request = wanted - s->filled;
        S32 got = s->fetch(s->user, s->input + s->filled,
                           (S32)request, s->next_offset);
        TRACE("ASI fetch user=%08x total=%u offset=%d request=%u got=%d fetched=%llu\n",
              s->user, s->total, s->next_offset, (unsigned)request, got,
              (unsigned long long)s->fetched);
        s->next_offset = -1;
        if (got < 0 || (size_t)got > request)
            return fail(s, "Invalid source callback result");
        if (!got) {
            s->eof = 1;
            break;
        }
        s->filled += (size_t)got;
        s->fetched += (U32)got;
    }
    return !s->failed && s->filled >= wanted;
}

static int next_frame(STREAM *s) {
    unsigned scanned = 0;
    s->pcm_used = s->pcm_bytes = 0;
    while (!s->failed) {
        if (!refill(s, 4)) return 0;
        if (!memcmp(s->input, "ID3", 3)) {
            if (!refill(s, 10)) return 0;
            if ((s->input[6] | s->input[7] | s->input[8] | s->input[9]) & 0x80)
                return fail(s, "Invalid ID3 size");
            U32 length = ((U32)s->input[6] << 21) |
                         ((U32)s->input[7] << 14) |
                         ((U32)s->input[8] << 7) | s->input[9];
            if (s->input[3] == 4 && (s->input[5] & 0x10)) length += 10;
            if (length > SCAN_LIMIT) return fail(s, "ID3 tag exceeds four MiB");
            s->used = 10;
            while (length) {
                size_t part = length > INPUT_CAP ? INPUT_CAP : length;
                if (!refill(s, part)) return 0;
                s->used = part;
                length -= (U32)part;
            }
            continue;
        }
        if (!hdr_valid(s->input)) {
            s->used = 1;
            if (++scanned >= SCAN_LIMIT)
                return fail(s, "No MP3 header within four MiB");
            continue;
        }
        if (HDR_GET_LAYER(s->input) != 1)
            return fail(s, "Only MPEG Layer 3 is supported");
        int unpadded = hdr_frame_bytes(s->input, 0);
        if (!unpadded)
            return fail(s, "Free-format MP3 is not supported");
        size_t available = (size_t)unpadded + hdr_padding(s->input);
        if (available < 4 || !refill(s, available)) return 0;
        mp3dec_frame_info_t info = {0};
        const unsigned char *frame = s->input;
        int samples = mp3dec_decode_frame(&s->decoder, frame,
                                          (int)available, s->pcm, &info);
        if (info.frame_bytes < 0 || (size_t)info.frame_bytes > available ||
            samples < 0 || samples > 1152)
            return fail(s, "Decoder returned invalid frame bounds");
        if (!info.frame_bytes) {
            s->used = 1;
            ++scanned;
        } else {
            if (info.frame_offset >= 0 &&
                (size_t)info.frame_offset + 2 <= available &&
                info.hz && info.layer == 3) {
                int version = (frame[info.frame_offset + 1] >> 3) & 3;
                s->mpeg_version = version == 3 ? 1 : version == 2 ? 2 : 25;
            }
            s->used += (size_t)info.frame_bytes;
            scanned += (unsigned)info.frame_bytes;
        }
        if (samples) {
            if ((info.channels != 1 && info.channels != 2) ||
                info.hz <= 0 || info.layer != 3 ||
                (s->info.hz && (s->info.hz != info.hz ||
                               s->info.channels != info.channels)))
                return fail(s, "Unsupported midstream MP3 format change");
            s->info = info;
            s->pcm_bytes = (size_t)samples * info.channels * sizeof(s->pcm[0]);
            return 1;
        }
        if (scanned >= SCAN_LIMIT)
            return fail(s, "No decodable frame within four MiB");
    }
    return 0;
}

static S32 __stdcall asi_startup(void) {
    if (InterlockedCompareExchange(&started, 1, 0)) return 2;
    InterlockedExchangePointer(&error_text, NULL);
    return 0;
}
static S32 __stdcall asi_shutdown(void) {
    return InterlockedExchange(&started, 0) ? 0 : 8;
}
static char *__stdcall asi_error(void) {
    return (char *)InterlockedExchangePointer(&error_text, NULL);
}
static HSTREAM __stdcall asi_open(U32 user, FETCH fetch, U32 total) {
    TRACE("ASI open user=%08x total=%u\n", user, total);
    if (!started || !fetch) {
        error("Invalid ASI stream open");
        return 0;
    }
    STREAM *s = (STREAM *)calloc(1, sizeof(*s));
    if (!s) {
        error("Out of memory");
        return 0;
    }
    s->magic = MAGIC;
    s->user = user;
    s->fetch = fetch;
    s->total = total;
    s->requested_rate = s->requested_bits = s->requested_channels = -1;
    mp3dec_init(&s->decoder);
    if (!next_frame(s)) {
        if (!s->failed) error("No decodable MP3 audio");
        s->magic = 0;
        free(s);
        return 0;
    }
    return (HSTREAM)(uintptr_t)s;
}
static S32 __stdcall asi_process(HSTREAM handle, void *dest, S32 bytes) {
    STREAM *s = stream_ptr(handle);
    if (!s || bytes < 0 || (bytes && !dest)) {
        error("Invalid ASI process arguments");
        return 0;
    }
    /* Zero from a streaming callback can mean temporary starvation. */
    s->eof = 0;
    S32 done = 0;
    while (done < bytes && !s->failed) {
        if (s->pcm_used == s->pcm_bytes && !next_frame(s)) break;
        size_t count = s->pcm_bytes - s->pcm_used;
        if (count > (size_t)(bytes - done)) count = (size_t)(bytes - done);
        memcpy((unsigned char *)dest + done,
               (unsigned char *)s->pcm + s->pcm_used, count);
        s->pcm_used += count;
        s->output_bytes += count;
        done += (S32)count;
    }
    /* The original provider zero-fills the unproduced tail, but returns only
       the number of real decoded bytes. */
    if (done < bytes) memset((unsigned char *)dest + done, 0, bytes - done);
    return done;
}
static S32 __stdcall asi_seek(HSTREAM handle, S32 offset) {
    TRACE("ASI seek handle=%08x offset=%d\n", (U32)handle, offset);
    STREAM *s = stream_ptr(handle);
    if (!s || offset < -2) return 3;
    s->used = s->filled = 0;
    s->eof = s->failed = 0;
    s->output_bytes = 0;
    if (offset != -2) {
        mp3dec_init(&s->decoder);
        s->pcm_used = s->pcm_bytes = 0;
    }
    /* -2 is an observed Miles extension: retain synthesis/PCM history while
       notifying the callback that input will continue from its current point.
       High-level Miles loop tests, rather than this inference, gate use. */
    s->next_offset = offset < 0 ? -1 : offset;
    if (offset >= 0) s->fetched = (U32)offset;
    else s->fetched = 0; /* External reposition: absolute offset is unknown. */
    return 0;
}
static S32 __stdcall asi_close(HSTREAM handle) {
    STREAM *s = stream_ptr(handle);
    if (!s) return 3;
    s->magic = 0;
    free(s);
    return 0;
}

static S32 copy_property(void *before, void *after, const void *value, size_t n) {
    if (before) memcpy(before, value, n);
    if (after) memcpy(after, value, n);
    return before || after;
}
static S32 __stdcall provider_property(U32 property, void *before,
                                       const void *value, void *after) {
    if (value) return 0;
    U32 number;
    const char *text;
    switch (property) {
    case (U32)-100: text = provider_name; break;
    case 0: case 1: text = input_types; break;
    case 3: text = output_types; break;
    /* This is the RIB compatibility version. Miles 7 rejects versions <1.2. */
    case (U32)-101: number = 0x120; goto integer;
    case 2: number = 0x55; goto integer;
    case 4: number = 4096; goto integer;
    default: return 0;
    }
    return copy_property(before, after, &text, sizeof(text));
integer:
    return copy_property(before, after, &number, sizeof(number));
}
static S32 __stdcall asi_property(HSTREAM handle, U32 property, void *before,
                                  const void *value, void *after) {
    STREAM *s = stream_ptr(handle);
    if (!s) return 0;
    if (property >= 20 && property <= 22) {
        S32 *requested = property == 20 ? &s->requested_rate :
                         property == 21 ? &s->requested_bits :
                                          &s->requested_channels;
        S32 native = property == 20 ? s->info.hz :
                     property == 21 ? 16 : s->info.channels;
        if (before) memcpy(before, requested, 4);
        if (value) {
            S32 wanted;
            memcpy(&wanted, value, 4);
            /* Conversion remains with Miles. Do not claim unsupported output. */
            if (wanted != -1 && wanted != 0 && wanted != native) return 0;
            *requested = wanted;
        }
        if (after) memcpy(after, requested, 4);
        return 1;
    }
    if (value) return 0;
    uint64_t consumed = s->fetched >= s->filled - s->used ?
                        s->fetched - (s->filled - s->used) : 0;
    U32 number;
    switch (property) {
    case 5: number = (U32)s->info.bitrate_kbps * 1000; break;
    case 6: case 10: number = s->info.hz; break;
    case 7:
        number = ((U32)s->info.bitrate_kbps * 1000) /
                 ((U32)s->info.hz * s->info.channels);
        if (!number) number = 1;
        break;
    case 8: case 12: number = s->info.channels; break;
    case 9: number = (U32)s->info.hz * s->info.channels * 16; break;
    case 11: number = 16; break;
    case 13: number = (U32)(s->pcm_bytes - s->pcm_used); break;
    case 14: number = consumed > INT32_MAX ? INT32_MAX : (U32)consumed; break;
    case 15: {
        /* Assumes a 64-bit output for this CUSTOM property. Its complete
           vendor ABI is unverified; see docs/VALIDATION.md. */
        uint64_t emitted = s->output_bytes / (2u * s->info.channels);
        return copy_property(before, after, &emitted, sizeof(emitted));
    }
    case 16: {
        float percent = s->total ? (float)(100.0 * consumed / s->total) : 0.0f;
        if (percent > 100.0f) percent = 100.0f;
        return copy_property(before, after, &percent, sizeof(percent));
    }
    case 17: number = 4096; break;
    case 18: number = s->mpeg_version; break;
    case 19: number = 3; break;
    default: return 0;
    }
    return copy_property(before, after, &number, sizeof(number));
}

#define FN(name, fn) { FUNCTION, name, (U32)(uintptr_t)(fn), NONE }
#define RO(name, id, kind) { PROPERTY, name, id, (kind) | READONLY }
#define RW(name, id, kind) { PROPERTY, name, id, kind }
static const ENTRY codec_entries[] = {
    FN("PROVIDER_property", provider_property),
    RO("Name", (U32)-100, STRING), RO("Version", (U32)-101, HEX),
    RO("Input file types", 0, STRING),
    RO("Editor-supported input file types", 1, STRING),
    RO("Input wave tag", 2, HEX), RO("Output file types", 3, STRING),
    RO("Maximum frame size", 4, DEC),
    FN("ASI_startup", asi_startup), FN("ASI_error", asi_error),
    FN("ASI_shutdown", asi_shutdown)
};
static const ENTRY stream_entries[] = {
    FN("ASI_stream_open", asi_open), FN("ASI_stream_process", asi_process),
    FN("ASI_stream_property", asi_property), FN("ASI_stream_seek", asi_seek),
    FN("ASI_stream_close", asi_close),
    RO("Input bit rate", 5, DEC), RO("Input sample rate", 6, DEC),
    RO("Input sample width", 7, DEC), RO("Input channels", 8, DEC),
    RO("Output bit rate", 9, DEC), RO("Output sample rate", 10, DEC),
    RO("Output sample width", 11, DEC), RO("Output channels", 12, DEC),
    RO("Output reservoir", 13, DEC), RO("Position", 14, DEC),
    RO("Emitted samples", 15, CUSTOM), RO("Percent done", 16, PERCENT),
    RO("Minimum input block size", 17, DEC), RO("MPEG version", 18, DEC),
    RO("MPEG layer", 19, DEC), RW("Requested sample rate", 20, DEC),
    RW("Requested bit width", 21, DEC), RW("Requested # of channels", 22, DEC)
};

void settlers_enable_dep_once(void);
S32 __stdcall RIB_Main(U32 provider, U32 up_down) {
    if (up_down) settlers_enable_dep_once();
    trace_enabled = getenv("ASI_TRACE") != NULL;
    HMODULE miles = GetModuleHandleA("mss32.dll");
    if (!miles) return 0;
    REGISTER reg = (REGISTER)(void *)GetProcAddress(miles, "RIB_register_interface");
    REGISTER unreg = (REGISTER)(void *)GetProcAddress(miles, "RIB_unregister_interface");
    if (!reg || !unreg) return 0;
    if (!up_down) {
        unreg(provider, NULL, 0, NULL);
        return 1;
    }
    if (reg(provider, "ASI codec",
            sizeof(codec_entries) / sizeof(codec_entries[0]), codec_entries) ||
        reg(provider, "ASI stream",
            sizeof(stream_entries) / sizeof(stream_entries[0]), stream_entries)) {
        unreg(provider, NULL, 0, NULL);
        return 0;
    }
    return 1;
}
