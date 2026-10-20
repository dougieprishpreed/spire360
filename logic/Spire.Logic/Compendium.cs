// The compendium's data (NCompendiumSubmenu and its screens): queries that
// change nothing. The card library's cards come one pool at a time, as the
// screen shows them (NCardLibrary selects one pool filter at a time).

using MegaCrit.Sts2.Core.Entities.Cards;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.CardPools;
using MegaCrit.Sts2.Core.Models.Characters;
using MegaCrit.Sts2.Core.Saves;

namespace Spire.Logic;

public partial class RunDriver
{
    // NCardLibrary._poolFilters
    private static readonly string[] LibraryPools = { "ironclad", "silent", "defect", "regent", "necrobinder", "colorless", "ancients", "misc" };

    private static bool InLibraryPool(CardModel c, string pool) => pool switch
    {
        "ironclad" => c.Pool is IroncladCardPool,
        "silent" => c.Pool is SilentCardPool,
        "defect" => c.Pool is DefectCardPool,
        "regent" => c.Pool is RegentCardPool,
        "necrobinder" => c.Pool is NecrobinderCardPool,
        "colorless" => c.Pool is ColorlessCardPool,
        "ancients" => c.Rarity == CardRarity.Ancient,
        "misc" => (uint)(c.Rarity - 6) <= 4u,
        _ => false,
    };

    // NCardGrid.GetCardRarityComparisonValue
    private static int RarityRank(CardRarity r)
    {
        if (r <= CardRarity.Ancient) return (int)r;
        return r switch
        {
            CardRarity.Status => 6,
            CardRarity.Curse => 7,
            CardRarity.Event => 8,
            CardRarity.Quest => 9,
            CardRarity.Token => 10,
            _ => 11,
        };
    }

    /// <summary>NCardLibrary: one pool's cards (InitialSorter order), each
    /// with its view, its visibility (NCardLibraryGrid.GetCardVisibility) and
    /// what the filters and sorters read; the characters whose pools show.</summary>
    public Dictionary<string, object?> Library(string pool)
    {
        EnsureModelDbInitialized();
        if (Array.IndexOf(LibraryPools, pool) < 0) return Error("Unknown pool " + pool);
        var seen = SaveManager.Instance.Progress?.DiscoveredCards?.ToHashSet() ?? new HashSet<ModelId>();
        var unlockState = SaveManager.Instance.GenerateUnlockStateFromProgress();
        var unlocked = ModelDb.AllCardPools.SelectMany(p => p.GetUnlockedCards(unlockState, CardMultiplayerConstraint.None)).ToHashSet();
        var pools = ModelDb.AllCardPools.ToList();
        var cards = ModelDb.AllCards.Where(c => c.ShouldShowInCardLibrary && InLibraryPool(c, pool)).ToList();
        cards.Sort((x, y) =>
        {
            int a = pools.IndexOf(x.Pool).CompareTo(pools.IndexOf(y.Pool));
            if (a != 0) return a;
            int b = x.Rarity.CompareTo(y.Rarity);
            return b != 0 ? b : x.Id.CompareTo(y.Id);
        });
        var list = cards.Select((model, i) =>
        {
            var card = model.ToMutable();
            string visibility = !unlocked.Contains(model) ? "locked" : !seen.Contains(model.Id) ? "not_seen" : "visible";
            var stats = new Dictionary<string, object?>();
            try { foreach (var dv in card.DynamicVars.Values) stats[dv.Name.ToLowerInvariant()] = (int)dv.BaseValue; } catch { }
            var d = WithCardView(card, PileType.None, new Dictionary<string, object?>
            {
                ["index"] = i,
                ["id"] = card.Id.ToString(),
                ["name"] = _loc.Card(card.Id.Entry),
                ["cost"] = card.EnergyCost?.GetResolved() ?? 0,
                ["type"] = card.Type.ToString(),
                ["rarity"] = card.Rarity.ToString(),
                ["upgraded"] = false,
                ["stats"] = stats.Count > 0 ? stats : null,
                ["visibility"] = visibility,
                ["rarity_rank"] = RarityRank(card.Rarity),
                ["type_rank"] = (int)card.Type,
                ["canonical_cost"] = card.EnergyCost?.Canonical ?? 0,
                ["costs_x"] = (card.EnergyCost?.CostsX ?? false) || card.HasStarCostX,
                ["upgrade_preview"] = visibility == "visible" ? UpgradePreviewView(card) : null,
            });
            return d;
        }).ToList();
        var characters = new List<string>();
        foreach (var c in ModelDb.AllCharacters)
            if (unlockState.Characters.Contains(c)) characters.Add(c.Id.Entry.ToLowerInvariant());
        return new Dictionary<string, object?>
        {
            ["type"] = "library",
            ["pool"] = pool,
            ["cards"] = list,
            ["characters"] = characters,
            ["character"] = _runState?.Players[0]?.Character?.Id.Entry.ToLowerInvariant(),
        };
    }
}
