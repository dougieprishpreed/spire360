#!/usr/bin/env python3
"""The presentation audit: everything the game's code asks of its UI layer,
against what the headless logic records and what the client plays, as one
checklist in which every item needs a verdict.

    dotnet out/logic/apidump/Spire.ApiDump.dll --presentation logic/lib/game/sts2.dll work/presentation.json
    dotnet out/logic/apidump/Spire.ApiDump.dll --ui-surface logic/lib/game/sts2.dll work/ui_surface.json
    python tools/content_coverage.py ... --work work/cov          (ledgers: <case>.ledger.jsonl)
    out/engine/spire_gaps --dt 0.1 PACK work/cov/gaps.json work/cov/*/*.jsonl
    python tools/presentation_audit.py --runs work/cov --gaps work/cov/gaps.json [--list open] [--out work/audit.md]

Sources:
  static   work/presentation.json (per game method: UI calls, Godot calls,
           TestMode reads, delegates to UI methods, asset literals, null
           guards on UI values) and work/ui_surface.json (game events UI
           classes subscribe to)
  ledgers  the logic's SPIRE_LEDGER files (content_coverage writes one per
           case): the UI calls made, the stand-ins' answers, stripped UI
           bodies that ran, exceptions
  gaps     spire_gaps reports: each cue kind the responses brought, how
           often the client played it and how often it dropped it

Categories (an item's key is what the verdicts file matches):
  sound     a sound a UI class plays            "<Class>.<Method> <sound>"
  scene     a scene a UI class instantiates      "<Class> <scene>"
  call      a UI method game code calls          "<Class>.<Method>"
  answer    a UI value game code tests for null that the stand-ins answer
            null at run time: what it guards is skipped  "<Class>.<Member>"
  event     a game event a UI class handles      "<Event> -> <Class>.<Handler>"
  testmode  a game method reading TestMode       "<Class>.<Method>"
  godot     a game method driving Godot objects  "<Class>.<Method>"
  delegate  a UI method game code binds as a delegate  "<Class>.<Method> -> <UI method>"
  headless  a stripped UI body that ran          "<Class>.<Method>"
  exception an exception game code threw         "<Type> at <Class>.<Method>"
  cue       a cue kind the client never plays    "<kind>"
  script    a scene script the replays met that has no port  "<Script>.cs"
  asset     the pack's scenes the replays never loaded, by directory  "scenes/<dir>/<dir>"
  content   a content model whose code calls the UI, never exercised by the
            runs                                 "<Class>"

Verdicts: notes/presentation_verdicts.tsv, one per line,
"category<TAB>key pattern<TAB>status<TAB>note" (fnmatch patterns; the first
match wins). Status: ported (where), n/a (why), open (to do). Items the
audit can settle itself are "auto" (a sound or scene the client names, a
call the client played). The rest without a verdict are "unclassified":
the audit is done when none are, and none are open.
"""
import argparse
import collections
import fnmatch
import glob
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CATEGORIES = ("sound", "scene", "call", "answer", "event", "testmode", "godot", "delegate", "headless", "exception", "skip",
              "cue", "script", "asset", "content")
# Not part of a console single-player game: the developer console and its
# autoplayer, multiplayer, platform services, debug and test helpers, mods.
IGNORED = re.compile(r"DevConsole|AutoSlay|Multiplayer|\.Debug\b|Debug\.|\.Debug\.|TestSupport|Modding|MemoryProfiler|"
                     r"Achievements|Platform\.|Sentry|Feedback|NDebug|Steam|Leaderboard|Telemetry|Analytics|"
                     r"\.Networking\.|Lobby|NetGame|NetService|Peer|ReleaseInfo|Benchmark")
QUERY = re.compile(r"^(get_|set_|Is[A-Z]|Has[A-Z]|Find|Get[A-Z]|Try[A-Z]|Can[A-Z]|Should|Format|op_|add_|remove_|\.ctor$|\.cctor$)")
CONTENT_BASES = ("CardModel", "RelicModel", "PotionModel", "PowerModel", "MonsterModel", "EventModel", "AncientEventModel",
                 "EnchantmentModel", "AfflictionModel", "OrbModel", "EncounterModel", "ModifierModel", "ActModel",
                 "CharacterModel")


def short(name):
    """"MegaCrit.Sts2.Core.Nodes.Combat.NCreature::OnPowerIncreased" -> "NCreature.OnPowerIncreased"."""
    if "::" in name:
        t, m = name.split("::", 1)
        return t.split(".")[-1].split("/")[-1] + "." + m
    return name.split(".")[-1].split("/")[-1]


NAMED = {}


def engine_text():
    """The client's and the adapter's sources; NAMED: where each "Class.Member"
    they name first appears (the ports cite the game method they follow)."""
    parts = []
    for d in ("engine", "tools/viewer", "logic/Spire.Logic"):
        for dp, _, fs in os.walk(os.path.join(ROOT, d)):
            if "bin" in dp.split(os.sep) or "obj" in dp.split(os.sep):
                continue
            for f in sorted(fs):
                if f.endswith((".cpp", ".h", ".cs", ".inc")):
                    path = os.path.join(dp, f)
                    with open(path, encoding="utf-8", errors="replace") as fh:
                        text = fh.read()
                    parts.append(text)
                    rel = os.path.relpath(path, ROOT).replace(os.sep, "/")
                    for n, line in enumerate(text.split("\n"), 1):
                        for m in re.finditer(r"\b([A-Z]\w+)\.(_?[A-Za-z]\w*)", line):
                            NAMED.setdefault(m.group(0), f"{rel}:{n}")
    return "\n".join(parts)


def named(key):
    """Where the sources name a "Class.Method" key (or its handler part)."""
    k = key.split(" -> ")[-1].split(" ")[0]
    return NAMED.get(k) or PATCHED.get(k) or PATCHED.get(key.split(" -> ")[0].split(" ")[0])


PATCHED = {}
TESTMODE_PATCHED = {}
HOOKED = {}  # methods a hook runs before / after / instead of


def load_patches():
    """patches.json: the methods its entries patch or scope ("Class.Method" ->
    entry id); the TestMode replacements' methods separately."""
    with open(os.path.join(ROOT, "logic", "patches.json"), encoding="utf-8") as fh:
        doc = json.load(fh)
    entries = doc if isinstance(doc, list) else doc.get("patches", [])
    for e in entries:
        if not isinstance(e, dict):
            continue
        pid = e.get("id", "?")
        if e.get("type") and e.get("method"):
            PATCHED.setdefault(short(e["type"] + "::" + e["method"]), "patches.json " + pid)
            if e.get("action") in ("call_before", "call_after", "redirect"):
                HOOKED.setdefault(short(e["type"] + "::" + e["method"]), pid)
        for scope in e.get("in_types") or []:
            if "::" in scope:
                PATCHED.setdefault(short(scope), "patches.json " + pid)
                if e.get("type", "").endswith("TestSupport.TestMode"):
                    TESTMODE_PATCHED[short(scope)] = pid


class Item:
    def __init__(self, cat, key, detail="", auto=None):
        self.cat, self.key, self.detail, self.auto = cat, key, detail, auto
        self.status, self.note = None, ""
        self.where = named(key) if cat in ("call", "event", "testmode", "godot", "delegate", "answer", "headless") else None

    def settle(self, verdicts):
        for cat, pat, status, note in verdicts:
            if cat == self.cat and fnmatch.fnmatchcase(self.key, pat):
                self.status, self.note = status, note
                return
        if self.auto:
            self.status, self.note = "auto", self.auto


def load_verdicts(path):
    out = []
    if not os.path.exists(path):
        return out
    with open(path, encoding="utf-8") as fh:
        for n, line in enumerate(fh, 1):
            line = line.rstrip("\n")
            if not line.strip() or line.startswith("#"):
                continue
            parts = line.split("\t")
            # the static audits' rows (tools/hook_caller_audit.py, ui_reach_audit.py)
            if parts[0] in ("hook", "reach"):
                continue
            if len(parts) < 3 or parts[0] not in CATEGORIES or parts[2] not in ("ported", "n/a", "open"):
                sys.exit(f"{path}:{n}: expected category<TAB>pattern<TAB>ported|n/a|open<TAB>note: {line!r}")
            out.append((parts[0], parts[1], parts[2], parts[3] if len(parts) > 3 else ""))
    return out


def load_ledgers(dirs):
    """(facts by kind, the cases each fact came from)."""
    facts = collections.defaultdict(dict)
    cases = collections.defaultdict(set)
    for d in dirs:
        for path in glob.glob(os.path.join(d, "**", "*.ledger.jsonl"), recursive=True):
            case = os.path.relpath(path, d)[: -len(".ledger.jsonl")]
            with open(path, encoding="utf-8", errors="replace") as fh:
                for line in fh:
                    try:
                        f = json.loads(line)
                    except ValueError:
                        continue
                    k = f.get("k")
                    if k == "ui":
                        key = (f["call"], f.get("caller") or "?", f.get("answer") or "?")
                    elif k in ("headless", "cue"):
                        key = (f["call"], f.get("caller") or "?")
                    elif k == "exception":
                        key = (f.get("type"), f.get("where") or "?")
                    elif k == "skip":
                        key = (f.get("what") or "?", f.get("detail") or "")
                    else:
                        continue
                    facts[k].setdefault(key, f)
                    cases[(k,) + key].add(case)
    return facts, cases


def load_gaps(paths):
    """(cue kinds with their counts, scripts without a port with theirs, the
    pack's scenes, the scenes the replays loaded)."""
    cues, scripts = {}, {}
    pack, loaded = set(), set()
    for p in paths:
        with open(p, encoding="utf-8") as fh:
            d = json.load(fh)
        for c in d.get("cues", []):
            e = cues.setdefault(c["kind"], {"seen": 0, "used": 0, "dropped": 0, "first": c.get("first", "")})
            for f in ("seen", "used", "dropped"):
                e[f] += c.get(f, 0)
        for g in d.get("gaps", []):
            if g["what"].startswith("script without a port: "):
                s = g["what"][len("script without a port: "):]
                e = scripts.setdefault(s, {"count": 0, "first": g.get("first", "")})
                e["count"] += g.get("count", 0)
        pack.update(d.get("pack_scenes", []))
        loaded.update(d.get("scenes_loaded", []))
    return cues, scripts, pack, loaded


def ui_closure(methods, start, limit=3000):
    seen, stack = {start}, [start]
    while stack and len(seen) < limit:
        m = stack.pop()
        for c in methods.get(m, {}).get("calls", []):
            if c not in seen and methods.get(c, {}).get("ui_type"):
                seen.add(c)
                stack.append(c)
    return seen


def effects(methods, members):
    snd, scn = set(), set()
    for m in members:
        for a in methods.get(m, {}).get("assets", []):
            if a.startswith(("event:/", "snapshot:/")) or a.endswith((".mp3", ".ogg", ".wav")):
                snd.add(a)
            elif a.endswith(".tscn") or a.startswith(("res://scenes", "vfx/", "scenes/")):
                scn.add(a)
    return snd, scn


def scene_name(a):
    a = re.sub(r"^res://", "", a)
    a = re.sub(r"^scenes/", "", a)
    return re.sub(r"\.tscn$", "", a)


def build(a):
    with open(a.static, encoding="utf-8") as fh:
        static = json.load(fh)
    with open(a.surface, encoding="utf-8") as fh:
        surface = json.load(fh)
    methods, types = static["methods"], static["types"]
    load_patches()
    eng = engine_text()
    facts, cases = load_ledgers(a.runs)
    cues, scripts, pack_scenes, loaded_scenes = load_gaps(a.gaps)
    items = []

    def kept(name):
        return not IGNORED.search(name)

    # sound / scene: what UI classes play themselves
    sounds, scenes = collections.defaultdict(set), collections.defaultdict(set)
    for k, v in methods.items():
        if not v.get("ui_type") or not kept(k):
            continue
        for s in v.get("assets", []):
            if s.startswith(("event:/", "snapshot:/")) or s.endswith((".mp3", ".ogg", ".wav")):
                sounds[short(k) + " " + s].add(s)
            elif s.endswith(".tscn") or s.startswith(("res://scenes", "vfx/", "scenes/")) or \
                    (s.startswith("ui/") and "." not in s.rsplit("/", 1)[-1]):
                scenes[short(k).split(".")[0] + " " + s].add(s)
    for key, ss in sorted(sounds.items()):
        s = next(iter(ss))
        items.append(Item("sound", key, auto="the client names it" if s in eng else None))
    for key, ss in sorted(scenes.items()):
        n = scene_name(next(iter(ss)))
        items.append(Item("scene", key, auto="the client names it" if len(n) > 3 and n in eng else None))

    # call: UI methods game code calls, against the ledgers and the client's cue counts
    called = collections.Counter()
    for (call, caller, answer) in facts["ui"]:
        called[call] += len(cases[("ui", call, caller, answer)])
    for callee, callers in sorted(surface["ui_calls"].items()):
        callers = [c for c in callers if kept(c)]
        if not callers or not kept(callee):
            continue
        meth = callee.rsplit("::", 1)[1]
        if QUERY.match(meth):
            continue
        key = short(callee)
        cue = cues.get("ui:" + key)
        runs = called.get(callee, 0)
        detail = f"from {', '.join(sorted(set(short(c) for c in callers))[:6])}; runs: {runs} cases"
        auto = None
        if cue:
            detail += f"; client: seen {cue['seen']}, played {cue['used']}, dropped {cue['dropped']}"
            if cue["used"] > 0:
                auto = "the client plays it"
        items.append(Item("call", key, detail, auto))

    # answer: null-tested UI values the stand-ins answer null
    nulls = collections.defaultdict(set)
    for (call, caller, answer) in facts["ui"]:
        if answer == "null":
            nulls[call].add(caller)
    guarded = collections.defaultdict(set)
    for k, v in methods.items():
        if v.get("ui_type") or not kept(k):
            continue
        for g in v.get("guards", []):
            guarded[g].add(k)
    # caller -> the UI calls the ledgers saw it make ("Class.Method"), through
    # the redirect or as a cue a hook recorded
    recorded = collections.defaultdict(set)
    for (call, caller, answer) in facts["ui"]:
        recorded[caller].add(short(call))
    for (call, caller) in facts["cue"]:
        recorded[caller].add(call)
    for g, where in sorted(guarded.items()):
        if g not in nulls:
            continue
        # the guard sites that ran and got null (the ledger's caller is the
        # method making the call, which is where its result is tested), and
        # the UI calls each has that were never recorded from it: what the
        # null skipped (a ?. statement on a singleton gets a stand-in
        # receiver, and its calls are recorded)
        hit = sorted(where & nulls[g])
        if not hit:
            continue
        # one item per guard site that skipped UI calls ("Value@Site": each
        # gets its own verdict), one for the rest of the value's sites
        quiet = []
        for m in hit:
            calls = {short(x) for x in methods.get(m, {}).get("ui", []) if not QUERY.match(x.rsplit("::", 1)[1])}
            missing = sorted(calls - recorded.get(m, set()))
            if not missing:
                quiet.append(m)
                continue
            detail = f"null there; never recorded from it: {', '.join(missing[:8])}"
            if PATCHED.get(short(m)):
                detail += f"; the site is patched ({PATCHED[short(m)]})"
            ev = [f"{c} ({named(c)})" for c in missing if named(c)]
            if ev:
                detail += "; named: " + "; ".join(ev[:4])
            items.append(Item("answer", f"{short(g)}@{short(m)}", detail))
        if quiet:
            items.append(Item("answer", short(g),
                              f"null at {len(quiet)} guards: {', '.join(short(x) for x in quiet[:8])}",
                              auto="every UI call behind the guards was recorded"))

    # event: UI handlers of game events, with what they play
    for ev, subs in sorted(surface["events"].items()):
        for s in subs:
            sub, handler = s.split(" -> ")
            if not kept(sub) or not kept(handler):
                continue
            snd, scn = effects(methods, ui_closure(methods, handler))
            missing = sorted(x for x in snd if x not in eng)
            detail = (f"sounds: {', '.join(sorted(snd))}" if snd else "") + \
                     (f"; not named by the client: {', '.join(missing)}" if missing else "")
            items.append(Item("event", short(ev) + " -> " + short(handler), detail.strip("; ")))

    # testmode / godot / delegate: game methods (static)
    for k, v in sorted(methods.items()):
        if v.get("ui_type") or not kept(k):
            continue
        if v.get("test"):
            ui = sorted(short(x) for x in v.get("ui", []) if not QUERY.match(x.rsplit("::", 1)[1]))
            pid = TESTMODE_PATCHED.get(short(k))
            hook = HOOKED.get(short(k))
            items.append(Item("testmode", short(k), "UI calls: " + ", ".join(ui[:8]) if ui else "",
                              auto=f"read as off or on by patches.json {pid}" if pid else
                              f"its presentation recorded by the hook patches.json {hook}" if hook else None))
        g = [x for x in v.get("godot", []) if not re.search(r"::(get_|op_|\.ctor)", x)]
        if g:
            items.append(Item("godot", short(k), ", ".join(sorted(set(short(x) for x in g))[:8])))
        for f in v.get("ftn", []):
            items.append(Item("delegate", short(k) + " -> " + short(f)))

    # headless / exception (ledgers)
    # skip: what the adapter passed over (the client shows nothing for it)
    for (what, detail), f in sorted(facts["skip"].items()):
        n = len(cases[("skip", what, detail)])
        items.append(Item("skip", f"{what}: {detail}", f"{n} cases, first {sorted(cases[('skip', what, detail)])[0]}"))

    for (call, caller), f in sorted(facts["headless"].items()):
        n = len(cases[("headless", call, caller)])
        items.append(Item("headless", short(call.replace("::", "::", 1)), f"from {short(caller)}; {n} cases"))
    for (typ, where), f in sorted(facts["exception"].items(), key=lambda x: (str(x[0][0]), str(x[0][1]))):
        if not kept(where or ""):
            continue
        n = len(cases[("exception", typ, where)])
        items.append(Item("exception", f"{typ} at {short(where)}", f"{f.get('message', '')[:120]}; thrown at "
                          f"{short(f.get('site', '?'))}; {n} cases"))

    # cue: kinds the client never plays
    for kind, c in sorted(cues.items()):
        if c["used"] == 0:
            items.append(Item("cue", kind, f"seen {c['seen']}, dropped {c['dropped']}, first {c['first']}"))

    # script: scene scripts the replays met with no port (a screen may fill
    # the node itself)
    for s, e in sorted(scripts.items()):
        items.append(Item("script", s.split("/")[-1], f"{s}; {e['count']} nodes, first {e['first']}"))

    # asset: the pack's scenes the replays never loaded, by directory
    if pack_scenes:
        by_dir = collections.defaultdict(list)
        for s in sorted(pack_scenes - loaded_scenes):
            parts = s.split("/")
            by_dir["/".join(parts[:3]) if len(parts) > 3 else "/".join(parts[:2])].append(parts[-1])
        for d, names in sorted(by_dir.items()):
            items.append(Item("asset", d, f"{len(names)} never loaded: {', '.join(names[:12])}" +
                              (" ..." if len(names) > 12 else "")))

    # content: models whose code calls the UI, never seen running in the ledgers
    def is_content(t):
        seen = set()
        while t and t not in seen:
            seen.add(t)
            info = types.get(t)
            if not info:
                return False
            base = info.get("base") or ""
            if base.split(".")[-1] in CONTENT_BASES:
                return True
            t = base
        return False

    ran = set()
    for kind in ("ui", "cue"):
        for key, f in facts[kind].items():
            for w in (f.get("model"), f.get("caller")):
                if w:
                    ran.add(w.split("::")[0])
    obligations = collections.defaultdict(set)
    for k, v in methods.items():
        t = k.split("::")[0]
        if v.get("ui_type") or not kept(k) or not is_content(t):
            continue
        for x in v.get("ui", []):
            if not QUERY.match(x.rsplit("::", 1)[1]):
                obligations[t].add(short(x))
    for t, obl in sorted(obligations.items()):
        if t in ran or types.get(t, {}).get("abstract"):
            continue
        items.append(Item("content", short(t), "UI calls: " + ", ".join(sorted(obl)[:8])))
    return items


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--static", default=os.path.join(ROOT, "work", "presentation.json"))
    ap.add_argument("--surface", default=os.path.join(ROOT, "work", "ui_surface.json"))
    ap.add_argument("--runs", action="append", default=[], help="a content_coverage work dir (its ledgers)")
    ap.add_argument("--gaps", action="append", default=[], help="a spire_gaps report (its cues)")
    ap.add_argument("--verdicts", default=os.path.join(ROOT, "notes", "presentation_verdicts.tsv"))
    ap.add_argument("--list", default="", help="comma-separated statuses to print: unclassified, open, ported, n/a, auto")
    ap.add_argument("--category", default="", help="only these categories (comma-separated)")
    ap.add_argument("--out", default="", help="write the whole checklist as Markdown")
    a = ap.parse_args()
    verdicts = load_verdicts(a.verdicts)
    items = build(a)
    only = set(filter(None, a.category.split(",")))
    if only:
        items = [i for i in items if i.cat in only]
    for i in items:
        i.settle(verdicts)
    table = collections.defaultdict(collections.Counter)
    for i in items:
        table[i.cat][i.status or "unclassified"] += 1
    cols = ("auto", "ported", "n/a", "open", "unclassified")
    print(f"{'category':10}" + "".join(f"{c:>14}" for c in cols))
    for cat in CATEGORIES:
        if cat in table:
            print(f"{cat:10}" + "".join(f"{table[cat][c]:>14}" for c in cols))
    want = set(filter(None, a.list.split(",")))
    for i in items:
        st = i.status or "unclassified"
        if st in want:
            print(f"[{st}] {i.cat}\t{i.key}" + (f"\t({i.detail})" if i.detail else "") +
                  (f"\t[named: {i.where}]" if i.where else "") + (f"\t-- {i.note}" if i.note else ""))
    if a.out:
        with open(a.out, "w", encoding="utf-8") as fh:
            fh.write("# Presentation audit\n\n")
            for cat in CATEGORIES:
                rows = [i for i in items if i.cat == cat]
                if not rows:
                    continue
                fh.write(f"\n## {cat}\n\n| status | item | detail | note |\n| --- | --- | --- | --- |\n")
                for i in rows:
                    cell = lambda s: s.replace("|", "\\|")
                    fh.write(f"| {i.status or 'unclassified'} | {cell(i.key)} | {cell(i.detail)} | {cell(i.note)} |\n")
    bad = sum(table[c]["unclassified"] + table[c]["open"] for c in table)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
