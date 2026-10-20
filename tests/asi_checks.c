/* Focused adapter checks; includes the same implementation as the DLL. */
#include "../src/asi_minimp3.c"

typedef struct {
    unsigned char *data;
    size_t length, position, max_read;
    int bad_result;
} SOURCE;

static S32 __stdcall memory_fetch(U32 user, void *dest, S32 bytes, S32 offset) {
    SOURCE *source = (SOURCE *)(uintptr_t)user;
    if (source->bad_result) return source->bad_result > 0 ? bytes + 1 : -1;
    if (bytes < 0 || offset < -1) return -1;
    if (offset >= 0) source->position = (U32)offset;
    if (source->position >= source->length) return 0;
    size_t count = source->length - source->position;
    if (count > (size_t)bytes) count = (size_t)bytes;
    if (source->max_read && count > source->max_read) count = source->max_read;
    memcpy(dest, source->data + source->position, count);
    source->position += count;
    return (S32)count;
}

static int decode(SOURCE *source, size_t chunk, unsigned char **pcm, size_t *size) {
    HSTREAM h = asi_open((U32)(uintptr_t)source, memory_fetch, 8192);
    if (!h) return 0;
    unsigned char *out = NULL;
    size_t used = 0, capacity = 0;
    unsigned char *guarded = malloc(chunk + 32);
    if (!guarded) return 0;
    for (;;) {
        memset(guarded, 0xa7, chunk + 32);
        S32 n = asi_process(h, guarded + 16, (S32)chunk);
        if (n < 0 || (size_t)n > chunk) return 0;
        for (size_t i = 0; i < 16; ++i)
            if (guarded[i] != 0xa7 || guarded[16 + chunk + i] != 0xa7) return 0;
        if (!n) break;
        if (used + n > capacity) {
            capacity = (used + n) * 2;
            unsigned char *grown = realloc(out, capacity);
            if (!grown) return 0;
            out = grown;
        }
        memcpy(out + used, guarded + 16, n);
        used += n;
        if (used > 64u * 1024u * 1024u) return 0;
    }
    unsigned char first[127], again[127];
    if (asi_seek(h, 0) || asi_process(h, first, sizeof(first)) != sizeof(first))
        return 0;
    if (asi_seek(h, 0) || asi_process(h, again, sizeof(again)) != sizeof(again) ||
        memcmp(first, again, sizeof(first))) return 0;
    source->position = 0;
    if (asi_seek(h, -1) || asi_process(h, again, sizeof(again)) != sizeof(again) ||
        memcmp(first, again, sizeof(first))) return 0;
    if (asi_seek(h, -3) != 3 || asi_process(h, NULL, 1) != 0 ||
        asi_process(h, first, -1) != 0) return 0;
    if (asi_close(h)) return 0;
    free(guarded);
    *pcm = out;
    *size = used;
    return 1;
}

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    FILE *f = fopen(argv[1], "rb");
    if (!f) return 2;
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    rewind(f);
    if (length <= 0 || length > 10000000) return 2;
    unsigned char *input = malloc((size_t)length);
    if (!input || fread(input, 1, length, f) != (size_t)length) return 2;
    fclose(f);
    if (asi_startup()) return 2;
    unsigned char *reference = NULL;
    size_t reference_size = 0;
    const size_t chunks[] = {4096, 1, 3, 511, 8191};
    for (size_t i = 0; i < sizeof(chunks) / sizeof(chunks[0]); ++i) {
        SOURCE source = {input, (size_t)length, 0, i == 4 ? 17 : 0, 0};
        unsigned char *pcm = NULL;
        size_t size = 0;
        if (!decode(&source, chunks[i], &pcm, &size)) {
            printf("FAIL chunk=%u error=%s\n", (unsigned)chunks[i], asi_error());
            return 1;
        }
        if (!i) {
            reference = pcm;
            reference_size = size;
        } else {
            if (size != reference_size || memcmp(pcm, reference, size)) {
                printf("FAIL PCM differs chunk=%u\n", (unsigned)chunks[i]);
                return 1;
            }
            free(pcm);
        }
        printf("PASS chunk=%u PCM=%u seek0/seek-1/canaries/short-reads\n",
               (unsigned)chunks[i], (unsigned)size);
    }
    unsigned char garbage[1024];
    memset(garbage, 0xa5, sizeof(garbage));
    for (int bad = -1; bad <= 1; ++bad) {
        SOURCE source = {garbage, sizeof(garbage), 0, 0, bad};
        if (asi_open((U32)(uintptr_t)&source, memory_fetch, 0)) {
            puts("FAIL accepted malformed source");
            return 1;
        }
    }
    puts("PASS malformed bytes and invalid callback results rejected");
    free(reference);
    free(input);
    asi_shutdown();
    return 0;
}
