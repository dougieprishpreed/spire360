/* Threads and locks (the audio thread): the Windows API on the console
 * (XSetThreadProcessor picks the hardware thread) and on Windows, POSIX
 * threads elsewhere. */
#ifndef SPIRE_THREAD_H
#define SPIRE_THREAD_H

namespace spire {

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

void sleep_ms(int ms);

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
