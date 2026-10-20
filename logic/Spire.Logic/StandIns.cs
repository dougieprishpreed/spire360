// UI objects for the logic's calls into the Godot UI (the 'ui' patch passes
// a hook's result through for UI-class and value results): while a fight is
// on, NCombatRoom.Instance is a room and GetCreatureNode(creature) a node
// standing for that creature, so the game's presentation code around them
// runs (and is recorded as cues) instead of stopping at a null. Their
// constructors only set plain fields (Godot is stubbed headless); every call
// on them is redirected to the hook again.
//
// A stand-in node's positions (VfxSpawnPosition, GetBottomOfHitbox, ...)
// are encoded: x = 1e6 + slot * 1e4, y = -1e6 - anchor * 1e4, plus whatever
// offset the game adds; Steps decodes them in cue arguments as
// "<who>@<anchor>[+dx,dy]" for the presentation to place there.
using System.Runtime.CompilerServices;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Entities.Creatures;
using MegaCrit.Sts2.Core.Nodes.Combat;
using MegaCrit.Sts2.Core.Nodes.Rooms;

namespace Spire.Logic;

internal static class StandIns
{
    private const float Base = 1_000_000f, Step = 10_000f, Slack = 4_000f;
    private static readonly string[] Anchors = { "center", "bottom", "top", "power" };

    private static NCombatRoom? _room;
    private static readonly ConditionalWeakTable<Creature, NCreature> Nodes = new();
    private static readonly ConditionalWeakTable<NCreature, Creature> Creatures = new();
    private static readonly List<Creature> Slots = new();
    private static bool _failed;

    /// <summary>The hook's answer for a redirected UI call, or null.</summary>
    public static object? Answer(string id, object?[] a)
    {
        if (_failed) return null;
        var sep = id.LastIndexOf("::", StringComparison.Ordinal);
        if (sep < 0) return null;
        var method = id[(sep + 2)..];
        try
        {
            if (id.EndsWith("NCombatRoom::get_Instance", StringComparison.Ordinal))
                return CombatManager.Instance.IsInProgress ? (_room ??= new NCombatRoom()) : null;
            if (id.EndsWith("NCombatRoom::GetCreatureNode", StringComparison.Ordinal))
            {
                foreach (var x in a)
                    if (x is Creature c)
                        return c.CombatState != null && CombatManager.Instance.IsInProgress ? Node(c) : null;
                return null;
            }
            if (a.Length > 0 && a[0] is NCreature n && Creatures.TryGetValue(n, out var cr))
            {
                switch (method)
                {
                    case "get_VfxSpawnPosition": return Encode(cr, 0);
                    case "GetBottomOfHitbox": return Encode(cr, 1);
                    case "GetTopOfHitbox": return Encode(cr, 2);
                    case "get_PowerAppliedVfxSpawnPosition": return Encode(cr, 3);
                    case "get_IsInteractable": return cr.IsAlive;
                }
            }
        }
        catch (Exception ex)
        {
            // A constructor that needs the engine after all: back to nulls.
            _failed = true;
            Console.Error.WriteLine($"[WARN] UI stand-ins disabled: {ex.GetType().Name}: {ex.Message}");
        }
        return null;
    }

    private static NCreature Node(Creature c)
    {
        if (Nodes.TryGetValue(c, out var n)) return n;
        n = new NCreature();
        Nodes.Add(c, n);
        Creatures.Add(n, c);
        return n;
    }

    private static Godot.Vector2 Encode(Creature c, int anchor)
    {
        int slot = Slots.IndexOf(c);
        if (slot < 0)
        {
            Slots.Add(c);
            slot = Slots.Count - 1;
        }
        return new Godot.Vector2(Base + slot * Step, -Base - anchor * Step);
    }

    /// <summary>The creature a stand-in node stands for.</summary>
    public static bool TryCreature(object? o, out Creature creature)
    {
        creature = null!;
        return o is NCreature n && Creatures.TryGetValue(n, out creature!);
    }

    /// <summary>"e1@bottom" (and "+dx,dy" when the game moved it) for an
    /// encoded position; null for any other.</summary>
    public static string? Describe(Godot.Vector2 v)
    {
        if (v.X < Base - Slack || v.Y > -Base + Slack) return null;
        int slot = (int)Math.Round((v.X - Base) / Step);
        int anchor = (int)Math.Round((-v.Y - Base) / Step);
        float dx = v.X - (Base + slot * Step), dy = v.Y - (-Base - anchor * Step);
        if (slot < 0 || slot >= Slots.Count || anchor < 0 || anchor >= Anchors.Length) return null;
        if (Math.Abs(dx) > Slack || Math.Abs(dy) > Slack) return null;
        var s = Steps.Who(Slots[slot]) + "@" + Anchors[anchor];
        if (Math.Abs(dx) > 0.01f || Math.Abs(dy) > 0.01f) s += $"{dx:+0.##;-0.##;+0},{dy:+0.##;-0.##;+0}";
        return s;
    }
}
