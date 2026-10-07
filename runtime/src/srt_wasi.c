/*
 * srt_wasi.c - WASI preview1 for w2c2-translated modules.
 *
 * Every preview1 function is defined, so any module links; the ones the game
 * cannot need (sockets, links, renumbering) return ENOSYS/ENOTSUP.
 *
 * File descriptors:
 *   0 stdin   - srt_g.stdin_pipe (blocking read)
 *   1 stdout  - srt_g.stdout_pipe
 *   2 stderr  - debug log (or config.stderr_sink)
 *   3 "/"     - preopened directory mapped to config.root_dir
 *   4..       - files and directories opened by the guest
 *
 * Guest paths are resolved inside the root; ".." can never leave it.
 * Linear memory is little-endian; all multi-byte fields go through the w2c2
 * load/store helpers so this file is correct on big-endian hosts.
 */
#include "w2c2_base.h"
#include <stdio.h>
#include "srt_internal.h"

#define WASI_FN(len, name) i22_wasiX5FsnapshotX5Fpreview1##len##_##name

#define SRT_MAX_FDS 64
#define SRT_MAX_PATH 1024

#define FDK_FREE   0
#define FDK_STDIN  1
#define FDK_STDOUT 2
#define FDK_STDERR 3
#define FDK_DIR    4
#define FDK_FILE   5

#define RIGHTS_ALL W2C2_LL(0x1FFFFFFF)

typedef struct srt_fd {
    int kind;
    srt_file* file;
    char* path;        /* guest path relative to the root, "" for the root */
    int append;
    int preopen;
} srt_fd;

static srt_fd srt_fds[SRT_MAX_FDS];
static int srt_fds_ready = 0;

/* ---- helpers ---- */

static void srt_fds_init(void) {
    if (srt_fds_ready) {
        return;
    }
    memset(srt_fds, 0, sizeof(srt_fds));
    srt_fds[0].kind = FDK_STDIN;
    srt_fds[1].kind = FDK_STDOUT;
    srt_fds[2].kind = FDK_STDERR;
    srt_fds[3].kind = FDK_DIR;
    srt_fds[3].path = (char*)calloc(1, 1);
    srt_fds[3].preopen = 1;
    srt_fds_ready = 1;
}

static srt_fd* srt_fd_get(U32 fd) {
    srt_fds_init();
    if (fd >= SRT_MAX_FDS || srt_fds[fd].kind == FDK_FREE) {
        return NULL;
    }
    return &srt_fds[fd];
}

static int srt_fd_alloc(void) {
    int i;
    srt_fds_init();
    for (i = 4; i < SRT_MAX_FDS; i++) {
        if (srt_fds[i].kind == FDK_FREE) {
            return i;
        }
    }
    return -1;
}

#define MEM (srt_g.memory)
#define CHECK_MEM(ptr, len) do { if (!srt_mem_ok((U32)(ptr), (U32)(len))) return SRT_EFAULT; } while (0)

static void put_u8(U32 addr, U8 v) { i32_store8(MEM, addr, v); }
static void put_u16(U32 addr, U16 v) { i32_store16(MEM, addr, v); }
static void put_u32(U32 addr, U32 v) { i32_store(MEM, addr, v); }
static void put_u64(U32 addr, U64 v) { i64_store(MEM, addr, v); }
static U32 get_u32(U32 addr) { return i32_load(MEM, addr); }
static U64 get_u64(U32 addr) { return i64_load(MEM, addr); }
static U16 get_u16(U32 addr) { return (U16)i32_load16_u(MEM, addr); }
static U8 get_u8(U32 addr) { return (U8)i32_load8_u(MEM, addr); }

/*
 * Resolve a guest path against a directory fd into a normalised path
 * relative to the root (no leading '/', components separated by '/').
 * Returns 0 or a WASI errno.
 */
static int srt_resolve(const srt_fd* dir, U32 path_ptr, U32 path_len, char* out, size_t cap) {
    char tmp[SRT_MAX_PATH];
    size_t n = 0;
    size_t i;
    size_t olen = 0;
    const U8* src;

    if (dir == NULL || dir->kind != FDK_DIR) {
        return SRT_EBADF;
    }
    if (!srt_mem_ok(path_ptr, path_len)) {
        return SRT_EFAULT;
    }
    src = srt_mem_ptr(path_ptr);
    if (path_len > 0 && src[0] == '/') {
        /* Absolute paths are taken relative to the root. */
        n = 0;
    } else {
        n = strlen(dir->path);
        if (n + 1 >= sizeof(tmp)) {
            return SRT_ENAMETOOLONG;
        }
        memcpy(tmp, dir->path, n);
        if (n > 0) {
            tmp[n++] = '/';
        }
    }
    if (n + path_len + 1 >= sizeof(tmp)) {
        return SRT_ENAMETOOLONG;
    }
    for (i = 0; i < path_len; i++) {
        char c = (char)src[i];
        if (c == '\0') {
            return SRT_EINVAL;
        }
        tmp[n++] = (c == '\\') ? '/' : c;
    }
    tmp[n] = '\0';

    /* Normalise: drop empty and "." components, apply "..". */
    i = 0;
    while (i < n) {
        size_t s = i;
        size_t len;
        while (i < n && tmp[i] != '/') {
            i++;
        }
        len = i - s;
        if (i < n) {
            i++;
        }
        if (len == 0 || (len == 1 && tmp[s] == '.')) {
            continue;
        }
        if (len == 2 && tmp[s] == '.' && tmp[s + 1] == '.') {
            if (olen == 0) {
                return SRT_ENOTCAPABLE;
            }
            while (olen > 0 && out[olen - 1] != '/') {
                olen--;
            }
            if (olen > 0) {
                olen--;
            }
            continue;
        }
        if (olen + (olen > 0 ? 1 : 0) + len + 1 > cap) {
            return SRT_ENAMETOOLONG;
        }
        if (olen > 0) {
            out[olen++] = '/';
        }
        memcpy(out + olen, tmp + s, len);
        olen += len;
    }
    out[olen] = '\0';
    return 0;
}

/* Join the root with a resolved relative path into a host path. */
static int srt_host_path(const char* rel, char* out, size_t cap) {
    size_t rl = strlen(srt_g.root);
    size_t pl = strlen(rel);
    if (rl + 1 + pl + 1 > cap) {
        return SRT_ENAMETOOLONG;
    }
    memcpy(out, srt_g.root, rl);
    if (pl > 0) {
        if (rl == 0 || out[rl - 1] != '/') {
            out[rl++] = '/';
        }
        memcpy(out + rl, rel, pl);
    }
    out[rl + pl] = '\0';
    return 0;
}

static int srt_resolve_host(U32 dirfd, U32 path_ptr, U32 path_len, char* rel, char* host) {
    int e = srt_resolve(srt_fd_get(dirfd), path_ptr, path_len, rel, SRT_MAX_PATH);
    if (e != 0) {
        return e;
    }
    return srt_host_path(rel, host, SRT_MAX_PATH);
}

static void srt_write_filestat(U32 buf, const srt_stat* st) {
    U32 i;
    for (i = 0; i < 64; i += 8) {
        put_u64(buf + i, 0);
    }
    put_u8(buf + 16, st->type);
    put_u64(buf + 24, 1);
    put_u64(buf + 32, st->size);
    put_u64(buf + 40, st->mtime_ns);
    put_u64(buf + 48, st->mtime_ns);
    put_u64(buf + 56, st->mtime_ns);
}

static U64 srt_next_random(void) {
    /* xorshift64* */
    U64 x = srt_g.det_random;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    srt_g.det_random = x;
    return x * W2C2_LL(2685821657736338717);
}

static void srt_stderr(const char* text, size_t len) {
    if (srt_g.config.stderr_sink != NULL) {
        srt_g.config.stderr_sink(text, len, srt_g.config.stderr_user);
    } else {
        srt_plat_log(2, text, len);
    }
}

/* ---- args / environ ---- */

static U32 srt_strings_get(int count, const char* const* strs, U32 ptrs, U32 buf) {
    int i;
    for (i = 0; i < count; i++) {
        U32 len = (U32)strlen(strs[i]) + 1;
        CHECK_MEM(ptrs + 4 * (U32)i, 4);
        CHECK_MEM(buf, len);
        put_u32(ptrs + 4 * (U32)i, buf);
        memcpy(srt_mem_ptr(buf), strs[i], len);
        buf += len;
    }
    return 0;
}

static U32 srt_strings_sizes(int count, const char* const* strs, U32 count_ptr, U32 size_ptr) {
    int i;
    U32 total = 0;
    CHECK_MEM(count_ptr, 4);
    CHECK_MEM(size_ptr, 4);
    for (i = 0; i < count; i++) {
        total += (U32)strlen(strs[i]) + 1;
    }
    put_u32(count_ptr, (U32)count);
    put_u32(size_ptr, total);
    return 0;
}

U32 WASI_FN(8, argsX5Fget)(wasmModuleInstance* inst, U32 argv, U32 buf) {
    (void)inst;
    srt_g.wasi_calls++;
    return srt_strings_get(srt_g.config.argc, srt_g.config.argv, argv, buf);
}

U32 WASI_FN(14, argsX5FsizesX5Fget)(wasmModuleInstance* inst, U32 count_ptr, U32 size_ptr) {
    (void)inst;
    srt_g.wasi_calls++;
    return srt_strings_sizes(srt_g.config.argc, srt_g.config.argv, count_ptr, size_ptr);
}

U32 WASI_FN(11, environX5Fget)(wasmModuleInstance* inst, U32 env, U32 buf) {
    (void)inst;
    srt_g.wasi_calls++;
    return srt_strings_get(srt_g.config.envc, srt_g.config.envp, env, buf);
}

U32 WASI_FN(17, environX5FsizesX5Fget)(wasmModuleInstance* inst, U32 count_ptr, U32 size_ptr) {
    (void)inst;
    srt_g.wasi_calls++;
    return srt_strings_sizes(srt_g.config.envc, srt_g.config.envp, count_ptr, size_ptr);
}

/* ---- clocks ---- */

static U64 srt_clock_now(U32 id) {
    if (srt_g.config.deterministic) {
        /* Every query advances time a little so busy-wait loops end. */
        srt_g.det_clock_ns += 1000000;
    }
    return id == 0 ? srt_clock_real_ns() : srt_clock_mono_ns();
}

U32 WASI_FN(13, clockX5FresX5Fget)(wasmModuleInstance* inst, U32 id, U32 res_ptr) {
    (void)inst;
    srt_g.wasi_calls++;
    if (id > 3) {
        return SRT_EINVAL;
    }
    CHECK_MEM(res_ptr, 8);
    put_u64(res_ptr, 1000);
    return 0;
}

U32 WASI_FN(14, clockX5FtimeX5Fget)(wasmModuleInstance* inst, U32 id, U64 precision, U32 time_ptr) {
    (void)inst;
    (void)precision;
    srt_g.wasi_calls++;
    if (id > 3) {
        return SRT_EINVAL;
    }
    CHECK_MEM(time_ptr, 8);
    put_u64(time_ptr, srt_clock_now(id));
    return 0;
}

/* ---- fd operations ---- */

U32 WASI_FN(9, fdX5Fadvise)(wasmModuleInstance* inst, U32 fd, U64 offset, U64 len, U32 advice) {
    (void)inst; (void)offset; (void)len; (void)advice;
    srt_g.wasi_calls++;
    return srt_fd_get(fd) != NULL ? 0 : SRT_EBADF;
}

U32 WASI_FN(11, fdX5Fallocate)(wasmModuleInstance* inst, U32 fd, U64 offset, U64 len) {
    (void)inst; (void)fd; (void)offset; (void)len;
    srt_g.wasi_calls++;
    return SRT_ENOTSUP;
}

U32 WASI_FN(8, fdX5Fclose)(wasmModuleInstance* inst, U32 fd) {
    srt_fd* f;
    (void)inst;
    srt_g.wasi_calls++;
    f = srt_fd_get(fd);
    if (f == NULL) {
        return SRT_EBADF;
    }
    if (fd < 4) {
        /* Standard streams and the root preopen stay open. */
        return 0;
    }
    if (f->kind == FDK_FILE && f->file != NULL) {
        srt_plat_close(f->file);
    }
    free(f->path);
    memset(f, 0, sizeof(*f));
    return 0;
}

U32 WASI_FN(11, fdX5Fdatasync)(wasmModuleInstance* inst, U32 fd) {
    srt_fd* f;
    (void)inst;
    srt_g.wasi_calls++;
    f = srt_fd_get(fd);
    if (f == NULL) {
        return SRT_EBADF;
    }
    return f->kind == FDK_FILE ? (U32)srt_plat_sync(f->file) : 0;
}

U32 WASI_FN(13, fdX5FfdstatX5Fget)(wasmModuleInstance* inst, U32 fd, U32 buf) {
    srt_fd* f;
    U8 type;
    (void)inst;
    srt_g.wasi_calls++;
    f = srt_fd_get(fd);
    if (f == NULL) {
        return SRT_EBADF;
    }
    CHECK_MEM(buf, 24);
    switch (f->kind) {
    case FDK_DIR: type = SRT_FT_DIRECTORY; break;
    case FDK_FILE: type = SRT_FT_REGULAR; break;
    default: type = SRT_FT_CHARDEV; break;
    }
    put_u8(buf, type);
    put_u8(buf + 1, 0);
    put_u16(buf + 2, (U16)(f->append ? 1 : 0));
    put_u32(buf + 4, 0);
    put_u64(buf + 8, RIGHTS_ALL);
    put_u64(buf + 16, RIGHTS_ALL);
    return 0;
}

U32 WASI_FN(19, fdX5FfdstatX5FsetX5Fflags)(wasmModuleInstance* inst, U32 fd, U32 flags) {
    srt_fd* f;
    (void)inst;
    srt_g.wasi_calls++;
    f = srt_fd_get(fd);
    if (f == NULL) {
        return SRT_EBADF;
    }
    f->append = (flags & 1) != 0;
    return 0;
}

U32 WASI_FN(20, fdX5FfdstatX5FsetX5Frights)(wasmModuleInstance* inst, U32 fd, U64 base, U64 inheriting) {
    (void)inst; (void)base; (void)inheriting;
    srt_g.wasi_calls++;
    return srt_fd_get(fd) != NULL ? 0 : SRT_EBADF;
}

U32 WASI_FN(15, fdX5FfilestatX5Fget)(wasmModuleInstance* inst, U32 fd, U32 buf) {
    srt_fd* f;
    srt_stat st;
    int e;
    (void)inst;
    srt_g.wasi_calls++;
    f = srt_fd_get(fd);
    if (f == NULL) {
        return SRT_EBADF;
    }
    CHECK_MEM(buf, 64);
    memset(&st, 0, sizeof(st));
    if (f->kind == FDK_FILE) {
        e = srt_plat_fstat(f->file, &st);
        if (e != 0) {
            return (U32)e;
        }
    } else if (f->kind == FDK_DIR) {
        st.type = SRT_FT_DIRECTORY;
    } else {
        st.type = SRT_FT_CHARDEV;
    }
    srt_write_filestat(buf, &st);
    return 0;
}

U32 WASI_FN(20, fdX5FfilestatX5FsetX5Fsize)(wasmModuleInstance* inst, U32 fd, U64 size) {
    srt_fd* f;
    (void)inst;
    srt_g.wasi_calls++;
    f = srt_fd_get(fd);
    if (f == NULL) {
        return SRT_EBADF;
    }
    if (f->kind != FDK_FILE) {
        return SRT_EINVAL;
    }
    return (U32)srt_plat_truncate(f->file, size);
}

U32 WASI_FN(21, fdX5FfilestatX5FsetX5Ftimes)(wasmModuleInstance* inst, U32 fd, U64 atim, U64 mtim, U32 flags) {
    (void)inst; (void)atim; (void)mtim; (void)flags;
    srt_g.wasi_calls++;
    return srt_fd_get(fd) != NULL ? 0 : SRT_EBADF;
}

/* Gathers iovecs and performs the transfer one buffer at a time. */
static U32 srt_do_read(srt_fd* f, U32 iovs, U32 iovs_len, U32* total) {
    U32 i;
    *total = 0;
    CHECK_MEM(iovs, iovs_len * 8);
    for (i = 0; i < iovs_len; i++) {
        U32 ptr = get_u32(iovs + i * 8);
        U32 len = get_u32(iovs + i * 8 + 4);
        size_t got = 0;
        int e = 0;
        CHECK_MEM(ptr, len);
        if (len == 0) {
            continue;
        }
        if (f->kind == FDK_STDIN) {
            /* Block for the first buffer only; then take what is there. */
            got = srt_pipe_read(&srt_g.stdin_pipe, srt_mem_ptr(ptr), len, *total == 0);
        } else if (f->kind == FDK_FILE) {
            e = srt_plat_read(f->file, srt_mem_ptr(ptr), len, &got);
        } else if (f->kind == FDK_DIR) {
            return SRT_EISDIR;
        } else {
            return SRT_EBADF;
        }
        if (e != 0) {
            return *total > 0 ? 0 : (U32)e;
        }
        *total += (U32)got;
        if (got < len) {
            break;
        }
    }
    return 0;
}

static U32 srt_do_write(srt_fd* f, U32 iovs, U32 iovs_len, U32* total) {
    U32 i;
    *total = 0;
    CHECK_MEM(iovs, iovs_len * 8);
    for (i = 0; i < iovs_len; i++) {
        U32 ptr = get_u32(iovs + i * 8);
        U32 len = get_u32(iovs + i * 8 + 4);
        size_t done = len;
        int e = 0;
        CHECK_MEM(ptr, len);
        if (len == 0) {
            continue;
        }
        switch (f->kind) {
        case FDK_STDOUT:
            srt_pipe_write(&srt_g.stdout_pipe, srt_mem_ptr(ptr), len);
            break;
        case FDK_STDERR:
            srt_stderr((const char*)srt_mem_ptr(ptr), len);
            break;
        case FDK_FILE:
            if (f->append) {
                U64 pos;
                srt_plat_seek(f->file, 0, SRT_SEEK_END, &pos);
            }
            e = srt_plat_write(f->file, srt_mem_ptr(ptr), len, &done);
            break;
        default:
            return SRT_EBADF;
        }
        if (e != 0) {
            return *total > 0 ? 0 : (U32)e;
        }
        *total += (U32)done;
    }
    return 0;
}

U32 WASI_FN(7, fdX5Fread)(wasmModuleInstance* inst, U32 fd, U32 iovs, U32 iovs_len, U32 nread) {
    srt_fd* f;
    U32 total;
    U32 e;
    (void)inst;
    srt_g.wasi_calls++;
    f = srt_fd_get(fd);
    if (f == NULL) {
        return SRT_EBADF;
    }
    CHECK_MEM(nread, 4);
    e = srt_do_read(f, iovs, iovs_len, &total);
    if (e == 0) {
        put_u32(nread, total);
    }
    return e;
}

U32 WASI_FN(8, fdX5Fwrite)(wasmModuleInstance* inst, U32 fd, U32 iovs, U32 iovs_len, U32 nwritten) {
    srt_fd* f;
    U32 total;
    U32 e;
    (void)inst;
    srt_g.wasi_calls++;
    f = srt_fd_get(fd);
    if (f == NULL) {
        return SRT_EBADF;
    }
    CHECK_MEM(nwritten, 4);
    e = srt_do_write(f, iovs, iovs_len, &total);
    if (e == 0) {
        put_u32(nwritten, total);
    }
    return e;
}

static U32 srt_positioned(srt_fd* f, U32 iovs, U32 iovs_len, U64 offset, U32 out, int writing) {
    U64 saved;
    U64 dummy;
    U32 total = 0;
    U32 e;
    int pe;
    if (f->kind != FDK_FILE) {
        return SRT_ESPIPE;
    }
    CHECK_MEM(out, 4);
    pe = srt_plat_seek(f->file, 0, SRT_SEEK_CUR, &saved);
    if (pe != 0) {
        return (U32)pe;
    }
    pe = srt_plat_seek(f->file, (I64)offset, SRT_SEEK_SET, &dummy);
    if (pe != 0) {
        return (U32)pe;
    }
    if (writing) {
        int was_append = f->append;
        f->append = 0;
        e = srt_do_write(f, iovs, iovs_len, &total);
        f->append = was_append;
    } else {
        e = srt_do_read(f, iovs, iovs_len, &total);
    }
    srt_plat_seek(f->file, (I64)saved, SRT_SEEK_SET, &dummy);
    if (e == 0) {
        put_u32(out, total);
    }
    return e;
}

U32 WASI_FN(8, fdX5Fpread)(wasmModuleInstance* inst, U32 fd, U32 iovs, U32 iovs_len, U64 offset, U32 nread) {
    srt_fd* f;
    (void)inst;
    srt_g.wasi_calls++;
    f = srt_fd_get(fd);
    if (f == NULL) {
        return SRT_EBADF;
    }
    return srt_positioned(f, iovs, iovs_len, offset, nread, 0);
}

U32 WASI_FN(9, fdX5Fpwrite)(wasmModuleInstance* inst, U32 fd, U32 iovs, U32 iovs_len, U64 offset, U32 nwritten) {
    srt_fd* f;
    (void)inst;
    srt_g.wasi_calls++;
    f = srt_fd_get(fd);
    if (f == NULL) {
        return SRT_EBADF;
    }
    return srt_positioned(f, iovs, iovs_len, offset, nwritten, 1);
}

U32 WASI_FN(14, fdX5FprestatX5Fget)(wasmModuleInstance* inst, U32 fd, U32 buf) {
    srt_fd* f;
    (void)inst;
    srt_g.wasi_calls++;
    f = srt_fd_get(fd);
    if (f == NULL || !f->preopen) {
        return SRT_EBADF;
    }
    CHECK_MEM(buf, 8);
    put_u8(buf, 0);
    put_u8(buf + 1, 0);
    put_u16(buf + 2, 0);
    put_u32(buf + 4, 1); /* name "/" */
    return 0;
}

U32 WASI_FN(19, fdX5FprestatX5FdirX5Fname)(wasmModuleInstance* inst, U32 fd, U32 path, U32 path_len) {
    srt_fd* f;
    (void)inst;
    srt_g.wasi_calls++;
    f = srt_fd_get(fd);
    if (f == NULL || !f->preopen) {
        return SRT_EBADF;
    }
    if (path_len < 1) {
        return SRT_ENAMETOOLONG;
    }
    CHECK_MEM(path, 1);
    put_u8(path, '/');
    return 0;
}

U32 WASI_FN(10, fdX5Freaddir)(wasmModuleInstance* inst, U32 fd, U32 buf, U32 buf_len, U64 cookie, U32 bufused) {
    srt_fd* f;
    char host[SRT_MAX_PATH];
    srt_dir* d;
    const char* name;
    U8 type;
    U64 index = 0;
    U32 used = 0;
    int e;
    (void)inst;
    srt_g.wasi_calls++;
    f = srt_fd_get(fd);
    if (f == NULL) {
        return SRT_EBADF;
    }
    if (f->kind != FDK_DIR) {
        return SRT_ENOTDIR;
    }
    CHECK_MEM(buf, buf_len);
    CHECK_MEM(bufused, 4);
    e = srt_host_path(f->path, host, sizeof(host));
    if (e != 0) {
        return (U32)e;
    }
    e = srt_plat_opendir(host, &d);
    if (e != 0) {
        return (U32)e;
    }
    /* Cookie = number of entries already returned. Simple and stateless. */
    while (used < buf_len && srt_plat_readdir(d, &name, &type) == 0) {
        U8 header[24];
        U32 namelen;
        U32 i;
        U32 chunk;
        U8* dst;
        index++;
        if (index <= cookie) {
            continue;
        }
        namelen = (U32)strlen(name);
        /* Build the little-endian dirent header byte by byte. */
        memset(header, 0, sizeof(header));
        for (i = 0; i < 8; i++) {
            header[i] = (U8)(index >> (8 * i));
            header[8 + i] = (U8)((index * W2C2_LL(0x9E3779B1)) >> (8 * i));
        }
        for (i = 0; i < 4; i++) {
            header[16 + i] = (U8)(namelen >> (8 * i));
        }
        header[20] = type;
        dst = srt_mem_ptr(buf);
        chunk = buf_len - used < 24 ? buf_len - used : 24;
        memcpy(dst + used, header, chunk);
        used += chunk;
        if (used >= buf_len) {
            break;
        }
        chunk = buf_len - used < namelen ? buf_len - used : namelen;
        memcpy(dst + used, name, chunk);
        used += chunk;
    }
    srt_plat_closedir(d);
    put_u32(bufused, used);
    return 0;
}

U32 WASI_FN(11, fdX5Frenumber)(wasmModuleInstance* inst, U32 fd, U32 to) {
    (void)inst; (void)fd; (void)to;
    srt_g.wasi_calls++;
    return SRT_ENOTSUP;
}

U32 WASI_FN(7, fdX5Fseek)(wasmModuleInstance* inst, U32 fd, U64 offset, U32 whence, U32 newoffset) {
    srt_fd* f;
    U64 pos;
    int e;
    (void)inst;
    srt_g.wasi_calls++;
    f = srt_fd_get(fd);
    if (f == NULL) {
        return SRT_EBADF;
    }
    if (f->kind != FDK_FILE) {
        return SRT_ESPIPE;
    }
    if (whence > 2) {
        return SRT_EINVAL;
    }
    CHECK_MEM(newoffset, 8);
    e = srt_plat_seek(f->file, (I64)offset, (int)whence, &pos);
    if (e != 0) {
        return (U32)e;
    }
    put_u64(newoffset, pos);
    return 0;
}

U32 WASI_FN(7, fdX5Fsync)(wasmModuleInstance* inst, U32 fd) {
    srt_fd* f;
    (void)inst;
    srt_g.wasi_calls++;
    f = srt_fd_get(fd);
    if (f == NULL) {
        return SRT_EBADF;
    }
    return f->kind == FDK_FILE ? (U32)srt_plat_sync(f->file) : 0;
}

U32 WASI_FN(7, fdX5Ftell)(wasmModuleInstance* inst, U32 fd, U32 offset_ptr) {
    srt_fd* f;
    U64 pos;
    int e;
    (void)inst;
    srt_g.wasi_calls++;
    f = srt_fd_get(fd);
    if (f == NULL) {
        return SRT_EBADF;
    }
    if (f->kind != FDK_FILE) {
        return SRT_ESPIPE;
    }
    CHECK_MEM(offset_ptr, 8);
    e = srt_plat_seek(f->file, 0, SRT_SEEK_CUR, &pos);
    if (e != 0) {
        return (U32)e;
    }
    put_u64(offset_ptr, pos);
    return 0;
}

/* ---- path operations ---- */

U32 WASI_FN(21, pathX5FcreateX5Fdirectory)(wasmModuleInstance* inst, U32 fd, U32 path, U32 path_len) {
    char rel[SRT_MAX_PATH];
    char host[SRT_MAX_PATH];
    int e;
    (void)inst;
    srt_g.wasi_calls++;
    e = srt_resolve_host(fd, path, path_len, rel, host);
    if (e != 0) {
        return (U32)e;
    }
    return (U32)srt_plat_mkdir(host);
}

U32 WASI_FN(17, pathX5FfilestatX5Fget)(wasmModuleInstance* inst, U32 fd, U32 flags, U32 path, U32 path_len, U32 buf) {
    char rel[SRT_MAX_PATH];
    char host[SRT_MAX_PATH];
    srt_stat st;
    int e;
    (void)inst;
    (void)flags;
    srt_g.wasi_calls++;
    CHECK_MEM(buf, 64);
    e = srt_resolve_host(fd, path, path_len, rel, host);
    if (e != 0) {
        return (U32)e;
    }
    memset(&st, 0, sizeof(st));
    e = srt_plat_stat(host, &st);
    if (e != 0) {
        return (U32)e;
    }
    srt_write_filestat(buf, &st);
    return 0;
}

U32 WASI_FN(23, pathX5FfilestatX5FsetX5Ftimes)(wasmModuleInstance* inst, U32 fd, U32 flags, U32 path, U32 path_len,
                                                U64 atim, U64 mtim, U32 fst_flags) {
    (void)inst; (void)fd; (void)flags; (void)path; (void)path_len; (void)atim; (void)mtim; (void)fst_flags;
    srt_g.wasi_calls++;
    return 0;
}

U32 WASI_FN(9, pathX5Flink)(wasmModuleInstance* inst, U32 old_fd, U32 old_flags, U32 old_path, U32 old_len,
                             U32 new_fd, U32 new_path, U32 new_len) {
    (void)inst; (void)old_fd; (void)old_flags; (void)old_path; (void)old_len;
    (void)new_fd; (void)new_path; (void)new_len;
    srt_g.wasi_calls++;
    return SRT_ENOTSUP;
}

#define OFLAG_CREAT     1
#define OFLAG_DIRECTORY 2
#define OFLAG_EXCL      4
#define OFLAG_TRUNC     8
#define RIGHT_FD_READ   (1u << 1)
#define RIGHT_FD_WRITE  (1u << 6)

U32 WASI_FN(9, pathX5Fopen)(wasmModuleInstance* inst, U32 fd, U32 dirflags, U32 path, U32 path_len,
                             U32 oflags, U64 rights_base, U64 rights_inheriting, U32 fdflags, U32 fd_out) {
    char rel[SRT_MAX_PATH];
    char host[SRT_MAX_PATH];
    srt_stat st;
    int e;
    int slot;
    int exists;
    srt_fd* nf;
    (void)inst;
    (void)dirflags;
    (void)rights_inheriting;
    srt_g.wasi_calls++;
    CHECK_MEM(fd_out, 4);
    e = srt_resolve_host(fd, path, path_len, rel, host);
    if (e != 0) {
        return (U32)e;
    }
    slot = srt_fd_alloc();
    if (slot < 0) {
        return SRT_EMFILE;
    }
    nf = &srt_fds[slot];
    memset(&st, 0, sizeof(st));
    exists = srt_plat_stat(host, &st) == 0;

    if (exists && st.type == SRT_FT_DIRECTORY) {
        if (oflags & (OFLAG_CREAT | OFLAG_TRUNC)) {
            return (oflags & OFLAG_EXCL) ? SRT_EEXIST : SRT_EISDIR;
        }
        nf->kind = FDK_DIR;
    } else {
        int flags = 0;
        srt_file* file;
        if (oflags & OFLAG_DIRECTORY) {
            return exists ? SRT_ENOTDIR : SRT_ENOENT;
        }
        if (rights_base & RIGHT_FD_READ) {
            flags |= SRT_OPEN_READ;
        }
        if (rights_base & RIGHT_FD_WRITE) {
            flags |= SRT_OPEN_WRITE;
        }
        if (flags == 0) {
            flags = SRT_OPEN_READ;
        }
        if (oflags & OFLAG_CREAT) {
            flags |= SRT_OPEN_CREATE | SRT_OPEN_WRITE;
        }
        if (oflags & OFLAG_EXCL) {
            flags |= SRT_OPEN_EXCL;
        }
        if (oflags & OFLAG_TRUNC) {
            flags |= SRT_OPEN_TRUNC | SRT_OPEN_WRITE;
        }
        if (fdflags & 1) {
            flags |= SRT_OPEN_APPEND;
        }
        e = srt_plat_open(host, flags, &file);
        if (e != 0) {
            return (U32)e;
        }
        nf->kind = FDK_FILE;
        nf->file = file;
        nf->append = (fdflags & 1) != 0;
    }
    nf->path = (char*)malloc(strlen(rel) + 1);
    if (nf->path == NULL) {
        if (nf->file != NULL) {
            srt_plat_close(nf->file);
        }
        memset(nf, 0, sizeof(*nf));
        return SRT_ENOMEM;
    }
    strcpy(nf->path, rel);
    put_u32(fd_out, (U32)slot);
    return 0;
}

U32 WASI_FN(13, pathX5Freadlink)(wasmModuleInstance* inst, U32 fd, U32 path, U32 path_len,
                                  U32 buf, U32 buf_len, U32 bufused) {
    (void)inst; (void)fd; (void)path; (void)path_len; (void)buf; (void)buf_len; (void)bufused;
    srt_g.wasi_calls++;
    return SRT_EINVAL; /* no symlinks in the mounted tree */
}

U32 WASI_FN(21, pathX5FremoveX5Fdirectory)(wasmModuleInstance* inst, U32 fd, U32 path, U32 path_len) {
    char rel[SRT_MAX_PATH];
    char host[SRT_MAX_PATH];
    int e;
    (void)inst;
    srt_g.wasi_calls++;
    e = srt_resolve_host(fd, path, path_len, rel, host);
    if (e != 0) {
        return (U32)e;
    }
    if (rel[0] == '\0') {
        return SRT_EACCES;
    }
    return (U32)srt_plat_rmdir(host);
}

U32 WASI_FN(11, pathX5Frename)(wasmModuleInstance* inst, U32 fd, U32 old_path, U32 old_len,
                                U32 new_fd, U32 new_path, U32 new_len) {
    char rel_a[SRT_MAX_PATH];
    char host_a[SRT_MAX_PATH];
    char rel_b[SRT_MAX_PATH];
    char host_b[SRT_MAX_PATH];
    int e;
    (void)inst;
    srt_g.wasi_calls++;
    e = srt_resolve_host(fd, old_path, old_len, rel_a, host_a);
    if (e != 0) {
        return (U32)e;
    }
    e = srt_resolve_host(new_fd, new_path, new_len, rel_b, host_b);
    if (e != 0) {
        return (U32)e;
    }
    return (U32)srt_plat_rename(host_a, host_b);
}

U32 WASI_FN(12, pathX5Fsymlink)(wasmModuleInstance* inst, U32 old_path, U32 old_len, U32 fd,
                                 U32 new_path, U32 new_len) {
    (void)inst; (void)old_path; (void)old_len; (void)fd; (void)new_path; (void)new_len;
    srt_g.wasi_calls++;
    return SRT_ENOTSUP;
}

U32 WASI_FN(16, pathX5FunlinkX5Ffile)(wasmModuleInstance* inst, U32 fd, U32 path, U32 path_len) {
    char rel[SRT_MAX_PATH];
    char host[SRT_MAX_PATH];
    int e;
    (void)inst;
    srt_g.wasi_calls++;
    e = srt_resolve_host(fd, path, path_len, rel, host);
    if (e != 0) {
        return (U32)e;
    }
    return (U32)srt_plat_unlink(host);
}

/* ---- poll / process / random / sched ---- */

/*
 * Only clock subscriptions are really waited on (they implement sleeps).
 * fd subscriptions report ready at once, except stdin, which is reported
 * ready only when it has data or is closed (otherwise it waits for the
 * shortest clock, or blocks if there is none).
 */
U32 WASI_FN(11, pollX5Foneoff)(wasmModuleInstance* inst, U32 in, U32 out, U32 nsubs, U32 nevents_ptr) {
    U32 i;
    U32 nevents = 0;
    U64 min_timeout = (U64)-1;
    int any_fd_ready = 0;
    (void)inst;
    srt_g.wasi_calls++;
    if (nsubs == 0) {
        return SRT_EINVAL;
    }
    CHECK_MEM(in, nsubs * 48);
    CHECK_MEM(out, nsubs * 32);
    CHECK_MEM(nevents_ptr, 4);

    for (i = 0; i < nsubs; i++) {
        U32 s = in + i * 48;
        U8 tag = get_u8(s + 8);
        if (tag == 0) {
            U64 timeout = get_u64(s + 24);
            U16 flags = get_u16(s + 40);
            if (flags & 1) {
                /* Absolute time: convert to a relative timeout. */
                U64 now = srt_clock_now(get_u32(s + 16));
                timeout = timeout > now ? timeout - now : 0;
            }
            if (timeout < min_timeout) {
                min_timeout = timeout;
            }
        } else {
            U32 fd = get_u32(s + 16);
            if (fd != 0) {
                any_fd_ready = 1;
            } else {
                srt_pipe* p = &srt_g.stdin_pipe;
                srt_mutex_lock(p->lock);
                if (p->end != p->start || p->closed) {
                    any_fd_ready = 1;
                }
                srt_mutex_unlock(p->lock);
            }
        }
    }

    if (!any_fd_ready) {
        if (min_timeout == (U64)-1) {
            /* Only a stdin subscription: block until it becomes readable. */
            srt_pipe* p = &srt_g.stdin_pipe;
            srt_mutex_lock(p->lock);
            while (p->end == p->start && !p->closed) {
                srt_cond_wait(p->changed, p->lock);
            }
            srt_mutex_unlock(p->lock);
            any_fd_ready = 1;
        } else if (min_timeout > 0) {
            srt_clock_wait_until(srt_clock_mono_ns() + min_timeout);
        }
    }

    for (i = 0; i < nsubs; i++) {
        U32 s = in + i * 48;
        U32 ev = out + nevents * 32;
        U8 tag = get_u8(s + 8);
        int fire;
        if (tag == 0) {
            fire = !any_fd_ready;
        } else {
            fire = any_fd_ready;
        }
        if (!fire) {
            continue;
        }
        put_u64(ev, get_u64(s));
        put_u16(ev + 8, 0);
        put_u8(ev + 10, tag);
        put_u8(ev + 11, 0);
        put_u32(ev + 12, 0);
        put_u64(ev + 16, tag == 0 ? 0 : 1);
        put_u16(ev + 24, 0);
        put_u16(ev + 26, 0);
        put_u32(ev + 28, 0);
        nevents++;
    }
    put_u32(nevents_ptr, nevents);
    return 0;
}

void WASI_FN(9, procX5Fexit)(wasmModuleInstance* inst, U32 code) {
    (void)inst;
    srt_g.wasi_calls++;
    srt_exit_guest((int)code);
}

U32 WASI_FN(10, procX5Fraise)(wasmModuleInstance* inst, U32 sig) {
    (void)inst;
    srt_g.wasi_calls++;
    srt_exit_guest(128 + (int)sig);
    return 0;
}

U32 WASI_FN(10, randomX5Fget)(wasmModuleInstance* inst, U32 buf, U32 len) {
    (void)inst;
    srt_g.wasi_calls++;
    CHECK_MEM(buf, len);
    if (srt_g.config.deterministic) {
        U32 i;
        U8* p = srt_mem_ptr(buf);
        for (i = 0; i < len; i++) {
            p[i] = (U8)(srt_next_random() >> 56);
        }
    } else {
        srt_plat_random(srt_mem_ptr(buf), len);
    }
    return 0;
}

U32 WASI_FN(11, schedX5Fyield)(wasmModuleInstance* inst) {
    (void)inst;
    srt_g.wasi_calls++;
    return 0;
}

/* ---- sockets: not supported ---- */

U32 WASI_FN(11, sockX5Faccept)(wasmModuleInstance* inst, U32 fd, U32 flags, U32 fd_out) {
    (void)inst; (void)fd; (void)flags; (void)fd_out;
    return SRT_ENOTSUP;
}

U32 WASI_FN(9, sockX5Frecv)(wasmModuleInstance* inst, U32 fd, U32 ri_data, U32 ri_len, U32 ri_flags,
                             U32 ro_datalen, U32 ro_flags) {
    (void)inst; (void)fd; (void)ri_data; (void)ri_len; (void)ri_flags; (void)ro_datalen; (void)ro_flags;
    return SRT_ENOTSUP;
}

U32 WASI_FN(9, sockX5Fsend)(wasmModuleInstance* inst, U32 fd, U32 si_data, U32 si_len, U32 si_flags,
                             U32 so_datalen) {
    (void)inst; (void)fd; (void)si_data; (void)si_len; (void)si_flags; (void)so_datalen;
    return SRT_ENOTSUP;
}

U32 WASI_FN(13, sockX5Fshutdown)(wasmModuleInstance* inst, U32 fd, U32 how) {
    (void)inst; (void)fd; (void)how;
    return SRT_ENOTSUP;
}
