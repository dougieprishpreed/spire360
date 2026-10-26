"""Cuts traces at each continue_run / start_game_run / start_run command, so
spire_check (which replays decisions into one client) sees each continued
or new run as the fresh screen build the client makes after the menu.
Usage: split_traces.py OUT_DIR trace.jsonl..."""
import os
import sys

out = sys.argv[1]
os.makedirs(out, exist_ok=True)
n = 0
for path in sys.argv[2:]:
    base = os.path.basename(os.path.dirname(path)) + "_" + os.path.basename(path)[:-6]
    seg, lines = 0, []

    def flush():
        global n
        if any(l.startswith("{") and '"type":"decision"' in l.replace(" ", "") for l in lines):
            with open(os.path.join(out, f"{base}_{seg:03d}.jsonl"), "w", encoding="utf-8") as f:
                f.writelines(lines)
            n += 1

    for line in open(path, encoding="utf-8", errors="replace"):
        if line.startswith(">") and any(c in line for c in ('"cmd":"continue_run"', '"cmd":"start_game_run"', '"cmd":"start_run"')):
            flush()
            seg += 1
            lines = []
        lines.append(line)
    flush()
print(n, "segments")
