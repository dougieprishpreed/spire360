#include "logic_link.h"

#if !defined(_XBOX) && !defined(__XSDK__) && !defined(_WIN32)
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#if defined(SPIRE_WITH_SRT)
#include "srt.h"
#endif

namespace spire {

/* ---- SrtLink ---- */

SrtLink::SrtLink() : buf_(1 << 20) {}

#if defined(SPIRE_WITH_SRT)
bool SrtLink::send(const std::string& line) {
    if (srt_guest_finished()) return false;
    srt_stdin_write(line.data(), line.size());
    srt_stdin_write("\n", 1);
    return true;
}

bool SrtLink::poll(std::string& line) {
    for (;;) {
        long n = srt_stdout_read_line(&buf_[0], buf_.size(), 0);
        if (n == -2) {
            /* A line longer than the buffer: grow it and read again. */
            buf_.resize(buf_.size() * 2);
            continue;
        }
        if (n < 0) return false;
        line.assign(&buf_[0], (size_t)n);
        return true;
    }
}

bool SrtLink::finished() { return srt_guest_finished() != 0; }
#else
/* Programs without the runtime (PC tools) never create an SrtLink. */
bool SrtLink::send(const std::string&) { return false; }
bool SrtLink::poll(std::string&) { return false; }
bool SrtLink::finished() { return true; }
#endif

/* ---- PipeLink ---- */

#if !defined(_XBOX) && !defined(__XSDK__) && !defined(_WIN32)
PipeLink::PipeLink() : pid_(-1), to_child_(-1), from_child_(-1), eof_(false) {}

PipeLink::~PipeLink() {
    if (to_child_ >= 0) close(to_child_);
    if (from_child_ >= 0) close(from_child_);
    if (pid_ > 0) {
        int status;
        /* Closing stdin ends the logic's read loop; give it a moment. */
        for (int i = 0; i < 50 && waitpid(pid_, &status, WNOHANG) == 0; ++i) usleep(20000);
        if (waitpid(pid_, &status, WNOHANG) == 0) {
            kill(pid_, SIGTERM);
            waitpid(pid_, &status, 0);
        }
    }
}

bool PipeLink::start(const std::string& command, const char* stderr_path, std::string* error) {
    int in[2], out[2];
    if (pipe(in) != 0 || pipe(out) != 0) {
        if (error) *error = "pipe failed";
        return false;
    }
    pid_ = fork();
    if (pid_ < 0) {
        if (error) *error = "fork failed";
        return false;
    }
    if (pid_ == 0) {
        dup2(in[0], 0);
        dup2(out[1], 1);
        if (stderr_path) {
            int fd = open(stderr_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd >= 0) dup2(fd, 2);
        }
        close(in[0]);
        close(in[1]);
        close(out[0]);
        close(out[1]);
        execl("/bin/sh", "sh", "-c", command.c_str(), (char*)NULL);
        _exit(127);
    }
    close(in[0]);
    close(out[1]);
    to_child_ = in[1];
    from_child_ = out[0];
    fcntl(from_child_, F_SETFL, fcntl(from_child_, F_GETFL) | O_NONBLOCK);
    signal(SIGPIPE, SIG_IGN);
    return true;
}

bool PipeLink::send(const std::string& line) {
    if (to_child_ < 0) return false;
    std::string s = line + "\n";
    size_t done = 0;
    while (done < s.size()) {
        ssize_t n = write(to_child_, s.data() + done, s.size() - done);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        done += (size_t)n;
    }
    return true;
}

bool PipeLink::poll(std::string& line) {
    for (;;) {
        size_t nl = pending_.find('\n');
        if (nl != std::string::npos) {
            line = pending_.substr(0, nl);
            pending_.erase(0, nl + 1);
            if (!line.empty() && line[line.size() - 1] == '\r') line.erase(line.size() - 1);
            return true;
        }
        if (eof_ || from_child_ < 0) return false;
        char buf[65536];
        ssize_t n = read(from_child_, buf, sizeof(buf));
        if (n > 0) {
            pending_.append(buf, (size_t)n);
            continue;
        }
        if (n == 0) eof_ = true;
        else if (errno == EINTR) continue;
        return false;
    }
}

bool PipeLink::finished() { return eof_ && pending_.find('\n') == std::string::npos; }
#endif

} /* namespace spire */
