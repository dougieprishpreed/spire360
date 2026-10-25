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
  card_sacrifice  Pael's Wing: a fight's card reward sacrificed (its
            card reward screen alternative)
  card_reroll Driftwood with Lava Lamp: the reward's cards upgraded (the
            lamp flashing on them), rerolled once, one taken
  hopper    the Thieving Hopper's theft (the stolen card in its hand),
            then killed (the card back as a reward)
  flask     Bouncing Flask and a Fire Potion (splashes, liquid overlays),
            Neutralize's Weak (the intents' numbers at once)
  doom      an enemy killed by Doom (End of Days): NDoomVfx with the body's
            frozen hurt pose
  double_down Trial's Reject, Double Down: the abandon run popup
  doll_room the Doll Room's ambience, stopped by its handle once a doll is
            taken
  sand_castle Sand Castle's upgrades (the preview grid at three columns)
  double_boss Ascension 10's last act (enter_act): the first boss's rewards
            lead to the second boss on the map, then The Architect

Each writes OUT/scenario/<name>.jsonl and .log (content_coverage's Runner).
"""
import argparse
import json
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
    for name, relics, pick in (("card_sacrifice", ["PAELS_WING"], "SACRIFICE"),
                               ("card_reroll", ["DRIFTWOOD", "LAVA_LAMP"], "REROLL")):
        if only and name not in only:
            continue
        case = cc.Case("scenario", name)
        lg = run.logic("scenario", name)
        try:
            run.new_run(case, lg, character="Ironclad", seed=name)
            run.step(case, lg, {"cmd": "obtain", "relics": relics})
            state = run.step(case, lg, {"cmd": "enter_room", "type": "monster", "encounter": "SHRINKER_BEETLE_WEAK"})
            run.step(case, lg, {"cmd": "set_player", "enemy_hp": 1})
            state = play(case, lg, state, "STRIKE_IRONCLAD")
            state = run.play_until(case, lg, state, lambda s: s.get("decision") == "rewards", 30)
            card = next((r for r in state.get("rewards") or [] if r.get("type") == "card"), None)
            if card:
                state = run.step(case, lg, {"cmd": "action", "action": "take_reward", "args": {"reward_index": card["index"]}})
            alt = next((a for a in state.get("alternatives") or [] if a.get("id") == pick), None)
            if alt is None:
                case.status = "error"
                case.notes.append(f"no {pick} alternative on the card reward")
            else:
                state = run.step(case, lg, {"cmd": "action", "action": "choose_card_reward_alternative",
                                            "args": {"alternative_index": alt["index"]}})
                if state.get("decision") == "card_reward":
                    state = run.step(case, lg, {"cmd": "action", "action": "select_card_reward", "args": {"card_index": 0}})
            run.play_until(case, lg, state, lambda s: s.get("decision") == "map_select", 30)
        finally:
            run.finish(case, lg)
    def event(name, ev):
        case = cc.Case("scenario", name)
        lg = run.logic("scenario", name)
        run.new_run(case, lg, character="Ironclad", seed=name)
        run.step(case, lg, {"cmd": "set_player", "gold": 500})
        state = run.step(case, lg, {"cmd": "enter_room", "type": "event", "event": ev})
        return case, lg, state

    def choose(case, lg, state, key):
        opt = next((o for o in state.get("options") or [] if (o.get("text_key") or "").endswith(key)), None)
        if opt is None:
            case.status = "error"
            case.notes.append(f"no {key} option")
            return state
        return run.step(case, lg, {"cmd": "action", "action": "choose_option", "args": {"option_index": opt["index"]}})

    def cues(case, call):
        """The ui cues of a call in the case's trace so far."""
        out = []
        with open(case_trace(case), encoding="utf-8") as fh:
            for line in fh:
                if line.startswith("{") and call in line:
                    d = json.loads(line)
                    for s in d.get("steps") or []:
                        for c in s if isinstance(s, list) else [s]:
                            if c.get("call") == call:
                                out.append(c)
        return out

    def case_trace(case):
        return os.path.join(a.work, "scenario", case.name + ".jsonl")

    if not only or "doom" in only:
        # an enemy dying to Doom (DoomPower.StartDoomAnim: the Hit trigger,
        # its hurt animation frozen, NDoomVfx taking the body)
        case, lg, state = fight("doom", "SHRINKER_BEETLE_WEAK")
        run.step(case, lg, {"cmd": "set_player", "enemy_hp": 5})
        state = play(case, lg, state, "END_OF_DAYS")
        state = end_turns(case, lg, state, 2)
        run.finish(case, lg)
        if not any(c.get("pose") for c in cues(case, "NDoomVfx.Create")):
            case.status = "error"
            case.notes.append("no NDoomVfx with its pose")
    if not only or "double_down" in only:
        # Trial's Reject, Double Down: NAbandonRunConfirmPopup over the event
        case, lg, state = event("double_down", "TRIAL")
        state = choose(case, lg, state, "REJECT")
        state = choose(case, lg, state, "DOUBLE_DOWN")
        run.finish(case, lg)
        if not cues(case, "NAbandonRunConfirmPopup.Create"):
            case.status = "error"
            case.notes.append("no abandon popup cue")
    if not only or "doll_room" in only:
        # the Doll Room's ambience (NDebugAudioManager.Play) stopped by its
        # handle when a doll is taken
        case, lg, state = event("doll_room", "DOLL_ROOM")
        state = choose(case, lg, state, "RANDOM")
        run.play_until(case, lg, state, lambda s: s.get("decision") == "map_select", 30)
        run.finish(case, lg)
        plays = [c.get("handle") for c in cues(case, "NDebugAudioManager.Play") if "doll_room_amb" in str(c.get("args"))]
        stops = [c.get("args", [None])[0] for c in cues(case, "NDebugAudioManager.Stop")]
        if not plays or plays[0] not in stops:
            case.status = "error"
            case.notes.append(f"ambience handles {plays}, stops {stops}")
    if not only or "sand_castle" in only:
        # Sand Castle: its upgrades in a grid of three columns
        # (NGridCardPreviewContainer.ForceMaxColumnsUntilEmpty)
        case = cc.Case("scenario", "sand_castle")
        lg = run.logic("scenario", "sand_castle")
        try:
            run.new_run(case, lg, character="Ironclad", seed="sand_castle")
            run.step(case, lg, {"cmd": "obtain", "relics": ["SAND_CASTLE"]})
        finally:
            run.finish(case, lg)
        if not cues(case, "NGridCardPreviewContainer.ForceMaxColumnsUntilEmpty"):
            case.status = "error"
            case.notes.append("no forced columns cue")
    if not only or "double_boss" in only:
        # Ascension 10 (DoubleBoss): the last act's first boss, its rewards
        # proceed to the map, its second boss, then the act change's vote
        # (The Architect)
        data = os.path.join(a.work, "scenario", "double_boss_data")
        shutil.rmtree(data, ignore_errors=True)
        shutil.copytree(a.data, data, ignore=shutil.ignore_patterns("user"))
        case = cc.Case("scenario", "double_boss")
        lg = cc.Logic(a.cmd, data, os.path.join(a.work, "scenario", "double_boss.jsonl"))
        bosses = []
        try:
            run.step(case, lg, {"cmd": "select_profile", "profile": 1})
            run.step(case, lg, {"cmd": "set_unlock_all", "on": True})
            run.step(case, lg, {"cmd": "start_game_run", "character": "Ironclad", "ascension": 10, "seed": "double_boss"})
            state = run.step(case, lg, {"cmd": "enter_act", "act": 2})
            run.step(case, lg, {"cmd": "set_player", "max_hp": 9999, "hp": 9999})
            state = run.step(case, lg, {"cmd": "get_state"})
            for _ in range(400):
                ctx = state.get("context") or {}
                d = state.get("decision")
                if state.get("event_id") == "the_architect" or d == "game_over" or case.status != "ok":
                    break
                if d == "combat_play":
                    if ctx.get("room_type") == "Boss" and (not bosses or bosses[-1] != bool(ctx.get("at_second_boss"))):
                        bosses.append(bool(ctx.get("at_second_boss")))
                    # the enemies at 1 HP and the player tanked again every few
                    # actions (the Test Subject comes back at full HP)
                    for _ in range(80):
                        if state.get("decision") != "combat_play":
                            break
                        run.step(case, lg, {"cmd": "set_player", "enemy_hp": 1, "hp": 9999})
                        state = run.step(case, lg, {"cmd": "get_state"})
                        state = run.play_until(case, lg, state, lambda s: s.get("decision") != "combat_play", 6)
                    case.notes = [n for n in case.notes if n != "step limit"]
                elif d == "map_select":
                    state = run.step(case, lg, cc.act_for(run.rng, state))
                else:
                    state = run.play_until(case, lg, state, lambda s: s.get("decision") in ("combat_play", "map_select", "game_over")
                                           or s.get("event_id") == "the_architect", 200)
            case.notes.append(f"bosses (second?): {bosses}; ended at {state.get('decision')} {state.get('event_id') or ''}")
        finally:
            run.finish(case, lg)
        if bosses != [False, True]:
            case.status = "error"
            case.notes.append("the last act's two bosses were not both fought")
    bad = [c for c in run.cases if c.status != "ok"]
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
