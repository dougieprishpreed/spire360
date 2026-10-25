using System.Runtime.CompilerServices;
using MegaCrit.Sts2.Core.Animation;
using MegaCrit.Sts2.Core.Bindings.MegaSpine;
using MegaCrit.Sts2.Core.Entities.Creatures;

namespace Spire.Logic;

/// <summary>
/// Each creature's CreatureAnimator (NCreature._Ready: the character's or
/// the monster's GenerateAnimator), run here so an animation trigger plays
/// what the game plays: the monster's own states and triggers (attack_double,
/// devour_start, ...), the conditions on them (low health, ravenous) and the
/// states queued after. Its spine calls are UI calls in the logic; they are
/// read as ops (set_animation / add_animation) in order. The skeleton's
/// animations are not known here: HasAnimation answers yes, and the client
/// stops a trigger's ops at the first animation the skeleton lacks, where the
/// game's SetNextState / AddNextState return.
/// </summary>
internal static class Animators
{
    private sealed class Holder
    {
        public object? Combat; // the fight it was made for (a new NCreature each fight)
        public CreatureAnimator? Animator;
        public List<Dictionary<string, object?>> Init = new();
        public string? Bounds; // the BoundsContainer BoundsUpdated last named
    }

    private static readonly ConditionalWeakTable<Creature, Holder> _holders = new();

    /// <summary>While set, MegaSprite.HasAnimation answers yes (StandIns).</summary>
    public static bool AnswerHasAnimation;

    private static List<Dictionary<string, object?>> Record(Action run)
    {
        var ops = new List<Dictionary<string, object?>>();
        void Call(string call, List<object?> a)
        {
            string Str(int i) => i < a.Count ? a[i] as string ?? "" : "";
            bool Bool(int i, bool dflt) => i < a.Count && a[i] is bool b ? b : dflt;
            int Int(int i) => i < a.Count && a[i] is int n ? n : 0;
            float Float(int i) => i < a.Count && a[i] is float or double or int ? Convert.ToSingle(a[i]) : 0f;
            // the receivers are null here (no UI): the args are the call's own
            // OffsetLoopingAnimation runs on every looping state the animator
            // sets (SetNextState: its track 0 entry) or queues (AddNextState:
            // AddAnimationTracked): a speed of 0.9-1.1 and a start within 0.1 s
            // of its loop point ("offset"; Rng.Chaotic: the client rolls its
            // own). The stand-ins answer GetCurrent with null, so it is read
            // from the calls, not from the track entry's.
            switch (call)
            {
                case "MegaAnimationState.SetAnimation":
                    ops.Add(new() { ["op"] = "set_animation", ["name"] = Str(0), ["loop"] = Bool(1, true), ["track"] = Int(2) });
                    if (Bool(1, true)) ops[^1]["offset"] = true;
                    break;
                case "MegaAnimationState.AddAnimation":
                case "MegaAnimationState.AddAnimationTracked":
                    ops.Add(new()
                    {
                        ["op"] = "add_animation", ["name"] = Str(0), ["delay"] = Float(1), ["loop"] = Bool(2, true), ["track"] = Int(3),
                    });
                    if (call == "MegaAnimationState.AddAnimationTracked") ops[^1]["offset"] = true;
                    break;
            }
        }
        var before = Steps.Intercept;
        Steps.Intercept = Call;
        AnswerHasAnimation = true;
        try
        {
            run();
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"[SIM] animator: {ex.Message}");
        }
        finally
        {
            AnswerHasAnimation = false;
            Steps.Intercept = before;
        }
        return ops;
    }

    private static readonly System.Reflection.FieldInfo? _currentState =
        typeof(CreatureAnimator).GetField("_currentState", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance);

    [System.Diagnostics.CodeAnalysis.DynamicDependency("_currentState", typeof(CreatureAnimator))]
    private static Holder Get(Creature c)
    {
        if (_holders.TryGetValue(c, out var held) && ReferenceEquals(held.Combat, c.CombatState)) return held;
        var h = new Holder { Combat = c.CombatState };
        h.Init = Record(() =>
        {
            var sprite = (MegaSprite)RuntimeHelpers.GetUninitializedObject(typeof(MegaSprite));
            h.Animator = c.Player != null ? c.Player.Character.GenerateAnimator(sprite, c) : c.Monster?.GenerateAnimator(sprite);
        });
        // the constructor starts an initial idle_loop anywhere in it
        // (SetTrackTime(Rng.Chaotic.NextFloat(its end)))
        if (h.Init.Count > 0 && h.Init[0]["op"] as string == "set_animation" && h.Init[0]["name"] as string == "idle_loop")
            h.Init[0]["random_start"] = true;
        if (h.Animator != null)
        {
            // the constructor's first state named it before anyone listened
            h.Bounds = (_currentState?.GetValue(h.Animator) as AnimState)?.BoundsContainer;
            h.Animator.BoundsUpdated += name => h.Bounds = name;
            Settle(h);
        }
        _holders.AddOrUpdate(c, h);
        return h;
    }

    /// <summary>CreatureAnimator.OnAnimationCompleted, headless (the sprite's
    /// signals never fire): a state that does not loop plays out and the
    /// animator moves on to the state after it, as the game does when its
    /// animation completes; each state started that way names its
    /// BoundsContainer (OnAnimationStarted). Settled right after a trigger,
    /// so the next one branches from where the game's animator will be (the
    /// Owl Magistrate's take_off has played into fly_loop, whose Attack is
    /// the dive) and the bounds are the ones the creature ends up in.</summary>
    private static void Settle(Holder h)
    {
        if (h.Animator == null || _currentState == null) return;
        for (int i = 0; i < 8; i++)
        {
            if (_currentState.GetValue(h.Animator) is not AnimState cur || cur.IsLooping) return;
            var next = cur.GetNextState();
            if (next == null) return;
            _currentState.SetValue(h.Animator, next);
            if (next.BoundsContainer != null) h.Bounds = next.BoundsContainer;
        }
    }

    /// <summary>What the animator played when it was made (its first state).</summary>
    public static List<Dictionary<string, object?>> Init(Creature c) => Get(c).Init;

    /// <summary>The BoundsContainer NCreature.UpdateBounds last had from the
    /// animator (BoundsUpdated: a state's IdleBounds, FlyingBounds, ...; null:
    /// the visuals' own).</summary>
    public static string? Bounds(Creature c) => Get(c).Bounds;

    /// <summary>NCreature.SetAnimationTrigger: what the trigger plays (null: no
    /// animator), and a "bounds" op when the new state moves the creature's
    /// bounds (SetNextState's BoundsUpdated: the intents go to that
    /// container's IntentPos).</summary>
    public static List<Dictionary<string, object?>>? Trigger(Creature c, string trigger)
    {
        var h = Get(c);
        if (h.Animator == null) return null;
        var animator = h.Animator;
        var before = h.Bounds;
        var ops = Record(() => animator.SetTrigger(trigger));
        Settle(h);
        if (h.Bounds != null && h.Bounds != before) ops.Add(new() { ["op"] = "bounds", ["name"] = h.Bounds });
        return ops;
    }
}
