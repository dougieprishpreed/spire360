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
using MegaCrit.Sts2.Core.Bindings.MegaSpine;
using MegaCrit.Sts2.Core.Nodes.Combat;
using MegaCrit.Sts2.Core.Nodes.Rooms;
using MegaCrit.Sts2.Core.Nodes.Screens.Bestiary;
using MegaCrit.Sts2.Core.Nodes.Vfx.Backgrounds;

namespace Spire.Logic;

internal static class StandIns
{
    private const float Base = 1_000_000f, Step = 10_000f, Slack = 4_000f;
    private static readonly string[] Anchors = { "center", "bottom", "top", "power", "hitbox", "hitbox_size" };

    private static NCombatRoom? _room;
    private static readonly ConditionalWeakTable<Creature, NCreature> Nodes = new();
    private static readonly ConditionalWeakTable<NCreature, Creature> Creatures = new();
    private static readonly List<Creature> Slots = new();
    private static bool _failed;

    /// <summary>While the Bestiary's monsters act (NBestiary.Instance is
    /// a stand-in; their nodes are its creatures, not the fight's).</summary>
    public static bool InBestiary;

    /// <summary>The animations the Bestiary's monster's skeleton has (the
    /// client's answer): NCreatureVisuals.SpineBody's skeleton data answers
    /// HasAnimation from them while set.</summary>
    public static HashSet<string>? SkeletonAnims;

    private static NBestiary? _bestiary;
    private static Godot.Control? _layout;
    private static NCombatBackground? _background;
    private static NKaiserCrabBossBackground? _kaiserCrab;

    /// <summary>An object of a UI class without running its constructor
    /// (its calls are redirected to the hook anyway).</summary>
    private static T Blank<T>() => (T)RuntimeHelpers.GetUninitializedObject(typeof(T));

    private static readonly ConditionalWeakTable<Creature, MegaCrit.Sts2.Core.Nodes.Combat.NCreatureVisuals> VisualsFor = new();
    private static readonly ConditionalWeakTable<object, Creature> VisualsOf = new();
    private static readonly ConditionalWeakTable<object, Creature> BodyOf = new();

    static StandIns()
    {
        Godot.Node.Resolve = (node, path, type) =>
        {
            // NCombatRoom.Background / NBestiary.Layout's %KaiserCrab
            // (NKaiserCrabBossBackground, which Crusher and Rocket drive).
            if ((ReferenceEquals(node, _background) || ReferenceEquals(node, _layout)) && path == "%KaiserCrab" &&
                type == typeof(NKaiserCrabBossBackground))
                return _kaiserCrab ??= Blank<NKaiserCrabBossBackground>();
            // a creature's body: its special nodes
            if (BodyOf.TryGetValue(node, out var c)) return Special(c, "body:" + path, type.FullName ?? "");
            return null;
        };
    }

    // NCreature.GetSpecialNode<T>(path): the scripts the game's code calls on
    // a creature's special nodes, as stand-ins named "<who>#<path>" (Steps
    // passes the name for the receiver; the client finds the node).
    private static readonly Dictionary<string, Func<object>> SpecialClasses = new()
    {
        [typeof(MegaCrit.Sts2.Core.Nodes.Vfx.NKinPriestBeamVfx).FullName!] = () => Blank<MegaCrit.Sts2.Core.Nodes.Vfx.NKinPriestBeamVfx>(),
        [typeof(MegaCrit.Sts2.Core.Nodes.Vfx.NDecimillipedeSegmentVfx).FullName!] = () => Blank<MegaCrit.Sts2.Core.Nodes.Vfx.NDecimillipedeSegmentVfx>(),
        [typeof(MegaCrit.Sts2.Core.Nodes.Animation.NDecimillipedeSegmentDriver).FullName!] =
            () => Blank<MegaCrit.Sts2.Core.Nodes.Animation.NDecimillipedeSegmentDriver>(),
        [typeof(MegaCrit.Sts2.Core.Nodes.Vfx.NMyteVfx).FullName!] = () => Blank<MegaCrit.Sts2.Core.Nodes.Vfx.NMyteVfx>(),
        [typeof(MegaCrit.Sts2.Core.Nodes.Vfx.NSnappingJaxfruitVfx).FullName!] = () => Blank<MegaCrit.Sts2.Core.Nodes.Vfx.NSnappingJaxfruitVfx>(),
        [typeof(MegaCrit.Sts2.Core.Nodes.Vfx.NFlyconidSporesVfx).FullName!] = () => Blank<MegaCrit.Sts2.Core.Nodes.Vfx.NFlyconidSporesVfx>(),
    };
    private static readonly Dictionary<string, object> Specials = new();
    private static readonly ConditionalWeakTable<object, string> SpecialNames = new();
    private static readonly ConditionalWeakTable<Creature, Godot.Node2D> Bodies = new();

    private static object? Special(Creature c, string path, string typeName)
    {
        if (!SpecialClasses.TryGetValue(typeName, out var make)) return null;
        var key = Steps.Who(c) + "#" + path;
        if (!Specials.TryGetValue(key, out var o))
        {
            o = make();
            Specials[key] = o;
            SpecialNames.AddOrUpdate(o, key);
        }
        return o;
    }

    /// <summary>The name of a special node's stand-in ("e3#Visuals/Beam").</summary>
    public static bool TrySpecial(object? o, out string name)
    {
        name = "";
        return o != null && SpecialNames.TryGetValue(o, out name!);
    }

    /// <summary>The hook's answer for a redirected UI call, or null.</summary>
    public static object? Answer(string id, object?[] a)
    {
        if (_failed) return null;
        var sep = id.LastIndexOf("::", StringComparison.Ordinal);
        if (sep < 0) return null;
        var method = id[(sep + 2)..];
        var typeArg = "";
        var lt = method.IndexOf('<');
        if (lt >= 0)
        {
            typeArg = method[(lt + 1)..^1];
            method = method[..lt];
        }
        try
        {
            // NCreature.GetSpecialNode<T>(name): a stand-in of T for a script the game calls
            if (method == "GetSpecialNode" && a.Length > 1 && a[0] is NCreature sn && Creatures.TryGetValue(sn, out var sc) &&
                a[1] is string spath)
                return Special(sc, spath, typeArg);
            // NCreature.Visuals.GetCurrentBody(): a body node standing for the creature's
            // (its GetNode<T>(path) gives the special node "<who>#body:<path>")
            if (method == "get_Visuals" && a.Length > 0 && a[0] is NCreature vn && Creatures.TryGetValue(vn, out var vc))
                return VisualsFor.GetValue(vc, cr =>
                {
                    var v = new MegaCrit.Sts2.Core.Nodes.Combat.NCreatureVisuals();
                    VisualsOf.AddOrUpdate(v, cr);
                    return v;
                });
            if (method == "GetCurrentBody" && a.Length > 0 && a[0] is MegaCrit.Sts2.Core.Nodes.Combat.NCreatureVisuals cv &&
                VisualsOf.TryGetValue(cv, out var bc))
                return Bodies.GetValue(bc, cr => { var b = new Godot.Node2D(); BodyOf.AddOrUpdate(b, cr); return b; });
            if (InBestiary)
            {
                if (id.EndsWith("NBestiary::get_Instance", StringComparison.Ordinal)) return _bestiary ??= Blank<NBestiary>();
                if (id.EndsWith("NBestiary::get_Layout", StringComparison.Ordinal)) return _layout ??= new Godot.Control();
                if (id.EndsWith("NBestiary::GetCreatureNode", StringComparison.Ordinal))
                {
                    foreach (var x in a)
                        if (x is Creature c)
                            return Node(c);
                    return null;
                }
                if (id.EndsWith("NCombatRoom::get_Instance", StringComparison.Ordinal)) return null;
            }
            if (Animators.AnswerHasAnimation && id.EndsWith("MegaSprite::HasAnimation", StringComparison.Ordinal)) return true;
            if (SkeletonAnims != null)
            {
                var typeFull = id[(id.IndexOf('|') + 1)..sep];
                switch (typeFull[(typeFull.LastIndexOf('.') + 1)..] + "::" + method)
                {
                    case "NCreatureVisuals::get_SpineBody": return Blank<MegaSprite>();
                    case "MegaSprite::GetSkeleton": return Blank<MegaSkeleton>();
                    case "MegaSkeleton::GetData": return Blank<MegaSkeletonDataResource>();
                    case "MegaSkeletonDataResource::HasAnimation":
                        foreach (var x in a)
                            if (x is string name)
                                return SkeletonAnims.Contains(name);
                        return false;
                }
            }
            if (id.EndsWith("NCombatRoom::get_Background", StringComparison.Ordinal))
                return CombatManager.Instance.IsInProgress ? _background ??= Blank<NCombatBackground>() : null;
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
                    case "get_Entity": return cr;
                    case "get_Hitbox": return Hitbox(cr);
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

    private static readonly ConditionalWeakTable<Creature, Godot.Control> Hitboxes = new();

    /// <summary>NCreature.Hitbox: a control whose GlobalPosition (its top
    /// left) and Size are encoded ("hitbox", "hitbox_size").</summary>
    private static Godot.Control Hitbox(Creature c)
    {
        if (!Hitboxes.TryGetValue(c, out var h))
        {
            h = new Godot.Control();
            Hitboxes.Add(c, h);
        }
        h.GlobalPosition = Encode(c, 4);
        h.Size = Encode(c, 5);
        return h;
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
