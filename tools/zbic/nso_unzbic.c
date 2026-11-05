// nso_unzbic IN OUT: rewrite an NSO whose compressed segments are ZBIC (NSO flag bit 7, Switch 2 builds) as an
// uncompressed NSO that recomp/nso.py loads like any other. Segments flagged uncompressed are copied; LZ4 segments
// (bit 7 clear) are refused, as recomp/nso.py reads those itself. The header keeps its segment SHA-256s and their
// check bits (3..5), so a reader can verify the result (tools/extract_nup.py does). Built by tools/build_zbic.py
// against upstream zstd with ZBIC's table headers (tools/zbic/zbic_ncount.c).
//
// NSO header fields used: flags at 0x0C; per segment i, file offset, memory offset and decompressed size at
// 0x10 + 0x10 * i; compressed sizes at 0x60.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zstd.h"

static uint32_t rd32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static void wr32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }

static int fail(const char *what, const char *detail) {
    fprintf(stderr, "nso_unzbic: %s%s%s\n", what, detail ? ": " : "", detail ? detail : "");
    return 1;
}

int main(int argc, char **argv) {
    if (argc != 3) return fail("usage: nso_unzbic IN OUT", NULL);
    FILE *f = fopen(argv[1], "rb");
    if (!f) return fail("cannot open", argv[1]);
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0x100) return fail("too short for an NSO", argv[1]);
    uint8_t *raw = malloc((size_t)n);
    if (!raw || fread(raw, 1, (size_t)n, f) != (size_t)n) return fail("cannot read", argv[1]);
    fclose(f);
    if (memcmp(raw, "NSO0", 4)) return fail("not an NSO", argv[1]);

    uint32_t flags = rd32(raw + 0x0C);
    uint8_t header[0x100];
    memcpy(header, raw, sizeof header);
    uint8_t *seg[3];
    uint32_t seg_size[3], out_off = 0x100;
    for (int i = 0; i < 3; i++) {
        uint32_t file_off = rd32(raw + 0x10 + 0x10 * i), size = rd32(raw + 0x18 + 0x10 * i);
        uint32_t stored = rd32(raw + 0x60 + 4 * i);
        if ((uint64_t)file_off + stored > (uint64_t)n) return fail("segment out of bounds", argv[1]);
        seg[i] = malloc(size ? size : 1);
        seg_size[i] = size;
        if (!seg[i]) return fail("out of memory", NULL);
        if (flags & (1u << i)) {
            if (!(flags & 0x80)) return fail("LZ4 segments are left to recomp/nso.py", argv[1]);
            size_t got = ZSTD_decompress(seg[i], size, raw + file_off, stored);
            if (ZSTD_isError(got)) return fail("ZBIC segment", ZSTD_getErrorName(got));
            if (got != size) return fail("ZBIC segment", "decompressed size differs from the header's");
        } else {
            if (stored < size) return fail("uncompressed segment shorter than its size", argv[1]);
            memcpy(seg[i], raw + file_off, size);
        }
        wr32(header + 0x10 + 0x10 * i, out_off);
        wr32(header + 0x60 + 4 * i, size);
        out_off += size;
    }
    wr32(header + 0x0C, flags & 0x38);

    FILE *o = fopen(argv[2], "wb");
    if (!o) return fail("cannot write", argv[2]);
    int ok = fwrite(header, 1, sizeof header, o) == sizeof header;
    for (int i = 0; i < 3; i++) ok = ok && fwrite(seg[i], 1, seg_size[i], o) == seg_size[i];
    if (fclose(o) || !ok) return fail("write failed", argv[2]);
    return 0;
}
