#include "thread.h"

#if defined(_XBOX)
#include <xtl.h>
#include <ppcintrinsics.h> /* MemoryBarrier (lwsync) */
#define SPIRE_WIN_THREADS 1
#elif defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#define SPIRE_WIN_THREADS 1
#else
#include <pthread.h>
#include <time.h>
#if defined(__XSDK__)
#include <xsdk/thread.h> /* xsdk_thread_move */
#endif
#endif

namespace spire {

#if defined(SPIRE_WIN_THREADS)

long atomic_add(volatile long* v, long delta) { return InterlockedExchangeAdd((LONG volatile*)v, delta) + delta; }

Mutex::Mutex() {
    CRITICAL_SECTION* cs = new CRITICAL_SECTION;
    InitializeCriticalSection(cs); /* recursive */
    impl_ = cs;
}

Mutex::~Mutex() {
    DeleteCriticalSection((CRITICAL_SECTION*)impl_);
    delete (CRITICAL_SECTION*)impl_;
}

void Mutex::lock() { EnterCriticalSection((CRITICAL_SECTION*)impl_); }
void Mutex::unlock() { LeaveCriticalSection((CRITICAL_SECTION*)impl_); }

namespace {
struct Start {
    Thread::Function fn;
    void* arg;
    HANDLE handle;
};

DWORD WINAPI thread_main(LPVOID p) {
    Start* s = (Start*)p;
    s->fn(s->arg);
    return 0;
}
} /* namespace */

bool Thread::start(Function fn, void* arg, int hw_thread) {
    if (impl_) return false;
    Start* s = new Start;
    s->fn = fn;
    s->arg = arg;
    s->handle = CreateThread(NULL, 256 * 1024, thread_main, s, CREATE_SUSPENDED, NULL);
    if (!s->handle) {
        delete s;
        return false;
    }
#if defined(_XBOX)
    if (hw_thread >= 0 && hw_thread < 6) XSetThreadProcessor(s->handle, (DWORD)hw_thread);
#else
    (void)hw_thread;
#endif
    ResumeThread(s->handle);
    impl_ = s;
    return true;
}

void Thread::join() {
    if (!impl_) return;
    Start* s = (Start*)impl_;
    WaitForSingleObject(s->handle, INFINITE);
    CloseHandle(s->handle);
    delete s;
    impl_ = 0;
}

void sleep_ms(int ms) { Sleep((DWORD)ms); }

/* Workers: an auto-reset start event per worker, one done event set by
 * the last worker to finish. The console's interlocked operations carry no
 * barrier: lwsync orders a worker's results before its count goes down. */
namespace {
struct WorkerArg {
    Workers* w;
    int index;
};
struct WorkerSync {
    HANDLE start[4];
    HANDLE done;
    WorkerArg arg[4];
};
} /* namespace */

bool Workers::start(int count, const int* hw_threads) {
    if (count_ || count <= 0) return false;
    if (count > MAX) count = MAX;
    WorkerSync* s = new WorkerSync;
    s->done = CreateEvent(NULL, FALSE, FALSE, NULL);
    impl_ = s;
    for (int i = 0; i < count; ++i) {
        s->start[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
        s->arg[i].w = this;
        s->arg[i].index = i;
        if (!s->start[i] || !threads_[i].start(worker_main, &s->arg[i], hw_threads ? hw_threads[i] : -1)) {
            if (s->start[i]) CloseHandle(s->start[i]);
            break;
        }
        ++count_;
    }
    return count_ > 0;
}

void Workers::worker_main(void* arg) {
    WorkerArg* a = (WorkerArg*)arg;
    Workers* w = a->w;
    WorkerSync* s = (WorkerSync*)w->impl_;
    for (;;) {
        WaitForSingleObject(s->start[a->index], INFINITE);
        if (w->quit_) break;
        w->work();
        MemoryBarrier();
        if (InterlockedDecrement((LONG volatile*)&w->pending_) == 0) SetEvent(s->done);
    }
}

void Workers::work() {
    for (;;) {
        long i = InterlockedIncrement((LONG volatile*)&next_) - 1;
        if (i >= n_) break;
        fn_(ctx_, (int)i);
    }
}

void Workers::run(Job fn, void* ctx, int n) {
    if (n <= 0) return;
    if (count_ == 0 || n == 1) {
        for (int i = 0; i < n; ++i) fn(ctx, i);
        return;
    }
    WorkerSync* s = (WorkerSync*)impl_;
    fn_ = fn;
    ctx_ = ctx;
    n_ = n;
    next_ = 0;
    pending_ = count_;
    MemoryBarrier();
    for (int i = 0; i < count_; ++i) SetEvent(s->start[i]);
    work();
    WaitForSingleObject(s->done, INFINITE);
    MemoryBarrier();
}

Workers::~Workers() {
    if (!impl_) return;
    WorkerSync* s = (WorkerSync*)impl_;
    quit_ = true;
    MemoryBarrier();
    for (int i = 0; i < count_; ++i) SetEvent(s->start[i]);
    for (int i = 0; i < count_; ++i) {
        threads_[i].join();
        CloseHandle(s->start[i]);
    }
    CloseHandle(s->done);
    delete s;
}

#else

long atomic_add(volatile long* v, long delta) { return __sync_add_and_fetch(v, delta); }

Mutex::Mutex() {
    pthread_mutex_t* m = new pthread_mutex_t;
    pthread_mutexattr_t a;
    pthread_mutexattr_init(&a);
    pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(m, &a);
    pthread_mutexattr_destroy(&a);
    impl_ = m;
}

Mutex::~Mutex() {
    pthread_mutex_destroy((pthread_mutex_t*)impl_);
    delete (pthread_mutex_t*)impl_;
}

void Mutex::lock() { pthread_mutex_lock((pthread_mutex_t*)impl_); }
void Mutex::unlock() { pthread_mutex_unlock((pthread_mutex_t*)impl_); }

namespace {
struct Start {
    Thread::Function fn;
    void* arg;
    int hw_thread;
    pthread_t thread;
};

void* thread_main(void* p) {
    Start* s = (Start*)p;
#if defined(__XSDK__)
    /* xsdk starts a thread on its creator's hardware thread */
    if (s->hw_thread >= 0 && s->hw_thread < 6) xsdk_thread_move((uint32_t)s->hw_thread);
#endif
    s->fn(s->arg);
    return 0;
}
} /* namespace */

bool Thread::start(Function fn, void* arg, int hw_thread) {
    if (impl_) return false;
    Start* s = new Start;
    s->fn = fn;
    s->arg = arg;
    s->hw_thread = hw_thread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 256 * 1024);
    int failed = pthread_create(&s->thread, &attr, thread_main, s);
    pthread_attr_destroy(&attr);
    if (failed != 0) {
        delete s;
        return false;
    }
    impl_ = s;
    return true;
}

void Thread::join() {
    if (!impl_) return;
    Start* s = (Start*)impl_;
    pthread_join(s->thread, 0);
    delete s;
    impl_ = 0;
}

void sleep_ms(int ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, 0);
}

/* Workers: a generation count under one mutex wakes them; the last to
 * finish signals the caller. */
namespace {
struct WorkerArg {
    Workers* w;
    int index;
};
struct WorkerSync {
    pthread_mutex_t m;
    pthread_cond_t start, done;
    unsigned generation;
    WorkerArg arg[4];
};
} /* namespace */

bool Workers::start(int count, const int* hw_threads) {
    if (count_ || count <= 0) return false;
    if (count > MAX) count = MAX;
    WorkerSync* s = new WorkerSync;
    pthread_mutex_init(&s->m, 0);
    pthread_cond_init(&s->start, 0);
    pthread_cond_init(&s->done, 0);
    s->generation = 0;
    impl_ = s;
    for (int i = 0; i < count; ++i) {
        s->arg[i].w = this;
        s->arg[i].index = i;
        if (!threads_[i].start(worker_main, &s->arg[i], hw_threads ? hw_threads[i] : -1)) break;
        ++count_;
    }
    return count_ > 0;
}

void Workers::worker_main(void* arg) {
    WorkerArg* a = (WorkerArg*)arg;
    Workers* w = a->w;
    WorkerSync* s = (WorkerSync*)w->impl_;
    unsigned seen = 0;
    for (;;) {
        pthread_mutex_lock(&s->m);
        while (s->generation == seen && !w->quit_) pthread_cond_wait(&s->start, &s->m);
        seen = s->generation;
        bool quit = w->quit_;
        pthread_mutex_unlock(&s->m);
        if (quit) break;
        w->work();
        pthread_mutex_lock(&s->m);
        if (--w->pending_ == 0) pthread_cond_signal(&s->done);
        pthread_mutex_unlock(&s->m);
    }
}

void Workers::work() {
    for (;;) {
        long i = __sync_fetch_and_add(&next_, 1L);
        if (i >= n_) break;
        fn_(ctx_, (int)i);
    }
}

void Workers::run(Job fn, void* ctx, int n) {
    if (n <= 0) return;
    if (count_ == 0 || n == 1) {
        for (int i = 0; i < n; ++i) fn(ctx, i);
        return;
    }
    WorkerSync* s = (WorkerSync*)impl_;
    pthread_mutex_lock(&s->m);
    fn_ = fn;
    ctx_ = ctx;
    n_ = n;
    next_ = 0;
    pending_ = count_;
    ++s->generation;
    pthread_cond_broadcast(&s->start);
    pthread_mutex_unlock(&s->m);
    work();
    pthread_mutex_lock(&s->m);
    while (pending_ > 0) pthread_cond_wait(&s->done, &s->m);
    pthread_mutex_unlock(&s->m);
}

Workers::~Workers() {
    if (!impl_) return;
    WorkerSync* s = (WorkerSync*)impl_;
    pthread_mutex_lock(&s->m);
    quit_ = true;
    pthread_cond_broadcast(&s->start);
    pthread_mutex_unlock(&s->m);
    for (int i = 0; i < count_; ++i) threads_[i].join();
    pthread_cond_destroy(&s->start);
    pthread_cond_destroy(&s->done);
    pthread_mutex_destroy(&s->m);
    delete s;
}

#endif

Thread::Thread() : impl_(0) {}
Thread::~Thread() { join(); }

Workers::Workers() : count_(0), impl_(0), fn_(0), ctx_(0), n_(0), next_(0), pending_(0), quit_(false) {}

const char* volatile g_trace_phase = "";
const char* volatile g_trace_step = "";
const void* volatile g_trace_node = 0;
const char* volatile g_trace_audio = "";

} /* namespace spire */
