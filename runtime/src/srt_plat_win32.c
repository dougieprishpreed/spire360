/*
 * srt_plat_win32.c - Win32 backend for srt_plat.h, shared by Windows PCs and
 * the Xbox 360 (XDK, _XBOX defined).
 *
 * Only APIs present in the Xbox 360 XDK's Win32 subset are used. Where the
 * console differs it is marked with _XBOX. Things to confirm on the first
 * XDK build are tagged "XDK-CHECK".
 */
#ifdef _XBOX
#include <xtl.h>
#else
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "srt_plat.h"

#define SRT_WPATH 1024

struct srt_file {
    HANDLE h;
};

struct srt_dir {
    HANDLE h;
    WIN32_FIND_DATAA data;
    int first;       /* data holds an entry not yet returned */
    int done;
};

static int srt_win_error(DWORD e) {
    switch (e) {
    case ERROR_SUCCESS: return SRT_ESUCCESS;
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
    case ERROR_INVALID_DRIVE:
        return SRT_ENOENT;
    case ERROR_ACCESS_DENIED:
    case ERROR_SHARING_VIOLATION:
        return SRT_EACCES;
    case ERROR_FILE_EXISTS:
    case ERROR_ALREADY_EXISTS:
        return SRT_EEXIST;
    case ERROR_DIR_NOT_EMPTY: return SRT_ENOTEMPTY;
    case ERROR_DISK_FULL: return SRT_ENOSPC;
    case ERROR_NOT_ENOUGH_MEMORY:
    case ERROR_OUTOFMEMORY:
        return SRT_ENOMEM;
    case ERROR_INVALID_NAME:
    case ERROR_BAD_PATHNAME:
        return SRT_EINVAL;
    case ERROR_FILENAME_EXCED_RANGE: return SRT_ENAMETOOLONG;
    case ERROR_TOO_MANY_OPEN_FILES: return SRT_EMFILE;
    case ERROR_HANDLE_EOF: return SRT_ESUCCESS;
    default: return SRT_EIO;
    }
}

/* '/' -> '\\'. Returns 0 or SRT_ENAMETOOLONG. */
static int srt_win_path(const char* path, char* out) {
    size_t i;
    size_t n = strlen(path);
    if (n + 1 > SRT_WPATH) {
        return SRT_ENAMETOOLONG;
    }
    for (i = 0; i <= n; i++) {
        out[i] = path[i] == '/' ? '\\' : path[i];
    }
    return 0;
}

int srt_plat_open(const char* path, int flags, srt_file** out) {
    char wp[SRT_WPATH];
    DWORD access = 0;
    DWORD disposition;
    HANDLE h;
    srt_file* f;
    int e = srt_win_path(path, wp);
    if (e != 0) {
        return e;
    }
    if (flags & SRT_OPEN_READ) {
        access |= GENERIC_READ;
    }
    if (flags & (SRT_OPEN_WRITE | SRT_OPEN_APPEND)) {
        access |= GENERIC_WRITE;
    }
    if (access == 0) {
        access = GENERIC_READ;
    }
    if ((flags & SRT_OPEN_CREATE) && (flags & SRT_OPEN_EXCL)) {
        disposition = CREATE_NEW;
    } else if ((flags & SRT_OPEN_CREATE) && (flags & SRT_OPEN_TRUNC)) {
        disposition = CREATE_ALWAYS;
    } else if (flags & SRT_OPEN_CREATE) {
        disposition = OPEN_ALWAYS;
    } else if (flags & SRT_OPEN_TRUNC) {
        disposition = TRUNCATE_EXISTING;
    } else {
        disposition = OPEN_EXISTING;
    }
    h = CreateFileA(wp, access, FILE_SHARE_READ, NULL, disposition, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        return srt_win_error(GetLastError());
    }
    f = (srt_file*)malloc(sizeof(*f));
    if (f == NULL) {
        CloseHandle(h);
        return SRT_ENOMEM;
    }
    f->h = h;
    *out = f;
    return 0;
}

int srt_plat_close(srt_file* f) {
    BOOL ok = CloseHandle(f->h);
    free(f);
    return ok ? 0 : SRT_EIO;
}

int srt_plat_read(srt_file* f, void* buf, size_t len, size_t* done) {
    DWORD got = 0;
    if (!ReadFile(f->h, buf, (DWORD)len, &got, NULL)) {
        DWORD e = GetLastError();
        *done = 0;
        return e == ERROR_HANDLE_EOF ? 0 : srt_win_error(e);
    }
    *done = got;
    return 0;
}

int srt_plat_write(srt_file* f, const void* buf, size_t len, size_t* done) {
    DWORD put = 0;
    if (!WriteFile(f->h, buf, (DWORD)len, &put, NULL)) {
        *done = 0;
        return srt_win_error(GetLastError());
    }
    *done = put;
    return 0;
}

int srt_plat_seek(srt_file* f, srt_i64 offset, int whence, srt_u64* newpos) {
    LONG high = (LONG)(offset >> 32);
    DWORD low;
    DWORD method = whence == SRT_SEEK_SET ? FILE_BEGIN : (whence == SRT_SEEK_CUR ? FILE_CURRENT : FILE_END);
    SetLastError(ERROR_SUCCESS);
    low = SetFilePointer(f->h, (LONG)(offset & 0xFFFFFFFF), &high, method);
    if (low == INVALID_SET_FILE_POINTER && GetLastError() != ERROR_SUCCESS) {
        return srt_win_error(GetLastError());
    }
    *newpos = ((srt_u64)(DWORD)high << 32) | low;
    return 0;
}

static srt_u64 srt_filetime_ns(const FILETIME* ft) {
    /* FILETIME: 100 ns since 1601-01-01. Unix epoch offset 11644473600 s. */
    srt_u64 t = ((srt_u64)ft->dwHighDateTime << 32) | ft->dwLowDateTime;
    srt_u64 offset = (srt_u64)11644473600 * (srt_u64)10000000;
    if (t < offset) {
        return 0;
    }
    return (t - offset) * 100;
}

int srt_plat_fstat(srt_file* f, srt_stat* st) {
    DWORD high = 0;
    DWORD low;
    FILETIME mtime;
    SetLastError(ERROR_SUCCESS);
    low = GetFileSize(f->h, &high);
    if (low == INVALID_FILE_SIZE && GetLastError() != ERROR_SUCCESS) {
        return srt_win_error(GetLastError());
    }
    st->type = SRT_FT_REGULAR;
    st->size = ((srt_u64)high << 32) | low;
    st->mtime_ns = 0;
    if (GetFileTime(f->h, NULL, NULL, &mtime)) {
        st->mtime_ns = srt_filetime_ns(&mtime);
    }
    return 0;
}

int srt_plat_truncate(srt_file* f, srt_u64 size) {
    srt_u64 saved;
    srt_u64 dummy;
    int e = srt_plat_seek(f, 0, SRT_SEEK_CUR, &saved);
    if (e != 0) {
        return e;
    }
    e = srt_plat_seek(f, (srt_i64)size, SRT_SEEK_SET, &dummy);
    if (e != 0) {
        return e;
    }
    if (!SetEndOfFile(f->h)) {
        e = srt_win_error(GetLastError());
    }
    srt_plat_seek(f, (srt_i64)saved, SRT_SEEK_SET, &dummy);
    return e;
}

int srt_plat_sync(srt_file* f) {
    return FlushFileBuffers(f->h) ? 0 : srt_win_error(GetLastError());
}

int srt_plat_stat(const char* path, srt_stat* st) {
    char wp[SRT_WPATH];
    WIN32_FILE_ATTRIBUTE_DATA a;
    int e = srt_win_path(path, wp);
    size_t n;
    if (e != 0) {
        return e;
    }
    /* A bare drive root such as "game:" needs its trailing separator. */
    n = strlen(wp);
    if (n > 0 && wp[n - 1] == ':' && n + 1 < SRT_WPATH) {
        wp[n] = '\\';
        wp[n + 1] = '\0';
    }
    /* XDK-CHECK: GetFileAttributesExA on the 360 file system. */
    if (!GetFileAttributesExA(wp, GetFileExInfoStandard, &a)) {
        return srt_win_error(GetLastError());
    }
    st->type = (a.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? SRT_FT_DIRECTORY : SRT_FT_REGULAR;
    st->size = ((srt_u64)a.nFileSizeHigh << 32) | a.nFileSizeLow;
    st->mtime_ns = srt_filetime_ns(&a.ftLastWriteTime);
    if (st->type == SRT_FT_REGULAR) {
        /* The size by path can be stale right after a write (Xenia reported
         * 11 bytes for a 12-byte file just appended to); a handle's size is
         * current. */
        HANDLE h = CreateFileA(wp, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            DWORD high = 0;
            DWORD low;
            SetLastError(ERROR_SUCCESS);
            low = GetFileSize(h, &high);
            if (!(low == INVALID_FILE_SIZE && GetLastError() != ERROR_SUCCESS)) {
                st->size = ((srt_u64)high << 32) | low;
            }
            CloseHandle(h);
        }
    }
    return 0;
}

int srt_plat_mkdir(const char* path) {
    char wp[SRT_WPATH];
    int e = srt_win_path(path, wp);
    if (e != 0) {
        return e;
    }
    return CreateDirectoryA(wp, NULL) ? 0 : srt_win_error(GetLastError());
}

int srt_plat_rmdir(const char* path) {
    char wp[SRT_WPATH];
    int e = srt_win_path(path, wp);
    if (e != 0) {
        return e;
    }
    return RemoveDirectoryA(wp) ? 0 : srt_win_error(GetLastError());
}

int srt_plat_unlink(const char* path) {
    char wp[SRT_WPATH];
    int e = srt_win_path(path, wp);
    if (e != 0) {
        return e;
    }
    return DeleteFileA(wp) ? 0 : srt_win_error(GetLastError());
}

int srt_plat_rename(const char* from, const char* to) {
    char a[SRT_WPATH];
    char b[SRT_WPATH];
    int e = srt_win_path(from, a);
    if (e == 0) {
        e = srt_win_path(to, b);
    }
    if (e != 0) {
        return e;
    }
    /* POSIX rename replaces the target; MoveFileA does not. */
    DeleteFileA(b);
    return MoveFileA(a, b) ? 0 : srt_win_error(GetLastError());
}

int srt_plat_opendir(const char* path, srt_dir** out) {
    char wp[SRT_WPATH];
    srt_dir* d;
    size_t n;
    int e = srt_win_path(path, wp);
    if (e != 0) {
        return e;
    }
    n = strlen(wp);
    if (n + 3 > SRT_WPATH) {
        return SRT_ENAMETOOLONG;
    }
    if (n > 0 && wp[n - 1] != '\\') {
        wp[n++] = '\\';
    }
    wp[n++] = '*';
    wp[n] = '\0';
    d = (srt_dir*)malloc(sizeof(*d));
    if (d == NULL) {
        return SRT_ENOMEM;
    }
    d->h = FindFirstFileA(wp, &d->data);
    d->done = 0;
    d->first = 1;
    if (d->h == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_NO_MORE_FILES) {
            /* Empty directory. */
            d->done = 1;
        } else {
            free(d);
            return srt_win_error(err);
        }
    }
    *out = d;
    return 0;
}

int srt_plat_readdir(srt_dir* d, const char** name, srt_u8* type) {
    for (;;) {
        if (d->done) {
            return SRT_ENOENT;
        }
        if (!d->first) {
            if (!FindNextFileA(d->h, &d->data)) {
                d->done = 1;
                return SRT_ENOENT;
            }
        }
        d->first = 0;
        if (strcmp(d->data.cFileName, ".") == 0 || strcmp(d->data.cFileName, "..") == 0) {
            continue;
        }
        *name = d->data.cFileName;
        *type = (d->data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? SRT_FT_DIRECTORY : SRT_FT_REGULAR;
        return 0;
    }
}

void srt_plat_closedir(srt_dir* d) {
    if (d->h != INVALID_HANDLE_VALUE) {
        FindClose(d->h);
    }
    free(d);
}

/* ---- clocks ---- */

srt_u64 srt_plat_time_realtime_ns(void) {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    return srt_filetime_ns(&ft);
}

srt_u64 srt_plat_time_monotonic_ns(void) {
    static LARGE_INTEGER freq;
    LARGE_INTEGER now;
    srt_u64 sec;
    srt_u64 rem;
    if (freq.QuadPart == 0) {
        QueryPerformanceFrequency(&freq);
    }
    QueryPerformanceCounter(&now);
    sec = (srt_u64)now.QuadPart / (srt_u64)freq.QuadPart;
    rem = (srt_u64)now.QuadPart % (srt_u64)freq.QuadPart;
    return sec * (srt_u64)1000000000 + rem * (srt_u64)1000000000 / (srt_u64)freq.QuadPart;
}

void srt_plat_sleep_ns(srt_u64 ns) {
    DWORD ms = (DWORD)(ns / 1000000);
    Sleep(ms == 0 && ns > 0 ? 1 : ms);
}

void srt_plat_random(void* buf, size_t len) {
    /* Weak by design: only seeds .NET hash randomisation and the like. */
    static srt_u64 x = 0;
    unsigned char* p = (unsigned char*)buf;
    size_t i;
    if (x == 0) {
        x = srt_plat_time_monotonic_ns() ^ ((srt_u64)GetTickCount() << 32) ^ (srt_u64)(size_t)buf;
        x |= 1;
    }
    for (i = 0; i < len; i++) {
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        p[i] = (unsigned char)(x >> 24);
    }
}

/* ---- log ---- */

static FILE* srt_log_file = NULL;

void srt_plat_set_log_file(const char* path) {
    char wp[SRT_WPATH];
    if (srt_log_file != NULL) {
        fclose(srt_log_file);
        srt_log_file = NULL;
    }
    if (path != NULL && srt_win_path(path, wp) == 0) {
        srt_log_file = fopen(wp, "ab");
    }
}

void srt_plat_log(int stream, const char* text, size_t len) {
    char chunk[512];
    while (len > 0) {
        size_t n = len < sizeof(chunk) - 1 ? len : sizeof(chunk) - 1;
        memcpy(chunk, text, n);
        chunk[n] = '\0';
        OutputDebugStringA(chunk);
#ifndef _XBOX
        fputs(chunk, stream == 1 ? stdout : stderr);
#endif
        text += n;
        len -= n;
        if (srt_log_file != NULL) {
            fwrite(chunk, 1, n, srt_log_file);
        }
    }
    if (srt_log_file != NULL) {
        fflush(srt_log_file);
    }
#ifdef _XBOX
    (void)stream;
#endif
}

/* ---- threads ---- */

struct srt_mutex {
    CRITICAL_SECTION cs;
};

/*
 * Condition variable from a semaphore and a waiter count. The Xbox 360 has
 * no CONDITION_VARIABLE. Correct for the runtime's usage: srt_cond_broadcast
 * is always called with the associated mutex held, and every wait is in a
 * loop that re-checks its predicate (spurious wake-ups are possible).
 */
struct srt_cond {
    HANDLE sema;
    LONG waiters;
};

struct srt_thread {
    HANDLE h;
    srt_thread_fn fn;
    void* arg;
};

srt_mutex* srt_mutex_create(void) {
    srt_mutex* m = (srt_mutex*)malloc(sizeof(*m));
    if (m != NULL) {
        InitializeCriticalSection(&m->cs);
    }
    return m;
}

void srt_mutex_destroy(srt_mutex* m) {
    DeleteCriticalSection(&m->cs);
    free(m);
}

void srt_mutex_lock(srt_mutex* m) {
    EnterCriticalSection(&m->cs);
}

void srt_mutex_unlock(srt_mutex* m) {
    LeaveCriticalSection(&m->cs);
}

srt_cond* srt_cond_create(void) {
    srt_cond* c = (srt_cond*)malloc(sizeof(*c));
    if (c != NULL) {
        c->sema = CreateSemaphoreA(NULL, 0, 0x7FFFFFFF, NULL);
        c->waiters = 0;
        if (c->sema == NULL) {
            free(c);
            return NULL;
        }
    }
    return c;
}

void srt_cond_destroy(srt_cond* c) {
    CloseHandle(c->sema);
    free(c);
}

void srt_cond_wait(srt_cond* c, srt_mutex* m) {
    c->waiters++;
    LeaveCriticalSection(&m->cs);
    WaitForSingleObject(c->sema, INFINITE);
    EnterCriticalSection(&m->cs);
}

void srt_cond_broadcast(srt_cond* c) {
    LONG n = c->waiters;
    c->waiters = 0;
    if (n > 0) {
        ReleaseSemaphore(c->sema, n, NULL);
    }
}

static DWORD WINAPI srt_thread_main(LPVOID p) {
    srt_thread* t = (srt_thread*)p;
    t->fn(t->arg);
    return 0;
}

srt_thread* srt_thread_start(srt_thread_fn fn, void* arg, size_t stack_bytes, int hw_thread) {
    srt_thread* t = (srt_thread*)malloc(sizeof(*t));
    if (t == NULL) {
        return NULL;
    }
    t->fn = fn;
    t->arg = arg;
    t->h = CreateThread(NULL, (SIZE_T)stack_bytes, srt_thread_main, t, CREATE_SUSPENDED, NULL);
    if (t->h == NULL) {
        free(t);
        return NULL;
    }
#ifdef _XBOX
    if (hw_thread >= 0 && hw_thread < 6) {
        XSetThreadProcessor(t->h, (DWORD)hw_thread);
    }
#else
    (void)hw_thread;
#endif
    ResumeThread(t->h);
    return t;
}

void srt_thread_join(srt_thread* t) {
    WaitForSingleObject(t->h, INFINITE);
    CloseHandle(t->h);
    free(t);
}

/* ---- linear memory ---- */

void* srt_plat_reserve(srt_u64 bytes, srt_u64* got) {
    void* p;
    if (bytes > (srt_u64)(SIZE_T)-1) {
        return NULL;
    }
    /* XDK-CHECK: MEM_RESERVE followed by incremental MEM_COMMIT. */
    p = VirtualAlloc(NULL, (SIZE_T)bytes, MEM_RESERVE, PAGE_READWRITE);
    *got = p != NULL ? bytes : 0;
    return p;
}

int srt_plat_commit(void* base, srt_u64 from, srt_u64 to) {
    /* Freshly committed pages are zero-filled by the OS. */
    if (to <= from) {
        return 0;
    }
    if (VirtualAlloc((char*)base + (SIZE_T)from, (SIZE_T)(to - from), MEM_COMMIT, PAGE_READWRITE) == NULL) {
        return SRT_ENOMEM;
    }
    return 0;
}

void srt_plat_release(void* p) {
    if (p != NULL) {
        VirtualFree(p, 0, MEM_RELEASE);
    }
}
