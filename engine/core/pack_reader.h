/* Textures read from the pack ahead of their use on a thread of their own,
 * so the frame loop never waits for the console's USB drive: the warm-up's
 * reads were the frame's, a character select portrait 0.2-0.3 s with the
 * menu on the screen, Neow's 12 MB background 0.6 s behind the embark's
 * black. The canvas makes each texture from what was read (Canvas::
 * prefetch_async), or takes one a draw needs now (Canvas::load_gpu_texture:
 * a read under way is waited for, not made twice). At most AHEAD bytes
 * read and not taken; the rest wait their turn. */
#ifndef SPIRE_PACK_READER_H
#define SPIRE_PACK_READER_H

#include "pack.h"
#include "thread.h"

#include <deque>
#include <map>

namespace spire {

class PackReader {
public:
    explicit PackReader(const Pack* pack);
    ~PackReader(); /* stops the thread (after the read under way) */
    /* hw_thread: the console's hardware thread (-1 any) */
    bool start(int hw_thread);
    bool started() const { return thread_.started(); }

    enum State { NONE, QUEUED, READING, READY, FAILED };
    /* A texture's read queued (one already queued, read or failed: nothing). */
    void want(const std::string& name);
    State state(const std::string& name);
    /* The texture read (READY), taken out; a failed one forgotten. */
    bool take(const std::string& name, TextureData& out);
    /* For a draw that needs it now: a read under way waited for and taken;
     * one still queued taken off the queue (false: the caller reads it). */
    bool wait_take(const std::string& name, TextureData& out);

private:
    PackReader(const PackReader&);
    PackReader& operator=(const PackReader&);
    enum { AHEAD = 32 * 1024 * 1024 };
    struct Entry {
        State state;
        TextureData data;
        size_t bytes;
        Entry() : state(NONE), bytes(0) {
            data.format = data.flags = data.width = data.height = data.orig_width = data.orig_height = 0;
        }
    };
    static void thread_main(void* self);
    void run();
    const Pack* pack_;
    Mutex lock_;
    std::deque<std::string> queue_;
    std::map<std::string, Entry> entries_;
    size_t ahead_;
    Thread thread_;
    volatile bool stopping_;
};

} /* namespace spire */

#endif
