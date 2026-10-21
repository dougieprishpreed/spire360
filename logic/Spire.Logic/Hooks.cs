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
                                     "remove", "deck", "hand", "hand_upgrade" })
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
        Spire.HookBridge.Hooks.RegisterAfter("vfx.on_creature_center", (a, r) =>
        {
            // The Architect's hit: its damage number and spark (see architect.parts)
            if (_architectParts.Count > 0 && a.Length > 0 && a[0] is Creature hit)
            {
                Steps.Cue(new Dictionary<string, object?> { ["t"] = "ui", ["call"] = "NDamageNumVfx.Create",
                                                            ["args"] = new List<object?> { Steps.Who(hit), _architectParts.Dequeue(), false } });
                Steps.Cue(new Dictionary<string, object?> { ["t"] = "ui", ["call"] = "NHitSparkVfx.Create",
                                                            ["args"] = new List<object?> { Steps.Who(hit), false } });
            }
            VfxCue(a, "center");
        });
        Spire.HookBridge.Hooks.RegisterAfter("architect.parts", (a, r) =>
        {
            _architectParts.Clear();
            if (r is int[] parts) foreach (var p in parts) _architectParts.Enqueue(p);
        });
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
        // RelicModel.Flash (Player.OnRelicFlashed: this, relic, targets) and
        // PowerModel.Flash (this): the flashes the UI would show.
        Spire.HookBridge.Hooks.RegisterAfter("relic.flashed", (a, r) =>
        {
            if (a.Length > 2 && a[0] is Player p && a[1] is RelicModel relic)
            {
                var who = new List<string>();
                if (a[2] is IEnumerable<Creature> targets)
                    foreach (var t in targets) who.Add(Steps.Who(t));
                Steps.Cue(new Dictionary<string, object?>
                {
                    ["t"] = "relic_flash", ["relic"] = relic.Id.Entry, ["index"] = p.Relics.ToList().IndexOf(relic),
                    ["targets"] = who,
                });
            }
        });
        Spire.HookBridge.Hooks.RegisterAfter("power.flashed", (a, r) =>
        {
            if (a.Length > 0 && a[0] is PowerModel pw && pw.Owner != null && pw.ShouldPlayVfx)
                Steps.Cue(new Dictionary<string, object?> { ["t"] = "power_flash", ["power"] = pw.Id.Entry, ["who"] = Steps.Who(pw.Owner) });
        });
        // Deck changes outside combat: what NRun.GlobalUi's preview containers
        // show (TestMode returns before the UI, so they are recorded here).
        // CardCmd.Preview(card or cards, time, style)
        Spire.HookBridge.Hooks.RegisterBefore("deck.preview", a =>
        {
            if (a.Length < 3 || a[1] is not float time || a[2] is not MegaCrit.Sts2.Core.Nodes.CommonUi.CardPreviewStyle style) return;
            if (a[0] is CardModel card) PreviewCue("show", card, style, time);
            else if (a[0] is IReadOnlyList<CardModel> cards)
                foreach (var c in cards) PreviewCue("show", c, style, time);
        });
        // CardCmd.PreviewCardPileAdd(result or results, time, style): the successful ones
        Spire.HookBridge.Hooks.RegisterBefore("deck.preview_add", a =>
        {
            if (a.Length < 3 || a[1] is not float time || a[2] is not MegaCrit.Sts2.Core.Nodes.CommonUi.CardPreviewStyle style) return;
            if (a[0] is CardPileAddResult r) { if (r.success) PreviewCue("add", r.cardAdded, style, time); }
            else if (a[0] is IReadOnlyList<CardPileAddResult> results)
                foreach (var x in results)
                    if (x.success) PreviewCue("add", x.cardAdded, style, time);
        });
        Spire.HookBridge.Hooks.RegisterBefore("deck.remove", a =>
        {
            // CardPileCmd.RemoveFromDeck(cards, showPreview)
            if (a.Length > 1 && a[0] is IReadOnlyList<CardModel> cards && a[1] is true)
                foreach (var c in cards)
                    if (c.Pile?.Type == PileType.Deck) DeckCue("remove", c, null, null, 0);
        });
        Spire.HookBridge.Hooks.RegisterBefore("deck.upgrade.begin", a =>
            _upgradeStyle = a.Length > 1 ? a[1] as MegaCrit.Sts2.Core.Nodes.CommonUi.CardPreviewStyle? : null);
        Spire.HookBridge.Hooks.RegisterAfter("deck.upgrade.end", (a, r) => _upgradeStyle = null);
        Spire.HookBridge.Hooks.RegisterAfter("deck.upgraded", (a, r) =>
        {
            if (a.Length > 0 && a[0] is CardModel c && c.Pile?.Type == PileType.Deck && _upgradeStyle is { } s
                && s != MegaCrit.Sts2.Core.Nodes.CommonUi.CardPreviewStyle.None)
                DeckCue("upgrade", c, null, s, 0);
        });
        Spire.HookBridge.Hooks.RegisterBefore("deck.transform.begin", a =>
        {
            _transformStyle = a.Length > 2 ? a[2] as MegaCrit.Sts2.Core.Nodes.CommonUi.CardPreviewStyle? : null;
            _transformedFrom = null;
        });
        Spire.HookBridge.Hooks.RegisterAfter("deck.transformed_from", (a, r) => _transformedFrom = a.Length > 0 ? a[0] as CardModel : null);
        Spire.HookBridge.Hooks.RegisterAfter("deck.transformed_to", (a, r) =>
        {
            if (a.Length > 0 && a[0] is CardModel c && c.Pile?.Type == PileType.Deck && _transformStyle is { } s
                && s != MegaCrit.Sts2.Core.Nodes.CommonUi.CardPreviewStyle.None)
                DeckCue("transform", c, _transformedFrom, s, 0);
            _transformedFrom = null;
        });
        // TheArchitect: OnRoomEnter looks for the Architect among the room's
        // creature nodes; headless the layout combat's enemy stands in, and
        // its head animations (track 1) become anim cues.
        Spire.HookBridge.Hooks.RegisterBefore("architect.line", a =>
        {
            if (a.Length == 0 || a[0] is not EventModel ev) return;
            var prop = ev.GetType().GetProperty("ArchitectCreature", BindingFlags.Instance | BindingFlags.NonPublic);
            if (prop == null || prop.GetValue(ev) != null) return;
            var sync = typeof(EventModel).GetField("_combatSynchronizer", BindingFlags.Instance | BindingFlags.NonPublic)?.GetValue(ev)
                as MegaCrit.Sts2.Core.Multiplayer.Game.EventCombatSynchronizer;
            var architect = sync?.CombatStateForLayout?.Enemies.FirstOrDefault();
            if (architect == null) return;
            prop.SetValue(ev, architect);
            // OnRoomEnter's SetAnimation("_tracks/head_reading", loop, 1)
            HeadCue(architect, new Dictionary<string, object?> { ["op"] = "set_animation", ["name"] = "_tracks/head_reading", ["loop"] = true, ["track"] = 1 });
        });
        Spire.HookBridge.Hooks.RegisterAfter("architect.head", (a, r) =>
        {
            // the first line: head_stop_reading, 0.5 s later head_normal (looping)
            var prop = a.Length > 0 && a[0] != null ? a[0]!.GetType().GetProperty("ArchitectCreature", BindingFlags.Instance | BindingFlags.NonPublic) : null;
            if (prop?.GetValue(a[0]) is Creature architect)
                HeadCue(architect,
                    new Dictionary<string, object?> { ["op"] = "set_animation", ["name"] = "_tracks/head_stop_reading", ["loop"] = false, ["track"] = 1 },
                    new Dictionary<string, object?> { ["op"] = "add_animation", ["name"] = "_tracks/head_normal", ["loop"] = true, ["delay"] = 0.5, ["track"] = 1 });
        });
        // Trial.Accept's new portrait: cleared, the accused's VFX, trial_started.
        Spire.HookBridge.Hooks.RegisterBefore("event.trial_portrait", a =>
        {
            if (a.Length < 2 || a[0] == null || a[1] is not string vfx) return;
            var image = a[0]!.GetType().GetProperty("TrialStartedPath", BindingFlags.Static | BindingFlags.NonPublic)?.GetValue(null) as string;
            static string Res(string p) => p.StartsWith("res://") ? p.Substring(6) : p;
            Steps.Cue(new Dictionary<string, object?>
            {
                ["t"] = "event_portrait", ["vfx"] = Res(vfx), ["at"] = new List<object?> { 292, 68 },
                ["image"] = image != null ? Res(image) : null,
            });
        });
        // The events' room effects added in OnRoomEnter (see patches.json).
        Spire.HookBridge.Hooks.RegisterAfter("event.room_vfx.mirror", (a, r) =>
            Steps.Cue(new Dictionary<string, object?> { ["t"] = "ui", ["call"] = "NMirrorVfx.Create", ["args"] = new List<object?>() }));
        Spire.HookBridge.Hooks.RegisterAfter("event.room_vfx.rain", (a, r) =>
            Steps.Cue(new Dictionary<string, object?> { ["t"] = "ui", ["call"] = "NRainVfx.Create", ["args"] = new List<object?>() }));
        Spire.HookBridge.Hooks.RegisterAfter("anim.trigger", (a, r) =>
        {
            if (a.Length > 1 && a[0] is Creature c && a[1] is string name && !c.IsDead)
                Steps.Cue(new Dictionary<string, object?>
                {
                    ["t"] = "anim", ["who"] = Steps.Who(c), ["name"] = name,
                    ["wait"] = a.Length > 2 && a[2] is float w ? w : 0f,
                    // what the creature's CreatureAnimator plays for it
                    ["play"] = Animators.Trigger(c, name),
                });
        });

        // TestMode at the rules that depend on it (testmode.game_flow.*): off
        // for runs that follow the game's own flow, the real value otherwise.
        Bridge.Register("testmode.game_flow.is_off", _ => Driver?.GameFlow == true || TestMode.IsOff);
        Bridge.Register("testmode.game_flow.is_on", _ => Driver?.GameFlow != true && TestMode.IsOn);
        // Presentation-only sites (testmode.presentation.is_off): always off.
        Bridge.Register("testmode.presentation.is_off", _ => true);
        Bridge.Register("testmode.presentation.is_on", _ => false);
        // A move's own PlayFullScreenInCombat: the scene at the screen's centre
        Bridge.Register("vfx.scene_at_screen", a =>
        {
            var path = a.OfType<string>().FirstOrDefault();
            if (path != null)
            {
                const string root = "res://scenes/";
                if (path.StartsWith(root)) path = path.Substring(root.Length);
                if (path.EndsWith(".tscn")) path = path.Substring(0, path.Length - 5);
                Steps.Cue(new Dictionary<string, object?> { ["t"] = "vfx", ["path"] = path, ["at"] = "screen" });
            }
            return null;
        });
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
            // The events' NCardEnchantVfx.Create(card): the enchanted card's view.
            if (id.EndsWith("NCardEnchantVfx::Create", StringComparison.Ordinal) && a.Length > 0 && a[0] is CardModel enchanted)
                DeckCue("enchant", enchanted, null, null, 0);
            Driver?.OnUiCall(id, a);
            return StandIns.Answer(id, a);
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

    private static readonly Queue<int> _architectParts = new();

    private static void HeadCue(Creature who, params Dictionary<string, object?>[] ops) =>
        Steps.Cue(new Dictionary<string, object?> { ["t"] = "anim", ["who"] = Steps.Who(who), ["name"] = "Head", ["wait"] = 0, ["play"] = ops.ToList() });

    private static MegaCrit.Sts2.Core.Nodes.CommonUi.CardPreviewStyle? _upgradeStyle, _transformStyle;

    /// <summary>CardCmd.PreviewInternal's checks: a card in a pile, not while the
    /// combat ends; the combat piles' previews are the fight's.</summary>
    private static void PreviewCue(string op, CardModel card, MegaCrit.Sts2.Core.Nodes.CommonUi.CardPreviewStyle style, float time)
    {
        if (card.Pile == null || card.Pile.Type.IsCombatPile() || CombatManager.Instance.IsEnding) return;
        DeckCue(op, card, null, style, time);
    }
    private static CardModel? _transformedFrom;

    /// <summary>A deck_card cue: op (add, show, remove, upgrade, transform,
    /// enchant), the card's view, the original's for a transform, the preview
    /// style and time when the game gives them.</summary>
    private static void DeckCue(string op, CardModel card, CardModel? from, MegaCrit.Sts2.Core.Nodes.CommonUi.CardPreviewStyle? style, float time)
    {
        if (Driver == null) return;
        var cue = new Dictionary<string, object?> { ["t"] = "deck_card", ["op"] = op, ["card"] = Driver.DeckCardView(card) };
        if (from != null) cue["from"] = Driver.DeckCardView(from);
        if (style != null) cue["style"] = style.ToString();
        if (time > 0) cue["time"] = time;
        Steps.Cue(cue);
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
