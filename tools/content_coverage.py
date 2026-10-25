"""Content coverage: every card, relic, potion, enchantment, affliction,
event, encounter and act of the game played through the real logic, with
everything unlocked.

    python tools/content_coverage.py --cmd "dotnet out/logic/jit/Spire.Logic.dll" \
        --data work/pack/data --work work/coverage [--suite cards,relics,...] [--limit N]

Suites (default: all):
  cards       each card generated into the hand and played (and upgraded)
  potions     each potion obtained and used in a fight
  relics      each relic obtained (RelicCmd.Obtain) by its character, then
              fights with trigger cards played, low HP, its rest site
              option, a shop with gold, a treasure room
  enchantments each enchantment on the deck's cards it can enchant
              (CardCmd.Enchant), then a fight, a rest site and a shop
  afflictions each affliction on the hand's cards (CardCmd.Afflict), the
              fight played to its end
  events      each event entered and played through from each of its first
              options
  encounters  each encounter fought to its end (999 HP)
  acts        whole runs of the game's flow with everything unlocked, every
              character, until each act (alternates included) and the
              ending were reached
  scenarios   chains the suites above do not reach on their own (the
              Byrdonis egg hatched into Byrdpip at a rest site, ...)

Each case's protocol trace goes to <work>/<suite>/<case>.jsonl (replayable
by spire_check); <work>/report.json has every case's outcome, the errors
and the decision kinds, cues and screens the runs used. The logic's stderr
is scanned for exceptions per case. Each case's presentation ledger (the
logic's SPIRE_LEDGER: UI calls and their answers, stripped UI bodies run,
exceptions) goes to <case>.ledger.jsonl for tools/presentation_audit.py
(--no-ledger: none).
"""
import argparse
import collections
import json
import os
import random
import re
import shlex
import shutil
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import random_runs  # noqa: E402


def act_for(rng, state, taken=None):
    """random_runs' agent, with everything a coverage run needs to keep going.
    taken: what this coverage run has picked so far; a pick of one card (the
    Knowledge Demon's curses, a choose-a-card screen) or a rest site option
    goes to one never taken when there is one, so a rare choice is not left
    to chance."""
    cmd = random_runs.act(rng, state)
    if taken is None:
        return cmd
    d = state.get("decision")
    args = cmd.get("args") or {}
    if d == "card_select" and cmd.get("action") == "select_cards":
        cards = state.get("cards") or []
        picks = [int(i) for i in str(args.get("indices", "")).split(",") if i != ""]
        fresh = [i for i, c in enumerate(cards) if ("card", c.get("id")) not in taken]
        if len(picks) == 1 and fresh:
            picks = [rng.choice(fresh)]
            cmd = dict(cmd, args=dict(args, indices=str(picks[0])))
        taken.update(("card", cards[i].get("id")) for i in picks if i < len(cards))
    elif d == "rest_site" and cmd.get("action") == "choose_option":
        opts = [o for o in state.get("options", []) if o.get("is_enabled", True)]
        fresh = [o for o in opts if ("rest", o.get("option_id")) not in taken]
        if fresh:
            pick = rng.choice(fresh)
            cmd = dict(cmd, args=dict(args, option_index=pick["index"]))
        chosen = [o for o in opts if o.get("index") == cmd["args"].get("option_index")]
        taken.update(("rest", o.get("option_id")) for o in chosen)
    return cmd


LEDGER = True


class Logic:
    """One logic process. A command that takes longer than TIMEOUT kills it
    (hung); once it is gone every command answers an error, so the case runs
    out and the next one starts a new process."""

    TIMEOUT = 300

    def __init__(self, cmd, data, trace):
        self.cmd, self.data, self.trace = cmd, data, trace
        self.dead = None  # why the process is gone
        self.hung = False
        if LEDGER and os.path.exists(trace[:-6] + ".ledger.jsonl"):
            os.remove(trace[:-6] + ".ledger.jsonl")
        self.start()

    def start(self):
        env = dict(os.environ)
        env["SPIRE_DATA"] = self.data
        if LEDGER:
            env["SPIRE_LEDGER"] = os.path.abspath(self.trace[:-6] + ".ledger.jsonl")
        self.s = random_runs.Session(shlex.split(self.cmd), env, self.trace, False)
        ready = self.s.read()
        if ready.get("type") != "ready":
            raise RuntimeError(f"logic did not start: {ready}")

    def send(self, cmd):
        if self.dead:
            return {"type": "error", "message": self.dead}
        timer = threading.Timer(self.TIMEOUT, self._kill)
        timer.daemon = True
        timer.start()
        try:
            return self.s.send(cmd)
        except (RuntimeError, OSError, ValueError) as ex:
            text = json.dumps(cmd)[:200]
            self.dead = (f"logic hung on {text} (killed after {self.TIMEOUT} s)" if self.hung
                         else f"logic ended on {text}: {str(ex)[:300]}")
            return {"type": "error", "message": self.dead}
        finally:
            timer.cancel()

    def _kill(self):
        self.hung = True
        try:
            self.s.proc.kill()
        except Exception:
            pass

    def close(self):
        try:
            self.s.close()
        except Exception:
            pass


class Case:
    """One coverage case: its trace and the facts collected from it."""

    def __init__(self, suite, name):
        self.suite, self.name = suite, name
        self.status = "ok"
        self.errors = []
        self.decisions = {}
        self.cues = set()
        self.notes = []
        self.monsters = set()  # the enemies' ids the fight's states showed

    def record(self, state):
        t = state.get("type")
        if t == "error":
            self.errors.append((state.get("message") or "")[:400])
            if self.status == "ok":
                self.status = "error"
            return
        d = state.get("decision")
        if d:
            self.decisions[d] = self.decisions.get(d, 0) + 1
        for e in state.get("enemies") or []:
            if isinstance(e, dict) and e.get("id"):
                self.monsters.add(e["id"])
        for step in state.get("steps") or []:
            for ch in step if isinstance(step, list) else [step]:
                if not isinstance(ch, dict):
                    continue
                kind = ch.get("t")
                if kind in ("vfx", "sfx", "music", "anim", "ui"):
                    key = ch.get("path") or ch.get("event") or ch.get("name") or ch.get("call")
                    if key:
                        self.cues.add(f"{kind}:{key}")

    def as_dict(self):
        return {"suite": self.suite, "name": self.name, "status": self.status, "errors": self.errors[:10],
                "decisions": self.decisions, "notes": self.notes[:20]}


class Runner:
    def __init__(self, a):
        self.a = a
        self.cases = []
        self.cues = {}
        self.rng = random.Random(a.seed)
        self.taken = set()  # act_for's picks so far

    # ---- plumbing ----

    def logic(self, suite, name):
        d = os.path.join(self.a.work, suite)
        os.makedirs(d, exist_ok=True)
        safe = re.sub(r"[^A-Za-z0-9_.-]", "_", name)
        return Logic(self.a.cmd, self.a.data, os.path.join(d, safe + ".jsonl"))

    def finish(self, case, logic):
        logic.close()
        log = logic.trace[:-6] + ".log"
        try:
            with open(log, encoding="utf-8", errors="replace") as fh:
                text = fh.read()
            for m in re.finditer(r"^.*(Exception|\[ERROR\]|Unhandled).*$", text, re.M):
                line = m.group(0)[:300]
                if "InitProgressData" in line:  # the fresh profile's one known warning
                    continue
                case.notes.append(line)
                if case.status == "ok":
                    case.status = "exception"
        except OSError:
            pass
        for c in case.cues:
            self.cues[c] = self.cues.get(c, 0) + 1
        self.cases.append(case)
        print(f"  {case.suite}/{case.name}: {case.status}" + (f" ({case.errors[0][:120]})" if case.errors else "")
              + (f" [{case.notes[0][:120]}]" if case.notes and case.status != "ok" else ""), flush=True)

    def step(self, case, logic, cmd):
        state = logic.send(cmd)
        case.record(state)
        if logic.dead and case.status in ("ok", "error"):
            case.status = "hung" if logic.hung else "crashed"
        return state

    def play_until(self, case, logic, state, stop, limit=400):
        """random_runs' agent until stop(state), game over or the limit."""
        last, same = None, 0
        for _ in range(limit):
            if state.get("type") == "error":
                state = self.step(case, logic, {"cmd": "get_state"})
                if state.get("type") == "error":
                    state = self.step(case, logic, {"cmd": "action", "action": "proceed"})
                    if state.get("type") == "error":
                        return state
                continue
            if stop(state) or state.get("decision") == "game_over":
                return state
            key = json.dumps({k: state.get(k) for k in ("decision", "round", "energy", "hand", "enemies", "options")},
                             sort_keys=True)
            same = same + 1 if key == last else 0
            last = key
            if same > 30:
                case.status = "stuck"
                case.notes.append(f"stuck at {state.get('decision')}")
                return state
            state = self.step(case, logic, act_for(self.rng, state, self.taken))
        case.notes.append("step limit")
        return state

    def new_run(self, case, logic, character="Ironclad", seed="coverage"):
        state = self.step(case, logic, {"cmd": "start_run", "character": character, "seed": seed})
        self.step(case, logic, {"cmd": "set_player", "max_hp": 9999, "hp": 9999})
        return state

    def content(self):
        lg = self.logic("content", "content")
        c = lg.send({"cmd": "content"})
        lg.close()
        if c.get("type") != "content":
            raise RuntimeError(f"content failed: {c}")
        return c

    def limited(self, items):
        only = set(filter(None, (getattr(self.a, "only", "") or "").split(",")))
        if only:
            items = [x for x in items if (x.get("id") if isinstance(x, dict) else x) in only]
        return items[: self.a.limit] if self.a.limit else items

    # ---- suites ----

    def suite_cards(self, content):
        """Each card into the hand of a fight against several enemies, played
        (its target the first enemy), its prompts answered; then upgraded.
        A power is followed by two turns (its turn start and end triggers:
        Noxious Fumes, The Bomb); an unplayable card stays in hand through
        an end of turn (Burn, Infection)."""
        cards = self.limited(content["cards"])
        batch = 25
        # A character's cards are tried with that character (the Regent's
        # stars, the Necrobinder's Osty); the others with the Ironclad.
        chunks = []
        for pool in sorted({c["pool"] for c in cards}):
            owner = pool.split("_")[0]
            ch = owner.capitalize() if owner in content["characters"] else "Ironclad"
            mine = [c for c in cards if c["pool"] == pool]
            for start in range(0, len(mine), batch):
                chunks.append((f"{pool.lower().replace('_card_pool', '')}_{start:03d}", ch, mine[start:start + batch]))
        for tag, ch, chunk in chunks:
            for upgraded in (False, True):
                name = f"cards_{tag}{'_plus' if upgraded else ''}"
                # the upgraded pass kills: among several enemies, so a kill is
                # not always the fight's last (The Hunt's power: PowerCmd.Apply
                # does nothing once the combat is ending)
                fight = ({"type": "monster", "encounter": KILL_FIGHT} if upgraded
                         else {"type": "elite", "encounter": self.a.fight})
                case = Case("cards", name)
                lg = self.logic("cards", name)
                in_fight = lambda s: s.get("decision") in ("combat_play", "rewards", "map_select")

                def end_turn(state):
                    if state.get("decision") != "combat_play":
                        return state
                    state = self.step(case, lg, {"cmd": "action", "action": "end_turn", "args": {}})
                    return self.play_until(case, lg, state, in_fight, 60)

                try:
                    self.new_run(case, lg, character=ch, seed=f"cards-{tag}")
                    state = self.step(case, lg, {"cmd": "enter_room", **fight})
                    for card in chunk:
                        if card.get("upgradable") is False and upgraded:
                            continue
                        if card.get("multiplayer_only"):
                            case.notes.append(f"{card['id']}: multiplayer only")
                            continue
                        if state.get("decision") == "game_over":
                            # the card killed the player (The Gambit): a new run
                            case.notes.append(f"{card['id']}: the player died; a new run")
                            self.new_run(case, lg, character=ch, seed=f"cards-{tag}-{card['id']}")
                            state = self.step(case, lg, {"cmd": "enter_room", **fight})
                        if state.get("decision") != "combat_play":
                            # The fight is over: through its rewards to the map, then a new one.
                            state = self.play_until(case, lg, state, lambda s: s.get("decision") == "map_select", 60)
                            state = self.step(case, lg, {"cmd": "enter_room", **fight})
                            self.step(case, lg, {"cmd": "set_player", "hp": 9999})
                        if len(state.get("hand") or []) >= 9:
                            # A full hand takes no more cards: a new turn discards it.
                            state = self.step(case, lg, {"cmd": "action", "action": "end_turn", "args": {}})
                            state = self.play_until(case, lg, state, lambda s: s.get("decision") == "combat_play", 60)
                            if state.get("decision") != "combat_play":
                                state = self.play_until(case, lg, state, lambda s: s.get("decision") == "map_select", 60)
                                state = self.step(case, lg, {"cmd": "enter_room", **fight})
                        state = self.step(case, lg, {"cmd": "add_to_hand", "cards": [card["id"]], "energy": 9,
                                                     "stars": 9, "upgraded": upgraded})
                        hand = state.get("hand") or []
                        mine = [h for h in hand if (h.get("id") or "").split(".")[-1] == card["id"]]
                        if not mine or (not mine[-1].get("can_play") and
                                        "BlockedByCardLogic" in (mine[-1].get("unplayable_reason") or "")):
                            # its own condition (an empty draw pile, a hand of
                            # attacks), or a full hand (cards the last one
                            # made): the card alone in hand, nothing to draw
                            state = self.step(case, lg, {"cmd": "add_to_hand", "cards": [card["id"]], "energy": 9,
                                                         "stars": 9, "upgraded": upgraded, "clear": True})
                            hand = state.get("hand") or []
                            mine = [h for h in hand if (h.get("id") or "").split(".")[-1] == card["id"]]
                        if not mine:
                            case.notes.append(f"{card['id']}: not in hand")
                            continue
                        h = mine[-1]
                        if not h.get("can_play"):
                            case.notes.append(f"{card['id']}: unplayable ({h.get('unplayable_reason') or ''})")
                            state = end_turn(state)
                            continue
                        args = {"card_index": h["index"]}
                        if h.get("target_type") == "AnyEnemy" and state.get("enemies"):
                            args["target_index"] = 0
                        if upgraded and card.get("type") == "Attack":
                            # the upgraded pass kills with it: what a card does
                            # on a kill (Feed, The Hunt, Hand of Greed, Sunder,
                            # Fatal) and the death hooks it sets off
                            self.step(case, lg, {"cmd": "set_player", "enemy_hp": 1})
                        elif not upgraded:
                            # the plain pass never kills: what comes after the
                            # attack (Rebound's power) runs
                            self.step(case, lg, {"cmd": "set_player", "enemy_heal": True})
                        powers = {p.get("id") for p in state.get("player_powers") or []}
                        state = self.step(case, lg, {"cmd": "action", "action": "play_card", "args": args})
                        # Its prompts (card selections, choices) until the hand is back.
                        state = self.play_until(case, lg, state, in_fight, 40)
                        # A power, or a card that left a new power on the player
                        # (Shadow Step's next turn): its turn triggers.
                        if card.get("type") == "Power" or {p.get("id") for p in state.get("player_powers") or []} - powers:
                            state = end_turn(end_turn(state))
                finally:
                    self.finish(case, lg)

    def suite_potions(self, content):
        for p in self.limited(content["potions"]):
            case = Case("potions", p["id"])
            lg = self.logic("potions", p["id"])
            try:
                self.new_run(case, lg, seed=f"potion-{p['id']}")
                state = self.step(case, lg, {"cmd": "obtain", "potions": [p["id"]]})
                if p.get("usage") not in ("CombatOnly", "None"):
                    # Out of combat first, where the potion allows it.
                    pots = [x for x in (state.get("player") or {}).get("potions") or [] if x]
                    if pots and pots[0].get("can_use"):
                        state = self.step(case, lg, {"cmd": "action", "action": "use_potion",
                                                     "args": {"potion_index": pots[0]["index"]}})
                        state = self.play_until(case, lg, state, lambda s: s.get("decision") == "map_select", 30)
                        state = self.step(case, lg, {"cmd": "obtain", "potions": [p["id"]]})
                    # And in a shop (Foul Potion thrown at the merchant)
                    state = self.step(case, lg, {"cmd": "enter_room", "type": "shop"})
                    pots = [x for x in (state.get("player") or {}).get("potions") or [] if x]
                    if pots and pots[0].get("can_use"):
                        state = self.step(case, lg, {"cmd": "action", "action": "use_potion",
                                                     "args": {"potion_index": pots[0]["index"]}})
                    state = self.step(case, lg, {"cmd": "action", "action": "leave_room", "args": {}})
                    state = self.step(case, lg, {"cmd": "obtain", "potions": [p["id"]]})
                state = self.step(case, lg, {"cmd": "enter_room", "type": "elite", "encounter": self.a.fight})
                pots = [x for x in (state.get("player") or {}).get("potions") or [] if x]
                if pots:
                    args = {"potion_index": pots[0]["index"]}
                    if pots[0].get("target_type") == "AnyEnemy" and state.get("enemies"):
                        args["target_index"] = 0
                    state = self.step(case, lg, {"cmd": "action", "action": "use_potion", "args": args})
                    self.play_until(case, lg, state, lambda s: s.get("decision") == "combat_play", 30)
                else:
                    case.notes.append("no potion to use")
            finally:
                self.finish(case, lg)

    def suite_relics(self, content):
        """Each relic obtained (RelicCmd.Obtain) by its pool's character, then
        what its triggers need: fights with a kit of trigger cards generated
        into the hand and played (RELIC_KIT), two more turns, a fight at 40%
        HP (Red Skull), a third fight (every-third-combat counters), its own
        rest site option, a shop with gold to spend, a treasure room, and a
        last fight at 1 HP (Lizard Tail; the run may end there)."""
        weak = "SHRINKER_BEETLE_WEAK"
        for r in self.limited(content["relics"]):
            case = Case("relics", r["id"])
            lg = self.logic("relics", r["id"])
            in_fight = lambda s: s.get("decision") in ("combat_play", "rewards", "map_select", "game_over")
            to_map = lambda s: s.get("decision") == "map_select"
            try:
                owner = (r.get("pool") or "").split("_")[0]
                ch = owner.capitalize() if owner in content["characters"] else "Ironclad"
                self.new_run(case, lg, character=ch, seed=f"relic-{r['id']}")
                state = self.step(case, lg, {"cmd": "obtain", "relics": [r["id"]]})
                if state.get("not_allowed"):
                    case.notes.append("multiplayer only: not in solo runs")
                    continue
                state = self.play_until(case, lg, state, to_map, 30)
                for n, (room, encounter, hp) in enumerate((("elite", self.a.fight, None), ("monster", weak, 4000),
                                                           ("monster", weak, None))):
                    if hp:
                        self.step(case, lg, {"cmd": "set_player", "hp": hp})
                    state = self.step(case, lg, {"cmd": "enter_room", "type": room, "encounter": encounter})
                    if n < 2 and state.get("decision") == "combat_play":
                        state = self.play_kit(case, lg, state, RELIC_KIT, in_fight)
                        if n == 0:
                            # the first fight then runs its course (Pen Nib's
                            # tenth attack, Stone Calendar's seventh turn)
                            self.step(case, lg, {"cmd": "set_player", "enemy_heal": True})
                        for _ in range(2):
                            if state.get("decision") != "combat_play":
                                break
                            state = self.step(case, lg, {"cmd": "action", "action": "end_turn", "args": {}})
                            state = self.play_until(case, lg, state, in_fight, 60)
                    if state.get("decision") == "combat_play" and n > 0:
                        self.step(case, lg, {"cmd": "set_player", "enemy_hp": 5})
                    state = self.play_until(case, lg, state, to_map, 300)
                for room in ({"type": "rest_site"}, {"type": "shop"}, {"type": "treasure"}):
                    if room["type"] == "shop":
                        self.step(case, lg, {"cmd": "set_player", "gold": 999})
                    state = self.step(case, lg, dict({"cmd": "enter_room"}, **room))
                    if state.get("decision") == "rest_site":
                        # the relic's own rest option (Dig, Lift, Kindle, Clone,
                        # Hatch, ...) rather than whatever the agent picks
                        own = [o for o in state.get("options") or []
                               if o.get("option_id") not in ("HEAL", "SMITH") and not o.get("is_locked")]
                        if own:
                            state = self.step(case, lg, {"cmd": "action", "action": "choose_option",
                                                         "args": {"option_index": own[0]["index"]}})
                    state = self.play_until(case, lg, state, to_map, 300)
                self.step(case, lg, {"cmd": "set_player", "hp": 1})
                state = self.step(case, lg, {"cmd": "enter_room", "type": "monster", "encounter": weak})
                if state.get("decision") == "combat_play":
                    state = self.step(case, lg, {"cmd": "action", "action": "end_turn", "args": {}})
                    state = self.play_until(case, lg, state, in_fight, 60)
                    if state.get("decision") == "combat_play":
                        self.step(case, lg, {"cmd": "set_player", "enemy_hp": 5})
                    self.play_until(case, lg, state, lambda s: to_map(s) or s.get("decision") == "game_over", 300)
            finally:
                self.finish(case, lg)

    def play_kit(self, case, lg, state, kit, in_fight):
        """The kit's cards generated into a cleared hand with energy and stars,
        each played (the first enemy its target), its prompts answered."""
        state = self.step(case, lg, {"cmd": "add_to_hand", "cards": kit, "energy": 20, "stars": 20, "clear": True})
        for card_id in kit:
            if state.get("decision") != "combat_play":
                break
            mine = [h for h in state.get("hand") or [] if (h.get("id") or "").split(".")[-1] == card_id and h.get("can_play")]
            if not mine:
                continue
            args = {"card_index": mine[0]["index"]}
            if mine[0].get("target_type") == "AnyEnemy" and state.get("enemies"):
                args["target_index"] = 0
            state = self.step(case, lg, {"cmd": "action", "action": "play_card", "args": args})
            state = self.play_until(case, lg, state, in_fight, 40)
        return state

    def suite_enchantments(self, content):
        """Each enchantment on every deck card it can enchant (CardCmd.Enchant;
        a power and an exhausting skill join the starter deck first), then a
        fight played to its end, a rest site and a shop: its draw, play, turn
        and deck triggers run."""
        for e in self.limited(content.get("enchantments") or []):
            case = Case("enchantments", e)
            lg = self.logic("enchantments", e)
            try:
                state = self.new_run(case, lg, seed=f"enchant-{e}")
                # Neow first, as in a run (Leafy Poultice transforms cards an
                # enchantment may have made unremovable: Tezcatara's Ember)
                self.play_until(case, lg, state, lambda s: s.get("decision") == "map_select", 30)
                self.step(case, lg, {"cmd": "obtain", "cards": ["INFLAME", "DOMINATE"]})
                enchant = {"id": e, "amount": ENCHANT_AMOUNTS.get(e, 1)}
                if e in ENCHANT_TARGETS:
                    enchant["target"] = ENCHANT_TARGETS[e]
                state = self.step(case, lg, {"cmd": "obtain", "enchant": [enchant]})
                if state.get("not_allowed"):
                    case.status = "error"
                    case.notes.append("no deck card it can enchant")
                    continue
                state = self.play_until(case, lg, state, lambda s: s.get("decision") == "map_select", 30)
                for room in ({"type": "elite", "encounter": self.a.fight}, {"type": "rest_site"}, {"type": "shop"}):
                    state = self.step(case, lg, dict({"cmd": "enter_room"}, **room))
                    state = self.play_until(case, lg, state, lambda s: s.get("decision") == "map_select", 300)
            finally:
                self.finish(case, lg)

    def suite_afflictions(self, content):
        """Each affliction on the hand's cards at a fight's start
        (CardCmd.Afflict), the fight played to its end: the afflicted cards
        played or held through ends of turn."""
        for af in self.limited(content.get("afflictions") or []):
            case = Case("afflictions", af)
            lg = self.logic("afflictions", af)
            try:
                self.new_run(case, lg, seed=f"afflict-{af}")
                self.step(case, lg, {"cmd": "enter_room", "type": "elite", "encounter": self.a.fight})
                state = self.step(case, lg, {"cmd": "add_to_hand", "afflict": {"id": af, "amount": 1}})
                if not any(h.get("affliction") for h in state.get("hand") or []):
                    case.status = "error"
                    case.notes.append("no hand card afflicted")
                    continue
                self.play_until(case, lg, state, lambda s: s.get("decision") in ("rewards", "map_select"), 300)
            finally:
                self.finish(case, lg)

    def suite_events(self, content):
        events = []
        for a in content["acts"]:
            events += [(e, a["id"]) for e in a["events"]]
            events += [(e, a["id"]) for e in a["ancients"]]
        events += [(e, "") for e in content["shared_events"]]
        events += [(e, "") for e in content["shared_ancients"]]
        seen = set()
        for ev, act in self.limited(events):
            if ev in seen:
                continue
            seen.add(ev)
            # The event's option tree, breadth first (the same seed each
            # time, so a path leads to the same pages): every option of
            # every page up to 4 deep, at most 16 paths; each path then
            # played out. Gold enough that its gold options are open.
            paths = [[]]
            done_paths = 0
            while paths and done_paths < 16:
                path = paths.pop(0)
                name = f"{ev}_" + ("_".join(str(k) for k in path) if path else "probe")
                case = Case("events", name)
                lg = self.logic("events", name)
                try:
                    self.new_run(case, lg, seed=f"event-{ev}")
                    self.step(case, lg, {"cmd": "set_player", "gold": 500})
                    state = self.step(case, lg, {"cmd": "enter_room", "type": "event", "event": ev})
                    ok = True
                    for k in path:
                        if state.get("decision") != "event_choice":
                            ok = False
                            break
                        state = self.step(case, lg, {"cmd": "action", "action": "choose_option",
                                                     "args": {"option_index": k}})
                    if ok and state.get("decision") == "event_choice" and len(path) < 4:
                        opts = [o["index"] for o in state.get("options") or [] if not o.get("is_locked")]
                        locked = [o["index"] for o in state.get("options") or [] if o.get("is_locked")]
                        if locked:
                            case.notes.append(f"locked at {path}: {locked}")
                        paths += [path + [k] for k in opts]
                    if path:
                        done_paths += 1
                        self.play_until(case, lg, state, lambda s: s.get("decision") == "map_select", 400)
                finally:
                    self.finish(case, lg)
            if paths:
                case.notes.append(f"{len(paths)} paths left unexplored")

    def suite_encounters(self, content):
        encs = []
        for a in content["acts"]:
            encs += [(e["id"], e["room"]) for e in a["encounters"]]
        encs += [(e, "Monster") for e in content["event_encounters"]]
        seen = set()
        possible = content.get("encounter_monsters") or {}
        todo = []
        for enc, room in self.limited(encs):
            if enc in seen:
                continue
            seen.add(enc)
            todo.append((enc, room, 0, 150))
        met = collections.defaultdict(set)
        while todo:
            enc, room, attempt, cap_after = todo.pop(0)
            name = enc if attempt == 0 else f"{enc}_{attempt}"
            case = Case("encounters", name)
            lg = self.logic("encounters", name)
            try:
                self.new_run(case, lg, seed=f"enc-{enc}" + (f"-{attempt}" if attempt else ""))
                kind = {"Boss": "elite", "Elite": "elite"}.get(room, "monster")
                state = self.step(case, lg, {"cmd": "enter_room", "type": kind, "encounter": enc})
                # Random play for a while (the moves), then the enemies' HP
                # capped now and then (set_player enemy_hp) until they die of
                # the game's own damage: every fight reaches its deaths, last
                # phases and rewards (bosses outlasted 9999 HP of random play).
                done = lambda s: s.get("decision") in ("rewards", "map_select", "card_reward", "game_over")
                state = self.play_until(case, lg, state, done, cap_after)
                for _ in range(12):
                    if done(state):
                        break
                    if state.get("decision") == "combat_play":
                        self.step(case, lg, {"cmd": "set_player", "hp": 9999, "enemy_hp": 12})
                        state = self.step(case, lg, {"cmd": "get_state"})
                    state = self.play_until(case, lg, state, done, 60)
                # the monsters this fight had (spawns included); another seed
                # while some of the encounter's possible ones never came
                met[enc] |= case.monsters
                missing = set(possible.get(enc) or []) - met[enc]
                if state.get("decision") == "game_over" and cap_after > 10:
                    # the player died despite 9999 HP (The Insatiable's
                    # Sandpit kills outright): again with the enemies' HP
                    # capped after a few steps, so their own deaths run
                    case.notes.append("the player died: again with the cap early")
                    todo.append((enc, room, attempt + 1, 10))
                elif missing and attempt < 7:
                    todo.append((enc, room, attempt + 1, cap_after))
                elif missing:
                    case.notes.append("never met: " + ", ".join(sorted(missing)))
                if state.get("decision") not in ("rewards", "map_select", "card_reward", "game_over"):
                    case.notes.append(f"ended at {state.get('decision')}")
            finally:
                self.finish(case, lg)

    def suite_acts(self, content):
        """Whole runs of the game's flow (unlock everything, a new profile),
        each character, until every act and the ending were reached."""
        acts_seen = set()
        endings = 0
        chars = content["characters"]
        for i in range(self.a.act_runs):
            ch = chars[i % len(chars)]
            name = f"run_{i:02d}_{ch.lower()}"
            case = Case("acts", name)
            data = os.path.join(self.a.work, "acts", name + "_data")
            shutil.rmtree(data, ignore_errors=True)
            shutil.copytree(self.a.data, data, ignore=shutil.ignore_patterns("user"))
            lg = Logic(self.a.cmd, data, os.path.join(self.a.work, "acts", name + ".jsonl"))
            try:
                self.step(case, lg, {"cmd": "select_profile", "profile": 1})
                self.step(case, lg, {"cmd": "set_unlock_all", "on": True})
                # a new profile's first act is the undiscovered alternate
                # (Underdocks); every other run is a seasoned profile's, its
                # first act drawn from both (Overgrowth's flow end to end)
                if i % 2:
                    self.step(case, lg, {"cmd": "discover_acts"})
                state = self.step(case, lg, {"cmd": "start_game_run", "character": ch, "ascension": i % 11,
                                             "seed": f"acts-{i}"})
                tank = self.step(case, lg, {"cmd": "set_player", "max_hp": 9999, "hp": 9999})
                state = self.step(case, lg, {"cmd": "get_state"})
                for _ in range(12000):
                    ctx = state.get("context") or {}
                    if ctx.get("act_name"):
                        acts_seen.add(ctx.get("act_name"))
                    if state.get("decision") == "game_over":
                        break
                    if state.get("decision") == "map_select" and (state.get("player") or {}).get("hp", 9999) < 5000:
                        self.step(case, lg, {"cmd": "set_player", "hp": 9999})
                    state = self.play_until(case, lg, state, lambda s: s.get("decision") == "map_select", 2000)
                    if state.get("decision") == "map_select":
                        state = self.step(case, lg, act_for(self.rng, state))
                    if case.status == "stuck" or state.get("type") == "error":
                        break
                if state.get("decision") == "game_over":
                    case.notes.append(f"{'victory' if state.get('victory') else 'defeat'} in act "
                                      f"{state.get('act')} floor {state.get('floor')}")
                    if state.get("victory"):
                        endings += 1
                else:
                    case.notes.append(f"stopped at {state.get('decision')}")
                del tank
            finally:
                self.finish(case, lg)
        print(f"  acts seen: {sorted(acts_seen)}; victories {endings}")

    def suite_scenarios(self, content):
        # The Knowledge Demon's three curse choices (a pick is required): one
        # fight taking Disintegration each time, one taking the other curse
        # (Mind Rot, Sloth, Waste Away)
        for side in (0, 1):
            case = Case("scenarios", f"knowledge_demon_{side}")
            lg = self.logic("scenarios", f"knowledge_demon_{side}")
            try:
                state = self.new_run(case, lg, seed=f"knowledge-{side}")
                state = self.play_until(case, lg, state, lambda s: s.get("decision") == "map_select", 30)
                state = self.step(case, lg, {"cmd": "enter_room", "type": "monster", "encounter": "KNOWLEDGE_DEMON_BOSS"})
                chosen = 0
                for _ in range(200):
                    d = state.get("decision")
                    if d in ("rewards", "map_select", "game_over") or state.get("type") == "error":
                        break
                    if d == "card_select" and state.get("kind") == "choose_a_card":
                        cards = state.get("cards") or []
                        pick = next((c for c in cards if ((c.get("id") or "").endswith("DISINTEGRATION")) == (side == 0)),
                                    cards[0] if cards else None)
                        if pick is not None:
                            state = self.step(case, lg, {"cmd": "action", "action": "select_cards",
                                                         "args": {"indices": str(pick["index"])}})
                            chosen += 1
                            if chosen == 3:
                                self.step(case, lg, {"cmd": "set_player", "enemy_hp": 5})
                                break
                            continue
                    if d == "combat_play":
                        # end turns until the demon has asked three times
                        state = self.step(case, lg, {"cmd": "action", "action": "end_turn", "args": {}})
                        continue
                    state = self.step(case, lg, act_for(self.rng, state))
                if chosen < 3:
                    case.status = "error"
                    case.notes.append(f"{chosen} curse choices, not 3")
                self.play_until(case, lg, state, lambda s: s.get("decision") == "map_select", 100)
            finally:
                self.finish(case, lg)
        # Tinker Time's Mad Science: each of its nine type and rider variants
        # (EventSetup gives each new one the next) played, its powers' turns
        # (Curious, Improvement)
        case = Case("scenarios", "mad_science")
        lg = self.logic("scenarios", "mad_science")
        try:
            self.new_run(case, lg, seed="mad-science")
            state = self.step(case, lg, {"cmd": "enter_room", "type": "elite", "encounter": self.a.fight})
            in_fight = lambda s: s.get("decision") in ("combat_play", "rewards", "map_select")
            for _ in range(9):
                if state.get("decision") != "combat_play":
                    state = self.play_until(case, lg, state, lambda s: s.get("decision") == "map_select", 60)
                    state = self.step(case, lg, {"cmd": "enter_room", "type": "elite", "encounter": self.a.fight})
                state = self.play_kit(case, lg, state, ["MAD_SCIENCE"], in_fight)
                for _ in range(2):
                    if state.get("decision") == "combat_play":
                        state = self.step(case, lg, {"cmd": "action", "action": "end_turn", "args": {}})
                        state = self.play_until(case, lg, state, in_fight, 60)
        finally:
            self.finish(case, lg)
        # The Byrdonis egg: the event's egg card, hatched at a rest site
        # into Byrdpip, which then fights beside the player.
        case = Case("scenarios", "byrdpip")
        lg = self.logic("scenarios", "byrdpip")
        try:
            self.new_run(case, lg, seed="byrdpip")
            state = self.step(case, lg, {"cmd": "obtain", "cards": ["BYRDONIS_EGG"]})
            state = self.step(case, lg, {"cmd": "enter_room", "type": "rest_site"})
            opts = [o for o in state.get("options") or [] if (o.get("id") or o.get("option_id") or "").upper() == "HATCH"
                    or "HATCH" in json.dumps(o).upper()]
            if not opts:
                case.status = "error"
                case.notes.append("no Hatch option at the rest site: " + json.dumps(state.get("options"))[:300])
            else:
                state = self.step(case, lg, {"cmd": "action", "action": "choose_option",
                                             "args": {"option_index": opts[0]["index"]}})
                state = self.play_until(case, lg, state, lambda s: s.get("decision") == "map_select", 20)
                state = self.step(case, lg, {"cmd": "enter_room", "type": "monster", "encounter": self.a.fight})
                allies = json.dumps(state.get("allies") or state.get("pets") or state.get("player", {}).get("pets") or "")
                case.notes.append("allies: " + allies[:200])
                self.play_until(case, lg, state, lambda s: s.get("decision") in ("rewards", "map_select"), 300)
        finally:
            self.finish(case, lg)
        # The Byrdonis Nest event itself.
        case = Case("scenarios", "byrdonis_nest")
        lg = self.logic("scenarios", "byrdonis_nest")
        try:
            self.new_run(case, lg, seed="nest")
            state = self.step(case, lg, {"cmd": "enter_room", "type": "event", "event": "BYRDONIS_NEST"})
            self.play_until(case, lg, state, lambda s: s.get("decision") == "map_select", 50)
        finally:
            self.finish(case, lg)


SUITES = ["cards", "potions", "relics", "enchantments", "afflictions", "events", "encounters", "scenarios", "acts"]
# CardCmd.Enchant's amount where the game's own sources give more than 1
ENCHANT_AMOUNTS = {"CLONE": 4, "SHARP": 2, "NIMBLE": 2, "SWIFT": 2}
# Cards that set relics' triggers off, generated into the hand (Regalite)
# and played: Shivs (Helical Dart), an X cost (Chemical X), an exhaust
# (Charon's Ashes, Burning Sticks), a discard (Tingsha, Tough Bandages), an
# orb (Metronome), poison (Snecko Skull), Strength (Ruined Helmet), stars
# spent (Galactic Dust, Mini Regent); three skills (Letter Opener) and the
# three card types (Rainbow Ring) among them
# The cards suite's kill pass: three enemies
KILL_FIGHT = "TWO_TAILED_RATS_NORMAL"
RELIC_KIT = ["SHIV", "SHIV", "WHIRLWIND", "TRUE_GRIT", "SURVIVOR", "ZAP", "DEADLY_POISON", "INFLAME", "FALLING_STAR"]
# The cards the game's own sources give it: Inky only on Blade of Ink's
# Shivs (its OnPlay weakens cardPlay.Target, null on an untargeted card)
ENCHANT_TARGETS = {"INKY": "AnyEnemy"}


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--cmd", required=True)
    ap.add_argument("--data", required=True)
    ap.add_argument("--work", required=True)
    ap.add_argument("--suite", default=",".join(SUITES))
    ap.add_argument("--limit", type=int, default=0, help="first N items of each suite (0: all)")
    ap.add_argument("--only", default="", help="comma-separated item ids (a relic, a potion, ...) to run alone")
    ap.add_argument("--fight", default="BYGONE_EFFIGY_ELITE", help="the encounter cards and potions are tried in")
    ap.add_argument("--act-runs", type=int, default=10)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--no-ledger", action="store_true", help="no presentation ledgers")
    a = ap.parse_args()
    global LEDGER
    LEDGER = not a.no_ledger
    os.makedirs(a.work, exist_ok=True)
    r = Runner(a)
    started = time.time()
    content = r.content()
    print(f"content: {len(content['cards'])} cards, {len(content['relics'])} relics, {len(content['potions'])} potions, "
          f"{sum(len(x['events']) for x in content['acts']) + len(content['shared_events'])} events, "
          f"{sum(len(x['encounters']) for x in content['acts'])} encounters, {len(content['acts'])} acts", flush=True)
    for s in a.suite.split(","):
        print(f"== {s}", flush=True)
        getattr(r, "suite_" + s)(content)
    by = {}
    for c in r.cases:
        by.setdefault(c.suite, {}).setdefault(c.status, 0)
        by[c.suite][c.status] += 1
    report = {"seconds": round(time.time() - started), "summary": by, "cases": [c.as_dict() for c in r.cases],
              "cues": r.cues}
    with open(os.path.join(a.work, "report.json"), "w", encoding="utf-8") as fh:
        json.dump(report, fh, indent=1)
    print(json.dumps(by, indent=1))
    bad = sum(n for s in by.values() for k, n in s.items() if k != "ok")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
