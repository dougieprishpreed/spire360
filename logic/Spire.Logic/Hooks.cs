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
using MegaCrit.Sts2.Core.Entities.Potions;
using MegaCrit.Sts2.Core.Entities.Powers;
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
using MegaCrit.Sts2.Core.Settings;
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
        InstallPresentation();
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

        // MapPathPruning.FindMatchingSegments(start): the same segments, faster
        // (the act change's map generation; MapPruning.cs).
        Bridge.Register("map.find_matching_segments", a => MapPruning.FindMatchingSegments((MapPoint)a[0]!));

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
        // RelicModel.Flash(targets) (this, targets) and PowerModel.Flash (this):
        // the flashes the UI would show. The top bar's holder flashes for every
        // relic; the targets' NRelicFlashVfx only through Player.OnRelicFlashed,
        // which a relic with ShouldFlashOnPlayer false (Maw Bank) or a melted one
        // is not subscribed to.
        Spire.HookBridge.Hooks.RegisterAfter("relic.flash", (a, r) =>
        {
            if (a.Length > 1 && a[0] is RelicModel relic && relic.Owner is Player p)
            {
                var who = new List<string>();
                if (relic.ShouldFlashOnPlayer && !relic.IsMelted && a[1] is IEnumerable<Creature> targets)
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
        // Creature.InvokePowerModified(power, change, silent): PowerIncreased
        // (a rise, or a fall of a counter that may go negative), which
        // NCreature.OnPowerIncreased shows unless silent: NPowerAppliedVfx
        // with the buff or debuff burst and the buff / debuff sound when the
        // power ShouldPlayVfx, by the type at the new amount; a debuff shakes
        // the creature (AnimShake) either way.
        Spire.HookBridge.Hooks.RegisterBefore("power.modified", a =>
        {
            if (a.Length < 4 || a[1] is not PowerModel pw || a[2] is not int change || a[3] is not bool silent) return;
            if (silent || pw.Owner == null || !Steps.InCombat) return;
            bool rise = change > 0 || (pw.StackType == PowerStackType.Counter && pw.AllowNegative && change < 0);
            if (!rise) return;
            string id = pw.Id.Entry.ToLowerInvariant();
            Steps.Cue(new Dictionary<string, object?>
            {
                ["t"] = "power_vfx", ["power"] = pw.Id.Entry, ["who"] = Steps.Who(pw.Owner),
                ["title"] = SafeTitle(pw),
                ["icon"] = "images/powers/" + id + ".png", ["beta_icon"] = "images/powers/beta/" + id + ".png",
                ["buff"] = pw.GetTypeForAmount(pw.Amount) == PowerType.Buff,
                ["vfx"] = pw.ShouldPlayVfx,
                // SfxCmd.Play stays quiet while the combat ends
                ["sound"] = pw.ShouldPlayVfx && !CombatManager.Instance.IsEnding,
            });
        });
        // VfxCmd.PlayNonCombatVfx(container, position, path): the event's effect
        Bridge.Register("vfx.non_combat", a =>
        {
            if (a.Length > 2 && a[1] is Godot.Vector2 at && a[2] is string path)
                Steps.Cue(new Dictionary<string, object?>
                {
                    ["t"] = "ui", ["call"] = "VfxCmd.PlayNonCombatVfx", ["args"] = new List<object?> { path, at.X, at.Y },
                });
            return new Godot.Node2D();
        });
        // RunSaveManager.SaveRun (its Saved): NSaveIndicator.SavedGame. A
        // deferred save made later does not show it again.
        Spire.HookBridge.Hooks.RegisterBefore("save.indicator", a =>
        {
            if (!Deferred.Running) Steps.Cue(new Dictionary<string, object?> { ["t"] = "saved" });
        });
        // The saves after the response (Deferred): the progress file, then
        // the run's snapshot as the call took it
        Spire.HookBridge.Hooks.RegisterDefer("save.defer.progress", a =>
            a[0] is MegaCrit.Sts2.Core.Saves.Managers.ProgressSaveManager m && Deferred.Offer("progress", m.SaveProgress));
        Spire.HookBridge.Hooks.RegisterDefer("save.defer.run", a =>
            a[0] is MegaCrit.Sts2.Core.Saves.Managers.RunSaveManager m && a[1] is MegaCrit.Sts2.Core.Saves.SerializableRun save &&
            a[2] is bool mp && Deferred.Offer(mp ? "run.multiplayer" : "run", () => Deferred.Finish(m.SaveRun(save, mp), "run save")));
        // ForgeCmd.PreviewSovereignBlade: the blades in the hand hammered
        // where they are, the others shown and sent back to their piles
        Spire.HookBridge.Hooks.RegisterBefore("forge.preview", a =>
        {
            if (a.Length < 1 || a[0] is not IEnumerable<CardModel> blades || Driver == null) return;
            var list = blades.ToList();
            var hand = list.Where(c => c.Pile?.Type == PileType.Hand).Select(c => (object?)Steps.IdOf(c)).ToList();
            var others = list.Where(c => c.Pile != null && c.Pile.Type != PileType.Hand).Select(c => (object?)new Dictionary<string, object?>
            {
                ["card"] = Driver.DeckCardView(c),
                ["pile"] = c.Pile!.Type.ToString().ToLowerInvariant(),
            }).ToList();
            Steps.Cue(new Dictionary<string, object?> { ["t"] = "forge_preview", ["hand"] = hand, ["others"] = others });
        });
        // FoulPotion.OnUse out of combat: thrown at the merchant (the shop's
        // or the Fake Merchant)
        Spire.HookBridge.Hooks.RegisterBefore("foul_potion.on_use", a =>
        {
            if (CombatManager.Instance.IsInProgress) return;
            var room = MegaCrit.Sts2.Core.Runs.RunManager.Instance.DebugOnlyGetState()?.CurrentRoom;
            string? where = room is MegaCrit.Sts2.Core.Rooms.MerchantRoom ? "merchant"
                : room is MegaCrit.Sts2.Core.Rooms.EventRoom er && er.CanonicalEvent is MegaCrit.Sts2.Core.Models.Events.FakeMerchant ? "fake_merchant"
                : null;
            if (where != null) Steps.Cue(new Dictionary<string, object?> { ["t"] = "foul_potion", ["where"] = where });
        });
        // LordsParasol.PurchaseEverything: the shop as entered, before it is
        // bought out (the decision after it shows every slot sold)
        Spire.HookBridge.Hooks.RegisterBefore("parasol.purchase_everything", a =>
        {
            var view = Driver?.ShopView();
            if (view != null) Steps.Cue(new Dictionary<string, object?> { ["t"] = "shop_entry", ["shop"] = view });
        });
        // MerchantEntry.InvokePurchaseCompleted: NMerchantInventory's slot
        // (OnPurchaseCompleted: the card to the deck, a relic or potion to
        // the top bar, the thank-yous). auto: the shop bought it by itself
        // (Lord's Parasol), not a buy command the client made
        Spire.HookBridge.Hooks.RegisterBefore("merchant.purchase_completed", a =>
        {
            if (a.Length < 1 || a[0] is not MegaCrit.Sts2.Core.Entities.Merchant.MerchantEntry e) return;
            bool auto = !Steps.PlayerPurchases.Remove(e);
            var inv = (MegaCrit.Sts2.Core.Runs.RunManager.Instance.DebugOnlyGetState()?.CurrentRoom as MegaCrit.Sts2.Core.Rooms.MerchantRoom)?.GetLocalInventory();
            if (inv == null) return;
            string? kind = null;
            int index = -1;
            switch (e)
            {
                case MegaCrit.Sts2.Core.Entities.Merchant.MerchantCardEntry c:
                    kind = "card";
                    index = inv.CharacterCardEntries.Concat(inv.ColorlessCardEntries).ToList().IndexOf(c);
                    break;
                case MegaCrit.Sts2.Core.Entities.Merchant.MerchantRelicEntry r:
                    kind = "relic";
                    index = inv.RelicEntries.ToList().IndexOf(r);
                    break;
                case MegaCrit.Sts2.Core.Entities.Merchant.MerchantPotionEntry p:
                    kind = "potion";
                    index = inv.PotionEntries.ToList().IndexOf(p);
                    break;
                default:
                    if (ReferenceEquals(e, inv.CardRemovalEntry)) kind = "removal";
                    index = 0;
                    break;
            }
            if (kind != null && index >= 0)
                Steps.Cue(new Dictionary<string, object?> { ["t"] = "merchant_purchase", ["kind"] = kind, ["index"] = index, ["auto"] = auto });
        });
        Spire.HookBridge.Hooks.RegisterBefore("fiend_fire.on_play", a =>
        {
            _fiendFire = a.Length > 0 ? a[0] as CardModel : null;
            _fiendFireScale = 0.8f;
        });
        // Player.AddPotionInternal's AddPotionFailed: NPotionContainer.PlayAddFailedAnim
        Spire.HookBridge.Hooks.RegisterAfter("potion.add_failed", (a, r) =>
        {
            bool silent = a.Length > 3 && a[3] is bool s && s;
            if (!silent && r is PotionProcureResult res && !res.success && res.failureReason == PotionProcureFailureReason.TooFull)
                Steps.Cue(new Dictionary<string, object?> { ["t"] = "potion_full" });
        });
        // PowerModel.StartPulsing / StopPulsing: NPower's pulse (the icon's shader)
        foreach (var (hook, on) in new[] { ("power.pulse_start", true), ("power.pulse_stop", false) })
            Spire.HookBridge.Hooks.RegisterAfter(hook, (a, _) =>
            {
                if (a.Length > 0 && a[0] is PowerModel pw && pw.Owner != null)
                    Steps.Cue(new Dictionary<string, object?> { ["t"] = "power_pulse", ["who"] = Steps.Who(pw.Owner), ["power"] = pw.Id.Entry, ["on"] = on });
            });
        // Creature.RemovePowerInternal: the PowerRemoved event NCreature.OnPowerRemoved
        // answers with NPowerRemovedVfx ("wears off", ShouldPlayVfx) where the
        // creature's node still is.
        Spire.HookBridge.Hooks.RegisterBefore("power.removed", a =>
        {
            if (a.Length < 2 || a[1] is not PowerModel pw || pw.Owner == null || !Steps.InCombat) return;
            if (!pw.ShouldPlayVfx || pw.Owner.IsDead) return;
            string id = pw.Id.Entry.ToLowerInvariant();
            Steps.CueAsIs(new Dictionary<string, object?>
            {
                ["t"] = "power_removed", ["power"] = pw.Id.Entry, ["who"] = Steps.Who(pw.Owner), ["title"] = SafeTitle(pw),
                ["icon"] = "images/powers/" + id + ".png", ["beta_icon"] = "images/powers/beta/" + id + ".png",
            });
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
            if (a[0] is CardPileAddResult r) { if (r.success) PreviewCue("add", r.cardAdded, style, time, r.modifyingModels); }
            else if (a[0] is IReadOnlyList<CardPileAddResult> results)
                foreach (var x in results)
                    if (x.success) PreviewCue("add", x.cardAdded, style, time, x.modifyingModels);
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
        // The Sovereign Blade's node (see patches.json blade.*): which blades
        // exist is kept here, per combat.
        Bridge.Register("blade.forge_vfx", a =>
        {
            if (a.Length < 2 || a[0] is not Player player || a[1] is not CardModel card) return null;
            var combat = player.Creature?.CombatState;
            if (combat == null) return null;
            if (!ReferenceEquals(combat, _bladesCombat))
            {
                _blades.Clear();
                _bladesCombat = combat;
            }
            var original = card.DupeOf ?? card;
            bool isNew = _blades.Add(original);
            SfxCmd.Play(isNew ? "event:/sfx/characters/regent/regent_forge" : "event:/sfx/characters/regent/regent_refine");
            BladeCue("forge", original, "damage", card.DynamicVars.Damage.IntValue, "flames", isNew);
            if (isNew)
            {
                // the blades share the orbit evenly
                var blades = player.PlayerCombatState!.AllCards
                    .Where(c => !c.IsDupe && (c.Pile == null || c.Pile.Type != PileType.Exhaust))
                    .OfType<MegaCrit.Sts2.Core.Models.Cards.SovereignBlade>().ToList();
                for (int i = 0; i < blades.Count; i++)
                    if (_blades.Contains(blades[i])) BladeCue("orbit", blades[i], "progress", (float)i / blades.Count);
            }
            return null;
        });
        Spire.HookBridge.Hooks.RegisterBefore("blade.play", a =>
            _bladeTarget = a.Length > 2 && a[2] is CardPlay play ? play.Target : null);
        Bridge.Register("blade.attack_node", a =>
        {
            if (a.Length > 1 && a[1] is CardModel card && _blades.Contains(card.DupeOf ?? card))
            {
                // OnPlay: Seeking Edge aims at the first hittable enemy
                bool seeking = card.GetType().GetProperty("HasSeekingEdge", BindingFlags.Instance | BindingFlags.NonPublic)?.GetValue(card) is true;
                var target = seeking ? card.CombatState?.HittableEnemies.FirstOrDefault() : _bladeTarget;
                if (target != null) BladeCue("attack", card.DupeOf ?? card, "target", Steps.Who(target));
            }
            return null;
        });
        Bridge.Register("blade.remove_node", a =>
        {
            if (a.Length > 1 && a[1] is CardModel card && _blades.Remove(card.DupeOf ?? card))
                BladeCue("remove", card.DupeOf ?? card);
            return null;
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
        // CreatureCmd.TriggerAnim(creature, trigger, waitTime): the trigger as
        // it starts; its pacing is the CustomScaledWait it ends with (a "wait"
        // step after this one), so the cue itself waits for nothing.
        Spire.HookBridge.Hooks.RegisterBefore("anim.trigger", a =>
        {
            if (a.Length > 1 && a[0] is Creature c && a[1] is string name && !c.IsDead)
                Steps.Cue(new Dictionary<string, object?>
                {
                    ["t"] = "anim", ["who"] = Steps.Who(c), ["name"] = name,
                    ["wait"] = 0f,
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
        // Cmd.CustomScaledWait(fast, standard, ignoreCombatEnd, ct): skipped
        // headless (NonInteractiveMode under TestMode), so its pacing (0.8 s
        // after each turn banner, ...) is recorded here as the game would wait.
        Bridge.Register("cmd.custom_scaled_wait", a =>
        {
            ScaledWait(a.Length > 0 ? a[0] : null, a.Length > 1 ? a[1] : null, a.Length > 2 && a[2] is bool b && b);
            return Task.CompletedTask;
        });

        // The pacing the game takes from its UI, which TestMode leaves out.
        // CardModel.OnPlayWrapper's minimum play time (CustomScaledWait(0.15 -
        // elapsed, 0.3 - elapsed)) measures Time.GetTicksMsec: the waits
        // recorded since the play started.
        Bridge.Register("time.play_clock", _ => (ulong)Steps.ClockMs);
        // CardPileCmd.Add awaits its cards' tween (GetTweenForCardsChangingPiles,
        // null in TestMode): the time it would take.
        Bridge.Register("card.pile_tween", a =>
        {
            if (a.Length > 0 && a[0] is IEnumerable<CardPileAddResult> results)
            {
                PileCues(results);
                Steps.Wait(CardTweenMs(results), "card");
            }
            return ((Godot.Tween?)null, false);
        });
        // CardPileCmd.Shuffle: each card of the discard pile flies to the draw
        // pile (NCardFlyShuffleVfx) min(0.045, 0.8 / n) s after the one before.
        Spire.HookBridge.Hooks.RegisterBefore("card.shuffle", a =>
        {
            _shuffleStepMs = 0;
            if (a.Length > 1 && a[1] is Player p && !CombatManager.Instance.IsOverOrEnding)
            {
                int n = PileType.Discard.GetPile(p).Cards.Count;
                if (n > 0) _shuffleStepMs = (int)(Math.Min(0.045f, 0.8f / n) * 1000);
            }
        });
        Bridge.Register("card.shuffle_tween", _ =>
        {
            Steps.Wait(_shuffleStepMs, "shuffle");
            return ((Godot.Tween?)null, false);
        });
        // CardPileCmd.AddDuringManualCardPlay: a power card waits for its move
        // to the play pile (the 0.25 s scale to 0.8) before it flies.
        Bridge.Register("card.manual_play_tween", a =>
        {
            if (a.Length > 2 && a[2] is CardModel card && card.Type == CardType.Power) Steps.Wait(250, "card");
            return MegaCrit.Sts2.Core.Hooks.Hook.AfterCardChangedPiles((IRunState)a[0]!, (ICombatState?)a[1], (CardModel)a[2]!,
                (PileType)a[3]!, (AbstractModel?)a[4]);
        });
        // CardPileCmd.RemoveFromCombat(cards, skipVisuals): a card in the hand or
        // the play pile (not a power played there) burns away where it is
        // (NCardExhaustVfx); FindOnTable finds no other.
        Spire.HookBridge.Hooks.RegisterBefore("card.remove_from_combat", a =>
        {
            if (a.Length < 2 || a[0] is not IEnumerable<CardModel> cards || a[1] is true) return;
            if (!CombatManager.Instance.IsInProgress) return;
            foreach (var c in cards.ToList())
            {
                var pile = c.Pile?.Type;
                if (pile == PileType.Hand || (pile == PileType.Play && c.Type != CardType.Power))
                    Steps.Cue(new Dictionary<string, object?> { ["t"] = "card_burn", ["card"] = Steps.IdOf(c) });
            }
        });
        // Creature.SetNodeVisible(visible): the creature's node shown or
        // hidden (InfestedPower's Wrigglers until the elite's death
        // animation is over, ...).
        Spire.HookBridge.Hooks.RegisterBefore("creature.node_visible", a =>
        {
            if (a.Length < 2 || a[0] is not Creature c || a[1] is not bool visible) return;
            if (!CombatManager.Instance.IsInProgress) return;
            Steps.Cue(new Dictionary<string, object?> { ["t"] = "node_visible", ["who"] = Steps.Who(c), ["visible"] = visible });
        });
        // Presentation tasks the game runs alongside the turn (patches
        // branch.*): their waits delay only their own cues.
        foreach (var branch in new[] { "card_enqueue_vfx", "relic_activate", "infested_reveal", "stock_reveal", "reattach_vfx", "lords_parasol" })
        {
            Spire.HookBridge.Hooks.RegisterBefore("branch." + branch + ".begin", _ => Steps.BeginBranch());
            Spire.HookBridge.Hooks.RegisterAfter("branch." + branch + ".end", (_, _) => Steps.EndBranch());
        }
        // CardModel.OnPlayWrapper's replays (CurrentPlayIndex = i > 0, not a
        // power): NCard.AnimMultiCardPlay in the play pile, then
        // CustomScaledWait(0.4, 0.5).
        Spire.HookBridge.Hooks.RegisterBefore("card.play_index", a =>
        {
            if (a.Length < 2 || a[0] is not CardModel card || a[1] is not int i || i <= 0 || card.Type == CardType.Power) return;
            if (!CombatManager.Instance.IsInProgress) return;
            Steps.Cue(new Dictionary<string, object?> { ["t"] = "multi_play", ["card"] = Steps.IdOf(card) });
            ScaledWait(0.4f, 0.5f);
        });
        // CardModel.PlayPowerCardFlyVfx: the card in the play pile swoops into
        // its owner (NCardFlyPowerVfx), then CustomScaledWait(duration * 0.2,
        // duration); a replay flies a new copy after CustomScaledWait(0.1, 0.8).
        // The duration is the swoosh path's length at 3000 px/s, which the
        // presentation measures; the wait carries the usual one and its scale.
        Bridge.Register("card.power_fly", a =>
        {
            if (a.Length < 1 || a[0] is not CardModel card) return Task.CompletedTask;
            if (card.CurrentPlayIndex > 0)
            {
                Steps.Cue(new Dictionary<string, object?> { ["t"] = "power_card", ["card"] = Steps.IdOf(card) });
                ScaledWait(0.1f, 0.8f);
            }
            Steps.Cue(new Dictionary<string, object?> { ["t"] = "power_fly", ["card"] = Steps.IdOf(card) });
            var mode = SaveManager.Instance.PrefsSave?.FastMode ?? FastModeType.Normal;
            if (mode != FastModeType.Instant && !CombatManager.Instance.IsEnding)
            {
                double scale = mode == FastModeType.Fast ? 0.2 : 1.0;
                Steps.Wait((int)(PowerFlySeconds * scale * 1000), "power_fly", scale);
            }
            return Task.CompletedTask;
        });

        // Every call from game logic into the UI layer (redirect_calls_into "ui"):
        // nothing runs, the call is recorded as a presentation cue. The patched
        // call site supplies a safe default return value itself.
        Spire.HookBridge.Hooks.RegisterPrefix("ui", (id, a) =>
        {
            Steps.Ui(id, a);
            // NCreature.PerformIntent (CombatManager.ExecuteEnemyTurn awaits it
            // before each enemy acts): the intents hide, CustomScaledWait(0.25, 0.4).
            if (id.EndsWith("NCreature::PerformIntent", StringComparison.Ordinal)) ScaledWait(0.25f, 0.4f);
            // NCreature.RefreshIntents / UpdateIntent (a turn's new moves, a
            // power changing their numbers): the intents as the call shows them
            if ((id.EndsWith("NCreature::RefreshIntents", StringComparison.Ordinal) ||
                 id.EndsWith("NCreature::UpdateIntent", StringComparison.Ordinal)) &&
                a.Length > 0 && StandIns.TryCreature(a[0], out var intentOf) && intentOf.Monster != null)
                Steps.SetOnLastUi("intents", RunDriver.IntentsView(intentOf, intentOf.CombatState?.PlayerCreatures?.ToList()));
            // The events' NCardEnchantVfx.Create(card): the enchanted card's view.
            if (id.EndsWith("NCardEnchantVfx::Create", StringComparison.Ordinal) && a.Length > 0 && a[0] is CardModel enchanted)
                DeckCue("enchant", enchanted, null, null, 0);
            // Fiend Fire's fires grow by 0.1 a hit from 0.8 (their Scale)
            if (id.EndsWith("NGroundFireVfx::Create", StringComparison.Ordinal) && _fiendFire?.Pile?.Type == PileType.Play)
            {
                Steps.AppendToLastUi(_fiendFireScale);
                _fiendFireScale += 0.1f;
            }
            // NSimpleCardsViewScreen.ShowScreen(results, infoText) (Pandora's Box):
            // the cards the screen lists, with its text
            if (id.EndsWith("NSimpleCardsViewScreen::ShowScreen", StringComparison.Ordinal) && Driver != null &&
                a.Length > 1 && a[0] is IEnumerable<CardPileAddResult> shown)
                Steps.Cue(new Dictionary<string, object?>
                {
                    ["t"] = "cards_view",
                    ["cards"] = shown.Select(r => (object?)Driver.DeckCardView(r.cardAdded)).ToList(),
                    ["info"] = a[1] is LocString info ? info.GetFormattedText() : null,
                });
            // ThievingHopper.ThieveryMove: NCard.Create(stolen) added under the
            // %StolenCardPos it asked for just before (the card in its hand)
            if (id.EndsWith("NCard::Create", StringComparison.Ordinal) && _stolenCardHolder != null && Driver != null &&
                a.Length > 0 && a[0] is CardModel stolen)
                Steps.Cue(new Dictionary<string, object?> { ["t"] = "held_card", ["who"] = _stolenCardHolder, ["card"] = Driver.DeckCardView(stolen) });
            _stolenCardHolder = id.Contains("NCreature::GetSpecialNode", StringComparison.Ordinal) && a.Length > 1 &&
                a[1] as string == "%StolenCardPos" && StandIns.TryCreature(a[0], out var holder) ? Steps.Who(holder) : null;
            // DoomPower.StartDoomAnim (a creature dying to Doom): SetAnimationTrigger("Hit"),
            // its hurt animation frozen at HurtAnimationTrackOffsetForDoom
            // (SetTrackTime, SetTimeScale(0)), then NDoomVfx.Create(visuals, ...,
            // shouldDie) takes the body: the pose on that cue
            if (id.EndsWith("NCreature::SetAnimationTrigger", StringComparison.Ordinal))
                _doomHit = a.Length > 1 && a[1] as string == "Hit" && StandIns.TryCreature(a[0], out var hit) ? hit : null;
            if (id.EndsWith("NDoomVfx::Create", StringComparison.Ordinal) && a.Length > 3 && a[3] is true && _doomHit != null)
            {
                Steps.SetOnLastUi("pose", Animators.Trigger(_doomHit, "Hit"));
                Steps.SetOnLastUi("hurt_offset", _doomHit.Monster?.HurtAnimationTrackOffsetForDoom ?? 0.1f);
                _doomHit = null;
            }
            // NCreature.StartReviveAnim: its Revive trigger's animation, if any
            if (id.EndsWith("NCreature::StartReviveAnim", StringComparison.Ordinal) && a.Length > 0 &&
                StandIns.TryCreature(a[0], out var revived) && Animators.Revive(revived) is { } reviveOps)
                Steps.SetOnLastUi("play", reviveOps);
            Driver?.OnUiCall(id, a);
            var answer = StandIns.Answer(id, a);
            // NDebugAudioManager.Play's handle (Dense Vegetation stops its
            // sleep.tres by it): a number of its own, on the cue
            if (id.EndsWith("NDebugAudioManager::Play", StringComparison.Ordinal))
            {
                answer = ++_debugAudioHandle;
                Steps.SetOnLastUi("handle", answer);
            }
            if (Ledger.On) Ledger.Ui(id, answer);
            return answer;
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

    /// <summary>Cmd.CustomScaledWait as the game waits it: the fast or the
    /// standard time by PrefsSave.FastMode, none in Instant or (unless
    /// ignoreCombatEnd) while the combat ends.</summary>
    private static void ScaledWait(object? fast, object? standard, bool ignoreCombatEnd = false)
    {
        var mode = SaveManager.Instance.PrefsSave?.FastMode ?? FastModeType.Normal;
        if (mode != FastModeType.Instant && (ignoreCombatEnd || !CombatManager.Instance.IsEnding))
            Steps.Wait(WaitMs(mode == FastModeType.Fast ? fast : standard), "cmd");
    }

    private static int _shuffleStepMs;

    /// <summary>The Fiend Fire being played and its next fire's scale.</summary>
    private static CardModel? _fiendFire;
    private static int _debugAudioHandle;
    /// <summary>The creature whose %StolenCardPos the last UI call asked for.</summary>
    private static string? _stolenCardHolder;
    /// <summary>The creature the last direct SetAnimationTrigger("Hit") was for (DoomPower's).</summary>
    private static Creature? _doomHit;
    private static float _fiendFireScale;

    /// <summary>NCardFlyPowerVfx.GetDuration with the play pile and the
    /// player where they usually are (the swoosh path's baked length / 3000,
    /// + 0.05).</summary>
    private const double PowerFlySeconds = 0.55;

    /// <summary>How long CardPileCmd's tween for these pile changes runs
    /// (GetTweenForCardsChangingPiles, AppendPileLerpTween): a card coming
    /// into the hand grows in over the lerp time; one going to the play pile
    /// takes its 0.25 s scale; one exhausted from another pile flies to the
    /// play pile and waits there (0.5 s, Fast 0.2); the rest fly off in
    /// callbacks the tween does not wait for. The cards run in parallel.</summary>
    private static int CardTweenMs(IEnumerable<CardPileAddResult> results)
    {
        var mode = SaveManager.Instance.PrefsSave?.FastMode ?? FastModeType.Normal;
        if (CombatManager.Instance.IsEnding) return 0;
        float lerp = mode switch { FastModeType.Instant => 0.01f, FastModeType.Fast => 0.1f, _ => 0.25f };
        float hold = mode switch { FastModeType.Instant => 0.01f, FastModeType.Fast => 0.2f, _ => 0.5f };
        float longest = 0;
        foreach (var r in results)
        {
            if (!r.success) continue;
            var to = r.cardAdded.Pile?.Type;
            var from = r.oldPile?.Type;
            float t = to switch
            {
                PileType.Hand => from.HasValue ? lerp : 0,
                PileType.Play => Math.Max(lerp, 0.25f),
                PileType.Exhaust => from.HasValue && from != PileType.Hand && from != PileType.Play ? Math.Max(lerp, 0.25f) + hold : 0,
                _ => 0,
            };
            longest = Math.Max(longest, t);
        }
        return (int)Math.Round(longest * 1000);
    }

    private static string PileName(PileType p) => p.ToString().ToLowerInvariant();

    /// <summary>GetTweenForCardsChangingPiles for the cards with no node on
    /// the table (FindOnTable finds only the hand's and the play pile's,
    /// whose moves the card changes show): one between the draw, discard,
    /// exhaust piles and the deck flies there as a silhouette
    /// (NCardFlyShuffleVfx); one going to the play pile (Havoc, Mayhem) or
    /// exhausted from another pile is made at its old pile
    /// (CreateCardNodeAndUpdateVisuals) and goes to the play pile, to burn
    /// there for the exhaust.</summary>
    private static void PileCues(IEnumerable<CardPileAddResult> results)
    {
        if (!CombatManager.Instance.IsInProgress) return;
        foreach (var r in results)
        {
            if (!r.success) continue;
            var card = r.cardAdded;
            var to = card.Pile?.Type;
            var from = r.oldPile?.Type;
            if (to == null || from == null || from == PileType.Hand || from == PileType.Play) continue;
            if (from is not (PileType.Draw or PileType.Discard or PileType.Exhaust or PileType.Deck)) continue;
            if (to is PileType.Draw or PileType.Discard or PileType.Deck)
                Steps.Cue(new Dictionary<string, object?>
                {
                    ["t"] = "card_fly", ["card"] = Steps.IdOf(card), ["from"] = PileName(from.Value), ["to"] = PileName(to.Value),
                });
            else if (to is PileType.Play or PileType.Exhaust && Driver != null)
                Steps.Cue(new Dictionary<string, object?>
                {
                    ["t"] = "card_create", ["card"] = Steps.IdOf(card), ["from"] = PileName(from.Value), ["to"] = PileName(to.Value),
                    ["view"] = Driver.DeckCardView(card),
                });
        }
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
    private static readonly HashSet<CardModel> _blades = new();
    private static object? _bladesCombat;
    private static Creature? _bladeTarget;

    /// <summary>A Sovereign Blade cue: op, the blade's card, then key / value pairs.</summary>
    private static void BladeCue(string op, CardModel card, params object?[] kv)
    {
        var cue = new Dictionary<string, object?> { ["t"] = "blade", ["op"] = op, ["card"] = Steps.IdOf(card) };
        for (int i = 0; i + 1 < kv.Length; i += 2) cue[(string)kv[i]!] = kv[i + 1];
        Steps.Cue(cue);
    }

    private static void HeadCue(Creature who, params Dictionary<string, object?>[] ops) =>
        Steps.Cue(new Dictionary<string, object?> { ["t"] = "anim", ["who"] = Steps.Who(who), ["name"] = "Head", ["wait"] = 0, ["play"] = ops.ToList() });

    private static MegaCrit.Sts2.Core.Nodes.CommonUi.CardPreviewStyle? _upgradeStyle, _transformStyle;

    /// <summary>CardCmd.PreviewInternal's checks: a card in a pile, not while the
    /// combat ends; the combat piles' previews are the fight's.</summary>
    private static string SafeTitle(PowerModel pw)
    {
        try { return pw.Title.GetFormattedText(); }
        catch { return pw.Id.Entry; }
    }

    private static void PreviewCue(string op, CardModel card, MegaCrit.Sts2.Core.Nodes.CommonUi.CardPreviewStyle style, float time,
                                   List<AbstractModel>? modifying = null)
    {
        // CardCmd.FlashRelics: the relics that changed the card flash on it
        // and in the top bar once it has grown in (their places in the
        // player's relics)
        var owned = card.Owner.Relics.ToList();
        _flashRelics = modifying?.OfType<RelicModel>().Select(rl => (object?)owned.IndexOf(rl))
            .Where(i => i is int n && n >= 0).ToList();
        if (card.Pile == null || CombatManager.Instance.IsEnding) return;
        if (card.Pile.Type.IsCombatPile())
        {
            // NCombatRoom.Ui's preview containers, then NCardFlyVfx to that
            // pile (a status card an enemy gives, ...); the hand's own
            // holders show cards that come into it
            if (card.Pile.Type == PileType.Hand || card.Pile.Type == PileType.Play) return;
            DeckCue(op, card, null, style, time, card.Pile.Type.ToString().ToLowerInvariant());
            return;
        }
        DeckCue(op, card, null, style, time);
    }
    private static CardModel? _transformedFrom;

    /// <summary>A deck_card cue: op (add, show, remove, upgrade, transform,
    /// enchant), the card's view, the original's for a transform, the preview
    /// style and time when the game gives them.</summary>
    private static void DeckCue(string op, CardModel card, CardModel? from, MegaCrit.Sts2.Core.Nodes.CommonUi.CardPreviewStyle? style, float time, string? pile = null)
    {
        if (Driver == null) return;
        var cue = new Dictionary<string, object?> { ["t"] = "deck_card", ["op"] = op, ["card"] = Driver.DeckCardView(card) };
        if (pile != null) cue["pile"] = pile;
        if (from != null) cue["from"] = Driver.DeckCardView(from);
        if (style != null) cue["style"] = style.ToString();
        if (time > 0) cue["time"] = time;
        if (op == "add" && _flashRelics?.Count > 0) cue["relics"] = _flashRelics;
        _flashRelics = null;
        Steps.Cue(cue);
    }

    private static List<object?>? _flashRelics;

    /// <summary>VfxCmd.PlayOnCreature / PlayOnCreatureCenter(target, path): at the
    /// creature's feet (its position) or its centre (VfxSpawnPosition); none
    /// on the dead, as the game.</summary>
    private static void VfxCue(object?[] a, string at)
    {
        if (a.Length > 1 && a[0] is Creature c && a[1] is string path && !c.IsDead)
            Steps.Cue(new Dictionary<string, object?> { ["t"] = "vfx", ["path"] = path, ["who"] = Steps.Who(c), ["at"] = at });
    }
}
