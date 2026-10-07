/*
 * srt_wasi_p2.c - the few WASI preview2 functions that .NET's CoreLib calls
 * directly on WASI (as core-module imports, no component model):
 *
 *   wasi:clocks/monotonic-clock@0.2.0  now, resolution,
 *                                      subscribe-instant, subscribe-duration
 *   wasi:io/poll@0.2.0                 poll, [resource-drop]pollable,
 *                                      [method]pollable.ready / .block
 *
 * CoreLib's single-threaded event loop (WasiEventLoop, TimerQueue.Wasi)
 * uses them for Task.Delay, timers and "don't block while there is work".
 *
 * Pollables here are only clock deadlines. With config.virtual_timers set,
 * waiting on a deadline skips the clock forward instead of sleeping, so game
 * code that waits for an animation (Task.Delay) finishes at once.
 */
#include "w2c2_base.h"
#include "srt_internal.h"

#define P2_CLOCK(len, name) i33_wasiX3AclocksX2FmonotonicX2DclockX400X2E2X2E0##len##_##name
#define P2_POLL(len, name) i18_wasiX3AioX2FpollX400X2E2X2E0##len##_##name

#define SRT_MAX_POLLABLES 256

typedef struct srt_pollable {
    int used;
    U64 deadline_ns;
} srt_pollable;

static srt_pollable srt_pollables[SRT_MAX_POLLABLES];

/* Handles are index + 1, so 0 is never a valid handle. */
static U32 srt_pollable_new(U64 deadline) {
    U32 i;
    for (i = 0; i < SRT_MAX_POLLABLES; i++) {
        if (!srt_pollables[i].used) {
            srt_pollables[i].used = 1;
            srt_pollables[i].deadline_ns = deadline;
            return i + 1;
        }
    }
    srt_plat_log(2, "[srt] out of pollables\n", 23);
    trap(trapAllocationFailed);
    return 0;
}

static srt_pollable* srt_pollable_get(U32 handle) {
    if (handle == 0 || handle > SRT_MAX_POLLABLES || !srt_pollables[handle - 1].used) {
        srt_plat_log(2, "[srt] bad pollable handle\n", 26);
        trap(trapUnreachable);
    }
    return &srt_pollables[handle - 1];
}

U64 P2_CLOCK(3, now)(wasmModuleInstance* inst) {
    (void)inst;
    srt_g.wasi_calls++;
    return srt_clock_mono_ns();
}

U64 P2_CLOCK(10, resolution)(wasmModuleInstance* inst) {
    (void)inst;
    srt_g.wasi_calls++;
    return 1000;
}

U32 P2_CLOCK(17, subscribeX2Dinstant)(wasmModuleInstance* inst, U64 when) {
    (void)inst;
    srt_g.wasi_calls++;
    return srt_pollable_new(when);
}

U32 P2_CLOCK(18, subscribeX2Dduration)(wasmModuleInstance* inst, U64 duration) {
    U64 now;
    (void)inst;
    srt_g.wasi_calls++;
    now = srt_clock_mono_ns();
    return srt_pollable_new(duration > (U64)-1 - now ? (U64)-1 : now + duration);
}

void P2_POLL(23, X5BresourceX2DdropX5Dpollable)(wasmModuleInstance* inst, U32 handle) {
    (void)inst;
    srt_g.wasi_calls++;
    srt_pollable_get(handle)->used = 0;
}

U32 P2_POLL(22, X5BmethodX5DpollableX2Eready)(wasmModuleInstance* inst, U32 handle) {
    (void)inst;
    srt_g.wasi_calls++;
    return srt_clock_mono_ns() >= srt_pollable_get(handle)->deadline_ns ? 1 : 0;
}

void P2_POLL(22, X5BmethodX5DpollableX2Eblock)(wasmModuleInstance* inst, U32 handle) {
    (void)inst;
    srt_g.wasi_calls++;
    srt_clock_wait_until(srt_pollable_get(handle)->deadline_ns);
}

/*
 * poll(list<borrow<pollable>>) -> list<u32>, lowered as (ptr, len, retptr).
 * The canonical ABI would allocate the result list with the guest's
 * cabi_realloc. Instead the indices are written over the input array, which
 * is at least as long as any result; CoreLib's caller pins that array and
 * copies the result out before releasing it.
 */
void P2_POLL(4, poll)(wasmModuleInstance* inst, U32 list, U32 count, U32 retptr) {
    U32 i;
    U32 n = 0;
    U64 earliest = (U64)-1;
    U64 now;
    (void)inst;
    srt_g.wasi_calls++;
    if (count == 0 || !srt_mem_ok(list, count * 4) || !srt_mem_ok(retptr, 8)) {
        trap(trapMemoryOutOfBounds);
    }
    for (i = 0; i < count; i++) {
        U64 d = srt_pollable_get(i32_load(srt_g.memory, list + i * 4))->deadline_ns;
        if (d < earliest) {
            earliest = d;
        }
    }
    srt_clock_wait_until(earliest);
    now = srt_clock_mono_ns();
    /* Ready indices are ascending, so in-place writes never overtake reads. */
    for (i = 0; i < count; i++) {
        U64 d = srt_pollable_get(i32_load(srt_g.memory, list + i * 4))->deadline_ns;
        if (d <= now) {
            i32_store(srt_g.memory, list + n * 4, i);
            n++;
        }
    }
    i32_store(srt_g.memory, retptr, list);
    i32_store(srt_g.memory, retptr + 4, n);
}
