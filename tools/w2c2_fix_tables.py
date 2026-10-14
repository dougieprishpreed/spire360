#!/usr/bin/env python3
"""Rewrites w2c2's table initialisation in logic.c as data plus a loop.

w2c2 emits one statement per element of the indirect-call table
(wasmTableSetTyped(&i->t0, offset+k, &fN, ...)): for the game logic that is
one function of ~103,000 statements, and the XDK's 32-bit compiler runs out
of heap generating code for it (C1002 in pass 2). As a static array of
{function, type} the same data compiles in seconds.

    python tools/w2c2_fix_tables.py out/logic/c/logic.c

Idempotent (a rewritten file is left alone). Run by build_logic.py's c stage.
"""
import re
import sys

SET = re.compile(r"^wasmTableSetTyped\(&i->(t\d+),offset\+(\d+),\(wasmFunc\)&(\w+),&i->common,&(\w+)\);$")
MIN_ELEMENTS = 256  # small tables stay as they are


def fix(path):
    with open(path, encoding="utf-8") as f:
        lines = f.read().split("\n")
    out = []
    i = 0
    changed = 0
    while i < len(lines):
        m = SET.match(lines[i])
        if not m:
            out.append(lines[i])
            i += 1
            continue
        # A run of consecutive elements of one table from offset+0.
        table, start = m.group(1), int(m.group(2))
        run = []
        while i < len(lines):
            m = SET.match(lines[i])
            if not m or m.group(1) != table or int(m.group(2)) != start + len(run):
                break
            run.append((m.group(3), m.group(4)))
            i += 1
        if len(run) < MIN_ELEMENTS:
            out.extend(f"wasmTableSetTyped(&i->{table},offset+{start + k},(wasmFunc)&{fn},&i->common,&{ty});"
                       for k, (fn, ty) in enumerate(run))
            continue
        changed += 1
        name = f"spire_{table}_init{changed}"
        # Hoisted above the function: a file-scope array (C89: no
        # declarations after statements, and static data is not code).
        # The type descriptors exist only with W2C2_RUNTIME_CHECKS (as in
        # wasmTableSetTyped, which drops its type argument otherwise).
        decl = ["#if W2C2_RUNTIME_CHECKS", "#define SPIRE_FT(t) (&t)", "#else", "#define SPIRE_FT(t) NULL", "#endif"] \
            if changed == 1 else []
        decl += [f"static const struct {{ wasmFunc func; const wasmFuncType* type; }} {name}[{len(run)}] = {{"]
        decl += [f"{{(wasmFunc)&{fn},SPIRE_FT({ty})}}," for fn, ty in run]
        decl.append("};")
        fn_start = max(k for k, l in enumerate(out) if l.startswith("static void ") and l.endswith("{"))
        out[fn_start:fn_start] = decl + [""]
        out.append(f"{{ U32 k; for (k = 0; k < {len(run)}U; ++k) "
                   f"wasmTableSetTyped(&i->{table},offset+{start}U+k,{name}[k].func,&i->common,{name}[k].type); }}")
    if changed:
        with open(path, "w", encoding="utf-8") as f:
            f.write("\n".join(out))
    return changed


if __name__ == "__main__":
    for p in sys.argv[1:]:
        n = fix(p)
        print(f"{p}: {n} table initialiser(s) rewritten" if n else f"{p}: nothing to rewrite")
