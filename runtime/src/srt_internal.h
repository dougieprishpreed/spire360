/*
 * srt_internal.h - state shared between the runtime's source files.
 * Include w2c2_base.h before this header.
 */
#ifndef SRT_INTERNAL_H
#define SRT_INTERNAL_H

#include "srt.h"
#include "srt_plat.h"

/* A growable byte FIFO guarded by a mutex, with a condition variable for
 * blocking readers. Used for the guest's stdin and stdout. */
typedef struct srt_pipe {
    srt_mutex* lock;
    srt_cond* changed;
    U8* data;
    size_t start;      /* index of the first unread byte */
    size_t end;        /* one past the last written byte */
    size_t capacity;
    int closed;        /* writer side closed */
    U64 total_written;
} srt_pipe;

int srt_pipe_init(srt_pipe* p);
void srt_pipe_write(srt_pipe* p, const void* data, size_t len);
/* Blocks (if wait) until data is available or the pipe is closed. */
size_t srt_pipe_read(srt_pipe* p, void* buf, size_t cap, int wait);
long srt_pipe_read_line(srt_pipe* p, char* buf, size_t cap, int wait);
void srt_pipe_close(srt_pipe* p);

typedef struct srt_state {
    srt_config config;
    char* root;               /* normalised root dir, no trailing '/' */
    wasmMemory* memory;
    srt_pipe stdin_pipe;
    srt_pipe stdout_pipe;
    U64 det_clock_ns;         /* deterministic clock */
    U64 skipped_ns;           /* time skipped by virtual timers */
    U64 det_random;           /* deterministic random state */
    U64 wasi_calls;
    U64 memory_capacity;
} srt_state;

extern srt_state srt_g;

/* Clocks as the guest sees them. srt_clock_mono_ns includes time skipped
 * by virtual timers; srt_clock_wait_until sleeps or skips to a deadline. */
U64 srt_clock_mono_ns(void);
U64 srt_clock_real_ns(void);
void srt_clock_wait_until(U64 mono_deadline_ns);

/* Called by proc_exit: unwinds to srt_run_guarded. */
void srt_exit_guest(int code);

/* Guest-memory helpers. All return 0 if the range is outside memory. */
int srt_mem_ok(U32 ptr, U32 len);
U8* srt_mem_ptr(U32 ptr);

#endif /* SRT_INTERNAL_H */
