// FSE table header reader for ZBIC, the zstd variant of Switch firmware 22.0.0 and later (Switch 2 NSOs mark ZBIC
// segments with flag bit 7). ZBIC frames are zstd frames in every respect but two: the frame magic ("ZBIC",
// 0x4349425A) and the way an FSE table's normalized counts ("NCount") are stored, which this file decodes. zstd's
// FSE_readNCount_body calls ZBIC_readNCount when built with ZSTD_ZBIC (tools/zbic/zbic.patch), so the sequence tables
// and the Huffman weight tables both go through it.
//
// Written for this project from the format's behaviour, as kinnay/zbic (GPL-2.0, its zstd.c FSE_readNCount_bic) reads
// it; none of that code is copied. The header, as decoded here:
//
// - Byte 0: bit 7 = L, set when the table has "less than one" symbols (count -1); bits 0..6 = R, the number of bytes
//   that follow. The header is R + 1 bytes long.
// - Bytes 1..R are a stream of mixed-radix digits, read from the last byte towards the first through a 64-bit window
//   that takes bytes in at the bottom while its upper half is below 0x100. A digit of radix r is the window mod r;
//   the window is then divided by r and refilled. Every byte must have been taken in by the last digit.
// - Digits, in order: maxSymbol - 1 (radix 52); tableLog - 5 (radix 8); a value t (radix 2^tableLog) from which the
//   total of the cumulative counts follows, t + 1, or maxSymbol + t + 2 when L is set.
// - The cumulative counts C[0..P], P the smallest power of two above maxSymbol, C[0] = 0 and C[P] = that total, are
//   filled by binary interpolative coding: the intervals (lo, hi) of a binary split of [0, P] are visited in pre-order,
//   the upper half first, then one last node (0, 0, 1). A node whose ends hold equal values gives that value to every
//   index between them and takes no digit; any other node sets C[mid] = C[lo] + a digit of radix C[hi] - C[lo] + 1.
// - The count of symbol s (0..maxSymbol) is C[s + 1] - C[s] - L; their absolute values must add up to 2^tableLog.
#include <stddef.h>
#include <stdint.h>

#include "zstd_errors.h"

#define ZBIC_CORRUPT ((size_t) - (int)ZSTD_error_corruption_detected)

typedef struct {
    const uint8_t *bytes;   // header byte 0; the digit bytes are bytes[1..left]
    size_t left;            // digit bytes not yet taken into the window
    uint64_t window;
} Digits;

static void digits_refill(Digits *d) {
    while (d->left && (d->window >> 32) < 0x100) d->window = (d->window << 8) | d->bytes[d->left--];
}

static uint64_t digits_take(Digits *d, uint64_t radix) {
    uint64_t digit = d->window % radix;
    d->window /= radix;
    digits_refill(d);
    return digit;
}

// One interpolative node and, before returning, the nodes of its upper then its lower half.
static void bic_node(Digits *d, uint32_t *cum, uint32_t lo, uint32_t hi) {
    if (hi - lo < 2) return;
    uint32_t mid = (lo + hi) / 2;
    if (cum[lo] == cum[hi]) {
        for (uint32_t i = lo + 1; i < hi; i++) cum[i] = cum[lo];
        return;   // the nodes inside see equal ends too and take no digit
    }
    cum[mid] = cum[lo] + (uint32_t)digits_take(d, (uint64_t)cum[hi] - cum[lo] + 1);
    bic_node(d, cum, mid, hi);
    bic_node(d, cum, lo, mid);
}

size_t ZBIC_readNCount(short *normalizedCounter, unsigned *maxSVPtr, unsigned *tableLogPtr, const void *headerBuffer,
                       size_t hbSize) {
    const uint8_t *p = (const uint8_t *)headerBuffer;
    if (!hbSize) return ZBIC_CORRUPT;
    size_t nbytes = p[0] & 0x7F;
    unsigned low = p[0] >> 7;
    if (nbytes + 1 >= hbSize) return ZBIC_CORRUPT;

    Digits d = {p, nbytes, 0};
    digits_refill(&d);
    unsigned max_symbol = (unsigned)digits_take(&d, 52) + 1;
    if (max_symbol > *maxSVPtr) return ZBIC_CORRUPT;
    unsigned table_log = (unsigned)digits_take(&d, 8) + 5;
    int remaining = 1 << table_log;
    uint64_t t = digits_take(&d, (uint64_t)remaining);

    uint32_t span = 1;
    while (span <= max_symbol) span <<= 1;
    if (span > 0xFF) return ZBIC_CORRUPT;
    uint32_t cum[257] = {0};
    cum[span] = (uint32_t)(low ? max_symbol + t + 2 : t + 1);
    bic_node(&d, cum, 0, span);
    if (cum[0] != cum[1]) cum[0] += (uint32_t)digits_take(&d, (uint64_t)cum[1] - cum[0] + 1);   // the last node (0, 0, 1)

    for (unsigned s = 0; s <= max_symbol; s++) {
        int count = (int)(int16_t)(uint16_t)(cum[s + 1] - cum[s]) - (int)low;
        normalizedCounter[s] = (short)count;
        remaining -= count < 0 ? -count : count;
    }
    if (remaining || d.left) return ZBIC_CORRUPT;
    *maxSVPtr = max_symbol;
    *tableLogPtr = table_log;
    return nbytes + 1;
}
