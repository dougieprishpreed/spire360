#!/usr/bin/env python3
"""hot_functions.py: which of the translated logic's functions run often, for the console's -O2 / -Os split.

    python tools/hot_functions.py STACKS_DIR... [--out logic/hot_functions.txt] [--min-share 0.0002]

STACKS_DIR holds tools/sample_logic.py's stacks_<pid>.txt files (one sample a line, leaf first, functions as
the C names them), sampled on a PC logic_host built from the same C while the drivers played: random runs
of every character through the acts, the presentation scenarios, the content coverage (every card, relic,
potion, event, encounter and epoch). A function is hot when its own code was the leaf of any sample, or it
was on the stack in at least --min-share of them (a caller whose loop runs its callees).

The list names each hot function by the FNV-1a 64 hash of its method's name in the wasm link map (the C
stage reads the names the same way from the build's own map), so it holds from build to build where the
function numbers move, and it carries no names of the game's code. tools/build_logic.py's C stage keeps
the hot functions in their files (-O2) and moves the others to <file>_cold.c, which the console compiles
for size (-Os, xsdk/CMakeLists.txt): a function the sampling never reached is compiled -Os, so sample
again after large changes. Prints how many are hot."""
import argparse
import collections
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from build_logic import LINKMAP, function_names, name_hash  # noqa: E402


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("dirs", nargs="+")
    ap.add_argument("--linkmap", default=LINKMAP, help="the sampled build's wasm link map")
    ap.add_argument("--header", default=os.path.join(ROOT, "out", "logic", "c", "logic.h"),
                    help="the sampled build's logic.h (its imports come first in the numbering)")
    ap.add_argument("--out", default=os.path.join(ROOT, "logic", "hot_functions.txt"))
    ap.add_argument("--min-share", type=float, default=0.0002)
    a = ap.parse_args()
    fn = re.compile(r"^f\d+$")
    leaf, onstack = collections.Counter(), collections.Counter()
    n = files = 0
    for d in a.dirs:
        for name in sorted(os.listdir(d)):
            if not name.startswith("stacks_"):
                continue
            files += 1
            for line in open(os.path.join(d, name), encoding="utf-8", errors="replace"):
                st = [x for x in line.rstrip("\n").split(";") if x]
                if not st:
                    continue
                n += 1
                ff = [x for x in st if fn.match(x)]
                if ff and fn.match(st[0]):
                    leaf[st[0]] += 1
                for x in set(ff):
                    onstack[x] += 1
    if n == 0:
        sys.exit("no samples")
    hot = set(leaf) | {f for f, c in onstack.items() if c >= a.min_share * n}
    names = function_names(a.linkmap, a.header)
    hashes = sorted(set(name_hash(names[f]) for f in hot if f in names))
    with open(a.out, "w", encoding="utf-8", newline="\n") as f:
        f.write("# The translated logic's functions that ran often when sampled (tools/hot_functions.py): FNV-1a 64\n"
                "# of each one's name in the wasm link map. The C stage compiles these -O2 and the rest -Os.\n"
                "# %d samples from %d logic processes; %d of %d functions hot.\n" % (n, files, len(hashes), len(names)))
        for h in hashes:
            f.write(h + "\n")
    leaves = sum(leaf.values())
    print("%d samples from %d processes (%d with translated code at the leaf); %d of %d functions hot "
          "(%d as leaves) -> %s" % (n, files, leaves, len(hashes), len(names), len(leaf), a.out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
