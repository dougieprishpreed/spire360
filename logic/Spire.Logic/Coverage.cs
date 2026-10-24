// Test aids for the content coverage harness (tools/content_coverage.py):
// the game's content as ModelDb lists it, and ways to give a run any card,
// relic or potion through the game's own commands (RelicCmd.Obtain,
// PotionCmd.TryToProcure, CardPileCmd), so their effects and hooks run as
// they would when the game hands them out. Not used by the game's flow.
using System.Text.Json;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Commands;
using MegaCrit.Sts2.Core.Entities.Cards;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Cards;
using MegaCrit.Sts2.Core.Models.Events;
using MegaCrit.Sts2.Core.Models.Relics;
using MegaCrit.Sts2.Core.Rooms;
using MegaCrit.Sts2.Core.Saves;
using MegaCrit.Sts2.Core.Timeline;

namespace Spire.Logic;

public partial class RunDriver
{
    /// <summary>Every act discovered (SerializableProgress.DiscoveredActs), as
    /// on a profile that has played them: a new run's acts are then drawn
    /// from the unlocked ones (ActModel.GetRandomList), where a new profile
    /// always gets the undiscovered alternate first (Underdocks).</summary>
    public Dictionary<string, object?> DiscoverActs()
    {
        if (_profileId <= 0) return Error("No profile selected");
        EnsureModelDbInitialized();
        var progress = SaveManager.Instance.Progress;
        foreach (var act in ModelDb.ActsByIndex.SelectMany(a => a)) progress.MarkActAsSeen(act.Id);
        SaveManager.Instance.SaveProgressFile();
        return new Dictionary<string, object?> { ["type"] = "ok", ["acts"] = progress.DiscoveredActs.Select(a => a.Entry).ToList() };
    }

    /// <summary>Everything ModelDb knows a run can contain.</summary>
    public Dictionary<string, object?> Content()
    {
        try
        {
            EnsureModelDbInitialized();
            var cards = ModelDb.AllCardPools.SelectMany(p => p.AllCards.Select(c => (pool: p, card: c)))
                .GroupBy(x => x.card.Id).Select(g => g.First())
                .Select(x => new Dictionary<string, object?>
                {
                    ["id"] = x.card.Id.Entry,
                    ["pool"] = x.pool.Id.Entry,
                    ["type"] = x.card.Type.ToString(),
                    ["rarity"] = x.card.Rarity.ToString(),
                    ["target"] = x.card.TargetType.ToString(),
                    ["upgradable"] = Safe(() => x.card.IsUpgradable),
                    // CardMultiplayerConstraint.MultiplayerOnly: never in a solo run
                    ["multiplayer_only"] = x.card.MultiplayerConstraint == CardMultiplayerConstraint.MultiplayerOnly,
                }).ToList();
            // Starting decks' cards outside any pool.
            foreach (var c in ModelDb.AllCards)
                if (!cards.Any(d => (string?)d["id"] == c.Id.Entry))
                    cards.Add(new Dictionary<string, object?>
                    {
                        ["id"] = c.Id.Entry, ["pool"] = "STARTER", ["type"] = c.Type.ToString(),
                        ["rarity"] = c.Rarity.ToString(), ["target"] = c.TargetType.ToString(),
                        ["upgradable"] = Safe(() => c.IsUpgradable),
                    });
            var relics = ModelDb.AllRelicPools.SelectMany(p => p.AllRelics.Select(r => (pool: p, relic: r)))
                .GroupBy(x => x.relic.Id).Select(g => g.First())
                .Select(x => new Dictionary<string, object?>
                {
                    ["id"] = x.relic.Id.Entry, ["pool"] = x.pool.Id.Entry, ["rarity"] = x.relic.Rarity.ToString(),
                }).ToList();
            foreach (var r in ModelDb.AllRelics)
                if (!relics.Any(d => (string?)d["id"] == r.Id.Entry))
                    relics.Add(new Dictionary<string, object?> { ["id"] = r.Id.Entry, ["pool"] = "STARTER", ["rarity"] = r.Rarity.ToString() });
            var potions = ModelDb.AllPotions.Select(p => new Dictionary<string, object?>
            {
                ["id"] = p.Id.Entry, ["rarity"] = p.Rarity.ToString(), ["target"] = p.TargetType.ToString(),
                ["usage"] = p.Usage.ToString(),
            }).ToList();
            var acts = ModelDb.Acts.Select(a => new Dictionary<string, object?>
            {
                ["id"] = a.Id.Entry,
                ["index"] = a.Index,
                ["default"] = a.IsDefault,
                ["events"] = a.AllEvents.Select(e => e.Id.Entry).ToList(),
                ["ancients"] = a.AllAncients.Select(e => e.Id.Entry).ToList(),
                ["encounters"] = a.AllEncounters.Select(e => new Dictionary<string, object?>
                {
                    ["id"] = e.Id.Entry, ["room"] = e.RoomType.ToString(),
                }).ToList(),
            }).ToList();
            return new Dictionary<string, object?>
            {
                ["type"] = "content",
                ["characters"] = ModelDb.AllCharacters.Select(c => c.Id.Entry).ToList(),
                ["cards"] = cards,
                ["relics"] = relics,
                ["potions"] = potions,
                ["acts"] = acts,
                ["shared_events"] = ModelDb.AllSharedEvents.Select(e => e.Id.Entry).ToList(),
                ["shared_ancients"] = ModelDb.AllSharedAncients.Select(e => e.Id.Entry).ToList(),
                ["event_encounters"] = ModelDb.AllEncounters.Where(e => !ModelDb.Acts.Any(a => a.AllEncounters.Contains(e)))
                    .Select(e => e.Id.Entry).ToList(),
                ["monsters"] = ModelDb.Monsters.Select(m => m.Id.Entry).ToList(),
                // EncounterModel.AllPossibleMonsters: an encounter that draws its
                // monsters at random (Ruby Raiders: 3 of 5) needs several seeds
                ["encounter_monsters"] = ModelDb.AllEncounters.ToDictionary(e => e.Id.Entry,
                    e => (object?)e.AllPossibleMonsters.Select(m => m.Id.Entry).Distinct().ToList()),
                ["powers"] = ModelDb.AllPowers.Count(),
                ["epochs"] = EpochModel.AllEpochIds.ToList(),
            };
        }
        catch (Exception ex) { return ErrorWithTrace("content failed", ex); }
    }

    /// <summary>The skin a pet's relic chose (Byrdpip.Skin, PaelsLegion.Skin).</summary>
    private static string? PetSkin(MegaCrit.Sts2.Core.Entities.Players.Player player, MegaCrit.Sts2.Core.Entities.Creatures.Creature pet)
    {
        var name = pet.Monster?.GetType().Name;
        var relic = player.Relics.FirstOrDefault(r => r.GetType().Name == name);
        return relic?.GetType().GetProperty("Skin")?.GetValue(relic) as string;
    }

    private static object? Safe(Func<object?> f)
    {
        try { return f(); }
        catch { return null; }
    }

    private static int _madScienceVariant;

    /// <summary>
    /// What the event that hands out a piece of content sets on it first:
    /// Darv's Dusty Tome picks its ancient card, Orobas's Sea Glass a
    /// character (another one than the player's), Tinker Time's Mad Science a
    /// type and rider (each new one the next of the nine).
    /// </summary>
    private static void EventSetup(AbstractModel model, Player player)
    {
        switch (model)
        {
            case DustyTome tome:
                tome.SetupForPlayer(player);
                break;
            case SeaGlass glass:
                glass.CharacterId = (ModelDb.AllCharacters.FirstOrDefault(c => c.Id != player.Character.Id)
                                     ?? player.Character).Id;
                break;
            case MadScience science:
                int v = _madScienceVariant++ % 9;
                science.TinkerTimeType = new[] { CardType.Attack, CardType.Skill, CardType.Power }[v / 3];
                science.TinkerTimeRider = new[]
                {
                    TinkerTime.RiderEffect.Sapping, TinkerTime.RiderEffect.Violence, TinkerTime.RiderEffect.Choking,
                    TinkerTime.RiderEffect.Energized, TinkerTime.RiderEffect.Wisdom, TinkerTime.RiderEffect.Chaos,
                    TinkerTime.RiderEffect.Expertise, TinkerTime.RiderEffect.Curious, TinkerTime.RiderEffect.Improvement,
                }[v];
                break;
        }
    }

    /// <summary>
    /// Gives the player relics, potions and cards (to the deck) the way the
    /// game does (RelicCmd.Obtain, PotionCmd.TryToProcure, CardPileCmd.Add),
    /// so their pick-up effects run.
    /// </summary>
    public Dictionary<string, object?> Obtain(JsonElement cmd)
    {
        try
        {
            if (_runState == null) return Error("No run in progress");
            var player = _runState.Players[0];
            var missing = new List<string>();
            var notAllowed = new List<string>();
            if (cmd.TryGetProperty("relics", out var rs))
                foreach (var r in rs.EnumerateArray())
                {
                    var model = ModelDb.GetByIdOrNull<RelicModel>(new ModelId("RELIC", r.GetString() ?? ""));
                    if (model == null) { missing.Add("RELIC." + r.GetString()); continue; }
                    // Multiplayer-only relics (Massive Scroll) never reach a solo run.
                    if (!model.IsAllowed(_runState)) { notAllowed.Add("RELIC." + r.GetString()); continue; }
                    var relic = model.ToMutable();
                    EventSetup(relic, player);
                    Sched.Run(() => RelicCmd.Obtain(relic, player));
                }
            if (cmd.TryGetProperty("potions", out var ps))
                foreach (var p in ps.EnumerateArray())
                {
                    var model = ModelDb.GetByIdOrNull<PotionModel>(new ModelId("POTION", p.GetString() ?? ""));
                    if (model == null) { missing.Add("POTION." + p.GetString()); continue; }
                    Sched.Run(() => PotionCmd.TryToProcure(model.ToMutable(), player));
                }
            if (cmd.TryGetProperty("cards", out var cs))
                foreach (var c in cs.EnumerateArray())
                {
                    var model = ModelDb.GetByIdOrNull<CardModel>(new ModelId("CARD", c.GetString() ?? ""));
                    if (model == null) { missing.Add("CARD." + c.GetString()); continue; }
                    var card = _runState.CreateCard(model, player);
                    EventSetup(card, player);
                    Sched.Run(() => CardPileCmd.Add(card, PileType.Deck));
                }
            WaitForActionExecutor();
            var d = DetectDecisionPoint();
            if (missing.Count > 0) d["missing"] = missing;
            if (notAllowed.Count > 0) d["not_allowed"] = notAllowed;
            return d;
        }
        catch (Exception ex) { return ErrorWithTrace("obtain failed", ex); }
    }

    /// <summary>
    /// In combat: cards generated into the hand (CardPileCmd.AddGeneratedCardToCombat,
    /// upgraded on request), energy (PlayerCmd.GainEnergy) and stars
    /// (PlayerCmd.GainStars); "clear": the hand and the draw pile to the
    /// discard pile first (cards whose own logic wants them empty: Grand
    /// Finale, Clash).
    /// </summary>
    public Dictionary<string, object?> AddToHand(JsonElement cmd)
    {
        try
        {
            if (_runState == null) return Error("No run in progress");
            var player = _runState.Players[0];
            if (!CombatManager.Instance.IsInProgress || player.Creature?.CombatState == null) return Error("Not in combat");
            var combat = player.Creature.CombatState;
            bool upgraded = cmd.TryGetProperty("upgraded", out var up) && up.ValueKind == JsonValueKind.True;
            if (cmd.TryGetProperty("clear", out var clear) && clear.ValueKind == JsonValueKind.True && player.PlayerCombatState != null)
            {
                var pcs = player.PlayerCombatState;
                var rest = pcs.Hand.Cards.Concat(pcs.DrawPile.Cards).ToList();
                if (rest.Count > 0) Sched.Run(() => CardPileCmd.Add(rest, PileType.Discard));
                WaitForActionExecutor();
            }
            if (cmd.TryGetProperty("cards", out var cs))
                foreach (var c in cs.EnumerateArray())
                {
                    var model = ModelDb.GetByIdOrNull<CardModel>(new ModelId("CARD", c.GetString() ?? ""));
                    if (model == null) return Error($"Unknown card {c.GetString()}");
                    var card = combat.CreateCard(model, player);
                    EventSetup(card, player);
                    if (upgraded && card.IsUpgradable) CardCmd.Upgrade(card);
                    Sched.Run(() => CardPileCmd.AddGeneratedCardToCombat(card, PileType.Hand, player));
                }
            if (cmd.TryGetProperty("energy", out var en) && en.TryGetInt32(out var e) && e > 0)
                Sched.Run(() => PlayerCmd.GainEnergy(e, player));
            if (cmd.TryGetProperty("stars", out var st) && st.TryGetInt32(out var s) && s > 0)
                Sched.Run(() => PlayerCmd.GainStars(s, player));
            WaitForActionExecutor();
            return DetectDecisionPoint();
        }
        catch (Exception ex) { return ErrorWithTrace("add_to_hand failed", ex); }
    }
}
