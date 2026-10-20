using System.Runtime.CompilerServices;
using MegaCrit.Sts2.Core.Entities.Creatures;
using MegaCrit.Sts2.Core.Models;

namespace Spire.Logic;

/// <summary>
/// What a monster's own code does to its spine sprite when its creature node
/// is made (NCreatureVisuals.SetUpSkin: MonsterModel.SetupSkins), so the
/// client can do the same: the skins it puts together, the animations it
/// sets on which tracks. The spine bindings are UI calls in the logic
/// (MegaSprite.NewSkin, MegaSkeletonDataResource.FindSkin, MegaSkin.AddSkin,
/// MegaSkeleton.SetSkin, MegaAnimationState.SetAnimation, ...); they are read
/// in order. Plus whether the monster swaps between "normal" and "phobia"
/// skins with phobia mode (MonsterModel.OnPhobiaModeToggled,
/// HasPhobiaSpineSkin).
/// </summary>
internal static class SpineRecorder
{
    private static readonly ConditionalWeakTable<Creature, List<Dictionary<string, object?>>> _ops = new();

    private static readonly System.Reflection.PropertyInfo? _hasPhobiaSkin = typeof(MonsterModel).GetProperty("HasPhobiaSpineSkin",
        System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Public);

    /// <summary>The creature's spine ops, recorded once (SetupSkins may pick at random).</summary>
    public static List<Dictionary<string, object?>> Ops(Creature c)
    {
        return _ops.GetValue(c, cr =>
        {
            var ops = new List<Dictionary<string, object?>>();
            if (cr.Monster == null) return ops;
            var found = new List<string>();  // FindSkin's names not yet added or set
            List<string>? custom = null;      // NewSkin's, as skins are added to it
            void Call(string call, List<object?> args)
            {
                string Str(int i) => i < args.Count ? args[i] as string ?? "" : "";
                bool Bool(int i, bool dflt) => i < args.Count && args[i] is bool b ? b : dflt;
                int Int(int i) => i < args.Count && args[i] is int n ? n : 0;
                float Float(int i) => i < args.Count ? Convert.ToSingle(args[i] is float or double or int ? args[i] : 0f) : 0f;
                // the receivers are null here (no UI): the args are the call's own
                switch (call)
                {
                    case "MegaSprite.NewSkin":
                        custom = new List<string>();
                        break;
                    case "MegaSkeletonDataResource.FindSkin":
                        found.Add(Str(0));
                        break;
                    case "MegaSkin.AddSkin":
                        if (custom != null && found.Count > 0) custom.Add(found[^1]);
                        if (found.Count > 0) found.RemoveAt(found.Count - 1);
                        break;
                    case "MegaSkeleton.SetSkin":
                        if (custom != null) ops.Add(new() { ["op"] = "skin", ["skins"] = custom });
                        else if (found.Count > 0) ops.Add(new() { ["op"] = "skin", ["skins"] = new List<string> { found[^1] } });
                        custom = null;
                        found.Clear();
                        break;
                    case "MegaSkeleton.SetSkinByName":
                        ops.Add(new() { ["op"] = "skin", ["skins"] = new List<string> { Str(0) } });
                        break;
                    case "MegaAnimationState.SetAnimation":
                        ops.Add(new() { ["op"] = "set_animation", ["name"] = Str(0), ["loop"] = Bool(1, true), ["track"] = Int(2) });
                        break;
                    case "MegaAnimationState.AddAnimation":
                        ops.Add(new()
                        {
                            ["op"] = "add_animation", ["name"] = Str(0), ["delay"] = Float(1), ["loop"] = Bool(2, true), ["track"] = Int(3),
                        });
                        break;
                    case "MegaAnimationState.AddEmptyAnimation":
                        ops.Add(new() { ["op"] = "add_empty_animation", ["track"] = Int(0) });
                        break;
                    case "MegaAnimationState.SetTimeScale":
                        ops.Add(new() { ["op"] = "time_scale", ["scale"] = Float(0) });
                        break;
                }
            }
            var before = Steps.Intercept;
            Steps.Intercept = Call;
            try
            {
                cr.Monster.SetupSkins(null!, null!);
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"[SIM] SetupSkins {cr.Monster.Id.Entry}: {ex.Message}");
            }
            finally
            {
                Steps.Intercept = before;
            }
            return ops;
        });
    }

    public static bool HasPhobiaSkin(Creature c)
    {
        try { return c.Monster != null && _hasPhobiaSkin?.GetValue(c.Monster) is true; }
        catch { return false; }
    }
}
