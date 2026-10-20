// Handlers for the IL patches in logic/patches.json.
//
// sts2-cli applied these as Harmony prefixes at run time (ported from its
// RunSimulator.cs, MIT, (c) 2025 Hao Wu). Harmony cannot run under NativeAOT,
// so Spire.Patcher rewrites the method bodies once on the player's PC to call
// Spire.HookBridge.Hooks.Invoke(id, args); the handlers are registered here.
// Patches that only need a constant result (Cmd.Wait, TalkCmd.Play, HasEntry,
// ...) need no handler at all.
using System.Reflection;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Commands;
using MegaCrit.Sts2.Core.Context;
using MegaCrit.Sts2.Core.Events;
using MegaCrit.Sts2.Core.Entities.Ascension;
using MegaCrit.Sts2.Core.Entities.Cards;
using MegaCrit.Sts2.Core.Entities.Creatures;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.GameActions;
using MegaCrit.Sts2.Core.Helpers;
using MegaCrit.Sts2.Core.Map;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Characters;
using MegaCrit.Sts2.Core.Multiplayer;
using MegaCrit.Sts2.Core.CardSelection;
using MegaCrit.Sts2.Core.Entities.CardRewardAlternatives;
using MegaCrit.Sts2.Core.Entities.Merchant;
using MegaCrit.Sts2.Core.Entities.RestSite;
using MegaCrit.Sts2.Core.Rewards;
using MegaCrit.Sts2.Core.Rooms;
using MegaCrit.Sts2.Core.Models.Powers;
using MegaCrit.Sts2.Core.GameActions.Multiplayer;
using MegaCrit.Sts2.Core.Runs;
using MegaCrit.Sts2.Core.TestSupport;
using MegaCrit.Sts2.Core.Localization;
using MegaCrit.Sts2.Core.Multiplayer.Serialization;
using MegaCrit.Sts2.Core.Saves;
using MegaCrit.Sts2.Core.Unlocks;

namespace Spire.Logic;

internal static partial class Hooks
{
    /// <summary>The active driver, for hooks that open decision points.</summary>
    internal static RunDriver? Driver;

    /// <summary>SPIRE_LOC_KEYS=1 restores sts2-cli's behaviour of returning keys for all raw text.</summary>
    private static readonly bool EchoKeys = Environment.GetEnvironmentVariable("SPIRE_LOC_KEYS") == "1";

    private static FieldInfo? _locTableName;

    private static string TableName(LocTable table)
    {
        _locTableName ??= typeof(LocTable).GetField("_name", BindingFlags.Instance | BindingFlags.NonPublic);
        return _locTableName?.GetValue(table) as string ?? "_unknown";
    }

    /// <summary>Registers every handler. Call before any game code runs.</summary>
    public static void Install()
    {
        InstallSound();
        // LocTable.GetRawText(string key): the table's text, or the key when missing
        // (the original throws for missing keys).
        Bridge.Register("loc.get_raw_text", a =>
        {
            var key = (string)a[1]!;
            if (EchoKeys) return key;
            var name = TableName((LocTable)a[0]!);
            if (LocSetup.RawByTable.TryGetValue(name, out var t) && t.TryGetValue(key, out var text))
                return text;
            return key;
        });

        // LocTable.GetLocString(string key): never throw for missing keys.
        Bridge.Register("loc.get_loc_string", a => new LocString(TableName((LocTable)a[0]!), (string)a[1]!));

        // LocTable.GetLocStringsWithPrefix(string prefix): empty list.
        Bridge.Register("loc.get_loc_strings_with_prefix", _ => new List<LocString>());

        // CardSelectCmd's entry points: which screen the pending card_select
        // stands for, and its prompt.
        foreach (var kind in new[] { "choose_a_card", "simple_grid", "combat_pile", "upgrade", "transform", "enchant",
                                     "remove", "deck", "hand" })
        {
            var k = kind;
            Spire.HookBridge.Hooks.RegisterAfter("cardselect.kind." + k, (a, r) => Driver?.AnnotateCardSelection(k, a, r));
        }

        // VfxCmd: the effects the game would spawn (it returns at once in
        // test mode); CreatureCmd.TriggerAnim: the creatures' animations.
        Spire.HookBridge.Hooks.RegisterAfter("orb.passive", (a, r) =>
        {
            if (a.Length > 0 && a[0] is OrbModel orb) Steps.OrbCue(orb, false, null);
        });
        Spire.HookBridge.Hooks.RegisterAfter("orb.evoke", (a, r) =>
        {
            if (a.Length > 1 && a[0] is OrbModel orb && a[1] is Creature[] targets)
                Steps.OrbCue(orb, true, targets);
        });
        Spire.HookBridge.Hooks.RegisterAfter("vfx.on_creature", (a, r) => VfxCue(a, "feet"));
        Spire.HookBridge.Hooks.RegisterAfter("vfx.on_creature_center", (a, r) => VfxCue(a, "center"));
        Spire.HookBridge.Hooks.RegisterAfter("vfx.on_side", (a, r) =>
        {
            if (a.Length > 1 && a[0] is CombatSide side && a[1] is string path)
                Steps.Cue(new Dictionary<string, object?> { ["t"] = "vfx", ["path"] = path, ["side"] = side == CombatSide.Player ? "player" : "enemy" });
        });
        Spire.HookBridge.Hooks.RegisterAfter("vfx.full_screen", (a, r) =>
        {
            if (a.Length > 0 && a[0] is string path)
                Steps.Cue(new Dictionary<string, object?> { ["t"] = "vfx", ["path"] = path, ["at"] = "screen" });
        });
        Spire.HookBridge.Hooks.RegisterAfter("anim.trigger", (a, r) =>
        {
            if (a.Length > 1 && a[0] is Creature c && a[1] is string name && !c.IsDead)
                Steps.Cue(new Dictionary<string, object?>
                {
                    ["t"] = "anim", ["who"] = Steps.Who(c), ["name"] = name,
                    ["wait"] = a.Length > 2 && a[2] is float w ? w : 0f,
                });
        });

        // TestMode at the rules that depend on it (testmode.game_flow.*): off
        // for runs that follow the game's own flow, the real value otherwise.
        Bridge.Register("testmode.game_flow.is_off", _ => Driver?.GameFlow == true || TestMode.IsOff);
        Bridge.Register("testmode.game_flow.is_on", _ => Driver?.GameFlow != true && TestMode.IsOn);
        // RunManager.OnEnded(isVictory): how the run ended.
        Spire.HookBridge.Hooks.RegisterAfter("run.ended", (a, r) =>
        {
            if (a.Length > 1 && a[1] is bool victory) Driver?.OnRunEnded(victory, r);
        });

        // CardSelectCmd.FromChooseABundleScreen(player, bundles): a bundle_select decision.
        Bridge.Register("cardselect.choose_bundle", a =>
        {
            var bundles = (IReadOnlyList<IReadOnlyList<CardModel>>)a[1]!;
            if (Driver != null)
                return Driver.BeginBundleSelection(bundles);
            return Task.FromResult<IEnumerable<CardModel>>(bundles.Count > 0 ? bundles[0] : Array.Empty<CardModel>());
        });

        // Cmd.Wait(seconds...) and every Task.Delay(...) in game code: finish at
        // once, record the pacing for the presentation.
        Bridge.Register("cmd.wait", a =>
        {
            Steps.Wait(a.Length > 0 ? WaitMs(a[0]) : 0, "cmd");
            return Task.CompletedTask;
        });
        Bridge.Register("task.delay", a =>
        {
            Steps.Wait(a.Length > 0 ? WaitMs(a[0]) : 0, "delay");
            return Task.CompletedTask;
        });

        // Every call from game logic into the UI layer (redirect_calls_into "ui"):
        // nothing runs, the call is recorded as a presentation cue. The patched
        // call site supplies a safe default return value itself.
        Spire.HookBridge.Hooks.RegisterPrefix("ui", (id, a) =>
        {
            Steps.Ui(id, a);
            Driver?.OnUiCall(id, a);
            return null;
        });

        // Neutralize.OnPlay(choiceContext, cardPlay): sts2-cli's safe re-implementation
        // (the original NREs inside DamageCmd.Attack().Execute() without the UI).
        // Disabled in patches.json by default; enable only if that NRE reproduces.
        Bridge.Register("neutralize.on_play", a =>
        {
            var card = (CardModel)a[0]!;
            var ctx = (PlayerChoiceContext)a[1]!;
            var play = (CardPlay)a[2]!;
            if (play.Target == null) return Task.CompletedTask;
            return NeutralizeSafe(card, ctx, play);
        });
    }

    private static int WaitMs(object? arg) => arg switch
    {
        float f => (int)(f * 1000),
        double d => (int)(d * 1000),
        int i => i,
        long l => (int)l,
        TimeSpan t => (int)t.TotalMilliseconds,
        _ => 0,
    };

    private static async Task NeutralizeSafe(CardModel card, PlayerChoiceContext ctx, CardPlay play)
    {
        try
        {
            await CreatureCmd.Damage(ctx, play.Target!, card.DynamicVars.Damage, card, play);
            await PowerCmd.Apply<WeakPower>(ctx, play.Target!, card.DynamicVars["WeakPower"].BaseValue,
                card.Owner.Creature, card, false);
        }
        catch (Exception ex)
        {
            Log.Warn($"Neutralize safe: {ex.Message}");
        }
    }

    /// <summary>Short alias so call sites read like the patch ids.</summary>
    private static class Bridge
    {
        public static void Register(string id, Func<object?[], object?> handler) =>
            Spire.HookBridge.Hooks.Register(id, handler);
    }

    /// <summary>VfxCmd.PlayOnCreature / PlayOnCreatureCenter(target, path): at the
    /// creature's feet (its position) or its centre (VfxSpawnPosition); none
    /// on the dead, as the game.</summary>
    private static void VfxCue(object?[] a, string at)
    {
        if (a.Length > 1 && a[0] is Creature c && a[1] is string path && !c.IsDead)
            Steps.Cue(new Dictionary<string, object?> { ["t"] = "vfx", ["path"] = path, ["who"] = Steps.Who(c), ["at"] = at });
    }
}
