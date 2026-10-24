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
  boulder   Rolling Boulder over three enemies (each hit as it passes, the
            boulder_hit marks), then the turns it rolls again
  dive      Minion Dive Bomb (NMinionDiveBombVfx)
  nightmare Nightmare (its vignette and hands), a debuff (the debuff sound,
            the shake), powers wearing off
  segments  the Decimillipede fought to the end (Reattach: the last
            segment's fade over all of them)
  pandora   Pandora's Box obtained (the simple cards view of what it changed)
  parasol   Lord's Parasol at a shop: everything bought one by one (cards,
            relics, potions), then the card removal
  queen     the Queen killed (her death: queen_progress 5, track 1 emptied)
  death     the player killed (CreatureCmd.Kill: the death, the game over)
  powers    powers whose triggers need a setup: Corrosive Wave then a draw
            the same turn, The Bomb through its three turns
  candy     Lasting Candy through three fights (every other combat reward has
            a fourth card: Hook.BeforeCombatRewardOffered counts them)
  boots     Winged Boots: the map offers the whole next row (MapTravel.
            GetTravelablePointsFrom, ShouldAllowFreeTravel), a flight used
  continue  a profile's run: Save and Quit at a won fight's rewards, then
            Continue (CombatRoom.StartPreFinishedCombat); again at the map
  continue2 a profile's run quit mid-fight (the fight again from its start),
            in a shop after a purchase, at a rest site, in an event
  sleepers  the Slumbering Beetle (its sleeping Zs from AfterAddedToRoom)
  fake      the Fake Merchant: a relic bought, a Foul Potion thrown, its fight
  hopper    the Thieving Hopper's theft (the stolen card in its hand),
            then killed (the card back as a reward)
  flask     Bouncing Flask and a Fire Potion (splashes, liquid overlays),
            Neutralize's Weak (the intents' numbers at once)

Each writes OUT/scenario/<name>.jsonl and .log (content_coverage's Runner).
"""
import argparse
import os
import shutil
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
    if not only or "boulder" in only:
        case, lg, state = fight("boulder", "TWO_TAILED_RATS_NORMAL")
        state = play(case, lg, state, "ROLLING_BOULDER")
        end_turns(case, lg, state, 2)
        run.finish(case, lg)
    if not only or "dive" in only:
        case, lg, state = fight("dive", "SHRINKER_BEETLE_WEAK")
        for _ in range(2):
            state = play(case, lg, state, "MINION_DIVE_BOMB")
        run.finish(case, lg)
    if not only or "nightmare" in only:
        case, lg, state = fight("nightmare", "SHRINKER_BEETLE_WEAK")
        state = play(case, lg, state, "NIGHTMARE")
        if state.get("decision") == "card_select":
            state = run.step(case, lg, {"cmd": "action", "action": "select_cards", "args": {"indices": "0"}})
        for card in ("BASH", "NEUTRALIZE"):
            state = play(case, lg, state, card)
        end_turns(case, lg, state, 3)
        run.finish(case, lg)
    if not only or "segments" in only:
        case, lg, state = fight("segments", "DECIMILLIPEDE_ELITE", "elite")
        # every segment killed in one turn (Whirlwind hits them all), so the
        # last one's death fades them together
        for _ in range(6):
            if state.get("decision") != "combat_play":
                break
            state = play(case, lg, state, "WHIRLWIND")
        run.play_until(case, lg, state, lambda s: s.get("decision") in ("rewards", "map_select"), 100)
        run.finish(case, lg)
    if not only or "flask" in only:
        # Bouncing Flask's splashes and liquid overlays, a thrown potion's
        # (NCombatRoom.PlaySplashVfx); a power changing the intents' numbers
        case, lg, state = fight("flask", "TWO_TAILED_RATS_NORMAL")
        state = play(case, lg, state, "BOUNCING_FLASK")
        state = run.step(case, lg, {"cmd": "obtain", "potions": ["FIRE_POTION"]})
        state = run.step(case, lg, {"cmd": "get_state"})
        pots = [x for x in (state.get("player") or {}).get("potions") or [] if x]
        if pots:
            state = run.step(case, lg, {"cmd": "action", "action": "use_potion",
                                        "args": {"potion_index": pots[0]["index"], "target_index": 0}})
            state = run.play_until(case, lg, state, lambda s: s.get("decision") == "combat_play", 20)
        state = play(case, lg, state, "NEUTRALIZE")
        end_turns(case, lg, state, 1)
        run.finish(case, lg)
    if not only or "sleepers" in only:
        # what the monsters set up as they join (AfterAddedToRoom, before the
        # fight is in progress): the Slumbering Beetle's sleeping Zs and loop
        for enc in ("SLUMBERING_BEETLE_NORMAL",):
            case, lg, state = fight("sleepers", enc)
            end_turns(case, lg, state, 1)
            run.finish(case, lg)
    if not only or "fake" in only:
        # the Fake Merchant: its room, a relic bought, a Foul Potion thrown
        # at it, its fight won (HP capped), the rewards
        case = cc.Case("scenario", "fake")
        lg = run.logic("scenario", "fake")
        run.new_run(case, lg, character="Ironclad", seed="fake")
        run.step(case, lg, {"cmd": "set_player", "gold": 300})
        run.step(case, lg, {"cmd": "obtain", "potions": ["FOUL_POTION"]})
        state = run.step(case, lg, {"cmd": "enter_room", "type": "event", "event": "FAKE_MERCHANT"})
        if state.get("decision") == "shop":
            relics = [r for r in state.get("relics") or [] if r.get("is_stocked")]
            if relics:
                state = run.step(case, lg, {"cmd": "action", "action": "buy_relic",
                                            "args": {"relic_index": relics[0]["index"]}})
            pots = [x for x in (state.get("player") or {}).get("potions") or [] if x]
            if pots:
                state = run.step(case, lg, {"cmd": "action", "action": "use_potion",
                                            "args": {"potion_index": pots[0]["index"]}})
        else:
            case.notes.append(f"no shop: {state.get('decision')}")
        if state.get("decision") == "combat_play":
            run.step(case, lg, {"cmd": "set_player", "enemy_hp": 5})
            state = run.step(case, lg, {"cmd": "get_state"})
        run.play_until(case, lg, state, lambda s: s.get("decision") == "map_select", 200)
        run.finish(case, lg)
    if not only or "pandora" in only:
        case = cc.Case("scenario", "pandora")
        lg = run.logic("scenario", "pandora")
        run.new_run(case, lg, character="Ironclad", seed="pandora")
        state = run.step(case, lg, {"cmd": "obtain", "relics": ["PANDORAS_BOX"]})
        run.play_until(case, lg, state, lambda s: s.get("decision") == "map_select", 30)
        run.finish(case, lg)
    if not only or "queen" in only:
        case, lg, state = fight("queen", "QUEEN_BOSS", "elite")
        for _ in range(12):
            if state.get("decision") != "combat_play":
                break
            run.step(case, lg, {"cmd": "set_player", "hp": 9999, "enemy_hp": 3})
            state = run.step(case, lg, {"cmd": "get_state"})
            state = run.play_until(case, lg, state, lambda s: s.get("decision") != "combat_play", 20)
        run.finish(case, lg)
    if not only or "boots" in only:
        case = cc.Case("scenario", "boots")
        lg = run.logic("scenario", "boots")
        state = run.new_run(case, lg, character="Ironclad", seed="boots")
        run.step(case, lg, {"cmd": "obtain", "relics": ["WINGED_BOOTS"]})
        state = run.step(case, lg, {"cmd": "get_state"})
        last_col = 0
        for _ in range(3):
            state = run.play_until(case, lg, state, lambda s: s.get("decision") == "map_select", 40)
            choices = state.get("choices") or []
            case.notes.append(f"choices: {[(c.get('col'), c.get('row')) for c in choices]}")
            if not choices:
                break
            # the one farthest along the row from the last (a flight: no
            # child of the room before)
            pick = max(choices, key=lambda c: abs(c["col"] - last_col))
            last_col = pick["col"]
            state = run.step(case, lg, {"cmd": "action", "action": "select_map_node",
                                        "args": {"col": pick["col"], "row": pick["row"]}})
            if state.get("decision") == "combat_play":
                run.step(case, lg, {"cmd": "set_player", "enemy_hp": 1})
                state = run.step(case, lg, {"cmd": "get_state"})
        run.finish(case, lg)
    if not only or "powers" in only:
        case, lg, state = fight("powers", "TWO_TAILED_RATS_NORMAL")
        for card in ("CORROSIVE_WAVE", "BACKFLIP", "THE_BOMB"):
            state = play(case, lg, state, card)
        end_turns(case, lg, state, 4)
        run.finish(case, lg)
    if not only or "candy" in only:
        case = cc.Case("scenario", "candy")
        lg = run.logic("scenario", "candy")
        run.new_run(case, lg, character="Ironclad", seed="candy")
        run.step(case, lg, {"cmd": "obtain", "relics": ["LASTING_CANDY"]})
        for _ in range(3):
            state = run.step(case, lg, {"cmd": "enter_room", "type": "monster", "encounter": "SHRINKER_BEETLE_WEAK"})
            run.step(case, lg, {"cmd": "set_player", "enemy_hp": 1})
            state = run.step(case, lg, {"cmd": "get_state"})
            state = run.play_until(case, lg, state, lambda s: s.get("decision") in ("rewards", "card_reward"), 40)
            cards = [r for r in state.get("rewards") or [] if r.get("type") == "card"]
            if cards:
                state = run.step(case, lg, {"cmd": "action", "action": "take_reward",
                                            "args": {"reward_index": cards[0]["index"]}})
            case.notes.append(f"card options: {len(state.get('cards') or [])}")
            run.play_until(case, lg, state, lambda s: s.get("decision") == "map_select", 30)
        run.finish(case, lg)
    if not only or "death" in only:
        # coverage keeps the player at 9999 HP: here the elite's first blow
        # kills (CreatureCmd.Kill's death path, the game over screen)
        case, lg, state = fight("death", "BYGONE_EFFIGY_ELITE", "elite")
        run.step(case, lg, {"cmd": "set_player", "max_hp": 80, "hp": 1})
        state = run.step(case, lg, {"cmd": "get_state"})
        run.play_until(case, lg, state, lambda s: s.get("decision") == "game_over", 60)
        run.finish(case, lg)
    if not only or "continue" in only:
        # its own data (the profile's saves), as the act runs
        data = os.path.join(a.work, "scenario", "continue_data")
        shutil.rmtree(data, ignore_errors=True)
        shutil.copytree(a.data, data, ignore=shutil.ignore_patterns("user"))
        case = cc.Case("scenario", "continue")
        lg = cc.Logic(a.cmd, data, os.path.join(a.work, "scenario", "continue.jsonl"))
        try:
            run.step(case, lg, {"cmd": "select_profile", "profile": 1})
            state = run.step(case, lg, {"cmd": "start_game_run", "character": "Ironclad", "ascension": 0,
                                        "seed": "continue"})
            run.step(case, lg, {"cmd": "set_player", "max_hp": 9999, "hp": 9999})
            # Neow, then the map's first rooms until a fight
            state = run.play_until(case, lg, state, lambda s: s.get("decision") == "combat_play", 60)
            if state.get("decision") == "combat_play":
                run.step(case, lg, {"cmd": "set_player", "enemy_hp": 1})
                state = run.step(case, lg, {"cmd": "get_state"})
                state = run.play_until(case, lg, state, lambda s: s.get("decision") in ("rewards", "map_select"), 60)
            case.notes.append(f"saved at {state.get('decision')}")
            run.step(case, lg, {"cmd": "return_to_menu"})
            state = run.step(case, lg, {"cmd": "continue_run"})
            case.notes.append(f"continued at {state.get('decision')}")
            state = run.play_until(case, lg, state, lambda s: s.get("decision") == "map_select", 30)
            run.step(case, lg, {"cmd": "return_to_menu"})
            state = run.step(case, lg, {"cmd": "continue_run"})
            case.notes.append(f"continued at {state.get('decision')}")
        finally:
            run.finish(case, lg)
    if not only or "continue2" in only:
        data = os.path.join(a.work, "scenario", "continue2_data")
        shutil.rmtree(data, ignore_errors=True)
        shutil.copytree(a.data, data, ignore=shutil.ignore_patterns("user"))
        case = cc.Case("scenario", "continue2")
        lg = cc.Logic(a.cmd, data, os.path.join(a.work, "scenario", "continue2.jsonl"))

        def quit_and_continue(where):
            run.step(case, lg, {"cmd": "return_to_menu"})
            s = run.step(case, lg, {"cmd": "continue_run"})
            p = s.get("player") or {}
            case.notes.append(f"{where}: continued at {s.get('decision')} (gold {p.get('gold')}, "
                              f"deck {p.get('deck_size')}, hp {p.get('hp')})")
            return s

        try:
            run.step(case, lg, {"cmd": "select_profile", "profile": 1})
            state = run.step(case, lg, {"cmd": "start_game_run", "character": "Ironclad", "ascension": 0,
                                        "seed": "continue2"})
            run.step(case, lg, {"cmd": "set_player", "max_hp": 9999, "hp": 9999, "gold": 500})
            visited = set()
            for _ in range(30):
                state = run.play_until(case, lg, state, lambda s: s.get("decision") in (
                    "combat_play", "shop", "rest_site", "event_choice", "map_select"), 40)
                kind = state.get("decision")
                if kind == "combat_play" and "fight" not in visited:
                    # a card played, then quit: the fight from its start
                    hand = [h for h in state.get("hand") or [] if h.get("can_play")]
                    if hand:
                        args = {"card_index": hand[0]["index"]}
                        if hand[0].get("target_type") == "AnyEnemy":
                            args["target_index"] = 0
                        state = run.step(case, lg, {"cmd": "action", "action": "play_card", "args": args})
                    visited.add("fight")
                    state = quit_and_continue("fight")
                    run.step(case, lg, {"cmd": "set_player", "enemy_hp": 1})
                    state = run.step(case, lg, {"cmd": "get_state"})
                elif kind == "shop" and "shop" not in visited:
                    relics = [r for r in state.get("relics") or [] if r.get("is_stocked")]
                    if relics:
                        state = run.step(case, lg, {"cmd": "action", "action": "buy_relic",
                                                    "args": {"relic_index": relics[0]["index"]}})
                    visited.add("shop")
                    state = quit_and_continue("shop")
                elif kind in ("rest_site", "event_choice") and kind not in visited and \
                        (state.get("context") or {}).get("point_type") != "Ancient":
                    visited.add(kind)
                    state = quit_and_continue(kind)
                elif kind == "map_select":
                    # rooms not seen yet first
                    choices = state.get("choices") or []
                    want = {"Shop": "shop", "RestSite": "rest_site", "Unknown": "event_choice", "Monster": "fight"}
                    pick = next((c for c in choices if want.get(c.get("type")) not in visited and want.get(c.get("type"))),
                                choices[0] if choices else None)
                    if not pick:
                        break
                    state = run.step(case, lg, {"cmd": "action", "action": "select_map_node",
                                                "args": {"col": pick["col"], "row": pick["row"]}})
                    continue
                if len(visited) >= 4:
                    break
                state = run.play_until(case, lg, state, lambda s: s.get("decision") == "map_select", 60)
        finally:
            run.finish(case, lg)
    if not only or "parasol" in only:
        case = cc.Case("scenario", "parasol")
        lg = run.logic("scenario", "parasol")
        run.new_run(case, lg, character="Ironclad", seed="parasol")
        run.step(case, lg, {"cmd": "obtain", "relics": ["LORDS_PARASOL"]})
        state = run.step(case, lg, {"cmd": "enter_room", "type": "shop"})
        run.play_until(case, lg, state, lambda s: s.get("decision") in ("shop", "map_select"), 30)
        run.finish(case, lg)
    if not only or "hopper" in only:
        case, lg, state = fight("hopper", "THIEVING_HOPPER_WEAK")
        state = end_turns(case, lg, state, 1)
        for _ in range(8):
            if state.get("decision") != "combat_play":
                break
            state = play(case, lg, state, "BLUDGEON")
        run.finish(case, lg)
    bad = [c for c in run.cases if c.status != "ok"]
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
