/*
 * srt_core.c - runtime state, linear-memory hooks, traps, guest thread and
 * the stdin/stdout pipes.
 */
#include "w2c2_base.h"
#include <setjmp.h>
#include <stdio.h>
#include "srt_internal.h"

srt_state srt_g;

static jmp_buf srt_guest_jmp;
static int srt_guest_jmp_set = 0;
static volatile int srt_guest_done = 0;
static int srt_guest_exit_code = 0;
static srt_thread* srt_guest_thread = NULL;

#define SRT_DEFAULT_MEMORY_LIMIT ((U64)256 * 1024 * 1024)

static char* srt_strdup(const char* s) {
    size_t n = strlen(s);
    char* r = (char*)malloc(n + 1);
    if (r != NULL) {
        memcpy(r, s, n + 1);
    }
    return r;
}

int srt_init(const srt_config* config) {
    size_t n;
    memset(&srt_g, 0, sizeof(srt_g));
    srt_g.config = *config;
    if (srt_g.config.memory_limit == 0) {
        srt_g.config.memory_limit = SRT_DEFAULT_MEMORY_LIMIT;
    }
    srt_g.root = srt_strdup(config->root_dir != NULL ? config->root_dir : ".");
    if (srt_g.root == NULL) {
        return -1;
    }
    /* Normalise separators and strip a trailing '/' (but keep a bare "/"). */
    for (n = 0; srt_g.root[n] != '\0'; n++) {
        if (srt_g.root[n] == '\\') {
            srt_g.root[n] = '/';
        }
    }
    while (n > 1 && srt_g.root[n - 1] == '/') {
        srt_g.root[--n] = '\0';
    }
    srt_g.det_clock_ns = (U64)1700000000 * (U64)1000000000;
    srt_g.det_random = W2C2_LL(0x9E3779B97F4A7C15);
    if (srt_pipe_init(&srt_g.stdin_pipe) != 0 || srt_pipe_init(&srt_g.stdout_pipe) != 0) {
        return -1;
    }
    return 0;
}

void srt_set_memory(void* w2c2_memory) {
    srt_g.memory = (wasmMemory*)w2c2_memory;
}

int srt_mem_ok(U32 ptr, U32 len) {
    if (srt_g.memory == NULL) {
        return 0;
    }
    return (U64)ptr + (U64)len <= srt_g.memory->size;
}

U8* srt_mem_ptr(U32 ptr) {
    return srt_g.memory->data + ptr;
}

/* ---- clocks ---- */

U64 srt_clock_mono_ns(void) {
    if (srt_g.config.deterministic) {
        return srt_g.det_clock_ns;
    }
    return srt_plat_time_monotonic_ns() + srt_g.skipped_ns;
}

U64 srt_clock_real_ns(void) {
    if (srt_g.config.deterministic) {
        return srt_g.det_clock_ns;
    }
    return srt_plat_time_realtime_ns() + srt_g.skipped_ns;
}

void srt_clock_wait_until(U64 deadline) {
    U64 now = srt_clock_mono_ns();
    if (deadline <= now) {
        return;
    }
    if (srt_g.config.deterministic) {
        srt_g.det_clock_ns = deadline;
    } else if (srt_g.config.virtual_timers) {
        srt_g.skipped_ns += deadline - now;
    } else {
        srt_plat_sleep_ns(deadline - now);
    }
}

/* ---- w2c2 host memory hooks (W2C2_HOST_MEMORY) ---- */

U8* w2c2HostMemoryReserve(U64 initialBytes, U64 maxBytes, U64* capacityBytes) {
    U64 want = maxBytes;
    U64 got = 0;
    U8* data;
    if (want > srt_g.config.memory_limit) {
        want = srt_g.config.memory_limit;
    }
    if (want < initialBytes) {
        want = initialBytes;
    }
    data = (U8*)srt_plat_reserve(want, &got);
    if (data == NULL) {
        return NULL;
    }
    if (srt_plat_commit(data, 0, initialBytes) != 0) {
        srt_plat_release(data);
        return NULL;
    }
    srt_g.memory_capacity = got;
    *capacityBytes = got;
    return data;
}

int w2c2HostMemoryGrow(U8* data, U64 oldBytes, U64 newBytes) {
    return srt_plat_commit(data, oldBytes, newBytes) == 0;
}

void w2c2HostMemoryRelease(U8* data) {
    srt_plat_release(data);
}

/* ---- traps and exit ---- */

void trap(Trap t) {
    char msg[96];
    sprintf(msg, "[srt] TRAP: %s\n", trapDescription(t));
    srt_plat_log(2, msg, strlen(msg));
    if (srt_guest_jmp_set) {
        longjmp(srt_guest_jmp, SRT_EXIT_TRAP_BASE + (int)t + 1);
    }
    abort();
}

void srt_exit_guest(int code) {
    if (srt_guest_jmp_set) {
        /* longjmp cannot carry 0; encode the exit code as code + 1. */
        longjmp(srt_guest_jmp, code + 1);
    }
    exit(code);
}

int srt_run_guarded(void (*fn)(void* instance), void* instance) {
    int r;
    r = setjmp(srt_guest_jmp);
    if (r == 0) {
        srt_guest_jmp_set = 1;
        fn(instance);
        srt_guest_jmp_set = 0;
        return 0;
    }
    srt_guest_jmp_set = 0;
    /* Exit codes and traps were both encoded as value + 1. */
    return r - 1;
}

typedef struct srt_guest_args {
    void (*fn)(void* instance);
    void* instance;
} srt_guest_args;

static srt_guest_args srt_guest_call;

static void srt_guest_entry(void* arg) {
    srt_guest_args* a = (srt_guest_args*)arg;
    srt_guest_exit_code = srt_run_guarded(a->fn, a->instance);
    srt_guest_done = 1;
    /* Wake any host reader waiting on stdout. */
    srt_pipe_close(&srt_g.stdout_pipe);
}

int srt_guest_start(void (*fn)(void* instance), void* instance, size_t stack_bytes, int hw_thread) {
    srt_guest_call.fn = fn;
    srt_guest_call.instance = instance;
    srt_guest_done = 0;
    srt_guest_thread = srt_thread_start(srt_guest_entry, &srt_guest_call,
                                        stack_bytes != 0 ? stack_bytes : (size_t)1024 * 1024,
                                        hw_thread);
    return srt_guest_thread != NULL ? 0 : -1;
}

int srt_guest_join(void) {
    if (srt_guest_thread != NULL) {
        srt_thread_join(srt_guest_thread);
        srt_guest_thread = NULL;
    }
    return srt_guest_exit_code;
}

int srt_guest_finished(void) {
    return srt_guest_done;
}

/* ---- pipes ---- */

int srt_pipe_init(srt_pipe* p) {
    memset(p, 0, sizeof(*p));
    p->lock = srt_mutex_create();
    p->changed = srt_cond_create();
    p->capacity = 4096;
    p->data = (U8*)malloc(p->capacity);
    return (p->lock != NULL && p->changed != NULL && p->data != NULL) ? 0 : -1;
}

void srt_pipe_write(srt_pipe* p, const void* data, size_t len) {
    srt_mutex_lock(p->lock);
    if (p->end + len > p->capacity) {
        size_t used = p->end - p->start;
        if (p->start > 0) {
            /* Compact first; grow only if still needed. */
            memmove(p->data, p->data + p->start, used);
            p->start = 0;
            p->end = used;
        }
        if (p->end + len > p->capacity) {
            size_t cap = p->capacity;
            U8* grown;
            while (p->end + len > cap) {
                cap *= 2;
            }
            grown = (U8*)realloc(p->data, cap);
            if (grown == NULL) {
                srt_mutex_unlock(p->lock);
                srt_plat_log(2, "[srt] pipe: out of memory\n", 26);
                return;
            }
            p->data = grown;
            p->capacity = cap;
        }
    }
    memcpy(p->data + p->end, data, len);
    p->end += len;
    p->total_written += len;
    srt_cond_broadcast(p->changed);
    srt_mutex_unlock(p->lock);
}

size_t srt_pipe_read(srt_pipe* p, void* buf, size_t cap, int wait) {
    size_t n;
    srt_mutex_lock(p->lock);
    while (wait && p->end == p->start && !p->closed) {
        srt_cond_wait(p->changed, p->lock);
    }
    n = p->end - p->start;
    if (n > cap) {
        n = cap;
    }
    memcpy(buf, p->data + p->start, n);
    p->start += n;
    if (p->start == p->end) {
        p->start = p->end = 0;
    }
    srt_mutex_unlock(p->lock);
    return n;
}

long srt_pipe_read_line(srt_pipe* p, char* buf, size_t cap, int wait) {
    long result = -1;
    srt_mutex_lock(p->lock);
    for (;;) {
        size_t i;
        for (i = p->start; i < p->end; i++) {
            if (p->data[i] == '\n') {
                break;
            }
        }
        if (i < p->end) {
            size_t len = i - p->start;
            if (cap == 0 || len > cap - 1) {
                result = -2;
            } else {
                memcpy(buf, p->data + p->start, len);
                buf[len] = '\0';
                p->start = i + 1;
                if (p->start == p->end) {
                    p->start = p->end = 0;
                }
                result = (long)len;
            }
            break;
        }
        if (!wait || p->closed) {
            break;
        }
        srt_cond_wait(p->changed, p->lock);
    }
    srt_mutex_unlock(p->lock);
    return result;
}

void srt_pipe_close(srt_pipe* p) {
    srt_mutex_lock(p->lock);
    p->closed = 1;
    srt_cond_broadcast(p->changed);
    srt_mutex_unlock(p->lock);
}

void srt_stdin_write(const void* data, size_t len) {
    srt_pipe_write(&srt_g.stdin_pipe, data, len);
}

void srt_stdin_close(void) {
    srt_pipe_close(&srt_g.stdin_pipe);
}

size_t srt_stdout_read(void* buf, size_t cap, int wait) {
    return srt_pipe_read(&srt_g.stdout_pipe, buf, cap, wait);
}

long srt_stdout_read_line(char* buf, size_t cap, int wait) {
    return srt_pipe_read_line(&srt_g.stdout_pipe, buf, cap, wait);
}

void srt_get_stats(srt_stats* out) {
    out->memory_bytes = srt_g.memory != NULL ? srt_g.memory->size : 0;
    out->memory_capacity = srt_g.memory_capacity;
    out->stdout_bytes = srt_g.stdout_pipe.total_written;
    out->wasi_calls = srt_g.wasi_calls;
}
