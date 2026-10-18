"""Exit 0 when a PC object needs rebuilding, 1 when it is current."""
from pathlib import Path
import shlex
import sys


def needs_rebuild(obj, source, stamp, deps):
    obj = Path(obj)
    if not obj.is_file() or not Path(deps).is_file():
        return True
    newest = obj.stat().st_mtime_ns
    # GCC depfiles escape spaces and continue long dependency lists. Ignore
    # the empty -MP header rules after the first logical line.
    text = Path(deps).read_text().replace("\\\n", "").split("\n", 1)[0]
    dependencies = shlex.split(text.split(":", 1)[1])
    for name in [source, stamp] + dependencies:
        path = Path(name)
        if not path.is_file() or path.stat().st_mtime_ns > newest:
            return True
    return False


if __name__ == "__main__":
    try:
        rebuild = needs_rebuild(*sys.argv[1:])
    except (OSError, ValueError, IndexError):
        rebuild = True # an unreadable cache must never skip compilation
    sys.exit(0 if rebuild else 1)
