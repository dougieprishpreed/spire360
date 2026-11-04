/* Big blocks (64 KB and up: a texture's levels, a background's pack read,
 * a sound's file) from the kernel's pages (4 KB each) instead of newlib's
 * heap, each given back whole when freed. xsdk's sbrk returns only the
 * heap's top, so a 10 MB block freed between smaller live ones stayed the
 * heap's: a console session's heap held 139 MB with 97 in use (each shop's
 * 10 MB backdrop took more), and the GPU's memory ran out (a texture made
 * again and again after a save and quit). From 1 MB up only, the small
 * textures and sounds read by the hundred at a warm-up still left the heap
 * 44 MB over what it used, and act 3's shops ran out again (rp108). The
 * blocks are found by their address in a table hashed on it (a free of
 * any pointer looks there). The link wraps malloc, free, realloc and calloc
 * (CMakeLists.txt); newlib's own _malloc_r / _free_r pairs never see a big
 * block (its buffers are small), and a block this file did not make goes
 * to newlib's free. Without the kernel's pages (address space gone) a big
 * block comes from the heap as before. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <xsdk/kernel.h>

void* __real_malloc(size_t n);
void __real_free(void* p);
void* __real_realloc(void* p, size_t n);
void* __real_calloc(size_t n, size_t size);

/* SLOTS a power of two, at most half of them used: a probe ends at an
 * empty slot, and a freed block's slot is filled from the ones after it
 * that probed past it, so the probes stay short (no tombstones) */
enum { BIG = 64 * 1024, SLOTS = 8192, MAX_BIG = SLOTS / 2 };

static struct {
    void* p;
    size_t n;
} g_big[SLOTS];
static unsigned g_live;
static volatile int g_lock;
/* the span of every big block made so far (never shrinks): a pointer out of
 * it is newlib's, without the lock */
static volatile uintptr_t g_lo = UINTPTR_MAX, g_hi = 0;
static volatile size_t g_bytes;

static void lock(void) {
    while (__sync_lock_test_and_set(&g_lock, 1))
        while (g_lock) {
        }
}

static void unlock(void) { __sync_lock_release(&g_lock); }

/* the slot a block's address starts its probe at (pages are 4 KB apart) */
static unsigned home(const void* p) { return (unsigned)(((uintptr_t)p >> 12) * 2654435761u) & (SLOTS - 1); }

static void* pages(size_t n) {
    void* p = NULL;
    uint32_t size = (uint32_t)n;
    if (n > 0x7FFFFFFFu || !NT_SUCCESS(NtAllocateVirtualMemory(&p, &size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE, 0)))
        return NULL;
    lock();
    if (g_live >= MAX_BIG) {
        unlock();
        uint32_t none = 0;
        NtFreeVirtualMemory(&p, &none, MEM_RELEASE, 0);
        return NULL;
    }
    unsigned i = home(p);
    while (g_big[i].p) i = (i + 1) & (SLOTS - 1);
    g_big[i].p = p;
    g_big[i].n = n;
    ++g_live;
    g_bytes += n;
    if ((uintptr_t)p < g_lo) g_lo = (uintptr_t)p;
    if ((uintptr_t)p + n > g_hi) g_hi = (uintptr_t)p + n;
    unlock();
    return p;
}

/* slot i emptied: the entries after it that probed past it move back */
static void erase_at(unsigned i) {
    g_big[i].p = NULL;
    for (unsigned j = (i + 1) & (SLOTS - 1); g_big[j].p; j = (j + 1) & (SLOTS - 1)) {
        unsigned h = home(g_big[j].p);
        if (((j - h) & (SLOTS - 1)) >= ((j - i) & (SLOTS - 1))) {
            g_big[i] = g_big[j];
            g_big[j].p = NULL;
            i = j;
        }
    }
}

/* the big block at p (its size in *n), taken out of the table if take */
static int big_block(void* p, size_t* n, int take) {
    if (!p || (uintptr_t)p < g_lo || (uintptr_t)p >= g_hi) return 0;
    int found = 0;
    lock();
    for (unsigned i = home(p); g_big[i].p; i = (i + 1) & (SLOTS - 1))
        if (g_big[i].p == p) {
            *n = g_big[i].n;
            if (take) {
                g_bytes -= g_big[i].n;
                --g_live;
                erase_at(i);
            }
            found = 1;
            break;
        }
    unlock();
    return found;
}

static void release(void* p) {
    uint32_t none = 0;
    NtFreeVirtualMemory(&p, &none, MEM_RELEASE, 0);
}

size_t spire_big_bytes(void) { return g_bytes; }

void* __wrap_malloc(size_t n) {
    if (n >= BIG) {
        void* p = pages(n);
        if (p) return p;
    }
    return __real_malloc(n);
}

void __wrap_free(void* p) {
    size_t n;
    if (big_block(p, &n, 1)) release(p);
    else __real_free(p);
}

void* __wrap_calloc(size_t count, size_t size) {
    if (size && count > (size_t)-1 / size) return NULL;
    /* the kernel's pages come zeroed */
    if (count * size >= BIG) {
        void* p = pages(count * size);
        if (p) return p;
    }
    return __real_calloc(count, size);
}

void* __wrap_realloc(void* p, size_t n) {
    size_t had;
    if (!big_block(p, &had, 0)) {
        /* newlib's block: grown or shrunk where it is */
        return __real_realloc(p, n);
    }
    if (n <= had && n >= BIG) return p;
    void* q = __wrap_malloc(n);
    if (!q) return NULL;
    memcpy(q, p, n < had ? n : had);
    __wrap_free(p);
    return q;
}
