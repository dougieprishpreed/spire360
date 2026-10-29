"""Exit 1 (listing each call) when code the XEX compiles calls a C library
function the XDK's Release build does not have; g++ -std=c++98 still
declares them all, so the PC build cannot catch it.

- C99 functions VS2010 (the XDK's compiler) does not have.
- CRT functions the XDK's headers drop under _XBSTRICT, which a Release
  build defines (NDEBUG): the environment, the working directory,
  processes and the console (stdlib.h, direct.h, process.h, conio.h, io.h).

Lines compiled out on the console (#if defined(_XBOX) ... #else, and
_MSC_VER, which the XDK's compiler defines) are skipped.
"""
from pathlib import Path
import re
import sys

C99 = r"fminf?|fmaxf?|roundf?|truncf?|l?lroundf?|cbrtf?|log2f?|exp2f?|copysignf?|nearbyintf?|rintf?|remainderf?|fdimf?|expm1f?|log1pf?|isnan|isinf|isfinite|signbit|fpclassify|strtof|strtoll|strtoull|snprintf|va_copy"
XBSTRICT = (r"getenv|getenv_s|_dupenv_s|_?putenv|_putenv_s|_?getcwd|_getdcwd|_?chdir|_chdrive|_getdrives?|_getdiskfree|_fullpath|_wfullpath|_getpid"
            r"|_popen|_pclose|_pipe|_cwait|_w?exec[lv]p?e?|_w?spawn[lv]p?e?|_?locking"
            r"|_?kbhit|_?getche?|_?putch|_?ungetch|_?cgets|_cgets_s|_?cputs|_v?cprintf\w*|_?cscanf\w*")
CALL = re.compile(r"\b(%s|%s)\s*\(" % (C99, XBSTRICT))
# On the console: defined, not defined. Anything else is unknown, and both
# branches of its #if count.
DEFINED = {"_XBOX", "_MSC_VER", "NDEBUG", "_XBSTRICT", "SPIRE_WITH_SRT"}
UNDEFINED = {"SPIRE_DEBUG_HEAP", "_DEBUG", "__GNUC__", "__clang__"}


def strip(text):
    """Comments and literals out, line numbers kept."""
    text = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    text = re.sub(r'"(?:\\.|[^"\\\n])*"', '""', text)
    return re.sub(r"'(?:\\.|[^'\\\n])*'", "''", text)


def evaluate(expr):
    """True / False / None (unknown) for a #if expression on the console."""
    def known(name):
        return True if name in DEFINED else False if name in UNDEFINED else None
    expr = re.sub(r"defined\s*\(\s*(\w+)\s*\)|defined\s+(\w+)",
                  lambda m: {True: " T ", False: " F ", None: " U "}[known(m.group(1) or m.group(2))], expr)
    tokens = re.findall(r"&&|\|\||[!()]|\S+", expr)
    if any(t not in ("T", "F", "U", "&&", "||", "!", "(", ")") for t in tokens):
        return None
    pos = [0]

    def peek():
        return tokens[pos[0]] if pos[0] < len(tokens) else None

    def take():
        pos[0] += 1
        return tokens[pos[0] - 1]

    def unary():
        t = take()
        if t == "!":
            v = unary()
            return None if v is None else not v
        if t == "(":
            v = disjunction()
            take()
            return v
        return {"T": True, "F": False, "U": None}[t]

    def conjunction():
        v = unary()
        while peek() == "&&":
            take()
            w = unary()
            v = False if v is False or w is False else (None if v is None or w is None else True)
        return v

    def disjunction():
        v = conjunction()
        while peek() == "||":
            take()
            w = conjunction()
            v = True if v is True or w is True else (None if v is None or w is None else False)
        return v

    try:
        return disjunction()
    except (IndexError, KeyError):
        return None


class Group(object):
    """One #if ... #endif: is the current branch compiled (True, False or
    None for unknown), was an earlier branch surely or possibly taken."""

    def __init__(self, value):
        self.live = value
        self.taken = value is True
        self.maybe = value is None

    def branch(self, value):
        if self.taken:
            self.live = False
        else:
            self.live = None if self.maybe and value is not False else value
        self.taken = self.taken or value is True
        self.maybe = self.maybe or value is None


def compiled_lines(text):
    """(line number, line) for each line the console may compile."""
    groups = []
    for number, line in enumerate(text.splitlines(), 1):
        m = re.match(r"\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)", line)
        if m:
            kind, rest = m.group(1), m.group(2).strip()
            if kind == "ifdef":
                rest = "defined(%s)" % rest.split()[0]
            elif kind == "ifndef":
                rest = "!defined(%s)" % rest.split()[0]
            if kind in ("if", "ifdef", "ifndef"):
                groups.append(Group(evaluate(rest)))
            elif kind == "elif" and groups:
                groups[-1].branch(evaluate(rest))
            elif kind == "else" and groups:
                groups[-1].branch(True)
            elif kind == "endif" and groups:
                groups.pop()
            continue
        if all(g.live is not False for g in groups):
            yield number, line


def library_call(before):
    """The library function, not a member or another namespace's: nothing,
    std:: or :: in front of the name."""
    before = before.rstrip()
    if before.endswith((".", "->")):
        return False
    if before.endswith("::"):
        return re.search(r"(\w*)\s*$", before[:-2]).group(1) in ("", "std")
    return True


def main(root):
    root = Path(root)
    # the XEX's sources and the runtime logic_lib compiles (not the copies
    # of the logic a build puts under xdk/spire360/Release)
    files = sorted(root.glob("engine/*/*")) + [p for d in ("xdk/spire360", "runtime/src", "runtime/include")
                                                 for p in sorted((root / d).glob("*"))]
    # (the Vulkan renderer is the xSDK build's: the XDK's projects leave it out)
    files = [p for p in files if p.suffix in (".cpp", ".c", ".h") and "posix" not in p.name and
             not p.name.startswith("vk_renderer.")]
    found = 0
    for path in files:
        for number, line in compiled_lines(strip(path.read_text(encoding="utf-8", errors="replace"))):
            for m in CALL.finditer(line):
                if not library_call(line[:m.start()]):
                    continue
                print("%s:%d: %s() is not in the XDK's Release build" % (path.relative_to(root).as_posix(), number, m.group(1)))
                found += 1
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else "."))
