#!/usr/bin/env python3
"""Scripted fights for presentation checks: protocol traces that
spire_play --replay-run (frame captures) and spire_check replay.

    python tools/presentation_scenarios.py [--cmd CMD] [--data DIR] --work OUT

  piles     cards that move cards between piles: Havoc (the draw pile's top
            card played and exhausted), Headbutt (a discard pile card onto
            the draw pile), Inflame's enqueue effect, an end of turn
  infested  the Phrog Parasite killed: its Wrigglers come after its death
            animation (InfestedPower)
  vantom    Vantom's charge-ups (its track-1 animations through
            SpineAnimationAccess, the music's progress)
  mecha     the Mecha Knight fought to the end (its hit effects at the
            player side's centre)

Each writes OUT/scenario/<name>.jsonl and .log (content_coverage's Runner).
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import content_coverage as cc  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cmd", default="dotnet out/logic/jit/Spire.Logic.dll")
    ap.add_argument("--data", default="work/play-data")
    ap.add_argument("--work", required=True)
    ap.add_argument("--only", default="", help="comma-separated scenario names (default: all)")
    a = ap.parse_args()
    run = cc.Runner(argparse.Namespace(cmd=a.cmd, data=a.data, work=a.work, limit=0, fight="BYGONE_EFFIGY_ELITE",
                                       seed=1, act_runs=0, suite=""))
    only = set(filter(None, a.only.split(",")))

    def fight(name, encounter, room="monster"):
        case = cc.Case("scenario", name)
        lg = run.logic("scenario", name)
        run.new_run(case, lg, character="Ironclad", seed=name)
        state = run.step(case, lg, {"cmd": "enter_room", "type": room, "encounter": encounter})
        return case, lg, state

    def play(case, lg, state, card_id):
        state = run.step(case, lg, {"cmd": "add_to_hand", "cards": [card_id], "energy": 9})
        mine = [h for h in state.get("hand") or [] if (h.get("id") or "").split(".")[-1] == card_id]
        if not mine:
            case.notes.append(f"{card_id}: not in hand")
            return state
        args = {"card_index": mine[-1]["index"]}
        if mine[-1].get("target_type") == "AnyEnemy" and state.get("enemies"):
            args["target_index"] = 0
        state = run.step(case, lg, {"cmd": "action", "action": "play_card", "args": args})
        return run.play_until(case, lg, state, lambda s: s.get("decision") in ("combat_play", "rewards", "map_select"), 20)

    def end_turns(case, lg, state, n):
        for _ in range(n):
            if state.get("decision") != "combat_play":
                break
            state = run.step(case, lg, {"cmd": "action", "action": "end_turn", "args": {}})
            state = run.play_until(case, lg, state, lambda s: s.get("decision") == "combat_play", 20)
        return state

    if not only or "piles" in only:
        case, lg, state = fight("piles", "SHRINKER_BEETLE_WEAK")
        for card in ("STRIKE_IRONCLAD", "DEFEND_IRONCLAD", "HAVOC", "HEADBUTT", "INFLAME", "HAVOC"):
            state = play(case, lg, state, card)
        end_turns(case, lg, state, 1)
        run.finish(case, lg)
    if not only or "infested" in only:
        case, lg, state = fight("infested", "PHROG_PARASITE_ELITE", "elite")
        for _ in range(6):
            alive = [e for e in state.get("enemies") or [] if e.get("hp", 0) > 0]
            if state.get("decision") != "combat_play" or not alive or alive[0].get("name", "").lower().startswith("wriggler"):
                break
            state = play(case, lg, state, "BLUDGEON")
        run.finish(case, lg)
    if not only or "vantom" in only:
        case, lg, state = fight("vantom", "VANTOM_BOSS")
        end_turns(case, lg, state, 5)
        run.finish(case, lg)
    if not only or "mecha" in only:
        case, lg, state = fight("mecha", "MECHA_KNIGHT_ELITE", "elite")
        run.play_until(case, lg, state, lambda s: s.get("decision") in ("rewards", "map_select"), 400)
        run.finish(case, lg)
    bad = [c for c in run.cases if c.status != "ok"]
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
