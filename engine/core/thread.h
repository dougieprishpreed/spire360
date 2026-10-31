/* Threads and locks (the audio thread): the Windows API on the XDK console
 * build (XSetThreadProcessor picks the hardware thread) and on Windows, POSIX
 * threads elsewhere (the xSDK console build: xsdk_thread_move picks it). */
#ifndef SPIRE_THREAD_H
#define SPIRE_THREAD_H

namespace spire {

/* *v += delta as one step (another thread's never lost); the new value. */
long atomic_add(volatile long* v, long delta);

/* A recursive lock. */
class Mutex {
public:
    Mutex();
    ~Mutex();
    void lock();
    void unlock();

private:
    Mutex(const Mutex&);
    Mutex& operator=(const Mutex&);
    void* impl_;
};

class ScopedLock {
public:
    explicit ScopedLock(Mutex& m) : m_(m) { m_.lock(); }
    ~ScopedLock() { m_.unlock(); }

private:
    ScopedLock(const ScopedLock&);
    ScopedLock& operator=(const ScopedLock&);
    Mutex& m_;
};

class Thread {
public:
    typedef void (*Function)(void* arg);
    Thread();
    ~Thread(); /* joins */
    /* fn(arg) on a new thread; hw_thread: the console's hardware thread
     * (0..5), -1 any. */
    bool start(Function fn, void* arg, int hw_thread = -1);
    void join();
    bool started() const { return impl_ != 0; }

private:
    Thread(const Thread&);
    Thread& operator=(const Thread&);
    void* impl_;
};

/* Worker threads that share a batch of jobs with the calling thread: run()
 * calls fn(ctx, i) for each i in [0, n), on the workers and the caller,
 * and returns once all are done (the console's spare hardware threads
 * take the particles' simulation). Without workers the caller runs them
 * all. */
class Workers {
public:
    typedef void (*Job)(void* ctx, int index);
    Workers();
    ~Workers(); /* stops and joins the workers */
    /* count workers, worker i on hw_threads[i] (NULL: any) */
    bool start(int count, const int* hw_threads);
    int count() const { return count_; }
    void run(Job fn, void* ctx, int n);

private:
    Workers(const Workers&);
    Workers& operator=(const Workers&);
    enum { MAX = 4 };
    static void worker_main(void* arg);
    void work();
    Thread threads_[MAX];
    int count_;
    void* impl_;
    Job fn_;
    void* ctx_;
    int n_;
    volatile long next_, pending_;
    volatile bool quit_;
};

void sleep_ms(int ms);
/* The calling thread a step below the others on its hardware thread (the
 * console's: they run first whenever they are ready); elsewhere nothing. */
void lower_priority();
/* A monotonic clock in milliseconds (the start is arbitrary). */
double clock_ms();

/* Where the main thread is, for a watchdog on another thread (the console
 * build logs it when a frame hangs): the frame loop's phase, the client's
 * step and the node being processed or drawn; and the audio thread's.
 * Plain stores, read only once the main thread has stopped. */
extern const char* volatile g_trace_phase;
extern const char* volatile g_trace_step;
extern const void* volatile g_trace_node;
extern const char* volatile g_trace_audio;

} /* namespace spire */

#endif
