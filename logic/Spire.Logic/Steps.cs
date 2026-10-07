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
    public static void Ui(string id, object?[] args)
    {
        if (!Enabled || !RecordUi) return;
        var sep = id.LastIndexOf("::", StringComparison.Ordinal);
        if (sep < 0) return;
        var method = id[(sep + 2)..];
        if (method.StartsWith("get_", StringComparison.Ordinal) || method.StartsWith("set_", StringComparison.Ordinal)
            || method.StartsWith("add_", StringComparison.Ordinal) || method.StartsWith("remove_", StringComparison.Ordinal))
            return;
        var typeFull = id[(id.IndexOf('|') + 1)..sep];
        var type = typeFull[(typeFull.LastIndexOf('.') + 1)..];
        var outArgs = new List<object?>();
        // args[0] is the receiver for instance calls; it is null here (no UI), skip it.
        foreach (var a in args)
        {
            switch (a)
            {
                case null: continue;
                case string str: outArgs.Add(str); break;
                case bool or int or long or float or double or decimal: outArgs.Add(a); break;
                case Enum e: outArgs.Add(e.ToString()); break;
                case MegaCrit.Sts2.Core.Models.AbstractModel m: outArgs.Add(m.Id.ToString()); break;
                case Creature c: outArgs.Add(c.IsPlayer ? "player" : "e" + IdOf(c)); break;
                default:
                    var tn = a.GetType().Name;
                    if (tn.StartsWith("N", StringComparison.Ordinal)) continue; // UI node receivers
                    outArgs.Add(tn);
                    break;
            }
        }
        if (_pendingUi.Count < 500)
            _pendingUi.Add(new Dictionary<string, object?> { ["t"] = "ui", ["call"] = type + "." + method, ["args"] = outArgs.Count > 0 ? outArgs : null });
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
            // Outside combat the cues still matter (event sounds, map music).
            if (_pendingUi.Count > 0 && _steps.Count < MaxStepsPerResponse)
            {
                _steps.Add(new List<Dictionary<string, object?>>(_pendingUi));
                _pendingUi.Clear();
            }
            _last = null;
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
    }

    /// <summary>Steps recorded since the previous call, or null if none.</summary>
    public static List<List<Dictionary<string, object?>>>? Drain()
    {
        Capture();
        if (_steps.Count == 0) return null;
        var copy = new List<List<Dictionary<string, object?>>>(_steps);
        _steps.Clear();
        return copy;
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
                AddCreature(s, "e" + IdOf(e), e, e.Monster?.Id.Entry);
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
                if (orbs != null)
                    s.Orbs = string.Join(",", orbs.Select(o => $"{o.Id.Entry}:{(int)o.PassiveVal}:{(int)o.EvokeVal}"));
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
            if (keepOrder) s.Hand.Add(id);
            i++;
        }
    }

    private static void AddCreature(Snapshot s, string who, Creature? c, string? monster)
    {
        if (c == null) return;
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
                    if (p != null) cs.Powers[p.Id.Entry] = Convert.ToInt32(p.Amount);
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
                if (old != amt || !ca.Powers.ContainsKey(pid))
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
        if (a.Orbs != b.Orbs)
            d.Add(new Dictionary<string, object?> { ["t"] = "orbs", ["orbs"] = b.Orbs });
        return d;
    }
}
