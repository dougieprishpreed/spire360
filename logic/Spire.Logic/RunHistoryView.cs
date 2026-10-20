// The compendium's Run History (NRunHistory and its parts): one run file at
// a time, newest first, with everything the screen shows formatted as the
// game formats it. Queries change nothing.

using System.Globalization;
using System.Text;
using MegaCrit.Sts2.Core.Entities.Cards;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.Entities.Relics;
using MegaCrit.Sts2.Core.Helpers;
using MegaCrit.Sts2.Core.Localization;
using MegaCrit.Sts2.Core.Map;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Exceptions;
using MegaCrit.Sts2.Core.Models.Relics;
using MegaCrit.Sts2.Core.Rooms;
using MegaCrit.Sts2.Core.Runs;
using MegaCrit.Sts2.Core.Runs.History;
using MegaCrit.Sts2.Core.Saves;

namespace Spire.Logic;

public partial class RunDriver
{
    private const string FontIcon = "[img=top]res://images/packed/sprite_fonts/";

    private static string? ResPath(string? p) => p != null && p.StartsWith("res://") ? p.Substring(6) : p;

    /// <summary>NCompendiumSubmenu.OnSubmenuOpened: NRunHistory.CanBeShown
    /// and NBestiary.CanBeShown.</summary>
    public Dictionary<string, object?> CompendiumState()
    {
        EnsureModelDbInitialized();
        var save = SaveManager.Instance;
        bool bestiary = false;
        try
        {
            bestiary = save.Progress.DiscoveredActs.Count > 0 && save.Progress.EnemyStats.Values.Any(e => e.TotalWins > 0);
        }
        catch { }
        int runs = 0;
        try { runs = save.GetRunHistoryCount(); } catch { }
        return new Dictionary<string, object?> { ["type"] = "compendium", ["run_history"] = runs > 0, ["bestiary"] = bestiary };
    }

    /// <summary>NRunHistory: the run at index (0 the newest).</summary>
    public Dictionary<string, object?> RunHistoryView(int index)
    {
        EnsureModelDbInitialized();
        var save = SaveManager.Instance;
        var names = save.GetAllRunHistoryNames();
        names.Reverse();
        var d = new Dictionary<string, object?> { ["type"] = "run_history", ["index"] = index, ["count"] = names.Count };
        if (index < 0 || index >= names.Count) { d["ok"] = false; return d; }
        RunHistory history;
        try
        {
            var read = save.LoadRunHistory(names[index]);
            if (!read.Success || read.SaveData == null) { d["ok"] = false; return d; }
            history = read.SaveData;
        }
        catch (Exception e)
        {
            Console.Error.WriteLine("[WARN] run history " + names[index] + ": " + e.Message);
            d["ok"] = false;
            return d;
        }
        d["ok"] = true;
        // LoadPlayerFloor, LoadGameModeDetails, LoadTimeDetails
        d["floor"] = history.MapPointHistory.Sum(rooms => rooms.Count);
        var mode = new LocString("run_history", "GAME_MODE.title");
        mode.Add("PlayerCount", new LocString("run_history", history.Players.Count > 1 ? "PLAYER_COUNT.multiplayer" : "PLAYER_COUNT.singleplayer"));
        mode.Add("GameMode", new LocString("run_history", history.GameMode switch
        {
            GameMode.Custom => "GAME_MODE.custom",
            GameMode.Daily => "GAME_MODE.daily",
            GameMode.Standard => "GAME_MODE.standard",
            _ => "GAME_MODE.unknown",
        }));
        d["game_mode"] = mode.GetFormattedText();
        var culture = LocManager.Instance?.CultureInfo ?? CultureInfo.GetCultureInfo("en-US");
        var start = TimeZoneInfo.ConvertTimeFromUtc(DateTimeOffset.FromUnixTimeSeconds(history.StartTime).UtcDateTime, TimeZoneInfo.Local);
        var dateTime = new LocString("run_history", "INFO.DATE_TIME");
        dateTime.Add("Date", start.ToString(new LocString("run_history", "INFO.DATE_FORMAT").GetRawText(), culture.DateTimeFormat));
        dateTime.Add("Time", start.ToString(new LocString("run_history", "INFO.TIME_FORMAT").GetRawText(), culture.DateTimeFormat));
        d["date"] = dateTime.GetFormattedText();
        var seed = new LocString("run_history", "INFO.SEED");
        seed.Add("Seed", history.Seed);
        d["seed"] = seed.GetFormattedText();
        d["build"] = history.BuildId;
        d["run_time"] = TimeFormatting.Format(history.RunTime);

        // the players (NRunHistoryPlayerIcon); the first is shown (single player)
        var players = new List<Dictionary<string, object?>>();
        foreach (var p in history.Players)
        {
            var c = SaveUtil.CharacterOrDeprecated(p.Character);
            bool locked = history.GameMode.AreAchievementsAndEpochsLocked();
            List<Dictionary<string, object?>>? tips;
            if (history.Ascension > 0 || locked)
                tips = Tips(() => new MegaCrit.Sts2.Core.HoverTips.IHoverTip[] { AscensionHelper.GetHoverTip(c, history.Ascension, locked) });
            else
            {
                var hover = new LocString("run_history", "PLAYER_HOVER");
                hover.Add("PlayerName", c.Title.GetFormattedText());
                hover.Add("CharacterName", string.Empty);
                tips = Tips(() => new MegaCrit.Sts2.Core.HoverTips.IHoverTip[] { new MegaCrit.Sts2.Core.HoverTips.HoverTip(hover) });
            }
            players.Add(new Dictionary<string, object?>
            {
                ["character"] = c.Id.Entry.ToLowerInvariant(),
                ["ascension"] = history.Ascension,
                ["achievements_locked"] = locked,
                ["tips"] = tips,
            });
        }
        d["players"] = players;
        var player = history.Players.FirstOrDefault();
        if (player == null) return d;
        var character = SaveUtil.CharacterOrDeprecated(player.Character);
        var unlockState = save.GenerateUnlockStateFromProgress();
        var owner = Player.CreateForNewRun(character, unlockState, player.Id);

        // LoadGoldHpAndPotionInfo
        if (!history.MapPointHistory.Any() || !history.MapPointHistory.Last().Any())
        {
            d["hp"] = $"{character.StartingHp}/{character.StartingHp}";
            d["gold"] = $"{character.StartingGold}";
        }
        else
        {
            var last = history.MapPointHistory.Last().Last().PlayerStats.First(s => s.PlayerId == player.Id);
            d["hp"] = $"{last.CurrentHp}/{last.MaxHp}";
            d["gold"] = $"{last.CurrentGold}";
        }
        d["potion_slots"] = player.MaxPotionSlotCount;
        d["potions"] = player.Potions.Select(PotionModel.FromSerializable).Select(pm => pm.Id.Entry).ToList();

        // LoadDeathQuote
        var quote = new StringBuilder(new LocString("game_over_screen", "ENCOUNTER_QUOTE_LEFT").GetRawText());
        var rng = new MegaCrit.Sts2.Core.Random.Rng(StringHelper.GetDeterministicHashCode(history.Seed));
        string? quoteColor = null;
        if (history.Win)
        {
            quoteColor = "green";
            var ls = RandomWithPrefix("run_history", "MAP_POINT_HISTORY.falseVictory", rng);
            character.AddDetailsTo(ls);
            quote.Append(ls.GetFormattedText());
        }
        else if (history.WasAbandoned)
        {
            quoteColor = "red";
            var ls = RandomWithPrefix("run_history", "MAP_POINT_HISTORY.abandon", rng);
            character.AddDetailsTo(ls);
            quote.Append(ls.GetFormattedText());
        }
        else if (history.KilledByEncounter != ModelId.none)
        {
            quoteColor = "red";
            quote.Append(SaveUtil.EncounterOrDeprecated(history.KilledByEncounter).GetLossMessageFor(character).GetFormattedText());
        }
        else if (history.KilledByEvent != ModelId.none)
        {
            quoteColor = "red";
            EventModel ev;
            try { ev = ModelDb.GetById<EventModel>(history.KilledByEvent); }
            catch (ModelNotFoundException) { ev = ModelDb.Event<MegaCrit.Sts2.Core.Models.Events.DeprecatedEvent>(); }
            string key = ev.Id.Entry + ".loss";
            var ls = LocString.Exists("events", key) ? new LocString("events", key) : new LocString("run_history", "DEFAULT_EVENT_LOSS_MESSAGE");
            character.AddDetailsTo(ls);
            ls.Add("event", ev.Title);
            quote.Append(ls.GetFormattedText());
        }
        quote.Append(new LocString("game_over_screen", "ENCOUNTER_QUOTE_RIGHT").GetRawText());
        d["quote"] = quote.ToString();
        d["quote_color"] = quoteColor;

        // LoadBadges (NBadge.Create(id, rarity))
        d["badges"] = player.Badges.Select(b => BadgeView(b.Id, b.Rarity.ToString())).ToList();

        // NRelicHistory.LoadRelics
        var relicCounts = Enum.GetValues<RelicRarity>().ToDictionary(r => r, _ => 0);
        var relics = new List<Dictionary<string, object?>>();
        foreach (var sr in player.Relics)
        {
            RelicModel relic;
            try { relic = RelicModel.FromSerializable(sr); }
            catch (ModelNotFoundException) { relic = ModelDb.Relic<DeprecatedRelic>().ToMutable(); }
            relic.Owner = owner;
            relicCounts[relic.Rarity]++;
            var rd = InspectRelicView(relic, unlockState.Relics.ToHashSet());
            rd["floor"] = relic.FloorAddedToDeck;
            rd["tips"] = Tips(() => relic.HoverTips);
            relics.Add(rd);
        }
        var relicHeader = new LocString("run_history", "RELIC_HISTORY.header");
        relicHeader.Add("totalRelics", player.Relics.Count());
        var relicCats = new LocString("run_history", "RELIC_HISTORY.categories");
        foreach (var kv in relicCounts) relicCats.Add(kv.Key + "Relics", kv.Value);
        d["relics_header"] = "[gold][b]" + relicHeader.GetFormattedText() + "[/b][/gold]" + relicCats.GetFormattedText().Trim(',');
        d["relics"] = relics;

        // NDeckHistory.LoadDeck
        var deck = player.Deck.ToList();
        var cardCounts = Enum.GetValues<CardRarity>().ToDictionary(r => r, _ => 0);
        foreach (var sc in deck) cardCounts[SaveUtil.CardOrDeprecated(sc.Id).Rarity]++;
        var deckHeader = new LocString("run_history", "DECK_HISTORY.header");
        deckHeader.Add("totalCards", deck.Count);
        var cardCats = new LocString("run_history", "DECK_HISTORY.categories");
        foreach (var kv in cardCounts) cardCats.Add(kv.Key + "Cards", kv.Value);
        d["deck_header"] = "[gold][b]" + deckHeader.GetFormattedText() + "[/b][/gold]" + cardCats.GetFormattedText().Trim(',');
        var entries = new List<Dictionary<string, object?>>();
        foreach (var group in deck.GroupBy(x => x))
        {
            var card = CardModel.FromSerializable(group.Key);
            card.Owner = owner;
            var c = card.Pool.DeckEntryCardColor;
            var e = WithCardView(card, PileType.None, new Dictionary<string, object?>
            {
                ["id"] = card.Id.ToString(),
                ["name"] = card.Title,
                ["amount"] = group.Count(),
                ["type"] = card.Type.ToString(),
                ["rarity"] = card.Rarity.ToString(),
                ["upgraded"] = card.CurrentUpgradeLevel >= 1,
                ["enchanted"] = card.Enchantment != null,
                ["enchantment_icon"] = ResPath(card.Enchantment?.IntendedIconPath),
                ["deck_color"] = new[] { c.R, c.G, c.B, c.A },
                ["floors"] = group.Where(x => x.FloorAddedToDeck.HasValue).Select(x => x.FloorAddedToDeck!.Value).ToList(),
            });
            e["inspect"] = InspectCardView(card);
            entries.Add(e);
        }
        d["deck"] = entries;

        // NMapPointHistory.LoadHistory / SetPlayer
        var acts = new List<Dictionary<string, object?>>();
        int floor = 1;
        for (int i = 0; i < history.MapPointHistory.Count; i++)
        {
            var points = new List<Dictionary<string, object?>>();
            foreach (var entry in history.MapPointHistory[i])
            {
                var first = entry.Rooms.First();
                bool modelIcon = entry.MapPointType == MapPointType.Boss || entry.MapPointType == MapPointType.Ancient;
                var stats = entry.GetEntry(player.Id);
                points.Add(new Dictionary<string, object?>
                {
                    ["floor"] = floor,
                    ["icon"] = ResPath(ImageHelper.GetRoomIconPath(entry.MapPointType, first.RoomType, modelIcon ? first.ModelId : null)),
                    ["outline"] = ResPath(ImageHelper.GetRoomIconOutlinePath(entry.MapPointType, first.RoomType, modelIcon ? first.ModelId : null)),
                    ["quest"] = stats.CompletedQuests.Count > 0 || stats.IsAffectedByFurCoat,
                    ["tip"] = MapPointTip(entry, floor, player.Id),
                });
                floor++;
            }
            acts.Add(new Dictionary<string, object?>
            {
                ["title"] = SaveUtil.ActOrDeprecated(history.Acts[i]).Title.GetFormattedText(),
                ["points"] = points,
            });
        }
        d["acts"] = acts;
        return d;
    }

    /// <summary>LocString.GetRandomWithPrefix on the loaded tables (the
    /// adapter's LocTable hook answers no keys there).</summary>
    private static LocString RandomWithPrefix(string table, string prefix, MegaCrit.Sts2.Core.Random.Rng rng)
    {
        var keys = LocSetup.RawByTable.TryGetValue(table, out var t)
            ? t.Keys.Where(k => k.StartsWith(prefix)).Distinct().ToList()
            : new List<string>();
        var key = rng.NextItem(keys);
        return new LocString(table, key ?? prefix);
    }

    /// <summary>NInspectCardScreen.UpdateCardDisplay's two cards: the card
    /// downgraded, and its upgrade preview (null when it has none).</summary>
    internal static Dictionary<string, object?> InspectCardView(CardModel card)
    {
        CardModel plain;
        try
        {
            plain = (CardModel)card.MutableClone();
            for (int i = 0; i < 10 && plain.IsUpgraded; i++) plain.DowngradeInternal();
        }
        catch { plain = ModelDb.GetById<CardModel>(card.Id).ToMutable(); }
        return new Dictionary<string, object?>
        {
            ["base"] = WithCardView(plain, PileType.None, new Dictionary<string, object?>
            {
                ["id"] = plain.Id.ToString(),
                ["type"] = plain.Type.ToString(),
                ["rarity"] = plain.Rarity.ToString(),
            }),
            ["upgraded"] = UpgradePreviewView(plain),
            ["start_upgraded"] = card.IsUpgraded,
            ["upgradable"] = card.MaxUpgradeLevel > 0,
        };
    }

    /// <summary>NBadge.Create(id, rarity): its base, icon and tip.</summary>
    internal static Dictionary<string, object?> BadgeView(string id, string rarity)
    {
        string prefix = rarity switch { "Bronze" => "bronze", "Silver" => "silver", "Gold" => "gold", _ => "ERROR" };
        bool ranked = LocString.Exists("badges", id + "." + prefix + "Title");
        var title = new LocString("badges", ranked ? id + "." + prefix + "Title" : id + ".title");
        var desc = new LocString("badges", ranked ? id + "." + prefix + "Description" : id + ".description");
        return new Dictionary<string, object?>
        {
            ["id"] = id,
            ["base"] = prefix == "ERROR" ? "images/atlases/power_atlas.sprites/missing_power.tres" : "images/ui/game_over_screen/badge_" + prefix + ".png",
            ["icon"] = "images/ui/game_over_screen/badge_" + id.ToLowerInvariant() + ".png",
            ["title"] = Text(() => title.GetFormattedText()),
            ["description"] = Text(() => desc.GetFormattedText()),
        };
    }

    /// <summary>NInspectRelicScreen.UpdateRelicDisplay's data for a relic.</summary>
    internal Dictionary<string, object?> InspectRelicView(RelicModel relic, HashSet<RelicModel> unlocked)
    {
        var canonical = relic.CanonicalInstance;
        bool isUnlocked = unlocked.Contains(canonical);
        bool seen = SaveManager.Instance.IsRelicSeen(relic);
        var d = new Dictionary<string, object?>
        {
            ["id"] = relic.Id.Entry,
            ["icon_base"] = RelicIconBase(relic),
            ["rarity"] = relic.Rarity.ToString(),
            ["unlocked"] = isUnlocked,
            ["seen"] = seen,
        };
        if (isUnlocked && seen)
        {
            d["title"] = Text(() => relic.Title.GetFormattedText());
            d["rarity_text"] = Text(() => new LocString("gameplay_ui", "RELIC_RARITY." + relic.Rarity.ToString().ToUpperInvariant()).GetFormattedText());
            d["description"] = Text(() => relic.DynamicDescription.GetFormattedText());
            d["flavor"] = Text(() => relic.Flavor.GetFormattedText());
            d["inspect_tips"] = Tips(() => relic.HoverTipsExcludingRelic);
        }
        return d;
    }

    /// <summary>NMapPointHistoryHoverTip's texts for a map point.</summary>
    private Dictionary<string, object?> MapPointTip(MapPointHistoryEntry entry, int floorNum, ulong playerId)
    {
        LocString L(string table, string key) => new LocString(table, key);
        var tip = new Dictionary<string, object?>();
        var title = L("run_history", "MAP_POINT_HISTORY.header");
        title.Add("FloorNum", floorNum);
        tip["title"] = Text(() => title.GetFormattedText());
        tip["rewards_header"] = Text(() => L("run_history", "HISTORY_ENTRY.rewardsHeader").GetFormattedText());
        tip["skipped_header"] = Text(() => L("run_history", "HISTORY_ENTRY.skippedHeader").GetFormattedText());
        var first = entry.Rooms.First();
        string? kind = entry.MapPointType switch
        {
            MapPointType.Shop => "ROOM_MERCHANT",
            MapPointType.Treasure => "ROOM_TREASURE",
            MapPointType.RestSite => "ROOM_REST",
            MapPointType.Monster => "ROOM_ENEMY",
            MapPointType.Elite => "ROOM_ELITE",
            MapPointType.Boss => "ROOM_BOSS",
            MapPointType.Ancient => "ROOM_ANCIENT",
            _ => null,
        };
        if (entry.MapPointType == MapPointType.Unknown)
            kind = first.RoomType switch
            {
                RoomType.Monster => "ROOM_UNKNOWN_ENEMY",
                RoomType.Treasure => "ROOM_UNKNOWN_TREASURE",
                RoomType.Shop => "ROOM_UNKNOWN_MERCHANT",
                RoomType.Elite => "ROOM_UNKNOWN_ELITE",
                RoomType.Event => "ROOM_EVENT",
                _ => null,
            };
        var room = L("run_history", "MAP_POINT_HISTORY.room_stats");
        room.Add("MapPointType", L("static_hover_tips", kind + ".title"));
        switch (first.RoomType)
        {
            case RoomType.Event: room.Add("ModelTitle", SaveUtil.EventOrDeprecated(first.ModelId).Title); break;
            case RoomType.Treasure:
            case RoomType.Shop:
            case RoomType.RestSite: room.Add("ModelTitle", ""); break;
            default: room.Add("ModelTitle", SaveUtil.EncounterOrDeprecated(first.ModelId).Title); break;
        }
        tip["room"] = Text(() => room.GetFormattedText());
        var pe = entry.PlayerStats.FirstOrDefault(e => e.PlayerId == playerId);
        if (pe == null) return tip;
        var ps = L("run_history", "MAP_POINT_HISTORY.player_stats");
        ps.Add("HP", pe.CurrentHp);
        ps.Add("MaxHP", pe.MaxHp);
        ps.Add("Gold", pe.CurrentGold);
        tip["player"] = Text(() => ps.GetFormattedText());

        // PopulateActionStats
        var sb = new StringBuilder();
        void Line(LocString ls, bool tab = true) { sb.Append(tab ? "\t" : "").Append(ls.GetFormattedText()).Append('\n'); }
        if (entry.MapPointType == MapPointType.Ancient)
        {
            var picked = pe.GetAncientPickedChoiceLoc();
            if (picked != null) { var c = L("run_history", "MAP_POINT_HISTORY.chose"); c.Add("Choice", picked); Line(c); }
            foreach (var skipped in pe.GetAncientSkippedChoiceLoc())
            { var c = L("run_history", "MAP_POINT_HISTORY.skipped"); c.Add("Choice", skipped); Line(c); }
        }
        else
        {
            var eventId = entry.FirstRoomOfType(RoomType.Event)?.ModelId;
            var ev = eventId != null ? SaveUtil.EventOrDeprecated(eventId) : null;
            if (ev != null)
                foreach (var choice in pe.EventChoices)
                {
                    ev.DynamicVars.AddTo(choice.Title);
                    if (choice.Variables != null)
                        foreach (var kv in choice.Variables) choice.Title.AddObj(kv.Key, kv.Value);
                    var c = L("run_history", "MAP_POINT_HISTORY.chose");
                    c.Add("Choice", choice.Title.GetFormattedText());
                    Line(c);
                }
        }
        foreach (var rest in pe.RestSiteChoices)
        {
            var c = L("run_history", "MAP_POINT_HISTORY.chose");
            c.Add("Choice", L("rest_site_ui", "OPTION_" + rest + ".name").GetFormattedText() ?? "");
            Line(c);
        }
        var combat = entry.Rooms.FirstOrDefault(r => r.RoomType.IsCombatRoom());
        if (pe.IsAffectedByFurCoat) sb.Append('\t').Append(FurCoat.HistoryEntry.GetFormattedText()).Append('\n');
        if (pe.MaxHpLost > 0) { var c = L("run_history", "MAP_POINT_HISTORY.maxHpLost"); c.Add("HP", pe.MaxHpLost); Line(c); }
        if (pe.DamageTaken > 0 || combat != null) { var c = L("run_history", "MAP_POINT_HISTORY.damageTaken"); c.Add("Damage", pe.DamageTaken); Line(c); }
        if (pe.MaxHpGained > 0) { var c = L("run_history", "MAP_POINT_HISTORY.maxHpGained"); c.Add("HP", pe.MaxHpGained); Line(c); }
        if (pe.HpHealed > 0) { var c = L("run_history", "MAP_POINT_HISTORY.healed"); c.Add("HP", pe.HpHealed); Line(c); }
        if (combat != null) { var c = L("run_history", "MAP_POINT_HISTORY.turnsTaken"); c.Add("Turns", combat.TurnsTaken); Line(c); }
        foreach (var q in pe.CompletedQuests)
        { var c = L("run_history", "MAP_POINT_HISTORY.questCompleted"); c.Add("Quest", SaveUtil.CardOrDeprecated(q).Title); Line(c); }
        foreach (var p in pe.PotionUsed)
        {
            var c = L("run_history", "HISTORY_ENTRY.used");
            c.Add("Icon", FontIcon + "potion_icon.png[/img]");
            c.Add("Title", SaveUtil.PotionOrDeprecated(p).Title);
            Line(c, false);
        }
        foreach (var p in pe.PotionDiscarded)
        {
            var c = L("run_history", "HISTORY_ENTRY.removed");
            c.Add("Icon", FontIcon + "potion_icon.png[/img]");
            c.Add("Title", SaveUtil.PotionOrDeprecated(p).Title);
            Line(c, false);
        }
        if (pe.GoldSpent > 0) { var c = L("run_history", "HISTORY_ENTRY.goldSpent"); c.Add("Amount", pe.GoldSpent); Line(c, false); }
        if (pe.GoldLost > 0) { var c = L("run_history", "HISTORY_ENTRY.goldLost"); c.Add("Amount", pe.GoldLost); Line(c, false); }
        if (pe.GoldStolen > 0)
        {
            var c = L("run_history", "HISTORY_ENTRY.goldStolen");
            c.Add("Icon", FontIcon + "gold_icon.png[/img]");
            c.Add("Amount", pe.GoldStolen);
            Line(c, false);
        }
        tip["actions"] = sb.ToString().Trim('\n');

        // PopulateRewardAndSkippedEntries
        var rewards = new List<string>();
        var skippedList = new List<string>();
        string Obtained(string icon, object titleLs)
        {
            var c = L("run_history", "HISTORY_ENTRY.obtained");
            c.Add("Icon", FontIcon + icon + "[/img]");
            if (titleLs is LocString tls) c.Add("Title", tls); else c.Add("Title", titleLs?.ToString() ?? "");
            return c.GetFormattedText();
        }
        string With(string key, string icon, object titleLs)
        {
            var c = L("run_history", key);
            c.Add("Icon", FontIcon + icon + "[/img]");
            if (titleLs is LocString tls) c.Add("Title", tls); else c.Add("Title", titleLs?.ToString() ?? "");
            return c.GetFormattedText() ?? "";
        }
        if (pe.GoldGained > 0)
        {
            var c = L("run_history", "HISTORY_ENTRY.goldGained");
            c.Add("Icon", FontIcon + "gold_icon.png[/img]");
            c.Add("Amount", pe.GoldGained);
            rewards.Add(c.GetFormattedText());
        }
        foreach (var card in pe.CardsGained) rewards.Add(Obtained("card_icon.png", CardModel.FromSerializable(card).Title));
        foreach (var choice in pe.CardChoices)
            if (!choice.wasPicked) skippedList.Add(Obtained("card_icon.png", CardModel.FromSerializable(choice.Card).Title));
        foreach (var choice in pe.RelicChoices)
            (choice.wasPicked ? rewards : skippedList).Add(Obtained("chest_icon.png", SaveUtil.RelicOrDeprecated(choice.choice).Title));
        foreach (var choice in pe.PotionChoices)
            (choice.wasPicked ? rewards : skippedList).Add(Obtained("potion_icon.png", SaveUtil.PotionOrDeprecated(choice.choice).Title));
        foreach (var card in pe.CardsRemoved) rewards.Add(With("HISTORY_ENTRY.removed", "card_icon.png", CardModel.FromSerializable(card).Title));
        foreach (var r in pe.RelicsRemoved) rewards.Add(With("HISTORY_ENTRY.removed", "chest_icon.png", SaveUtil.RelicOrDeprecated(r).Title));
        foreach (var c in pe.UpgradedCards) rewards.Add(With("HISTORY_ENTRY.upgraded", "card_icon.png", SaveUtil.CardOrDeprecated(c).Title));
        foreach (var c in pe.DowngradedCards) rewards.Add(With("HISTORY_ENTRY.downgraded", "card_icon.png", SaveUtil.CardOrDeprecated(c).Title));
        foreach (var e in pe.CardsEnchanted)
        {
            var c = L("run_history", "HISTORY_ENTRY.enchanted");
            c.Add("Icon", FontIcon + "card_icon.png[/img]");
            c.Add("Title1", CardModel.FromSerializable(e.Card).Title);
            c.Add("Title2", SaveUtil.EnchantmentOrDeprecated(e.Enchantment).Title);
            rewards.Add(c.GetFormattedText() ?? "");
        }
        foreach (var t in pe.CardsTransformed)
        {
            var c = L("run_history", "HISTORY_ENTRY.transformed");
            c.Add("Icon", FontIcon + "card_icon.png[/img]");
            c.Add("Title1", CardModel.FromSerializable(t.OriginalCard).Title);
            c.Add("Title2", CardModel.FromSerializable(t.FinalCard).Title);
            rewards.Add(c.GetFormattedText() ?? "");
        }
        tip["rewards"] = rewards;
        tip["skipped"] = skippedList;
        return tip;
    }
}
