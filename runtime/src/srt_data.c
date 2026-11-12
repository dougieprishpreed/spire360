/* The translated module's data segments (its initial memory), out of the
 * executable. tools/build_logic.py's C stage writes the large ones to
 * logic.dat as LZ4 blocks and has the C read each back while the module is
 * instantiated (srt_data_load); the console keeps the whole XEX image in
 * memory, where the segments were 17 MB read once. The C stage also writes
 * spire_logic_data_table (count, then each segment's stored size, size and
 * FNV-1a of its stored bytes), which srt_data_check holds the file to before
 * anything is instantiated: an executable and a logic.dat of different
 * builds stop there, with a line in the log.
 *
 * logic.dat: 'SPLD' u32 version (1) u32 count u32 0; count entries of
 * u32 offset, stored, size, fnv; then the blocks. Little-endian. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "srt_plat.h"
#include "srt_data.h"

static char data_path[512];
static srt_file* data_file;

void srt_data_path(const char* path) {
    if (strlen(path) < sizeof(data_path)) strcpy(data_path, path);
}

const char* srt_data_where(void) {
    return data_path;
}

static unsigned rd32(const unsigned char* p) {
    return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

static int fail(const char* why) {
    char msg[700];
    sprintf(msg, "[srt] %s: %s (the executable and logic.dat must come from the same build)\n", data_path, why);
    srt_plat_log(2, msg, strlen(msg));
    return -1;
}

/* Reads at an offset, in 256 KB pieces: one large read holds the console's
 * USB drive and starves the other threads' streams. */
static int read_at(unsigned at, unsigned char* dst, unsigned len) {
    srt_u64 pos;
    if (srt_plat_seek(data_file, (srt_i64)at, SRT_SEEK_SET, &pos) != 0 || pos != at) return -1;
    while (len > 0) {
        size_t piece = len < 262144u ? len : 262144u, done = 0;
        if (srt_plat_read(data_file, dst, piece, &done) != 0 || done == 0) return -1;
        dst += done;
        len -= (unsigned)done;
    }
    return 0;
}

static int open_data(void) {
    if (data_file != NULL) return 0;
    if (data_path[0] == 0) return fail("no path for the module's data");
    if (srt_plat_open(data_path, SRT_OPEN_READ, &data_file) != 0) {
        data_file = NULL;
        return fail("not found");
    }
    return 0;
}

int srt_data_check(const unsigned* table) {
    unsigned char head[16], entry[16];
    unsigned k;
    if (open_data() != 0) return -1;
    if (read_at(0, head, 16) != 0 || rd32(head) != 0x444c5053u || rd32(head + 4) != 1) return fail("not a logic data file");
    if (rd32(head + 8) != table[0]) return fail("another build's data");
    for (k = 0; k < table[0]; k++) {
        const unsigned* t = table + 1 + 3 * k;
        if (read_at(16 + 16 * k, entry, 16) != 0) return fail("read failed");
        if (rd32(entry + 4) != t[0] || rd32(entry + 8) != t[1] || rd32(entry + 12) != t[2]) return fail("another build's data");
    }
    return 0;
}

/* An LZ4 block (literals, then a 2-byte offset and a match of 4 or more; the
 * last sequence is literals only), bounds-checked. Returns the bytes
 * written, or -1 for bad data. */
static long lz4_decode(const unsigned char* src, unsigned srclen, unsigned char* dst, unsigned dstlen) {
    const unsigned char* ip = src;
    const unsigned char* iend = src + srclen;
    unsigned char* op = dst;
    unsigned char* oend = dst + dstlen;
    while (ip < iend) {
        unsigned token = *ip++, lit = token >> 4, ml, off, b;
        if (lit == 15)
            do {
                if (ip >= iend) return -1;
                b = *ip++;
                lit += b;
            } while (b == 255);
        if ((unsigned)(iend - ip) < lit || (unsigned)(oend - op) < lit) return -1;
        memcpy(op, ip, lit);
        op += lit;
        ip += lit;
        if (ip == iend) break;
        if (iend - ip < 2) return -1;
        off = (unsigned)ip[0] | ((unsigned)ip[1] << 8);
        ip += 2;
        if (off == 0 || off > (unsigned)(op - dst)) return -1;
        ml = token & 15;
        if (ml == 15)
            do {
                if (ip >= iend) return -1;
                b = *ip++;
                ml += b;
            } while (b == 255);
        ml += 4;
        if ((unsigned)(oend - op) < ml) return -1;
        if (off >= ml) {
            memcpy(op, op - off, ml);
            op += ml;
        } else {
            const unsigned char* m = op - off;
            while (ml--) *op++ = *m++;
        }
    }
    return (long)(op - dst);
}

int srt_data_load(unsigned index, unsigned stored, unsigned fnv, unsigned char* dst, unsigned size) {
    unsigned char head[16], entry[16], *buf;
    unsigned at, k, h = 0x811c9dc5u;
    if (open_data() != 0) return -1;
    if (read_at(0, head, 16) != 0 || rd32(head) != 0x444c5053u || rd32(head + 4) != 1) return fail("not a logic data file");
    if (index >= rd32(head + 8) || read_at(16 + index * 16, entry, 16) != 0) return fail("no such segment");
    at = rd32(entry);
    if (rd32(entry + 4) != stored || rd32(entry + 8) != size || rd32(entry + 12) != fnv) return fail("another build's data");
    buf = (unsigned char*)malloc(stored);
    if (buf == NULL) return fail("no memory to read it");
    if (read_at(at, buf, stored) != 0) {
        free(buf);
        return fail("read failed");
    }
    for (k = 0; k < stored; k++) h = (h ^ buf[k]) * 0x01000193u;
    if (h != fnv) {
        free(buf);
        return fail("damaged");
    }
    k = (unsigned)lz4_decode(buf, stored, dst, size);
    free(buf);
    if (k != size) return fail("didn't decode");
    return 0;
}

void srt_data_close(void) {
    if (data_file != NULL) srt_plat_close(data_file);
    data_file = NULL;
}
