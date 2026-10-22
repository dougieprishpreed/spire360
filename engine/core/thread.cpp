#include "thread.h"

#if defined(_XBOX)
#include <xtl.h>
#define SPIRE_WIN_THREADS 1
#elif defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#define SPIRE_WIN_THREADS 1
#else
#include <pthread.h>
#include <unistd.h>
#endif

namespace spire {

#if defined(SPIRE_WIN_THREADS)

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

#else

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
    pthread_t thread;
};

void* thread_main(void* p) {
    Start* s = (Start*)p;
    s->fn(s->arg);
    return 0;
}
} /* namespace */

bool Thread::start(Function fn, void* arg, int hw_thread) {
    (void)hw_thread;
    if (impl_) return false;
    Start* s = new Start;
    s->fn = fn;
    s->arg = arg;
    if (pthread_create(&s->thread, 0, thread_main, s) != 0) {
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

void sleep_ms(int ms) { usleep((useconds_t)ms * 1000); }

#endif

Thread::Thread() : impl_(0) {}
Thread::~Thread() { join(); }

} /* namespace spire */
