"""Strict comparison of two trees of .jsonl traces (the .NET logic against the
console's translated logic): every line, steps and all, except values that
are wall-clock or host specific. Usage: strict_compare.py A B"""
import json
import os
import sys

IGNORE = {"run_time", "data_dir", "seconds", "ms", "gc_heap_bytes", "pool_work_seen", "pool_work_completed",
          "callbacks_run"}


def clean(v):
    if isinstance(v, dict):
        return {k: clean(x) for k, x in v.items() if k not in IGNORE}
    if isinstance(v, list):
        return [clean(x) for x in v]
    return v


def first_diff(a, b, path=""):
    if type(a) != type(b):
        return f"{path}: {type(a).__name__} vs {type(b).__name__}"
    if isinstance(a, dict):
        for k in sorted(set(a) | set(b)):
            if k not in a or k not in b:
                return f"{path}.{k}: only in {'B' if k not in a else 'A'}"
            d = first_diff(a[k], b[k], path + "." + k)
            if d:
                return d
        return None
    if isinstance(a, list):
        if len(a) != len(b):
            return f"{path}: length {len(a)} vs {len(b)}"
        for i, (x, y) in enumerate(zip(a, b)):
            d = first_diff(x, y, f"{path}[{i}]")
            if d:
                return d
        return None
    return None if a == b else f"{path}: {str(a)[:120]!r} vs {str(b)[:120]!r}"


def lines(p):
    out = []
    with open(p, encoding="utf-8", errors="replace") as f:
        for ln in f:
            ln = ln.rstrip("\n")
            if not ln:
                continue
            if ln.startswith(">"):
                out.append(("cmd", ln))
            elif ln.startswith("{"):
                try:
                    out.append(("json", clean(json.loads(ln))))
                except ValueError:
                    out.append(("text", ln))
    return out


def main():
    a, b = sys.argv[1], sys.argv[2]
    files = differ = same = 0
    for root, _, names in os.walk(a):
        for n in sorted(names):
            if not n.endswith(".jsonl"):
                continue
            pa = os.path.join(root, n)
            pb = os.path.join(b, os.path.relpath(pa, a))
            files += 1
            if not os.path.exists(pb):
                print("missing:", os.path.relpath(pa, a))
                differ += 1
                continue
            la, lb = lines(pa), lines(pb)
            msg = None
            for i, (x, y) in enumerate(zip(la, lb)):
                if x[0] != y[0]:
                    msg = f"line {i}: {x[0]} vs {y[0]}"
                elif x[0] == "json":
                    d = first_diff(x[1], y[1])
                    if d:
                        msg = f"line {i}: {d}"
                elif x[1] != y[1]:
                    msg = f"line {i}: {x[1][:100]} vs {y[1][:100]}"
                if msg:
                    break
            if not msg and len(la) != len(lb):
                msg = f"lines {len(la)} vs {len(lb)}"
            if msg:
                differ += 1
                print(os.path.relpath(pa, a), "->", msg)
            else:
                same += 1
    print(f"{files} files, {same} identical, {differ} different")
    return 1 if differ else 0


if __name__ == "__main__":
    sys.exit(main())
