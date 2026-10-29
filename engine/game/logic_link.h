/* The game logic behind its line protocol (logic/Spire.Logic): the client
 * writes one JSON command per line and reads JSON lines back. Reads never
 * block, so a slow game action (a new run takes seconds on the console)
 * never stops rendering.
 *
 * SrtLink: the logic translated to C, running in this process on the wasm
 * runtime's guest thread (runtime/include/srt.h); the console's link. The
 * program starts the guest itself (the module's names are its own).
 * PipeLink: a child process (dotnet Spire.Logic.dll, logic_host) over
 * pipes; PC tools (POSIX). */
#ifndef SPIRE_LOGIC_LINK_H
#define SPIRE_LOGIC_LINK_H

#include "../core/base.h"

#include <deque>

namespace spire {

class LogicLink {
public:
    virtual ~LogicLink() {}
    /* Sends one command (a JSON object, no newline). */
    virtual bool send(const std::string& line) = 0;
    /* The next complete line from the logic, if one is ready. */
    virtual bool poll(std::string& line) = 0;
    /* The logic has exited (or failed to start). */
    virtual bool finished() = 0;
};

class SrtLink : public LogicLink {
public:
    SrtLink();
    virtual bool send(const std::string& line);
    virtual bool poll(std::string& line);
    virtual bool finished();

private:
    std::vector<char> buf_;
};

#if !defined(_XBOX) && !defined(__XSDK__) && !defined(_WIN32)
class PipeLink : public LogicLink {
public:
    PipeLink();
    virtual ~PipeLink();
    /* Runs command (via /bin/sh -c) with its stdin and stdout piped to us;
     * its stderr goes to stderr_path (NULL: inherited). */
    bool start(const std::string& command, const char* stderr_path, std::string* error);
    virtual bool send(const std::string& line);
    virtual bool poll(std::string& line);
    virtual bool finished();

private:
    int pid_, to_child_, from_child_;
    bool eof_;
    std::string pending_;
};
#endif

} /* namespace spire */

#endif
