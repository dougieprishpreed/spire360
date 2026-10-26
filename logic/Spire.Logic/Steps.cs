using System.Reflection;
using System.Runtime.CompilerServices;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Entities.Cards;
using MegaCrit.Sts2.Core.Entities.Creatures;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.Models;

namespace Spire.Logic;

/// <summary>
/// Records what changed between consecutive moments of a combat, so the
/// presentation layer can animate the real game's resolution order (each hit,
/// each card moving, each power ticking) instead of jumping to the end state.
///
/// After every queued continuation (Sched.AfterCallback) a compact snapshot
/// of the visible combat state is taken and compared with the previous one.
/// A non-empty difference becomes a "step". Steps are attached to the next
/// protocol response as "steps": [[change, ...], ...].
///
/// This needs no knowledge of the game's action classes: it only reads the
/// same public state the decision export reads. Several game effects that
/// resolve inside one continuation merge into one step.
/// </summary>
internal static class Steps
{
    private static Snapshot? _last;
    private static long _lastPrint;
    private static readonly List<List<Dictionary<string, object?>>> _steps = new();
    private static readonly ConditionalWeakTable<object, StrongBox<int>> _ids = new();
    private static int _nextId = 1;
    private static Func<Player?>? _player;
    private static PropertyInfo? _exhaustProp;
    private static bool _exhaustLooked;

    public static bool Enabled = Environment.GetEnvironmentVariable("SPIRE_STEPS") != "0";
    public const int MaxStepsPerResponse = 2000;

    /// <summary>Stable small integer per game object (card or creature instance).</summary>
    public static int IdOf(object o) => _ids.GetValue(o, _ => new StrongBox<int>(_nextId++)).Value!;

    public static void Attach(Func<Player?> player)
    {
        _player = player;
        Sched.AfterCallback = Capture;
    }

    private static readonly List<Dictionary<string, object?>> _pendingUi = new();
    public static bool RecordUi = Environment.GetEnvironmentVariable("SPIRE_UI_EVENTS") != "0";

    /// <summary>
    /// A call the game logic made into its UI layer ("ui|Namespace.Type::Method").
    /// Property reads and singleton lookups are not cues and are skipped.
    /// </summary>
    /// <summary>While set, UI calls go here instead of the steps (SpineRecorder
    /// reads what a monster's SetupSkins does to its spine).</summary>
    public static Action<string, List<object?>>? Intercept;

    public static void Ui(string id, object?[] args)
    {
        if (Intercept == null && (!Enabled || !RecordUi)) return;
        var sep = id.LastIndexOf("::", StringComparison.Ordinal);
        if (sep < 0) return;
        var method = id[(sep + 2)..];
        var generic = method.IndexOf('<');
        if (generic >= 0) method = method[..generic];
        if (method.StartsWith("get_", StringComparison.Ordinal) || method.StartsWith("set_", StringComparison.Ordinal)
            || method.StartsWith("add_", StringComparison.Ordinal) || method.StartsWith("remove_", StringComparison.Ordinal))
            return;
        var typeFull = id[(id.IndexOf('|') + 1)..sep];
        var type = typeFull[(typeFull.LastIndexOf('.') + 1)..];
        var outArgs = new List<object?>();
        // args[0] is the receiver for instance calls; it is null here (no UI), skip it.
        foreach (var a in args)
        {
            if (ReferenceEquals(a, Spire.HookBridge.Hooks.StandIn)) continue; // a singleton's stand-in receiver
            if (StandIns.TrySpecial(a, out var special))
            {
                outArgs.Add(special); // a creature's special node: "<who>#<path>"
                continue;
            }
            if (StandIns.TryCreature(a, out var standing))
            {
                outArgs.Add(Who(standing)); // a creature's stand-in node (the receiver of its calls)
                continue;
            }
            switch (a)
            {
                case null: continue;
                case Godot.Vector2 v:
                    outArgs.Add(StandIns.Describe(v) ?? "Vector2");
                    break;
                case Godot.Resource res when !string.IsNullOrEmpty(res.ResourcePath):
                    // A texture the stub loader handed out: its pack name.
                    outArgs.Add(res.ResourcePath.StartsWith("res://", StringComparison.Ordinal) ? res.ResourcePath[6..] : res.ResourcePath);
                    break;
                case string str: outArgs.Add(str); break;
                case bool or int or long or float or double or decimal: outArgs.Add(a); break;
                case IReadOnlyDictionary<string, float> parameters:
                    outArgs.Add(parameters.ToDictionary(p => p.Key, p => (object?)p.Value));
                    break;
                case Enum e: outArgs.Add(e.ToString()); break;
                case MegaCrit.Sts2.Core.Models.AbstractModel m: outArgs.Add(m.Id.ToString()); break;
                case Creature c: outArgs.Add(Who(c)); break;
                case IEnumerable<Creature> cs: outArgs.Add(cs.Select(x => (object?)Who(x)).ToList()); break;
                case MegaCrit.Sts2.Core.Entities.Creatures.DamageResult dr:
                    // NDamageNumVfx.Create(target, result): the number it shows.
                    outArgs.Add(dr.UnblockedDamage + dr.OverkillDamage);
                    break;
                case System.Collections.IEnumerable nodes when HasCreatureNodes(nodes):
                    // a list of creature nodes (NMonsterDeathVfx.Create(creatureNodes)): who they are
                    var whos = new List<object?>();
                    foreach (var x in nodes)
                        if (StandIns.TryCreature(x, out var cx)) whos.Add(Who(cx));
                    outArgs.Add(whos);
                    break;
                default:
                    var tn = a.GetType().Name;
                    if (tn == "Color" && a.GetType().Namespace == "Godot")
                    {
                        // PlaySplashVfx(target, color): the colour as #rrggbbaa.
                        float F(string f) => Convert.ToSingle(a.GetType().GetField(f)?.GetValue(a) ?? 1f);
                        int B(float v) => (int)Math.Round(Math.Clamp(v, 0f, 1f) * 255f);
                        outArgs.Add($"#{B(F("R")):x2}{B(F("G")):x2}{B(F("B")):x2}{B(F("A")):x2}");
                        break;
                    }
                    if (tn.StartsWith("N", StringComparison.Ordinal)) continue; // UI node receivers
                    outArgs.Add(tn);
                    break;
            }
        }
        if (Intercept != null)
        {
            Intercept(type + "." + method, outArgs);
            return;
        }
        FlushChanges();
        Add(new Dictionary<string, object?> { ["t"] = "ui", ["call"] = type + "." + method, ["args"] = outArgs.Count > 0 ? outArgs : null });
    }

    /// <summary>The death's animation (the animator's ops for the Dead
    /// trigger) on the death change recorded for that creature, or as an
    /// animation cue of its own.</summary>
    public static void AttachDeathAnimation(string who, List<Dictionary<string, object?>> ops)
    {
        if (!Enabled || !RecordUi) return;
        for (int i = _pendingUi.Count - 1; i >= 0; i--)
            if (_pendingUi[i]["t"] as string == "death" && _pendingUi[i]["who"] as string == who)
            {
                _pendingUi[i]["play"] = ops;
                return;
            }
        Cue(new Dictionary<string, object?> { ["t"] = "anim", ["who"] = who, ["name"] = "Dead", ["wait"] = 0, ["play"] = ops });
    }

    /// <summary>One more argument for the UI call just recorded (what the
    /// game sets on the node it made: Fiend Fire's fire's scale).</summary>
    public static void AppendToLastUi(object? value)
    {
        if (_pendingUi.Count == 0 || _pendingUi[^1]["t"] as string != "ui") return;
        var cue = _pendingUi[^1];
        var args = cue.TryGetValue("args", out var v) && v is List<object?> l ? l : new List<object?>();
        args.Add(value);
        cue["args"] = args;
    }

    /// <summary>A key on the ui cue just recorded (what the client needs
    /// from the game's state at the moment of the call).</summary>
    /// <summary>A fight set up or under way (CombatManager.StartCombatInternal
    /// runs the creatures' AfterAddedToRoom before IsInProgress): when the
    /// game's UI shows what the fight does.</summary>
    public static bool InCombat => CombatManager.Instance.IsInProgress || CombatManager.Instance.IsStarting;

    public static void SetOnLastUi(string key, object? value)
    {
        if (_pendingUi.Count == 0 || _pendingUi[^1]["t"] as string != "ui") return;
        _pendingUi[^1][key] = value;
    }

    private static bool HasCreatureNodes(System.Collections.IEnumerable items)
    {
        foreach (var x in items)
            if (StandIns.TryCreature(x, out _)) return true;
        return false;
    }

    /// <summary>NCreature.StartDeathAnim(shouldRemove: false): the creature
    /// stays (AnimDie neither fades nor frees it: a Decimillipede segment
    /// that will reattach); its death change says so ("keep").</summary>
    public static void MarkDeathKept(string who)
    {
        if (!Enabled || !RecordUi) return;
        for (int i = _pendingUi.Count - 1; i >= 0; i--)
            if (_pendingUi[i]["t"] as string == "death" && _pendingUi[i]["who"] as string == who)
            {
                _pendingUi[i]["keep"] = true;
                return;
            }
    }

    // Presentation the game runs alongside the turn (TaskHelper.RunSafely of
    // a visual task: a relic's DoActivateVisuals, a card's OnEnqueuePlayVfx,
    // a reveal after a death animation). Headless it runs inline, so its
    // waits must not hold back what follows: they only delay its own cues
    // and changes ("delay", ms after the point where it started).
    private static readonly List<int> _branches = new();

    public static void BeginBranch()
    {
        FlushChanges();
        _branches.Add(_branches.Count > 0 ? _branches[^1] : 0);
    }

    public static void EndBranch()
    {
        FlushChanges();
        if (_branches.Count > 0) _branches.RemoveAt(_branches.Count - 1);
    }

    private static void Add(Dictionary<string, object?> cue)
    {
        if (_pendingUi.Count >= 500) return;
        if (_branches.Count > 0 && _branches[^1] > 0) cue["delay"] = _branches[^1];
        _pendingUi.Add(cue);
    }

    /// <summary>
    /// The state changes since the last snapshot, ahead of a UI cue: one
    /// continuation can run a whole phase (the end of a turn discards the
    /// hand, plays the enemies' turn, starts the next turn and draws), and
    /// the game shows each change where it happens among its cues. A cheap
    /// fingerprint decides whether a snapshot is worth taking.
    /// </summary>
    private static void FlushChanges()
    {
        if (!Enabled || _player == null) return;
        Player? player;
        try { player = _player(); }
        catch { return; }
        long print = Fingerprint(player);
        if (_last == null)
        {
            // A fight began: its state at the first cue (CombatManager's
            // music update, before the starting turn's draws, energy and
            // relics) is what the changes after it are measured from; the
            // whole start runs in one continuation.
            if (print == 0) return;
            try { _last = Take(player); }
            catch { return; }
            if (_last != null) _lastPrint = print;
            return;
        }
        if (print == _lastPrint) return;
        Snapshot? now;
        try { now = Take(player); }
        catch { return; }
        if (now == null) return;
        var diff = Diff(_last, now);
        foreach (var change in diff) Add(change);
        _last = now;
        _lastPrint = print;
    }

    /// <summary>Round, energy, stars, gold, the piles' sizes and each
    /// creature's HP, block and powers, folded together.</summary>
    private static long Fingerprint(Player? player)
    {
        if (player == null || !CombatManager.Instance.IsInProgress) return 0;
        var state = CombatManager.Instance.DebugOnlyGetState();
        if (state == null) return 0;
        long h = 17;
        void Mix(long v) => h = unchecked(h * 31 + v);
        Mix(state.RoundNumber);
        Mix(player.Gold);
        var pcs = player.PlayerCombatState;
        if (pcs != null)
        {
            Mix(Convert.ToInt32(pcs.Energy));
            Mix(Convert.ToInt32(pcs.Stars));
            Mix(pcs.Hand?.Cards?.Count ?? -1);
            Mix(pcs.DrawPile?.Cards?.Count ?? -1);
            Mix(pcs.DiscardPile?.Cards?.Count ?? -1);
        }
        void Creature(Creature? c)
        {
            if (c == null) return;
            Mix(c.CurrentHp);
            Mix(c.MaxHp);
            Mix(c.Block);
            Mix(c.IsAlive ? 1 : 0);
            try
            {
                if (c.Powers != null)
                    foreach (var p in c.Powers)
                        if (p != null) Mix(Convert.ToInt32(p.Amount) + 7);
            }
            catch { }
        }
        Creature(player.Creature);
        Creature(player.Osty);
        if (state.Enemies != null)
            foreach (var e in state.Enemies) Creature(e);
        return h;
    }

    /// <summary>
    /// How steps name a creature: "player", "osty", or "e" + its combat id
    /// (the decision's enemies[].combat_id).
    /// </summary>
    public static string Who(Creature c)
    {
        if (c.IsPlayer) return "player";
        var p = _player?.Invoke();
        if (p != null && ReferenceEquals(p.Osty, c)) return "osty";
        return c.CombatId.HasValue ? "e" + c.CombatId.Value : "e" + IdOf(c);
    }

    /// <summary>
    /// A presentation cue the logic's own commands make (VfxCmd, CreatureCmd
    /// animation triggers), recorded in order with the other cues.
    /// </summary>
    /// <summary>A cue without taking the state's changes first: for hooks
    /// inside a change the game is still making (a snapshot there reads a
    /// half-updated combat: Creature.RemovePowerInternal).</summary>
    public static void CueAsIs(Dictionary<string, object?> cue)
    {
        if (!Enabled || !RecordUi) return;
        if (Ledger.On && cue.TryGetValue("t", out var t) && t is string kind) Ledger.Cue("t:" + kind);
        Add(cue);
    }

    public static void Cue(Dictionary<string, object?> cue)
    {
        if (!Enabled || !RecordUi) return;
        if (Ledger.On && cue.TryGetValue("t", out var t) && t is string kind)
            Ledger.Cue(kind == "ui" && cue.TryGetValue("call", out var c) && c is string call ? call : "t:" + kind);
        FlushChanges();
        Add(cue);
    }

    public static Dictionary<string, object?> OrbInfo(OrbModel orb) => new()
    {
        ["id"] = orb.Id.Entry, ["instance_id"] = IdOf(orb),
        ["passive"] = (int)orb.PassiveVal, ["evoke"] = (int)orb.EvokeVal,
    };

    // The UI subscribes to OrbModel's activation events. Record the same
    // moment even when no Godot orb node exists in the headless logic.
    public static void OrbCue(OrbModel orb, bool evoke, Creature[]? targets)
    {
        if (!Enabled || !RecordUi) return;
        Capture(); // channel/capacity changes precede the activation
        var cue = OrbInfo(orb);
        cue["t"] = "orb_activate";
        cue["evoking"] = evoke;
        cue["targets"] = targets?.Select(Who).ToArray();
        Cue(cue);
    }

    /// <summary>A pacing delay the game asked for (Cmd.Wait / Task.Delay).</summary>
    /// <summary>The presentation's clock: every wait recorded so far, in ms
    /// (what Time.GetTicksMsec would have moved on in the game).</summary>
    public static long ClockMs;

    /// <summary>The merchant entries the player is buying (buy_card, ...):
    /// their purchase_completed cue is the client's own purchase, not the
    /// shop buying by itself (Lord's Parasol).</summary>
    public static readonly HashSet<object> PlayerPurchases = new();

    public static void Wait(int ms, string source, double? scale = null)
    {
        if (ms > 0 && _branches.Count > 0)
        {
            FlushChanges();
            _branches[^1] += ms;
            return;
        }
        if (ms > 0) ClockMs += ms;
        if (!Enabled || !RecordUi || ms <= 0) return;
        FlushChanges();
        var cue = new Dictionary<string, object?> { ["t"] = "wait", ["ms"] = ms, ["src"] = source };
        if (scale != null) cue["scale"] = scale;
        Add(cue);
    }

    /// <summary>Called after each queued continuation.</summary>
    public static void Capture()
    {
        if (!Enabled || _player == null) return;
        Snapshot? now;
        try { now = Take(_player()); }
        catch { return; }
        if (now == null)
        {
            // The fight just ended: the enemies alive at its last moment died
            // with it (the snapshot that would show it is gone).
            if (_last != null)
                foreach (var c in _last.Creatures.Values)
                    if (c.Alive && c.Who.StartsWith("e", StringComparison.Ordinal))
                        _pendingUi.Add(new Dictionary<string, object?> { ["t"] = "death", ["who"] = c.Who });
            // Outside combat the cues still matter (event sounds, map music).
            if (_pendingUi.Count > 0 && _steps.Count < MaxStepsPerResponse)
            {
                _steps.Add(new List<Dictionary<string, object?>>(_pendingUi));
                _pendingUi.Clear();
            }
            _last = null;
            _lastPrint = 0;
            return;
        }
        if (_last != null && _steps.Count < MaxStepsPerResponse)
        {
            var diff = Diff(_last, now);
            if (_pendingUi.Count > 0)
            {
                diff.InsertRange(0, _pendingUi);
                _pendingUi.Clear();
            }
            if (diff.Count > 0) _steps.Add(diff);
        }
        _last = now;
        _lastPrint = Fingerprint(_player());
    }

    /// <summary>Steps recorded since the previous call, or null if none.</summary>
    public static List<List<Dictionary<string, object?>>>? Drain()
    {
        Capture();
        if (_steps.Count == 0) return null;
        var copy = new List<List<Dictionary<string, object?>>>(_steps);
        _steps.Clear();
        AttachPassingViews(copy);
        _seenCards.Clear();
        _seenPowers.Clear();
        _seenCreatures.Clear();
        return copy;
    }

    // The cards the snapshots of this command's steps saw, by id.
    private static readonly Dictionary<int, CardModel> _seenCards = new();
    // The powers they saw, by creature and power id.
    private static readonly Dictionary<(string, string), PowerModel> _seenPowers = new();
    // The creatures they saw.
    private static readonly Dictionary<string, Creature> _seenCreatures = new();

    /// <summary>
    /// A card that comes into the hand and goes again within one command's
    /// steps (drawn and played by Hellraiser, drawn and exhausted) is not in
    /// the decision's hand, where the client finds the cards it shows coming
    /// in: its coming in gets the card's view.
    /// </summary>
    private static void AttachPassingViews(List<List<Dictionary<string, object?>>> steps)
    {
        var entering = new Dictionary<int, Dictionary<string, object?>>();
        // A power applied and gone again within them (not in the decision's
        // powers either, where the client finds a new power's look): its
        // coming gets the power's view.
        var applied = new Dictionary<(string, string), Dictionary<string, object?>>();
        // So is a creature that comes and dies within them (summoned and
        // killed: the decision's enemies are the living): its spawn gets the
        // enemy's view.
        var spawned = new Dictionary<string, Dictionary<string, object?>>();
        foreach (var step in steps)
            foreach (var c in step)
            {
                if (c.TryGetValue("t", out var ct) && ct is string kind && (kind == "spawn" || kind == "death" || kind == "despawn") &&
                    c["who"] is string cwho)
                {
                    if (kind == "spawn") spawned[cwho] = c;
                    else if (spawned.TryGetValue(cwho, out var came) && !came.ContainsKey("view") &&
                             _seenCreatures.TryGetValue(cwho, out var creature) && Hooks.Driver != null)
                        came["view"] = Hooks.Driver.EnemyView(creature, -1, null);
                    continue;
                }
                if (c.TryGetValue("t", out var pt) && (pt as string) == "power" && c["who"] is string who && c["power"] is string pid)
                {
                    if (c.ContainsKey("added")) applied[(who, pid)] = c;
                    else if (c.ContainsKey("removed") && applied.TryGetValue((who, pid), out var came) && !came.ContainsKey("view") &&
                             _seenPowers.TryGetValue((who, pid), out var power) && Hooks.Driver != null)
                        came["view"] = Hooks.Driver.PowerView(power);
                    continue;
                }
                if (c.TryGetValue("t", out var t) && (t as string) != "card") continue;
                if (c["card"] is not int id) continue;
                bool toHand = (c["to"] as string) == "hand", fromHand = (c["from"] as string) == "hand";
                if (toHand && !fromHand) entering[id] = c;
                else if (fromHand && entering.TryGetValue(id, out var came) && !came.ContainsKey("view") &&
                         _seenCards.TryGetValue(id, out var card) && Hooks.Driver != null)
                    came["view"] = Hooks.Driver.DeckCardView(card);
            }
    }

    // ---- snapshot ----

    private sealed class CreatureSnap
    {
        public string Who = "";
        public string? Monster;
        public int Hp, MaxHp, Block;
        public bool Alive;
        public Dictionary<string, int> Powers = new();
        public string Intent = "";
    }

    private sealed class Snapshot
    {
        public int Round, Energy, MaxEnergy, Stars, Gold;
        public readonly Dictionary<string, CreatureSnap> Creatures = new();
        public readonly Dictionary<int, (string Pile, int Index, string CardId)> Cards = new();
        public readonly List<int> Hand = new();
        public string Orbs = "";
        public int OrbSlots;
        public List<Dictionary<string, object?>> OrbState = new();
    }

    private static Snapshot? Take(Player? player)
    {
        if (player == null || !CombatManager.Instance.IsInProgress) return null;
        var state = CombatManager.Instance.DebugOnlyGetState();
        if (state == null) return null;
        var pcs = player.PlayerCombatState;
        var s = new Snapshot
        {
            Round = state.RoundNumber,
            Energy = pcs != null ? Convert.ToInt32(pcs.Energy) : 0,
            MaxEnergy = pcs != null ? Convert.ToInt32(pcs.MaxEnergy) : 0,
            Stars = pcs != null ? Convert.ToInt32(pcs.Stars) : 0,
            Gold = player.Gold,
        };
        AddCreature(s, "player", player.Creature, null);
        if (player.Osty != null) AddCreature(s, "osty", player.Osty, player.Osty.Monster?.Id.Entry);
        if (state.Enemies != null)
        {
            foreach (var e in state.Enemies)
            {
                if (e == null) continue;
                AddCreature(s, Who(e), e, e.Monster?.Id.Entry);
            }
        }
        if (pcs != null)
        {
            AddPile(s, "hand", pcs.Hand?.Cards, true);
            AddPile(s, "draw", pcs.DrawPile?.Cards, false);
            AddPile(s, "discard", pcs.DiscardPile?.Cards, false);
            AddPile(s, "exhaust", ExhaustCards(pcs), false);
            try
            {
                var orbs = pcs.OrbQueue?.Orbs;
                s.OrbSlots = pcs.OrbQueue?.Capacity ?? 0;
                if (orbs != null)
                {
                    s.Orbs = string.Join(",", orbs.Select(o => $"{IdOf(o)}:{o.Id.Entry}:{(int)o.PassiveVal}:{(int)o.EvokeVal}"));
                    s.OrbState = orbs.Select(OrbInfo).ToList();
                }
            }
            catch { }
        }
        return s;
    }

    private static IEnumerable<CardModel>? ExhaustCards(object pcs)
    {
        // Looked up by name so the adapter compiles whatever the pile is called.
        if (!_exhaustLooked)
        {
            _exhaustLooked = true;
            _exhaustProp = pcs.GetType().GetProperty("ExhaustPile", BindingFlags.Instance | BindingFlags.Public);
        }
        var pile = _exhaustProp?.GetValue(pcs);
        var cards = pile?.GetType().GetProperty("Cards")?.GetValue(pile);
        return cards as IEnumerable<CardModel>;
    }

    private static void AddPile(Snapshot s, string name, IEnumerable<CardModel>? cards, bool keepOrder)
    {
        if (cards == null) return;
        int i = 0;
        foreach (var c in cards)
        {
            if (c == null) continue;
            int id = IdOf(c);
            s.Cards[id] = (name, i, c.Id.Entry);
            _seenCards[id] = c;
            if (keepOrder) s.Hand.Add(id);
            i++;
        }
    }

    private static void AddCreature(Snapshot s, string who, Creature? c, string? monster)
    {
        if (c == null) return;
        if (monster != null) _seenCreatures[who] = c;
        var cs = new CreatureSnap
        {
            Who = who,
            Monster = monster,
            Hp = c.CurrentHp,
            MaxHp = c.MaxHp,
            Block = c.Block,
            Alive = c.IsAlive,
        };
        try
        {
            if (c.Powers != null)
                foreach (var p in c.Powers)
                    if (p != null)
                    {
                        cs.Powers[p.Id.Entry] = Convert.ToInt32(p.Amount);
                        _seenPowers[(who, p.Id.Entry)] = p;
                    }
        }
        catch { }
        try
        {
            var intents = c.Monster?.NextMove?.Intents;
            if (intents != null) cs.Intent = string.Join(",", intents.Select(x => x.IntentType.ToString()));
        }
        catch { }
        s.Creatures[who] = cs;
    }

    // ---- diff ----

    private static Dictionary<string, object?> Change(string kind) => new() { ["t"] = kind };

    private static List<Dictionary<string, object?>> Diff(Snapshot a, Snapshot b)
    {
        var d = new List<Dictionary<string, object?>>();
        void Num(string kind, int from, int to, string? who = null)
        {
            if (from == to) return;
            var c = Change(kind);
            if (who != null) c["who"] = who;
            c["from"] = from;
            c["to"] = to;
            d.Add(c);
        }
        Num("round", a.Round, b.Round);
        Num("energy", a.Energy, b.Energy);
        Num("max_energy", a.MaxEnergy, b.MaxEnergy);
        Num("stars", a.Stars, b.Stars);
        Num("gold", a.Gold, b.Gold);

        foreach (var (who, cb) in b.Creatures)
        {
            if (!a.Creatures.TryGetValue(who, out var ca))
            {
                var c = Change("spawn");
                c["who"] = who;
                c["monster"] = cb.Monster;
                c["hp"] = cb.Hp;
                c["max_hp"] = cb.MaxHp;
                d.Add(c);
                continue;
            }
            Num("hp", ca.Hp, cb.Hp, who);
            Num("max_hp", ca.MaxHp, cb.MaxHp, who);
            Num("block", ca.Block, cb.Block, who);
            if (ca.Alive && !cb.Alive) d.Add(new Dictionary<string, object?> { ["t"] = "death", ["who"] = who });
            if (!ca.Alive && cb.Alive) d.Add(new Dictionary<string, object?> { ["t"] = "revive", ["who"] = who });
            foreach (var (pid, amt) in cb.Powers)
            {
                ca.Powers.TryGetValue(pid, out var old);
                if (!ca.Powers.ContainsKey(pid))
                    d.Add(new Dictionary<string, object?> { ["t"] = "power", ["who"] = who, ["power"] = pid, ["from"] = old, ["to"] = amt, ["added"] = true });
                else if (old != amt)
                    d.Add(new Dictionary<string, object?> { ["t"] = "power", ["who"] = who, ["power"] = pid, ["from"] = old, ["to"] = amt });
            }
            foreach (var (pid, amt) in ca.Powers)
                if (!cb.Powers.ContainsKey(pid))
                    d.Add(new Dictionary<string, object?> { ["t"] = "power", ["who"] = who, ["power"] = pid, ["from"] = amt, ["to"] = 0, ["removed"] = true });
            if (ca.Intent != cb.Intent)
                d.Add(new Dictionary<string, object?> { ["t"] = "intent", ["who"] = who, ["intent"] = cb.Intent });
        }
        foreach (var who in a.Creatures.Keys)
            if (!b.Creatures.ContainsKey(who))
                d.Add(new Dictionary<string, object?> { ["t"] = "despawn", ["who"] = who });

        foreach (var (id, nb) in b.Cards)
        {
            if (!a.Cards.TryGetValue(id, out var na))
            {
                d.Add(new Dictionary<string, object?> { ["t"] = "card", ["card"] = id, ["id"] = nb.CardId, ["from"] = "none", ["to"] = nb.Pile });
                continue;
            }
            if (na.Pile != nb.Pile)
                d.Add(new Dictionary<string, object?> { ["t"] = "card", ["card"] = id, ["id"] = nb.CardId, ["from"] = na.Pile, ["to"] = nb.Pile });
            else if (na.CardId != nb.CardId)
                d.Add(new Dictionary<string, object?> { ["t"] = "card_changed", ["card"] = id, ["from_id"] = na.CardId, ["id"] = nb.CardId });
        }
        foreach (var (id, na) in a.Cards)
            if (!b.Cards.ContainsKey(id))
                d.Add(new Dictionary<string, object?> { ["t"] = "card", ["card"] = id, ["id"] = na.CardId, ["from"] = na.Pile, ["to"] = "none" });

        if (!a.Hand.SequenceEqual(b.Hand))
            d.Add(new Dictionary<string, object?> { ["t"] = "hand", ["cards"] = b.Hand.ToList() });
        if (a.Orbs != b.Orbs || a.OrbSlots != b.OrbSlots)
            d.Add(new Dictionary<string, object?> { ["t"] = "orbs", ["orbs"] = b.OrbState, ["slots"] = b.OrbSlots });
        return d;
    }
}
