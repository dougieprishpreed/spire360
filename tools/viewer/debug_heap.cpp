/* debug_heap.cpp: a checking operator new / delete for the PC tools'
 * debug build (tools/viewer/build_debug.sh), to find the engine's memory
 * errors under random input (spire_play --monkey --sync):
 *
 * - each block has a header (magic, size, who deleted it) and a trailing
 *   canary: a delete of something new did not return, a second delete of a
 *   block, or a write past its end stops the program with the caller's
 *   frames;
 * - a freed block is filled with 0xDD and held back (the last 64 MB of
 *   frees) before it goes back to malloc: a pointer used after its delete
 *   reads 0xDDDD... (a fault where it is used) instead of a live object;
 * - debug_heap_check() (spire_play calls it every frame) and each block's
 *   return to malloc look for bytes written into a freed block (a tween or
 *   a pointer kept past its node's delete): the block's size, where it was
 *   written, and the frames that deleted it.
 *
 * Frames: the frame-pointer chain (built with -fno-omit-frame-pointer),
 * for addr2line -f -C -e out/engine-dbg/spire_play.exe. */
#include <new>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

const unsigned long long LIVE = 0x4C495645424C4F43ULL;  /* "LIVEBLOC" */
const unsigned long long FREED = 0x4652454544424C4BULL; /* "FREEDBLK" */
const unsigned long long CANARY = 0xC0FFEE11C0FFEE11ULL;
const int DELETERS = 8;

struct Header {
    unsigned long long magic;
    size_t size;
    void* deleted_by[DELETERS];
};

const size_t QUARANTINE_BYTES = (size_t)64 << 20;
const size_t SLOTS = 1 << 16;
Header* quarantine[SLOTS];
size_t q_head = 0, q_count = 0, q_bytes = 0;

/* The caller's frames (x64 Windows code keeps no rbp chain: the unwind
 * tables, through RtlCaptureStackBackTrace), this function's own skipped. */
void chain(void** out, int n) {
    USHORT got = RtlCaptureStackBackTrace(1, (ULONG)n, out, NULL);
    for (int i = got; i < n; ++i) out[i] = NULL;
}

void stop(const char* what, const void* p) {
    void* f[32];
    chain(f, 32);
    fprintf(stderr, "debug_heap: %s %p\n", what, p);
    for (int i = 0; i < 32 && f[i]; ++i) fprintf(stderr, "  ret %p\n", f[i]);
    fflush(stderr);
    abort();
}

/* Tween targets (TweenSet::add): the frames that started the last tween on
 * each address, in a table of their own (malloc, not operator new). */
struct Target {
    const void* p;
    void* frames[10];
};
const size_t TARGETS = 1 << 18;
Target* targets = NULL;

size_t target_slot(const void* p) { return ((size_t)p >> 2) * 2654435761u % TARGETS; }

/* A freed block that is no longer all 0xDD: someone wrote to it. */
bool written(const Header* h) {
    const unsigned char* b = (const unsigned char*)(h + 1);
    for (size_t i = 0; i < h->size; ++i)
        if (b[i] != 0xDD) {
            fprintf(stderr, "debug_heap: write after delete into a %lu-byte block %p at offset %lu:",
                    (unsigned long)h->size, (const void*)(h + 1), (unsigned long)i);
            for (size_t k = i; k < i + 16 && k < h->size; ++k) fprintf(stderr, " %02x", b[k]);
            fprintf(stderr, "\n  deleted by:\n");
            for (int k = 0; k < DELETERS && h->deleted_by[k]; ++k) fprintf(stderr, "  del %p\n", h->deleted_by[k]);
            const void* at = b + (i & ~(size_t)3);
            if (targets && targets[target_slot(at)].p == at) {
                fprintf(stderr, "  the last tween on it started at:\n");
                for (int k = 0; k < 10 && targets[target_slot(at)].frames[k]; ++k)
                    fprintf(stderr, "  twn %p\n", targets[target_slot(at)].frames[k]);
            }
            fflush(stderr);
            return true;
        }
    return false;
}

int big_reported = 0;

void* allocate(size_t n) {
    Header* h = (Header*)malloc(sizeof(Header) + n + sizeof(CANARY));
    if (!h) throw std::bad_alloc();
    if (n >= ((size_t)4 << 20) && big_reported < 40) {
        /* a block of 4 MB or more (the console's memory): where from */
        void* f[12];
        chain(f, 12);
        ++big_reported;
        fprintf(stderr, "debug_heap: big block %lu KB\n", (unsigned long)(n >> 10));
        for (int i = 0; i < 12 && f[i]; ++i) fprintf(stderr, "  big %p\n", f[i]);
        fflush(stderr);
    }
    h->magic = LIVE;
    h->size = n;
    memcpy((char*)(h + 1) + n, &CANARY, sizeof(CANARY));
    memset(h + 1, 0xCD, n);
    return h + 1;
}

void release(void* p) {
    if (!p) return;
    Header* h = (Header*)p - 1;
    if (h->magic == FREED) stop("delete of a deleted block", p);
    if (h->magic != LIVE) stop("delete of a block new did not return", p);
    if (memcmp((char*)p + h->size, &CANARY, sizeof(CANARY)) != 0) stop("write past the end of a block", p);
    h->magic = FREED;
    chain(h->deleted_by, DELETERS);
    memset(p, 0xDD, h->size);
    /* held back; the oldest goes back to malloc (checked first) */
    while (q_count > 0 && (q_bytes + h->size > QUARANTINE_BYTES || q_count == SLOTS)) {
        size_t tail = (q_head + SLOTS - q_count) % SLOTS;
        Header* old = quarantine[tail];
        if (written(old)) stop("found as the block went back to malloc", old + 1);
        q_bytes -= old->size;
        --q_count;
        free(old);
    }
    quarantine[q_head] = h;
    q_head = (q_head + 1) % SLOTS;
    ++q_count;
    q_bytes += h->size;
}

} /* namespace */

/* The blocks freed lately (the newest first, up to `recent`), checked for
 * writes after their delete; stops at the first. */
void debug_heap_check(size_t recent) {
    for (size_t i = 0; i < q_count && i < recent; ++i) {
        Header* h = quarantine[(q_head + SLOTS - 1 - i) % SLOTS];
        if (written(h)) stop("found by debug_heap_check", h + 1);
    }
}

extern "C" void debug_heap_note_target(const void* p) {
    if (!targets) targets = (Target*)calloc(TARGETS, sizeof(Target));
    if (!targets) return;
    Target& t = targets[target_slot(p)];
    t.p = p;
    chain(t.frames, 10);
}

void* operator new(size_t n) throw(std::bad_alloc) { return allocate(n); }
void* operator new[](size_t n) throw(std::bad_alloc) { return allocate(n); }
void operator delete(void* p) throw() { release(p); }
void operator delete[](void* p) throw() { release(p); }
