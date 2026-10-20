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
            switch (call)
            {
                case "MegaAnimationState.SetAnimation":
                    ops.Add(new() { ["op"] = "set_animation", ["name"] = Str(0), ["loop"] = Bool(1, true), ["track"] = Int(2) });
                    break;
                case "MegaAnimationState.AddAnimation":
                case "MegaAnimationState.AddAnimationTracked":
                    ops.Add(new()
                    {
                        ["op"] = "add_animation", ["name"] = Str(0), ["delay"] = Float(1), ["loop"] = Bool(2, true), ["track"] = Int(3),
                    });
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

    private static Holder Get(Creature c)
    {
        if (_holders.TryGetValue(c, out var held) && ReferenceEquals(held.Combat, c.CombatState)) return held;
        var h = new Holder { Combat = c.CombatState };
        h.Init = Record(() =>
        {
            var sprite = (MegaSprite)RuntimeHelpers.GetUninitializedObject(typeof(MegaSprite));
            h.Animator = c.Player != null ? c.Player.Character.GenerateAnimator(sprite, c) : c.Monster?.GenerateAnimator(sprite);
        });
        _holders.AddOrUpdate(c, h);
        return h;
    }

    /// <summary>What the animator played when it was made (its first state).</summary>
    public static List<Dictionary<string, object?>> Init(Creature c) => Get(c).Init;

    /// <summary>NCreature.SetAnimationTrigger: what the trigger plays (null: no animator).</summary>
    public static List<Dictionary<string, object?>>? Trigger(Creature c, string trigger)
    {
        var h = Get(c);
        if (h.Animator == null) return null;
        var animator = h.Animator;
        return Record(() => animator.SetTrigger(trigger));
    }
}
