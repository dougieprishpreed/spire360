/*
 * srt_plat.h - the small platform layer under the Spire360 wasm runtime.
 *
 * Everything the WASI layer needs from the host OS goes through these
 * functions: files, directories, clocks, randomness, threads and logging.
 * Implementations: srt_plat_posix.c (Linux/macOS, used for PC testing and
 * qemu big-endian runs) and srt_plat_win32.c (Windows and Xbox 360).
 *
 * C89. Paths given to these functions are host paths, already joined with
 * the mount root by the caller, using '/' separators; each backend converts
 * separators itself.
 *
 * All functions that can fail return 0 on success or a WASI errno value
 * (SRT_E*), so the WASI layer can pass errors straight through.
 */
#ifndef SRT_PLAT_H
#define SRT_PLAT_H

#include <stddef.h>
#include "srt_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* WASI preview1 errno values used by the runtime (subset). */
#define SRT_ESUCCESS     0
#define SRT_E2BIG        1
#define SRT_EACCES       2
#define SRT_EAGAIN       6
#define SRT_EBADF        8
#define SRT_EEXIST       20
#define SRT_EFAULT       21
#define SRT_EINVAL       28
#define SRT_EIO          29
#define SRT_EISDIR       31
#define SRT_ELOOP        32
#define SRT_EMFILE       33
#define SRT_ENAMETOOLONG 37
#define SRT_ENOENT       44
#define SRT_ENOMEM       48
#define SRT_ENOSPC       51
#define SRT_ENOSYS       52
#define SRT_ENOTDIR      54
#define SRT_ENOTEMPTY    55
#define SRT_ENOTSUP      58
#define SRT_ENOTCAPABLE  76
#define SRT_ESPIPE       70
#define SRT_EPERM        63

/* File types (WASI filetype values). */
#define SRT_FT_UNKNOWN   0
#define SRT_FT_CHARDEV   2
#define SRT_FT_DIRECTORY 3
#define SRT_FT_REGULAR   4

/* Open flags for srt_plat_open. */
#define SRT_OPEN_READ    0x01
#define SRT_OPEN_WRITE   0x02
#define SRT_OPEN_CREATE  0x04
#define SRT_OPEN_TRUNC   0x08
#define SRT_OPEN_EXCL    0x10
#define SRT_OPEN_APPEND  0x20

/* Seek origins (WASI whence values). */
#define SRT_SEEK_SET 0
#define SRT_SEEK_CUR 1
#define SRT_SEEK_END 2

typedef struct srt_stat {
    srt_u8  type;       /* SRT_FT_* */
    srt_u64 size;       /* bytes */
    srt_u64 mtime_ns;   /* modification time, ns since the Unix epoch, 0 if unknown */
} srt_stat;

typedef struct srt_file srt_file;   /* opaque, backend-defined */
typedef struct srt_dir srt_dir;     /* opaque, backend-defined */

/* Files */
int srt_plat_open(const char* path, int flags, srt_file** out);
int srt_plat_close(srt_file* f);
int srt_plat_read(srt_file* f, void* buf, size_t len, size_t* done);
int srt_plat_write(srt_file* f, const void* buf, size_t len, size_t* done);
int srt_plat_seek(srt_file* f, srt_i64 offset, int whence, srt_u64* newpos);
int srt_plat_fstat(srt_file* f, srt_stat* st);
int srt_plat_truncate(srt_file* f, srt_u64 size);
int srt_plat_sync(srt_file* f);

/* Paths */
int srt_plat_stat(const char* path, srt_stat* st);
int srt_plat_mkdir(const char* path);
int srt_plat_rmdir(const char* path);
int srt_plat_unlink(const char* path);
int srt_plat_rename(const char* from, const char* to);

/* Directories. srt_plat_readdir returns SRT_ENOENT at the end. The name
 * pointer stays valid until the next call on the same srt_dir. "." and ".."
 * are never returned. */
int srt_plat_opendir(const char* path, srt_dir** out);
int srt_plat_readdir(srt_dir* d, const char** name, srt_u8* type);
void srt_plat_closedir(srt_dir* d);

/* Clocks, in nanoseconds. */
srt_u64 srt_plat_time_realtime_ns(void);
srt_u64 srt_plat_time_monotonic_ns(void);
void srt_plat_sleep_ns(srt_u64 ns);

/* Entropy. May be weak; the game's own RNG is seeded separately. */
void srt_plat_random(void* buf, size_t len);

/* Debug log sink: stream 1 = stdout, 2 = stderr. Must accept partial lines. */
void srt_plat_log(int stream, const char* text, size_t len);

/* Threads and synchronisation (used by the stdio pipes and the host). */
typedef struct srt_mutex srt_mutex;
typedef struct srt_cond srt_cond;
typedef struct srt_thread srt_thread;
typedef void (*srt_thread_fn)(void* arg);

srt_mutex* srt_mutex_create(void);
void srt_mutex_destroy(srt_mutex* m);
void srt_mutex_lock(srt_mutex* m);
void srt_mutex_unlock(srt_mutex* m);

srt_cond* srt_cond_create(void);
void srt_cond_destroy(srt_cond* c);
/* Waits with m held; returns with m held. */
void srt_cond_wait(srt_cond* c, srt_mutex* m);
void srt_cond_broadcast(srt_cond* c);

/* stack_bytes 0 = backend default. hw_thread is a hint (Xbox 360 core/thread
 * index 0-5, -1 = any) and is ignored on PC. */
srt_thread* srt_thread_start(srt_thread_fn fn, void* arg, size_t stack_bytes, int hw_thread);
void srt_thread_join(srt_thread* t);

/* Large-block allocation for the wasm linear memory: reserve address space
 * once, then commit (and zero) ranges as memory.grow needs them. On the
 * console committing is what consumes physical memory. srt_plat_commit
 * returns 0 on success. */
void* srt_plat_reserve(srt_u64 bytes, srt_u64* got);
int srt_plat_commit(void* base, srt_u64 from, srt_u64 to);
void srt_plat_release(void* p);

/* Optional: also append the debug log to a file. NULL closes it. */
void srt_plat_set_log_file(const char* path);

#ifdef __cplusplus
}
#endif

#endif /* SRT_PLAT_H */
