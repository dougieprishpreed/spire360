"""Content coverage: every card, relic, potion, event, encounter and act of
the game played through the real logic, with everything unlocked.

    python tools/content_coverage.py --cmd "dotnet out/logic/jit/Spire.Logic.dll" \
        --data work/pack/data --work work/coverage [--suite cards,relics,...] [--limit N]

Suites (default: all):
  cards       each card generated into the hand and played (and upgraded)
  potions     each potion obtained and used in a fight
  relics      each relic obtained (RelicCmd.Obtain), then a fight, a rest
              site, a shop and a treasure room with it
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
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import random_runs  # noqa: E402


def act_for(rng, state):
    """random_runs' agent, with everything a coverage run needs to keep going."""
    return random_runs.act(rng, state)


LEDGER = True


class Logic:
    """One logic process; restarted when it dies."""

    def __init__(self, cmd, data, trace):
        self.cmd, self.data, self.trace = cmd, data, trace
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
        return self.s.send(cmd)

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
            state = self.step(case, logic, act_for(self.rng, state))
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
        return items[: self.a.limit] if self.a.limit else items

    # ---- suites ----

    def suite_cards(self, content):
        """Each card into the hand of a fight against several enemies, played
        (its target the first enemy), its prompts answered; then upgraded."""
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
                case = Case("cards", name)
                lg = self.logic("cards", name)
                try:
                    self.new_run(case, lg, character=ch, seed=f"cards-{tag}")
                    state = self.step(case, lg, {"cmd": "enter_room", "type": "elite", "encounter": self.a.fight})
                    for card in chunk:
                        if card.get("upgradable") is False and upgraded:
                            continue
                        if state.get("decision") != "combat_play":
                            # The fight is over: through its rewards to the map, then a new one.
                            state = self.play_until(case, lg, state, lambda s: s.get("decision") == "map_select", 60)
                            state = self.step(case, lg, {"cmd": "enter_room", "type": "elite", "encounter": self.a.fight})
                            self.step(case, lg, {"cmd": "set_player", "hp": 9999})
                        if len(state.get("hand") or []) >= 9:
                            # A full hand takes no more cards: a new turn discards it.
                            state = self.step(case, lg, {"cmd": "action", "action": "end_turn", "args": {}})
                            state = self.play_until(case, lg, state, lambda s: s.get("decision") == "combat_play", 60)
                            if state.get("decision") != "combat_play":
                                state = self.play_until(case, lg, state, lambda s: s.get("decision") == "map_select", 60)
                                state = self.step(case, lg, {"cmd": "enter_room", "type": "elite",
                                                             "encounter": self.a.fight})
                        state = self.step(case, lg, {"cmd": "add_to_hand", "cards": [card["id"]], "energy": 9,
                                                     "stars": 9, "upgraded": upgraded})
                        hand = state.get("hand") or []
                        mine = [h for h in hand if (h.get("id") or "").split(".")[-1] == card["id"]]
                        if mine and not mine[-1].get("can_play") and "BlockedByCardLogic" in (mine[-1].get("unplayable_reason") or ""):
                            # its own condition (an empty draw pile, a hand of
                            # attacks): the card alone in hand, nothing to draw
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
                            continue
                        args = {"card_index": h["index"]}
                        if h.get("target_type") == "AnyEnemy" and state.get("enemies"):
                            args["target_index"] = 0
                        state = self.step(case, lg, {"cmd": "action", "action": "play_card", "args": args})
                        # Its prompts (card selections, choices) until the hand is back.
                        state = self.play_until(case, lg, state, lambda s: s.get("decision") in ("combat_play", "rewards",
                                                                                             "map_select"), 40)
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
        for r in self.limited(content["relics"]):
            case = Case("relics", r["id"])
            lg = self.logic("relics", r["id"])
            try:
                self.new_run(case, lg, seed=f"relic-{r['id']}")
                state = self.step(case, lg, {"cmd": "obtain", "relics": [r["id"]]})
                if state.get("not_allowed"):
                    case.notes.append("multiplayer only: not in solo runs")
                    continue
                state = self.play_until(case, lg, state, lambda s: s.get("decision") == "map_select", 30)
                for room in ({"type": "monster", "encounter": self.a.fight}, {"type": "rest_site"}, {"type": "shop"},
                             {"type": "treasure"}):
                    state = self.step(case, lg, dict({"cmd": "enter_room"}, **room))
                    if state.get("decision") == "rest_site":
                        # the relic's own rest option (Dig, Lift, Kindle, Clone,
                        # Hatch, ...) rather than whatever the agent picks
                        own = [o for o in state.get("options") or []
                               if o.get("option_id") not in ("HEAL", "SMITH") and not o.get("is_locked")]
                        if own:
                            state = self.step(case, lg, {"cmd": "action", "action": "choose_option",
                                                         "args": {"option_index": own[0]["index"]}})
                    state = self.play_until(case, lg, state, lambda s: s.get("decision") == "map_select", 300)
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
            todo.append((enc, room, 0))
        met = collections.defaultdict(set)
        while todo:
            enc, room, attempt = todo.pop(0)
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
                state = self.play_until(case, lg, state, done, 150)
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
                if missing and attempt < 7:
                    todo.append((enc, room, attempt + 1))
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


SUITES = ["cards", "potions", "relics", "events", "encounters", "scenarios", "acts"]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--cmd", required=True)
    ap.add_argument("--data", required=True)
    ap.add_argument("--work", required=True)
    ap.add_argument("--suite", default=",".join(SUITES))
    ap.add_argument("--limit", type=int, default=0, help="first N items of each suite (0: all)")
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
