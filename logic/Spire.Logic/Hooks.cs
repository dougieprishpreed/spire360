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

internal static class Hooks
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

        // CardSelectCmd.FromChooseABundleScreen(player, bundles): a bundle_select decision.
        Bridge.Register("cardselect.choose_bundle", a =>
        {
            var bundles = (IReadOnlyList<IReadOnlyList<CardModel>>)a[1]!;
            if (Driver != null)
                return Driver.BeginBundleSelection(bundles);
            return Task.FromResult<IEnumerable<CardModel>>(bundles.Count > 0 ? bundles[0] : Array.Empty<CardModel>());
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

    private static async Task NeutralizeSafe(CardModel card, PlayerChoiceContext ctx, CardPlay play)
    {
        try
        {
            await CreatureCmd.Damage(ctx, play.Target!, card.DynamicVars.Damage.BaseValue,
                MegaCrit.Sts2.Core.ValueProps.ValueProp.Move, card);
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
}
