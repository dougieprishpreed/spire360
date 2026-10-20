"""Checks the logic's profile flow (main menu, new runs, continue, abandon,
give up, save and quit, the end of a run) against a copy of a data folder.

    python tools/profile_check.py --cmd "dotnet out/logic/jit/Spire.Logic.dll" \
        --data work/import-test/data --work work/profile-check [--case NAME ...]

Each case starts from a fresh copy of --data (its saves included), so the
source is never changed. Traces of every case are written to --work.
"""
import argparse
import json
import os
import random
import shlex
import shutil
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import random_runs  # noqa: E402


class Logic:
    def __init__(self, cmd, data, trace):
        env = dict(os.environ)
        env["SPIRE_DATA"] = data
        self.s = random_runs.Session(shlex.split(cmd), env, trace, False)
        ready = self.s.read()
        assert ready.get("type") == "ready", ready

    def send(self, **cmd):
        return self.s.send(cmd)

    def close(self):
        self.s.close()


def play(logic, state, rng, steps, stop=None):
    """Random play (random_runs' agent) until game over, stop(state) or steps."""
    for _ in range(steps):
        if state.get("type") == "error":
            raise AssertionError(f"logic error: {state.get('message')}\n{state.get('stack_trace', '')[:800]}")
        if state.get("decision") == "game_over" or (stop and stop(state)):
            return state
        state = logic.s.send(random_runs.act(rng, state))
    return state


def check(cond, what):
    if not cond:
        raise AssertionError(what)
    print("  ok: " + what)


def asc(d):
    return (d.get("player") or {}).get("ascension")


def saves_dir(data, profile=1):
    return os.path.join(data, "user", f"profile{profile}", "saves")


def case_menu(logic, data, rng):
    p = logic.send(cmd="profiles")
    check(p.get("type") == "profiles" and len(p["profiles"]) == 3, "profiles lists 3 profiles")
    m = logic.send(cmd="select_profile", profile=0)
    check(m.get("type") == "main_menu", f"select_profile answers the main menu (profile {m.get('profile')})")
    print("  menu:", json.dumps({k: m.get(k) for k in ("number_of_runs", "has_run_save", "run", "timeline", "singleplayer_enabled")})[:600])
    for c in m["characters"]:
        print(f"    {c['id']}: unlocked={c['unlocked']} max_ascension={c['max_ascension']} ascension={c['ascension']}")
    return m


def case_continue(logic, data, rng):
    m = case_menu(logic, data, rng)
    check(m.get("has_run_save"), "the profile has a saved run")
    d = logic.send(cmd="continue_run")
    check(d.get("type") == "decision", f"continue_run resumes at a {d.get('decision')} decision")
    ctx = d.get("context", {})
    print("  context:", json.dumps(ctx)[:300])
    if m["run"].get("hp") is not None and d.get("player"):
        check(d["player"].get("hp") == m["run"]["hp"] or d.get("decision") != "map_select",
              f"HP after continue {d['player'].get('hp')} matches the saved {m['run']['hp']}")
    d = play(logic, d, rng, 40)
    m2 = logic.send(cmd="return_to_menu")
    check(m2.get("type") == "main_menu", "return_to_menu (save and quit) answers the main menu")
    if d.get("decision") != "game_over":
        check(m2.get("has_run_save"), "the run is still saved after save and quit")
        d2 = logic.send(cmd="continue_run")
        check(d2.get("type") == "decision", f"the run continues again ({d2.get('decision')})")


def case_new_runs(logic, data, rng):
    m = case_menu(logic, data, rng)
    if m.get("has_run_save"):
        m = logic.send(cmd="abandon_run")
        check(m.get("type") == "main_menu" and not m.get("has_run_save"), "abandon_run deletes the saved run")
    runs_before = m.get("number_of_runs", 0)
    for c in m["characters"]:
        if not c["unlocked"]:
            continue
        d = logic.send(cmd="start_game_run", character=c["id"], ascension=c["max_ascension"], seed=f"pc-{c['id']}")
        check(d.get("type") == "decision", f"{c['id']} A{c['max_ascension']}: first decision {d.get('decision')}")
        check(asc(d) == c["max_ascension"], f"{c['id']} runs at ascension {asc(d)}")
        d = play(logic, d, rng, 60)
        check(os.path.isfile(os.path.join(saves_dir(data), "current_run.save")) or d.get("decision") == "game_over",
              f"{c['id']}: current_run.save written during the run")
        g = logic.send(cmd="give_up")
        check(g.get("decision") == "game_over" and not g.get("victory"), f"{c['id']}: give up ends the run in defeat")
        prog = g.get("progress") or {}
        print("    progress:", json.dumps(prog)[:400])
        check(not os.path.isfile(os.path.join(saves_dir(data), "current_run.save")), f"{c['id']}: the run save is deleted")
        m = logic.send(cmd="return_to_menu")
    check(m.get("number_of_runs") > runs_before, f"runs counted: {runs_before} -> {m.get('number_of_runs')}")


def case_fresh(logic, data, rng):
    """A new profile: only the Ironclad, no ascension, the first run's rules."""
    shutil.rmtree(os.path.join(data, "user"), ignore_errors=True)
    m = case_menu(logic, data, rng)
    unlocked = [c["id"] for c in m["characters"] if c["unlocked"]]
    check(unlocked == ["IRONCLAD"], f"a new profile has only the Ironclad ({unlocked})")
    check(all(c["max_ascension"] == 0 for c in m["characters"]), "no ascension on a new profile")
    d = logic.send(cmd="start_game_run", character="SILENT", seed="fresh")
    check(d.get("type") == "error", "a locked character cannot start")
    d = logic.send(cmd="start_game_run", character="IRONCLAD", ascension=5, seed="fresh")
    check(asc(d) == 0, "the ascension is clamped to the profile's maximum (0)")
    d = play(logic, d, rng, 3000)
    check(d.get("decision") == "game_over", f"the first run ends ({'victory' if d.get('victory') else 'defeat'})")
    print("    progress:", json.dumps(d.get("progress"))[:600])
    m = logic.send(cmd="return_to_menu")
    print("  after:", json.dumps({k: m.get(k) for k in ("number_of_runs", "timeline", "singleplayer_enabled")}))
    check(m.get("number_of_runs") == 1, "one run recorded")


def case_unlock_all(logic, data, rng):
    shutil.rmtree(os.path.join(data, "user"), ignore_errors=True)
    case_menu(logic, data, rng)
    m = logic.send(cmd="set_unlock_all", on=True)
    check(m.get("unlock_all") is True, "unlock everything is on")
    check(all(c["unlocked"] and c["max_ascension"] == 10 for c in m["characters"]),
          "every character, ascension up to 10")
    d = logic.send(cmd="start_game_run", character="NECROBINDER", ascension=10, seed="all")
    check(asc(d) == 10, "a new profile can start at ascension 10")
    m = logic.send(cmd="return_to_menu")
    m = logic.send(cmd="set_unlock_all", on=False)
    check(m.get("unlock_all") is False, "unlock everything is off again")


def reveal_all(logic):
    """Opens the timeline and reveals every obtained epoch, as a player would."""
    t = logic.send(cmd="timeline")
    check(t.get("type") == "timeline", f"timeline opens ({len(t.get('slots', []))} slots, first time: {t.get('first_time')})")
    if t.get("first_time"):
        t = logic.send(cmd="timeline_first_time_done")
    revealed = []
    for _ in range(80):
        ready = [x for x in t["slots"] if x["state"] == "Obtained"]
        if not ready:
            break
        r = logic.send(cmd="reveal_epoch", epoch=ready[0]["id"])
        check(r.get("type") == "timeline", f"reveal {ready[0]['id']}: " +
              ", ".join(f"{u['kind']}" + (f"({len(u.get('slots') or u.get('cards') or u.get('relics') or u.get('potions') or [])})"
                                          if u["kind"] in ("timeline", "cards", "relics", "potions") else
                                          f"({u.get('character') or (u.get('text') or '')[:40]})") for u in r.get("unlocks", [])))
        revealed.append(ready[0]["id"])
        t = r
    return revealed


def case_progression(logic, data, rng):
    """A new profile's first runs: the epochs they earn, revealed on the timeline."""
    shutil.rmtree(os.path.join(data, "user"), ignore_errors=True)
    case_menu(logic, data, rng)
    for run in range(3):
        m = logic.send(cmd="main_menu")
        chars = [c["id"] for c in m["characters"] if c["unlocked"]]
        d = logic.send(cmd="start_game_run", character=chars[-1], seed=f"prog-{run}")
        d = play(logic, d, rng, 4000)
        check(d.get("decision") == "game_over", f"run {run + 1} as {chars[-1]} ended; bar "
              + json.dumps((d.get("progress") or {}).get("score_bar")))
        m = logic.send(cmd="return_to_menu")
        print("  menu:", json.dumps({k: m.get(k) for k in ("number_of_runs", "timeline", "singleplayer_enabled")}))
        if m["timeline"]["notification"]:
            reveal_all(logic)
            m = logic.send(cmd="main_menu")
            check(m.get("singleplayer_enabled"), "single player is enabled once the epochs are revealed")
        print("  characters:", [c["id"] for c in m["characters"] if c["unlocked"]])


CASES = {"menu": case_menu, "continue": case_continue, "new_runs": case_new_runs, "fresh": case_fresh,
         "unlock_all": case_unlock_all, "progression": case_progression}


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--cmd", required=True)
    ap.add_argument("--data", required=True, help="data folder to copy for each case (localization + user/)")
    ap.add_argument("--work", required=True)
    ap.add_argument("--case", action="append", help="cases to run (default: all)")
    ap.add_argument("--seed", type=int, default=1)
    a = ap.parse_args()
    failed = 0
    for name in a.case or list(CASES):
        data = os.path.join(a.work, name, "data")
        shutil.rmtree(os.path.join(a.work, name), ignore_errors=True)
        shutil.copytree(a.data, data)
        print(f"== {name}")
        logic = Logic(a.cmd, data, os.path.join(a.work, name, "trace.jsonl"))
        try:
            CASES[name](logic, data, random.Random(a.seed))
        except AssertionError as ex:
            failed += 1
            print(f"  FAILED: {ex}")
        finally:
            logic.close()
    print(f"\n{len(a.case or CASES) - failed} passed, {failed} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
