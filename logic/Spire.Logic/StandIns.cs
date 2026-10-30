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
using System.Diagnostics.CodeAnalysis;
using System.Reflection;
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
    private static readonly string[] Anchors = { "center", "bottom", "top", "power", "hitbox", "hitbox_size", "side_floor" };

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
    // NCombatRoom.CombatVfxContainer / BackCombatVfxContainer (and the
    // Bestiary's): containers the effects are added to (their Create calls
    // are recorded; what is added is null)
    private static Godot.Control? _vfxBox, _backVfxBox, _eventVfxBox;
    private static MegaCrit.Sts2.Core.Nodes.Rooms.NEventRoom? _eventRoom;
    /// <summary>Steps.InCombat: the creatures' AfterAddedToRoom (Slumbering
    /// Beetle's sleep, ...) runs before IsInProgress.</summary>
    private static bool InCombat => Steps.InCombat;
    private static NMerchantRoom? _merchantRoom;
    private static MegaCrit.Sts2.Core.Nodes.Screens.Shops.NMerchantInventory? _merchantInventory;
    private static NMerchantButton? _merchantButton;

    /// <summary>The Fake Merchant's node (EventModel.Node, which its UI
    /// sets): its inventory and button are the stand-ins'.</summary>
    public static Godot.Control FakeMerchantNode() =>
        _fakeMerchant ??= Blank<MegaCrit.Sts2.Core.Nodes.Events.Custom.NFakeMerchant>();
    private static MegaCrit.Sts2.Core.Nodes.Events.Custom.NFakeMerchant? _fakeMerchant;
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
        // a position marker (%SleepVfxPos): its GlobalPosition names it (NodePlace)
        [typeof(Godot.Marker2D).FullName!] = () => new Godot.Marker2D(),
        [typeof(MegaCrit.Sts2.Core.Nodes.Vfx.NSnappingJaxfruitVfx).FullName!] = () => Blank<MegaCrit.Sts2.Core.Nodes.Vfx.NSnappingJaxfruitVfx>(),
        [typeof(MegaCrit.Sts2.Core.Nodes.Vfx.NFlyconidSporesVfx).FullName!] = () => Blank<MegaCrit.Sts2.Core.Nodes.Vfx.NFlyconidSporesVfx>(),
    };
    // The powers' forms (NFormVfx.Create(creature)): a stand-in per creature
    // and form ("player#form/NDemonFormVfx"), so SetActive and
    // OnEffectTriggered on the power's Vfx become cues.
    private static readonly Dictionary<string, Func<object>> FormClasses = new()
    {
        [typeof(MegaCrit.Sts2.Core.Nodes.Vfx.Forms.NDemonFormVfx).FullName!] = () => Blank<MegaCrit.Sts2.Core.Nodes.Vfx.Forms.NDemonFormVfx>(),
        [typeof(MegaCrit.Sts2.Core.Nodes.Vfx.Forms.NVoidFormVfx).FullName!] = () => Blank<MegaCrit.Sts2.Core.Nodes.Vfx.Forms.NVoidFormVfx>(),
        [typeof(MegaCrit.Sts2.Core.Nodes.Vfx.Forms.NSerpentFormVfx).FullName!] = () => Blank<MegaCrit.Sts2.Core.Nodes.Vfx.Forms.NSerpentFormVfx>(),
        [typeof(MegaCrit.Sts2.Core.Nodes.Vfx.Forms.NReaperFormVfx).FullName!] = () => Blank<MegaCrit.Sts2.Core.Nodes.Vfx.Forms.NReaperFormVfx>(),
        [typeof(MegaCrit.Sts2.Core.Nodes.Vfx.Forms.NEchoFormVfx).FullName!] = () => Blank<MegaCrit.Sts2.Core.Nodes.Vfx.Forms.NEchoFormVfx>(),
    };
    private static readonly Dictionary<string, object> Specials = new();
    private static readonly ConditionalWeakTable<object, string> SpecialNames = new();
    private static readonly ConditionalWeakTable<Creature, Godot.Node2D> Bodies = new();

    private static object? Special(Creature c, string path, string typeName)
    {
        if (!SpecialClasses.TryGetValue(typeName, out var make)) return null;
        var o = Named(c, path, make);
        if (o is Godot.Marker2D marker) marker.GlobalPosition = NodePlace(c, path);
        return o;
    }

    /// <summary>A creature's stand-in named "<who>#<path>" (the calls on it
    /// reach the client with that name), made once.</summary>
    private static object Named(Creature c, string path, Func<object> make)
    {
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
            // GodotObject.IsValid / NodeUtil.IsValid on a special node's stand-in:
            // the node exists (the game keeps it, e.g. a power's form Vfx)
            if (method == "IsValid" && a.Length > 0 && a[0] != null && SpecialNames.TryGetValue(a[0]!, out _)) return true;
            if (method == "Create" && a.Length > 0 && a[0] is Creature fc)
            {
                var formType = id[(id.IndexOf('|') + 1)..sep];
                if (FormClasses.TryGetValue(formType, out var makeForm))
                {
                    var key = Steps.Who(fc) + "#form/" + formType[(formType.LastIndexOf('.') + 1)..];
                    if (!Specials.TryGetValue(key, out var form))
                    {
                        form = makeForm();
                        Specials[key] = form;
                        SpecialNames.AddOrUpdate(form, key);
                    }
                    return form;
                }
            }
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
                if (id.EndsWith("NBestiary::get_VfxContainer", StringComparison.Ordinal)) return _vfxBox ??= new Godot.Control();
                if (id.EndsWith("NBestiary::get_BackVfxContainer", StringComparison.Ordinal)) return _backVfxBox ??= new Godot.Control();
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
            // Effects the game waits for or sounds only when they exist (null
            // headless skips it): Hyperbeam's and Sweeping Beam's 0.5 s, End of
            // Days' missile (its WaitTime), Fiend Fire's attack_fire per hit,
            // Grand Finale's anticipation (NGrandFinaleVfx.totalAnticipationDuration,
            // 1.625 s: the client's spotlight played out under the damage). The
            // calls themselves are cues already; a stand-in lets the code after
            // its null check run. The missiles' WaitTime is 0.2 s (Comet's and
            // Guiding Star's small ones waited 0 on the null).
            if (InCombat)
                switch (id[(id.LastIndexOf('.', sep) + 1)..])
                {
                    case "NHyperbeamVfx::Create": return Blank<MegaCrit.Sts2.Core.Nodes.Vfx.NHyperbeamVfx>();
                    case "NSweepingBeamVfx::Create": return Blank<MegaCrit.Sts2.Core.Nodes.Vfx.NSweepingBeamVfx>();
                    case "NLargeMagicMissileVfx::Create": return Blank<MegaCrit.Sts2.Core.Nodes.Vfx.NLargeMagicMissileVfx>();
                    case "NLargeMagicMissileVfx::get_WaitTime": return 0.2f;
                    case "NSmallMagicMissileVfx::get_WaitTime": return 0.2f;
                    case "NGrandFinaleVfx::Create": return Blank<MegaCrit.Sts2.Core.Nodes.Vfx.NGrandFinaleVfx>();
                    case "NGroundFireVfx::Create": return Blank<MegaCrit.Sts2.Core.Nodes.Vfx.NGroundFireVfx>();
                    // NSleepingVfx.Create(%SleepVfxPos) (the Lagavulin Matriarch's and
                    // the Slumbering Beetle's ZZZ): a stand-in named "<who>#sleeping",
                    // so their SleepingVfx?.Stop() (the first damage, the wake, each
                    // move, the death) reaches the client; null, the ZZZ never stopped.
                    case "NSleepingVfx::Create":
                        return a.Length > 0 && a[0] is Godot.Vector2 at && TryPlaced(at, out var sleeper)
                            ? Named(sleeper, "sleeping", () => Blank<MegaCrit.Sts2.Core.Nodes.Vfx.NSleepingVfx>())
                            : Blank<MegaCrit.Sts2.Core.Nodes.Vfx.NSleepingVfx>();
                }
            // NEventRoom.Instance / VfxContainer outside combat: the events' own
            // effects (Dense Vegetation's slashes, Jungle Maze's hits) are made
            // only when the container exists; its Size places them.
            if (id.EndsWith("NEventRoom::get_Instance", StringComparison.Ordinal))
                return CombatManager.Instance.IsInProgress ? null : _eventRoom ??= Blank<MegaCrit.Sts2.Core.Nodes.Rooms.NEventRoom>();
            // FoulPotion.GetFoulPotionMerchantTarget / OnUse: the merchant's
            // button beside its closed inventory (NMerchantRoom.Instance in a
            // shop; the Fake Merchant's node), so the potion can be thrown
            if (id.EndsWith("NMerchantRoom::get_Instance", StringComparison.Ordinal))
                return !CombatManager.Instance.IsInProgress &&
                       MegaCrit.Sts2.Core.Runs.RunManager.Instance.DebugOnlyGetState()?.CurrentRoom is MegaCrit.Sts2.Core.Rooms.MerchantRoom
                    ? _merchantRoom ??= Blank<NMerchantRoom>()
                    : null;
            if (id.EndsWith("NMerchantRoom::get_Inventory", StringComparison.Ordinal) ||
                id.EndsWith("NFakeMerchant::get_Inventory", StringComparison.Ordinal))
                return _merchantInventory ??= Blank<MegaCrit.Sts2.Core.Nodes.Screens.Shops.NMerchantInventory>();
            if (id.EndsWith("NMerchantRoom::get_MerchantButton", StringComparison.Ordinal) ||
                id.EndsWith("NFakeMerchant::get_MerchantButton", StringComparison.Ordinal))
                return _merchantButton ??= Blank<NMerchantButton>();
            if (id.EndsWith("NEventRoom::get_VfxContainer", StringComparison.Ordinal))
            {
                if (CombatManager.Instance.IsInProgress) return null;
                _eventVfxBox ??= new Godot.Control { Size = new Godot.Vector2(1920, 1080) };
                if (_eventVfxBox.GetChildCount() > 32)
                    foreach (var child in _eventVfxBox.GetChildren()) _eventVfxBox.RemoveChild(child);
                return _eventVfxBox;
            }
            if (id.EndsWith("NCombatRoom::get_CombatVfxContainer", StringComparison.Ordinal))
            {
                // effect stand-ins added to it stay its children: keep it small
                if (_vfxBox != null && _vfxBox.GetChildCount() > 32)
                    foreach (var child in _vfxBox.GetChildren()) _vfxBox.RemoveChild(child);
                return InCombat ? _vfxBox ??= new Godot.Control() : null;
            }
            if (id.EndsWith("NCombatRoom::get_BackCombatVfxContainer", StringComparison.Ordinal))
                return InCombat ? _backVfxBox ??= new Godot.Control() : null;
            if (id.EndsWith("NCombatRoom::get_Background", StringComparison.Ordinal))
                return InCombat ? _background ??= Blank<NCombatBackground>() : null;
            if (id.EndsWith("NCombatRoom::get_Instance", StringComparison.Ordinal))
                return InCombat ? (_room ??= new NCombatRoom()) : null;
            if (id.EndsWith("NCombatRoom::GetCreatureNode", StringComparison.Ordinal))
            {
                foreach (var x in a)
                    if (x is Creature c)
                        return c.CombatState != null && InCombat ? Node(c) : null;
                return null;
            }
            if (method == "GetAnimationState" && a.Length > 0 && TryCreature(a[0], out var sc2))
                return State(sc2);
            if (method == "get_SpineBody" && SkeletonAnims == null && a.Length > 0 &&
                a[0] is MegaCrit.Sts2.Core.Nodes.Combat.NCreatureVisuals sv && VisualsOf.TryGetValue(sv, out var svc))
                return Sprite(svc);
            if (method == "get_SpineAnimation" && a.Length > 0)
            {
                if (a[0] is NCreature an && Creatures.TryGetValue(an, out var ac)) return new SpineAnimationAccess(Sprite(ac));
                if (a[0] is MegaCrit.Sts2.Core.Nodes.Combat.NCreatureVisuals av && VisualsOf.TryGetValue(av, out var avc))
                    return new SpineAnimationAccess(Sprite(avc));
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
                    case "StartDeathAnim":
                        if (a.Length > 1 && a[1] is bool remove && !remove) Steps.MarkDeathKept(Steps.Who(cr));
                        // its death sound, which the game plays there (with a
                        // skeleton) and nowhere else: every monster's death was
                        // silent, the Phrog Parasite's burst into Wrigglers too
                        if (AnimLengths.HasSkeleton(cr))
                        {
                            if (cr.Monster != null && cr.Monster.HasDeathSfx)
                                MegaCrit.Sts2.Core.Commands.SfxCmd.PlayDeath(cr.Monster);
                            if (cr.Player != null) MegaCrit.Sts2.Core.Commands.SfxCmd.PlayDeath(cr.Player);
                        }
                        return DeathAnimLength(cr);
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

    /// <summary>NCreature.StartDeathAnim: with a skeleton, SetAnimationTrigger
    /// ("Dead") (the animator's death state: the death plays it) and that
    /// animation's length; the monster's DeathAnimLengthOverride over it; at
    /// most 30 s.</summary>
    private static float DeathAnimLength(Creature cr)
    {
        List<Dictionary<string, object?>>? ops = AnimLengths.HasSkeleton(cr) ? Animators.Trigger(cr, "Dead") : null;
        if (ops != null) Steps.AttachDeathAnimation(Steps.Who(cr), ops);
        if (cr.Monster != null && cr.Monster.HasDeathAnimLengthOverride) return cr.Monster.DeathAnimLengthOverride;
        string? anim = null;
        foreach (var op in ops ?? new())
            if (op["op"] as string == "set_animation" && (op["track"] is int track ? track : 0) == 0) { anim = op["name"] as string; break; }
        return anim == null ? 0f : Math.Min(AnimLengths.Of(cr, anim), 30f);
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

    /// <summary>A creature's VfxSpawnPosition as the stand-ins encode it.</summary>
    public static Godot.Vector2 CenterOf(Creature c) => Encode(c, 0);
    /// <summary>VfxCmd.GetSideCenterFloor for c's side ("eN@side_floor":
    /// the client takes its hittable creatures' floor).</summary>
    public static Godot.Vector2 SideFloorOf(Creature c) => Encode(c, 6);

    // A creature's node by its path ("eN@node:%SleepVfxPos"): anchors from
    // NodeAnchorBase on, one per path met
    private const int NodeAnchorBase = 100;
    private static readonly List<string> NodeAnchors = new();

    private static Godot.Vector2 NodePlace(Creature c, string path)
    {
        int i = NodeAnchors.IndexOf(path);
        if (i < 0)
        {
            NodeAnchors.Add(path);
            i = NodeAnchors.Count - 1;
        }
        return Encode(c, NodeAnchorBase + i);
    }

    /// <summary>The creature a stand-in node stands for (its node, spine
    /// sprite or animation state, or a SpineAnimationAccess on its sprite).</summary>
    public static bool TryCreature(object? o, out Creature creature)
    {
        creature = null!;
        if (o is NCreature n) return Creatures.TryGetValue(n, out creature!);
        if (o is SpineAnimationAccess access) o = SpriteOf(access);
        return o != null && SpineOf.TryGetValue(o, out creature!);
    }

    private static FieldInfo? _accessSprite;

    /// <summary>SpineAnimationAccess's sprite (a struct the game passes
    /// around: its calls arrive with a copy as the receiver).</summary>
    [DynamicDependency("_sprite", typeof(SpineAnimationAccess))]
    private static object? SpriteOf(SpineAnimationAccess access)
    {
        _accessSprite ??= typeof(SpineAnimationAccess).GetField("_sprite", BindingFlags.Instance | BindingFlags.NonPublic);
        return _accessSprite?.GetValue(access);
    }

    // A creature's spine (NCreature.SpineAnimation, NCreatureVisuals.SpineBody):
    // a sprite and its animation state standing for that creature's, so the
    // game's calls on them (SetAnimation on a track, ...) are recorded with
    // the creature as their receiver.
    private static readonly ConditionalWeakTable<Creature, MegaSprite> Sprites = new();
    private static readonly ConditionalWeakTable<Creature, MegaAnimationState> States = new();
    private static readonly ConditionalWeakTable<object, Creature> SpineOf = new();

    private static MegaSprite Sprite(Creature c) => Sprites.GetValue(c, cr =>
    {
        var s = Blank<MegaSprite>();
        SpineOf.AddOrUpdate(s, cr);
        return s;
    });

    private static MegaAnimationState State(Creature c) => States.GetValue(c, cr =>
    {
        var s = Blank<MegaAnimationState>();
        SpineOf.AddOrUpdate(s, cr);
        return s;
    });

    /// <summary>The creature an encoded position is about (Describe's slot).</summary>
    public static bool TryPlaced(Godot.Vector2 v, out Creature creature)
    {
        creature = null!;
        if (v.X < Base - Slack || v.Y > -Base + Slack) return false;
        int slot = (int)Math.Round((v.X - Base) / Step);
        if (slot < 0 || slot >= Slots.Count) return false;
        creature = Slots[slot];
        return true;
    }

    /// <summary>"e1@bottom" (and "+dx,dy" when the game moved it) for an
    /// encoded position; null for any other.</summary>
    public static string? Describe(Godot.Vector2 v)
    {
        if (v.X < Base - Slack || v.Y > -Base + Slack) return null;
        int slot = (int)Math.Round((v.X - Base) / Step);
        int anchor = (int)Math.Round((-v.Y - Base) / Step);
        float dx = v.X - (Base + slot * Step), dy = v.Y - (-Base - anchor * Step);
        bool node = anchor >= NodeAnchorBase && anchor - NodeAnchorBase < NodeAnchors.Count;
        if (slot < 0 || slot >= Slots.Count || anchor < 0 || (anchor >= Anchors.Length && !node)) return null;
        if (Math.Abs(dx) > Slack || Math.Abs(dy) > Slack) return null;
        var s = Steps.Who(Slots[slot]) + "@" + (node ? "node:" + NodeAnchors[anchor - NodeAnchorBase] : Anchors[anchor]);
        if (Math.Abs(dx) > 0.01f || Math.Abs(dy) > 0.01f) s += $"{dx:+0.##;-0.##;+0},{dy:+0.##;-0.##;+0}";
        return s;
    }
}
