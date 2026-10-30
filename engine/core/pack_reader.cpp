#include "pack_reader.h"

#include <algorithm>

namespace spire {

PackReader::PackReader(const Pack* pack) : pack_(pack), ahead_(0), stopping_(false) {}

PackReader::~PackReader() {
    stopping_ = true;
    thread_.join();
}

bool PackReader::start(int hw_thread) { return thread_.start(thread_main, this, hw_thread); }

void PackReader::thread_main(void* self) { ((PackReader*)self)->run(); }

static size_t data_bytes(const TextureData& t) {
    size_t n = 0;
    for (size_t i = 0; i < t.levels.size(); ++i) n += t.levels[i].size();
    return n;
}

void PackReader::run() {
    while (!stopping_) {
        std::string name;
        {
            ScopedLock lock(lock_);
            if (!queue_.empty() && ahead_ < (size_t)AHEAD) {
                name = queue_.front();
                queue_.pop_front();
                entries_[name].state = READING;
            }
        }
        if (name.empty()) {
            sleep_ms(2);
            continue;
        }
        TextureData td;
        bool ok = pack_->read_texture(name.c_str(), td);
        ScopedLock lock(lock_);
        Entry& e = entries_[name];
        e.state = ok ? READY : FAILED;
        if (ok) {
            e.data.format = td.format;
            e.data.flags = td.flags;
            e.data.width = td.width;
            e.data.height = td.height;
            e.data.orig_width = td.orig_width;
            e.data.orig_height = td.orig_height;
            /* swapped in: no copy of the levels */
            e.data.levels.swap(td.levels);
            e.bytes = data_bytes(e.data);
            ahead_ += e.bytes;
        }
    }
}

void PackReader::want(const std::string& name) {
    ScopedLock lock(lock_);
    Entry& e = entries_[name];
    if (e.state != NONE) return;
    e.state = QUEUED;
    queue_.push_back(name);
}

PackReader::State PackReader::state(const std::string& name) {
    ScopedLock lock(lock_);
    std::map<std::string, Entry>::iterator it = entries_.find(name);
    return it == entries_.end() ? NONE : it->second.state;
}

bool PackReader::take(const std::string& name, TextureData& out) {
    ScopedLock lock(lock_);
    std::map<std::string, Entry>::iterator it = entries_.find(name);
    if (it == entries_.end() || (it->second.state != READY && it->second.state != FAILED)) return false;
    bool ok = it->second.state == READY;
    if (ok) {
        TextureData& d = it->second.data;
        out.format = d.format;
        out.flags = d.flags;
        out.width = d.width;
        out.height = d.height;
        out.orig_width = d.orig_width;
        out.orig_height = d.orig_height;
        out.levels.swap(d.levels);
        ahead_ -= it->second.bytes;
    }
    entries_.erase(it);
    return ok;
}

bool PackReader::wait_take(const std::string& name, TextureData& out) {
    for (;;) {
        {
            ScopedLock lock(lock_);
            std::map<std::string, Entry>::iterator it = entries_.find(name);
            if (it == entries_.end()) return false;
            if (it->second.state == QUEUED) {
                std::deque<std::string>::iterator q = std::find(queue_.begin(), queue_.end(), name);
                if (q != queue_.end()) queue_.erase(q);
                entries_.erase(it);
                return false;
            }
            if (it->second.state != READING) break;
        }
        sleep_ms(1);
    }
    return take(name, out);
}

} /* namespace spire */
