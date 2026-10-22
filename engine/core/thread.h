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

} /* namespace spire */

#endif
