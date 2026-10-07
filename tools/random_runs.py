#!/usr/bin/env python3
"""Play full runs with a seeded random agent over the logic protocol.

    random_runs.py --cmd "dotnet out/logic/jit/Spire.Logic.dll" [--runs 5]
                   [--characters Ironclad,Silent,...] [--seed-prefix s] [--trace-dir DIR]

The same command line drives any build of the logic (JIT, NativeAOT, the
w2c2 native host, or the console via a bridge), so traces from different
builds can be compared byte for byte (tools/compare_traces.py).

A run passes when it reaches game_over (win or lose). Errors, stalls (the
same decision repeated without progress) and step-limit hits fail it.
Exit status 0 only if every run passed.
"""
import argparse
import json
import os
import random
import shlex
import subprocess
import sys
import time

CHARACTERS = ["Ironclad", "Silent", "Defect", "Regent", "Necrobinder"]


class Session:
    def __init__(self, cmd, env, trace_path, verbose):
        # stderr goes to a file: an unread pipe fills up and blocks the logic.
        self.log_path = (trace_path[:-6] + ".log") if trace_path else os.devnull
        self.log = open(self.log_path, "w", encoding="utf-8")
        self.proc = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                     stderr=self.log, text=True, bufsize=1, env=env)
        self.trace = open(trace_path, "w", encoding="utf-8", buffering=1) if trace_path else None
        self.verbose = verbose

    def read(self):
        while True:
            line = self.proc.stdout.readline()
            if not line:
                self.log.flush()
                err = ""
                if self.log_path != os.devnull:
                    with open(self.log_path, encoding="utf-8", errors="replace") as f:
                        err = f.read()[-3000:]
                raise RuntimeError("logic process ended (EOF)\n" + err)
            line = line.strip()
            if line.startswith("{"):
                msg = json.loads(line)
                if self.trace:
                    # Steps are recorded for animation; keep them in the trace.
                    self.trace.write(line + "\n")
                return msg

    def send(self, cmd):
        text = json.dumps(cmd, separators=(",", ":"))
        if self.trace:
            self.trace.write(">" + text + "\n")
        if self.verbose:
            print("  >", text[:160])
        self.proc.stdin.write(text + "\n")
        self.proc.stdin.flush()
        resp = self.read()
        if self.verbose:
            print("  <", resp.get("type"), resp.get("decision", ""), (resp.get("message") or "")[:160])
        return resp

    def close(self):
        try:
            self.proc.stdin.write(json.dumps({"cmd": "quit"}) + "\n")
            self.proc.stdin.flush()
            self.proc.wait(timeout=30)
        except Exception:
            self.proc.kill()
        if self.trace:
            self.trace.close()
        self.log.close()


def act(rng, state):
    """Pick the next command for a decision point."""
    d = state.get("decision", "")
    a = lambda action, **args: {"cmd": "action", "action": action, "args": args} if args else {"cmd": "action", "action": action}
    if d == "map_select":
        c = rng.choice(state["choices"])
        return a("select_map_node", col=c["col"], row=c["row"])
    if d == "combat_play":
        energy = state.get("energy", 0)
        hand = [c for c in state.get("hand", []) if c.get("can_play") and c.get("cost", 0) <= energy]
        # Mostly play cards, sometimes end the turn early, sometimes drink a potion.
        potions = [p for p in state.get("player", {}).get("potions", []) if p]
        if potions and rng.random() < 0.05:
            p = rng.choice(potions)
            args = {"potion_index": p["index"]}
            if p.get("target_type") == "AnyEnemy" and state.get("enemies"):
                args["target_index"] = rng.randrange(len(state["enemies"]))
            return a("use_potion", **args)
        if hand and rng.random() < 0.9:
            card = rng.choice(hand)
            args = {"card_index": card["index"]}
            if card.get("target_type") == "AnyEnemy" and state.get("enemies"):
                args["target_index"] = rng.randrange(len(state["enemies"]))
            return a("play_card", **args)
        return a("end_turn")
    if d == "event_choice":
        opts = [o for o in state.get("options", []) if not o.get("is_locked")]
        return a("choose_option", option_index=rng.choice(opts)["index"]) if opts else a("leave_room")
    if d == "rest_site":
        opts = [o for o in state.get("options", []) if o.get("is_enabled", True)]
        return a("choose_option", option_index=rng.choice(opts)["index"]) if opts else a("leave_room")
    if d == "card_reward":
        cards = state.get("cards", [])
        if cards and rng.random() < 0.8:
            return a("select_card_reward", card_index=rng.randrange(len(cards)))
        return a("skip_card_reward")
    if d == "bundle_select":
        return a("select_bundle", bundle_index=rng.randrange(len(state.get("bundles", [0]))))
    if d == "card_select":
        cards = state.get("cards", [])
        lo = max(0, state.get("min_select", 1))
        hi = min(len(cards), max(lo, state.get("max_select", 1)))
        if not cards or hi == 0:
            return a("skip_select")
        n = rng.randint(max(lo, 1) if lo else 0, hi)
        if n == 0:
            return a("skip_select")
        picks = rng.sample(range(len(cards)), n)
        return a("select_cards", indices=",".join(str(i) for i in sorted(picks)))
    if d == "crystal_sphere":
        if rng.random() < 0.2:
            return a("crystal_sphere_tool", tool=rng.choice(state.get("tools", ["Small"])))
        hidden = [c for c in state.get("cells", []) if c.get("hidden")]
        c = rng.choice(hidden or state.get("cells", [{"x": 0, "y": 0}]))
        return a("crystal_sphere_click", x=c["x"], y=c["y"])
    if d == "shop":
        gold = state.get("player", {}).get("gold", 0)
        buys = []
        for kind, action, key in (("cards", "buy_card", "card_index"), ("relics", "buy_relic", "relic_index"),
                                  ("potions", "buy_potion", "potion_index")):
            for item in state.get(kind, []):
                if item.get("is_stocked", True) and item.get("cost", 1 << 30) <= gold:
                    buys.append(a(action, **{key: item["index"]}))
        if buys and rng.random() < 0.5:
            return rng.choice(buys)
        return a("leave_room")
    return a("proceed")


def play(cmd, env, character, seed, max_steps, trace_path, verbose, tank=False):
    rng = random.Random(seed)
    s = Session(cmd, env, trace_path, verbose)
    started = time.time()
    result = {"character": character, "seed": seed}
    try:
        ready = s.read()
        if ready.get("type") != "ready":
            result.update(status="bad_init", detail=ready)
            return result
        state = s.send({"cmd": "start_run", "character": character, "seed": seed})
        if tank:
            # Test aid: survive to see every act. Not a game feature.
            s.send({"cmd": "set_player", "max_hp": 999, "hp": 999})
            state = s.send({"cmd": "get_state"})
        errors = 0
        last_key, same = None, 0
        for step in range(max_steps):
            if state.get("type") == "error":
                errors += 1
                result.setdefault("errors", []).append(state.get("message", "")[:300])
                if state.get("stack_trace"):
                    result.setdefault("stacks", []).append(state["stack_trace"][:1500])
                if errors > 25:
                    result.update(status="errors", steps=step)
                    return result
                # Recover: ask for the current decision and try something else.
                state = s.send({"cmd": "get_state"})
                if state.get("type") == "error":
                    state = s.send({"cmd": "action", "action": "proceed"})
                continue
            d = state.get("decision")
            if tank and d == "map_select" and state.get("player", {}).get("hp", 999) < 500:
                s.send({"cmd": "set_player", "hp": 999})
            if d == "game_over":
                p = state.get("player", {})
                result.update(status="victory" if state.get("victory") else "defeat", steps=step,
                              act=state.get("act"), floor=state.get("floor"), hp=p.get("hp"))
                return result
            key = json.dumps({k: state.get(k) for k in ("decision", "round", "energy", "hand", "enemies", "options", "choices")},
                             sort_keys=True)
            same = same + 1 if key == last_key else 0
            last_key = key
            if same > 30:
                result.update(status="stuck", steps=step, decision=d, floor=state.get("context", {}).get("floor"))
                return result
            state = s.send(act(rng, state))
        result.update(status="step_limit", steps=max_steps)
        return result
    except Exception as ex:
        result.update(status="exception", detail=str(ex)[-2000:])
        return result
    finally:
        result["seconds"] = round(time.time() - started, 1)
        s.close()


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--cmd", required=True, help="command that starts the logic (stdin/stdout protocol)")
    ap.add_argument("--runs", type=int, default=5, help="runs per character")
    ap.add_argument("--characters", default=",".join(CHARACTERS))
    ap.add_argument("--seed-prefix", default="spire360")
    ap.add_argument("--max-steps", type=int, default=6000)
    ap.add_argument("--trace-dir")
    ap.add_argument("--data-dir", help="SPIRE_DATA for the logic process")
    ap.add_argument("--verbose", action="store_true")
    ap.add_argument("--tank", action="store_true", help="999 HP so random play reaches the later acts")
    a = ap.parse_args()

    env = dict(os.environ)
    if a.data_dir:
        env["SPIRE_DATA"] = a.data_dir
    if a.trace_dir:
        os.makedirs(a.trace_dir, exist_ok=True)
    cmd = shlex.split(a.cmd)
    results = []
    for character in a.characters.split(","):
        for i in range(a.runs):
            seed = f"{a.seed_prefix}-{character.lower()}-{i}"
            trace = os.path.join(a.trace_dir, seed + ".jsonl") if a.trace_dir else None
            r = play(cmd, env, character, seed, a.max_steps, trace, a.verbose, a.tank)
            results.append(r)
            print(json.dumps(r)[:600], flush=True)
    ok = [r for r in results if r["status"] in ("victory", "defeat")]
    print(f"\nCompleted: {len(ok)}/{len(results)}  "
          f"(victories {sum(r['status'] == 'victory' for r in results)})")
    by = {}
    for r in results:
        by.setdefault(r["character"], []).append(r["status"] in ("victory", "defeat"))
    for c, v in by.items():
        print(f"  {c}: {sum(v)}/{len(v)}")
    return 0 if len(ok) == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
