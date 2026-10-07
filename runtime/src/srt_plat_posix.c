/*
 * srt_plat_posix.c - POSIX backend for srt_plat.h (Linux, macOS; also used
 * for big-endian test runs under qemu-ppc).
 */
#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include <unistd.h>

#include "srt_plat.h"

struct srt_file {
    int fd;
};

struct srt_dir {
    DIR* dir;
};

static int srt_errno(int e) {
    switch (e) {
    case 0: return SRT_ESUCCESS;
    case E2BIG: return SRT_E2BIG;
    case EACCES: return SRT_EACCES;
    case EAGAIN: return SRT_EAGAIN;
    case EBADF: return SRT_EBADF;
    case EEXIST: return SRT_EEXIST;
    case EFAULT: return SRT_EFAULT;
    case EINVAL: return SRT_EINVAL;
    case EISDIR: return SRT_EISDIR;
    case ELOOP: return SRT_ELOOP;
    case EMFILE: return SRT_EMFILE;
    case ENAMETOOLONG: return SRT_ENAMETOOLONG;
    case ENOENT: return SRT_ENOENT;
    case ENOMEM: return SRT_ENOMEM;
    case ENOSPC: return SRT_ENOSPC;
    case ENOSYS: return SRT_ENOSYS;
    case ENOTDIR: return SRT_ENOTDIR;
    case ENOTEMPTY: return SRT_ENOTEMPTY;
    case EPERM: return SRT_EPERM;
    case ESPIPE: return SRT_ESPIPE;
    default: return SRT_EIO;
    }
}

int srt_plat_open(const char* path, int flags, srt_file** out) {
    int oflags = 0;
    int fd;
    srt_file* f;
    if ((flags & SRT_OPEN_READ) && (flags & SRT_OPEN_WRITE)) {
        oflags = O_RDWR;
    } else if (flags & SRT_OPEN_WRITE) {
        oflags = O_WRONLY;
    } else {
        oflags = O_RDONLY;
    }
    if (flags & SRT_OPEN_CREATE) oflags |= O_CREAT;
    if (flags & SRT_OPEN_TRUNC) oflags |= O_TRUNC;
    if (flags & SRT_OPEN_EXCL) oflags |= O_EXCL;
    if (flags & SRT_OPEN_APPEND) oflags |= O_APPEND;
    fd = open(path, oflags, 0644);
    if (fd < 0) {
        return srt_errno(errno);
    }
    f = (srt_file*)malloc(sizeof(*f));
    if (f == NULL) {
        close(fd);
        return SRT_ENOMEM;
    }
    f->fd = fd;
    *out = f;
    return 0;
}

int srt_plat_close(srt_file* f) {
    int r = close(f->fd);
    free(f);
    return r == 0 ? 0 : srt_errno(errno);
}

int srt_plat_read(srt_file* f, void* buf, size_t len, size_t* done) {
    ssize_t r = read(f->fd, buf, len);
    if (r < 0) {
        *done = 0;
        return srt_errno(errno);
    }
    *done = (size_t)r;
    return 0;
}

int srt_plat_write(srt_file* f, const void* buf, size_t len, size_t* done) {
    ssize_t r = write(f->fd, buf, len);
    if (r < 0) {
        *done = 0;
        return srt_errno(errno);
    }
    *done = (size_t)r;
    return 0;
}

int srt_plat_seek(srt_file* f, srt_i64 offset, int whence, srt_u64* newpos) {
    int w = whence == SRT_SEEK_SET ? SEEK_SET : (whence == SRT_SEEK_CUR ? SEEK_CUR : SEEK_END);
    off_t r = lseek(f->fd, (off_t)offset, w);
    if (r == (off_t)-1) {
        return srt_errno(errno);
    }
    *newpos = (srt_u64)r;
    return 0;
}

static void srt_fill_stat(const struct stat* s, srt_stat* st) {
    if (S_ISDIR(s->st_mode)) {
        st->type = SRT_FT_DIRECTORY;
    } else if (S_ISREG(s->st_mode)) {
        st->type = SRT_FT_REGULAR;
    } else if (S_ISCHR(s->st_mode)) {
        st->type = SRT_FT_CHARDEV;
    } else {
        st->type = SRT_FT_UNKNOWN;
    }
    st->size = (srt_u64)s->st_size;
    st->mtime_ns = (srt_u64)s->st_mtime * (srt_u64)1000000000;
}

int srt_plat_fstat(srt_file* f, srt_stat* st) {
    struct stat s;
    if (fstat(f->fd, &s) != 0) {
        return srt_errno(errno);
    }
    srt_fill_stat(&s, st);
    return 0;
}

int srt_plat_truncate(srt_file* f, srt_u64 size) {
    return ftruncate(f->fd, (off_t)size) == 0 ? 0 : srt_errno(errno);
}

int srt_plat_sync(srt_file* f) {
    return fsync(f->fd) == 0 ? 0 : srt_errno(errno);
}

int srt_plat_stat(const char* path, srt_stat* st) {
    struct stat s;
    if (stat(path, &s) != 0) {
        return srt_errno(errno);
    }
    srt_fill_stat(&s, st);
    return 0;
}

int srt_plat_mkdir(const char* path) {
    return mkdir(path, 0755) == 0 ? 0 : srt_errno(errno);
}

int srt_plat_rmdir(const char* path) {
    return rmdir(path) == 0 ? 0 : srt_errno(errno);
}

int srt_plat_unlink(const char* path) {
    return unlink(path) == 0 ? 0 : srt_errno(errno);
}

int srt_plat_rename(const char* from, const char* to) {
    return rename(from, to) == 0 ? 0 : srt_errno(errno);
}

int srt_plat_opendir(const char* path, srt_dir** out) {
    srt_dir* d;
    DIR* dir = opendir(path);
    if (dir == NULL) {
        return srt_errno(errno);
    }
    d = (srt_dir*)malloc(sizeof(*d));
    if (d == NULL) {
        closedir(dir);
        return SRT_ENOMEM;
    }
    d->dir = dir;
    *out = d;
    return 0;
}

int srt_plat_readdir(srt_dir* d, const char** name, srt_u8* type) {
    struct dirent* e;
    for (;;) {
        e = readdir(d->dir);
        if (e == NULL) {
            return SRT_ENOENT;
        }
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) {
            continue;
        }
        break;
    }
    *name = e->d_name;
#ifdef DT_DIR
    if (e->d_type == DT_DIR) {
        *type = SRT_FT_DIRECTORY;
    } else if (e->d_type == DT_REG) {
        *type = SRT_FT_REGULAR;
    } else {
        *type = SRT_FT_UNKNOWN;
    }
#else
    *type = SRT_FT_UNKNOWN;
#endif
    return 0;
}

void srt_plat_closedir(srt_dir* d) {
    closedir(d->dir);
    free(d);
}

srt_u64 srt_plat_time_realtime_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (srt_u64)ts.tv_sec * (srt_u64)1000000000 + (srt_u64)ts.tv_nsec;
}

srt_u64 srt_plat_time_monotonic_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (srt_u64)ts.tv_sec * (srt_u64)1000000000 + (srt_u64)ts.tv_nsec;
}

void srt_plat_sleep_ns(srt_u64 ns) {
    struct timespec ts;
    ts.tv_sec = (time_t)(ns / (srt_u64)1000000000);
    ts.tv_nsec = (long)(ns % (srt_u64)1000000000);
    nanosleep(&ts, NULL);
}

void srt_plat_random(void* buf, size_t len) {
    FILE* f = fopen("/dev/urandom", "rb");
    size_t got = 0;
    if (f != NULL) {
        got = fread(buf, 1, len, f);
        fclose(f);
    }
    if (got < len) {
        size_t i;
        unsigned char* p = (unsigned char*)buf;
        srt_u64 x = srt_plat_time_monotonic_ns() | 1;
        for (i = got; i < len; i++) {
            x ^= x << 13;
            x ^= x >> 7;
            x ^= x << 17;
            p[i] = (unsigned char)x;
        }
    }
}

static FILE* srt_log_file = NULL;

void srt_plat_set_log_file(const char* path) {
    if (srt_log_file != NULL) {
        fclose(srt_log_file);
        srt_log_file = NULL;
    }
    if (path != NULL) {
        srt_log_file = fopen(path, "ab");
    }
}

void srt_plat_log(int stream, const char* text, size_t len) {
    FILE* f = stream == 1 ? stdout : stderr;
    fwrite(text, 1, len, f);
    fflush(f);
    if (srt_log_file != NULL) {
        fwrite(text, 1, len, srt_log_file);
        fflush(srt_log_file);
    }
}

/* ---- threads ---- */

struct srt_mutex {
    pthread_mutex_t m;
};

struct srt_cond {
    pthread_cond_t c;
};

struct srt_thread {
    pthread_t t;
    srt_thread_fn fn;
    void* arg;
};

srt_mutex* srt_mutex_create(void) {
    srt_mutex* m = (srt_mutex*)malloc(sizeof(*m));
    if (m != NULL) {
        pthread_mutex_init(&m->m, NULL);
    }
    return m;
}

void srt_mutex_destroy(srt_mutex* m) {
    pthread_mutex_destroy(&m->m);
    free(m);
}

void srt_mutex_lock(srt_mutex* m) {
    pthread_mutex_lock(&m->m);
}

void srt_mutex_unlock(srt_mutex* m) {
    pthread_mutex_unlock(&m->m);
}

srt_cond* srt_cond_create(void) {
    srt_cond* c = (srt_cond*)malloc(sizeof(*c));
    if (c != NULL) {
        pthread_cond_init(&c->c, NULL);
    }
    return c;
}

void srt_cond_destroy(srt_cond* c) {
    pthread_cond_destroy(&c->c);
    free(c);
}

void srt_cond_wait(srt_cond* c, srt_mutex* m) {
    pthread_cond_wait(&c->c, &m->m);
}

void srt_cond_broadcast(srt_cond* c) {
    pthread_cond_broadcast(&c->c);
}

static void* srt_thread_main(void* p) {
    srt_thread* t = (srt_thread*)p;
    t->fn(t->arg);
    return NULL;
}

srt_thread* srt_thread_start(srt_thread_fn fn, void* arg, size_t stack_bytes, int hw_thread) {
    pthread_attr_t attr;
    srt_thread* t = (srt_thread*)malloc(sizeof(*t));
    (void)hw_thread;
    if (t == NULL) {
        return NULL;
    }
    t->fn = fn;
    t->arg = arg;
    pthread_attr_init(&attr);
    if (stack_bytes != 0) {
        pthread_attr_setstacksize(&attr, stack_bytes);
    }
    if (pthread_create(&t->t, &attr, srt_thread_main, t) != 0) {
        pthread_attr_destroy(&attr);
        free(t);
        return NULL;
    }
    pthread_attr_destroy(&attr);
    return t;
}

void srt_thread_join(srt_thread* t) {
    pthread_join(t->t, NULL);
    free(t);
}

void* srt_plat_reserve(srt_u64 bytes, srt_u64* got) {
    void* p;
    if (bytes > (srt_u64)(size_t)-1) {
        return NULL;
    }
    /* calloc of a large block is lazily committed zero pages on Linux, so
     * there is nothing left for srt_plat_commit to do. */
    p = calloc((size_t)bytes, 1);
    *got = p != NULL ? bytes : 0;
    return p;
}

int srt_plat_commit(void* base, srt_u64 from, srt_u64 to) {
    (void)base;
    (void)from;
    (void)to;
    return 0;
}

void srt_plat_release(void* p) {
    free(p);
}
