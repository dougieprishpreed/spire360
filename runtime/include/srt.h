/*
 * srt.h - Spire360 wasm runtime: host-facing API.
 *
 * The game logic is a WASI preview1 "command" module (compiled from C# with
 * NativeAOT-LLVM, then translated to C by w2c2). The runtime gives it:
 *
 *   - a WASI preview1 implementation backed by srt_plat_* (files under one
 *     mounted root directory, clocks, randomness),
 *   - stdin/stdout as in-memory pipes, so the host talks to the module with
 *     the same line-based JSON protocol used on PC,
 *   - stderr routed to the debug log,
 *   - a guest thread that runs the module's _start, so a slow game action
 *     never blocks rendering.
 *
 * C89. Single module, single guest thread per process.
 */
#ifndef SRT_H
#define SRT_H

#include <stddef.h>
#include "srt_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct srt_config {
    /* Host directory mounted as the guest's "/" (preopened as fd 3). */
    const char* root_dir;
    /* Guest argv. argv[0] is the program name. May be NULL/0. */
    int argc;
    const char* const* argv;
    /* Guest environment, "KEY=VALUE" strings. May be NULL/0. */
    int envc;
    const char* const* envp;
    /* Non-zero: clocks advance by a fixed 1 ms per query and random_get is a
     * fixed-seed generator, so test runs are reproducible. */
    int deterministic;
    /* Non-zero: a guest wait on a timer (Task.Delay, Thread.Sleep, timer
     * pollables) moves the monotonic clock forward instead of sleeping.
     * The game logic's animation waits then cost nothing. */
    int virtual_timers;
    /* Upper bound for the guest's linear memory in bytes (0 = 256 MB). */
    srt_u64 memory_limit;
    /* Optional: called for each chunk the guest writes to stderr. If NULL,
     * stderr goes to srt_plat_log(2, ...). */
    void (*stderr_sink)(const char* text, size_t len, void* user);
    void* stderr_user;
} srt_config;

/* Must be called once before the module is instantiated. Returns 0 on success. */
int srt_init(const srt_config* config);

/* Tell the runtime where the module's memory 0 lives. Pass the wasmMemory*
 * returned by the module's memory export accessor, after instantiation. */
void srt_set_memory(void* w2c2_memory);

/* Run fn(instance) on the calling thread, catching proc_exit and traps.
 * Returns the exit code, or SRT_EXIT_TRAP_BASE + trap number on a trap. */
#define SRT_EXIT_TRAP_BASE 1000
int srt_run_guarded(void (*fn)(void* instance), void* instance);

/* Start fn(instance) on a dedicated guest thread (via srt_run_guarded).
 * stack_bytes 0 = 1 MB. hw_thread: Xbox 360 hardware thread hint, -1 = any. */
int srt_guest_start(void (*fn)(void* instance), void* instance, size_t stack_bytes, int hw_thread);
/* Wait for the guest thread. Returns its exit code. */
int srt_guest_join(void);
/* Non-zero once the guest has exited or trapped. */
int srt_guest_finished(void);

/* Host -> guest stdin. Writes never block. */
void srt_stdin_write(const void* data, size_t len);
/* After this the guest sees EOF once the buffered bytes are consumed. */
void srt_stdin_close(void);

/* Guest stdout -> host. Copies up to cap bytes. If wait is non-zero, blocks
 * until at least one byte is available or the guest has finished.
 * Returns the number of bytes copied (0 = nothing available / finished). */
size_t srt_stdout_read(void* buf, size_t cap, int wait);

/* Reads one '\n'-terminated line from guest stdout into buf (without the
 * '\n', NUL-terminated). Returns the line length, -1 if no complete line is
 * available (or the guest finished), -2 if the line does not fit in cap-1
 * bytes (the line is then left in the pipe). Blocks only if wait is set. */
long srt_stdout_read_line(char* buf, size_t cap, int wait);

/* Statistics for the debug overlay. */
typedef struct srt_stats {
    srt_u64 memory_bytes;      /* current linear memory size */
    srt_u64 memory_capacity;   /* reserved block size */
    srt_u64 stdout_bytes;      /* total bytes written by the guest to stdout */
    srt_u64 wasi_calls;        /* total WASI calls */
} srt_stats;
void srt_get_stats(srt_stats* out);

#ifdef __cplusplus
}
#endif

#endif /* SRT_H */
