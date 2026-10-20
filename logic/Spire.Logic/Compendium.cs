// The compendium's data (NCompendiumSubmenu and its screens): queries that
// change nothing. The card library's cards come one pool at a time, as the
// screen shows them (NCardLibrary selects one pool filter at a time).

using System.Globalization;
using MegaCrit.Sts2.Core.Entities.Cards;
using MegaCrit.Sts2.Core.Entities.Potions;
using MegaCrit.Sts2.Core.Entities.Relics;
using MegaCrit.Sts2.Core.Helpers;
using MegaCrit.Sts2.Core.Localization;
using MegaCrit.Sts2.Core.Models.Relics;
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

    // LocManager.StringComparer when the game's localisation is up.
    private static StringComparer TitleComparer()
    {
        try { if (LocManager.Instance?.StringComparer != null) return LocManager.Instance.StringComparer; } catch { }
        return StringComparer.Create(CultureInfo.GetCultureInfo("en-US"), false);
    }

    private static float[] OutlineColor(Godot.Color c) => new[] { c.R, c.G, c.B, 0.66f };

    /// <summary>A relic's icon name (RelicModel.IconBaseName, which some
    /// relics change: LoomingFruit, YummyCookie): its atlas sprite's.</summary>
    internal static string? RelicIconBase(RelicModel? r)
    {
        if (r == null) return null;
        try { return System.IO.Path.GetFileNameWithoutExtension(r.PackedIconPath); }
        catch { return r.Id.Entry.ToLowerInvariant(); }
    }

    private Dictionary<string, object?> CollectionEntry(string id, string visibility, float[]? outline, string table,
        Func<IEnumerable<MegaCrit.Sts2.Core.HoverTips.IHoverTip>> tips) => new Dictionary<string, object?>
    {
        ["id"] = id,
        ["visibility"] = visibility,
        ["outline"] = visibility == "visible" ? outline : null,
        ["tips"] = visibility == "visible" ? Tips(tips) : new List<Dictionary<string, object?>>
        {
            new Dictionary<string, object?>
            {
                ["title"] = _loc.Bilingual("main_menu_ui", table + "." + (visibility == "locked" ? "locked" : "unknown") + ".title"),
                ["description"] = _loc.Bilingual("main_menu_ui", table + "." + (visibility == "locked" ? "locked" : "unknown") + ".description"),
            },
        },
    };

    private Dictionary<string, object?> RelicEntry(RelicModel relic, HashSet<RelicModel> seen, HashSet<RelicModel> unlocked)
    {
        // NRelicCollectionCategory.LoadRelicNodes, NRelicCollectionEntry._Ready
        string visibility = !unlocked.Contains(relic) ? "locked" : seen.Contains(relic) ? "visible" : "not_seen";
        float[]? outline = null;
        foreach (var pool in ModelDb.AllCharacterRelicPools)
            if (pool.AllRelicIds.Contains(relic.Id)) { outline = OutlineColor(pool.LabOutlineColor); break; }
        var d = CollectionEntry(relic.Id.Entry, visibility, outline, "COMPENDIUM_RELIC_COLLECTION", () => relic.ToMutable().HoverTips);
        d["icon_base"] = RelicIconBase(relic);
        // NInspectRelicScreen (A on an entry)
        foreach (var kv in InspectRelicView(relic.ToMutable(), unlocked)) d.TryAdd(kv.Key, kv.Value);
        return d;
    }

    private static Dictionary<string, object?> CollectionGroup(string? header, string? icon, bool showHeader, bool spacer,
        List<Dictionary<string, object?>> items) => new Dictionary<string, object?>
    {
        ["header"] = header,
        ["icon"] = icon,
        ["show_header"] = showHeader,
        ["spacer"] = spacer,
        ["items"] = items,
    };

    /// <summary>NRelicCollection.LoadRelics: each category's header and its
    /// relics (or, for Starter and Ancient, its subcategories).</summary>
    public Dictionary<string, object?> RelicCollection()
    {
        EnsureModelDbInitialized();
        var progress = SaveManager.Instance.Progress;
        var seen = (progress?.DiscoveredRelics ?? Enumerable.Empty<ModelId>()).Select(ModelDb.GetByIdOrNull<RelicModel>).OfType<RelicModel>().ToHashSet();
        var unlockState = SaveManager.Instance.GenerateUnlockStateFromProgress();
        var unlocked = unlockState.Relics.ToHashSet();
        var cmp = TitleComparer();
        var categories = new List<Dictionary<string, object?>>();
        var rarities = new[] { (RelicRarity.Starter, "Starter", "STARTER"), (RelicRarity.Common, "Common", "COMMON"),
            (RelicRarity.Uncommon, "Uncommon", "UNCOMMON"), (RelicRarity.Rare, "Rare", "RARE"), (RelicRarity.Shop, "Shop", "SHOP"),
            (RelicRarity.Ancient, "Ancient", "ANCIENT"), (RelicRarity.Event, "Event", "EVENT") };
        foreach (var (rarity, node, key) in rarities)
        {
            var ofRarity = ModelDb.AllRelics.Where(r => r.Rarity == rarity).ToList();
            var groups = new List<Dictionary<string, object?>>();
            List<Dictionary<string, object?>>? items = null;
            if (rarity == RelicRarity.Starter)
            {
                var starters = ModelDb.AllCharacters.SelectMany(c => c.StartingRelics).ToList();
                groups.Add(CollectionGroup(null, null, false, false, starters.Select(r => RelicEntry(r, seen, unlocked)).ToList()));
                var orobas = ModelDb.Relic<TouchOfOrobas>();
                groups.Add(CollectionGroup(null, null, false, true,
                    starters.Select(r => RelicEntry(orobas.GetUpgradedStarterRelic(r), seen, unlocked)).ToList()));
            }
            else if (rarity == RelicRarity.Ancient)
            {
                var acts = ModelDb.Acts.ToList();
                var all = acts.SelectMany(a => a.AllAncients).Concat(ModelDb.AllSharedAncients).Distinct().ToList();
                var unlockedAncients = acts.SelectMany(a => a.GetUnlockedAncients(unlockState)).Concat(unlockState.SharedAncients).Distinct().ToHashSet();
                var stats = progress?.AncientStats;
                foreach (var ancient in all)
                {
                    if (!unlockedAncients.Contains(ancient)) continue;
                    var relics = ancient.AllPossibleOptions.Select(o => o.Relic?.CanonicalInstance).OfType<RelicModel>()
                        .Intersect(ofRarity).OrderBy(r => _loc.Relic(r.Id.Entry), cmp).ToList();
                    bool known = (stats?.ContainsKey(ancient.Id) ?? false) || relics.Any(seen.Contains);
                    string name = known ? _loc.Event(ancient.Id.Entry) : _loc.Bilingual("relic_collection", "UNKNOWN_ANCIENT");
                    string header = (_loc.Rich("relic_collection", "ANCIENT_SUBCATEGORY") ?? "{Ancient}").Replace("{Ancient}", name);
                    string? icon = ImageHelper.GetRoomIconPath(MegaCrit.Sts2.Core.Map.MapPointType.Ancient,
                        MegaCrit.Sts2.Core.Rooms.RoomType.Event, ancient.Id);
                    if (icon != null && icon.StartsWith("res://")) icon = icon.Substring(6);
                    groups.Add(CollectionGroup(header, icon, true, true, relics.Select(r => RelicEntry(r, seen, unlocked)).ToList()));
                }
            }
            else
            {
                var pooled = new List<RelicModel>();
                foreach (var pool in ModelDb.AllCharacterRelicPools)
                    foreach (var r in ofRarity)
                        if (pool.AllRelicIds.Contains(r.Id)) pooled.Add(r);
                var shared = ofRarity.Where(r => !pooled.Contains(r)).ToList();
                shared.Sort((a, b) => cmp.Compare(_loc.Relic(a.Id.Entry), _loc.Relic(b.Id.Entry)));
                items = shared.Concat(pooled).Select(r => RelicEntry(r, seen, unlocked)).ToList();
            }
            categories.Add(new Dictionary<string, object?>
            {
                ["node"] = node,
                ["header"] = _loc.Rich("relic_collection", key) ?? key,
                ["items"] = items,
                ["groups"] = groups.Count > 0 ? groups : null,
            });
        }
        return new Dictionary<string, object?> { ["type"] = "relic_collection", ["categories"] = categories };
    }

    /// <summary>NPotionLab.LoadPotions: Common, Uncommon, Rare and Special
    /// (Event and Token) potions, each category's shared potions by title,
    /// then the characters'.</summary>
    public Dictionary<string, object?> PotionLab()
    {
        EnsureModelDbInitialized();
        var unlockState = SaveManager.Instance.GenerateUnlockStateFromProgress();
        var unlocked = unlockState.Potions.ToHashSet();
        var seen = (SaveManager.Instance.Progress?.DiscoveredPotions ?? Enumerable.Empty<ModelId>())
            .Select(ModelDb.GetByIdOrNull<PotionModel>).OfType<PotionModel>().ToHashSet();
        var cmp = TitleComparer();
        var categories = new List<Dictionary<string, object?>>();
        var rarities = new[] { (PotionRarity.Common, (PotionRarity?)null, "Common", "COMMON"),
            (PotionRarity.Uncommon, (PotionRarity?)null, "Uncommon", "UNCOMMON"), (PotionRarity.Rare, (PotionRarity?)null, "Rare", "RARE"),
            (PotionRarity.Event, (PotionRarity?)PotionRarity.Token, "Special", "SPECIAL") };
        foreach (var (rarity, second, node, key) in rarities)
        {
            var ofRarity = ModelDb.AllPotions.Where(p => p.Rarity == rarity || p.Rarity == second).ToList();
            var pooled = new List<PotionModel>();
            float[]? Outline(PotionModel p)
            {
                foreach (var pool in ModelDb.AllCharacterPotionPools)
                    if (pool.AllPotions.Any(x => x.Id == p.Id)) return OutlineColor(pool.LabOutlineColor);
                return null;
            }
            foreach (var pool in ModelDb.AllCharacterPotionPools)
                foreach (var p in ofRarity)
                    if (pool.AllPotionIds.Contains(p.Id)) pooled.Add(p);
            var shared = ofRarity.Where(p => !pooled.Contains(p)).ToList();
            shared.Sort((a, b) => cmp.Compare(_loc.Potion(a.Id.Entry), _loc.Potion(b.Id.Entry)));
            var items = shared.Concat(pooled).Select(p =>
            {
                string visibility = !unlocked.Contains(p) ? "locked" : seen.Contains(p) ? "visible" : "not_seen";
                return CollectionEntry(p.Id.Entry, visibility, Outline(p), "POTION_LAB_COLLECTION", () => p.ToMutable().HoverTips);
            }).ToList();
            categories.Add(new Dictionary<string, object?>
            {
                ["node"] = node,
                ["header"] = _loc.Rich("potion_lab", key) ?? key,
                ["items"] = items,
            });
        }
        return new Dictionary<string, object?> { ["type"] = "potion_lab", ["categories"] = categories };
    }
}
