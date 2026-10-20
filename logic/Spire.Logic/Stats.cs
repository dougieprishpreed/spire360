// The compendium's Statistics (NStatsScreen, NGeneralStatsGrid,
// NCharacterStats): the entries' texts as the game formats them.

using MegaCrit.Sts2.Core.Helpers;
using MegaCrit.Sts2.Core.Localization;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Characters;
using MegaCrit.Sts2.Core.Platform;
using MegaCrit.Sts2.Core.Saves;
using MegaCrit.Sts2.Core.Timeline;

namespace Spire.Logic;

public partial class RunDriver
{
    private const string StatsAtlas = "images/atlases/stats_screen_atlas.sprites/";

    private static Dictionary<string, object?> StatEntry(string icon, string? top = null, string? bottom = null,
        string? tipKey = null)
    {
        var d = new Dictionary<string, object?> { ["icon"] = StatsAtlas + icon + ".tres", ["top"] = top, ["bottom"] = bottom };
        if (tipKey != null)
            d["tip"] = new Dictionary<string, object?>
            {
                ["title"] = new LocString("stats_screen", tipKey + ".header").GetFormattedText(),
                ["description"] = new LocString("stats_screen", tipKey + ".description").GetFormattedText(),
            };
        return d;
    }

    /// <summary>StringHelper.Radix on the logic's language (SettingsSave is
    /// not loaded here).</summary>
    private string Radix(int value)
    {
        string culture = _loc.GameLanguage switch
        {
            "deu" or "dut" or "gre" or "ind" or "ita" or "mal" or "nor" or "por" or "ptb" or "spa" or "tur" or "vie" => "es-ES",
            "pol" or "swe" or "cze" or "fin" or "fra" or "rus" or "ukr" => "fr-FR",
            "ben" or "hin" => "hi-IN",
            _ => "en-US",
        };
        return value.ToString("N0", new System.Globalization.CultureInfo(culture));
    }

    /// <summary>NGeneralStatsGrid.LoadStats and its NCharacterStats.</summary>
    public Dictionary<string, object?> Statistics()
    {
        EnsureModelDbInitialized();
        var save = SaveManager.Instance;
        var p = save.Progress;
        LocString L(string key) => new LocString("stats_screen", key);
        var overall = new List<Dictionary<string, object?>>();

        var ls = L("ENTRY_ACHIEVEMENTS.top");
        int total = 0, unlockedCount = 0;
        try { total = AchievementsUtil.TotalAchievementCount(); unlockedCount = AchievementsUtil.UnlockedAchievementCount(); } catch { }
        ls.Add("Amount", StringHelper.RatioFormat(unlockedCount, total));
        string achTop = ls.GetFormattedText();
        ls = L("ENTRY_ACHIEVEMENTS.bottom");
        int revealed = p.Epochs.Count(e => e.State >= EpochState.Revealed);
        if (EpochModel.AllEpochIds.All(id => p.Epochs.Any(e => e.Id == id)))
            ls.Add("Amount", StringHelper.RatioFormat(revealed, p.Epochs.Count));
        else
            ls.Add("Amount", StringHelper.RatioFormat(revealed.ToString(), "??"));
        overall.Add(StatEntry("stats_achievements", achTop, ls.GetFormattedText()));

        ls = L("ENTRY_PLAYTIME.top");
        ls.Add("Playtime", TimeFormatting.Format(p.TotalPlaytime));
        string playTop = ls.GetFormattedText();
        string? playBottom = null;
        if (p.Wins > 0)
        {
            ls = L("ENTRY_PLAYTIME.bottom");
            ls.Add("FastestWin", TimeFormatting.Format(p.FastestVictory));
            playBottom = ls.GetFormattedText();
        }
        overall.Add(StatEntry("stats_clock", playTop, playBottom, "TIP_PLAYTIME"));

        ls = L("ENTRY_CARDS.top");
        ls.Add("Amount", StringHelper.RatioFormat(save.GetTotalUnlockedCards(), SaveManager.GetUnlockableCardCount()));
        string cardsTop = ls.GetFormattedText();
        ls = L("ENTRY_CARDS.bottom");
        ls.Add("Amount", StringHelper.RatioFormat(p.DiscoveredCards.Count, ModelDb.AllCards.Count()));
        overall.Add(StatEntry("stats_cards", cardsTop, ls.GetFormattedText()));

        int ascension = save.GetAggregateAscensionProgress();
        string? winTop = null;
        if (ascension > 0)
        {
            ls = L("ENTRY_WIN_LOSS.top");
            ls.Add("Amount", StringHelper.RatioFormat(ascension, SaveManager.GetAggregateAscensionCount()));
            winTop = ls.GetFormattedText();
        }
        ls = L("ENTRY_WIN_LOSS.bottom");
        ls.Add("Wins", p.Wins);
        ls.Add("Losses", p.Losses);
        overall.Add(StatEntry("stats_swords", winTop, ls.GetFormattedText(), ascension > 0 ? "TIP_WIN_LOSS" : null));

        ls = L("ENTRY_MONSTER.top");
        ls.Add("Amount", Radix(save.GetTotalKills()));
        string monTop = ls.GetFormattedText();
        ls = L("ENTRY_MONSTER.bottom");
        ls.Add("Amount", StringHelper.RatioFormat(p.EnemyStats.Count, ModelDb.Monsters.Count()));
        overall.Add(StatEntry("stats_monsters", monTop, ls.GetFormattedText()));

        var events = ModelDb.AllEvents.Select(e => e.Id).ToHashSet();
        int eventsSeen = p.DiscoveredEvents.Intersect(events).Count();
        ls = L("ENTRY_RELIC.top");
        ls.Add("Amount", StringHelper.RatioFormat(save.GetTotalUnlockedRelics(), SaveManager.GetUnlockableRelicCount()));
        string relTop = ls.GetFormattedText();
        ls = L("ENTRY_RELIC.bottom");
        ls.Add("Amount", StringHelper.RatioFormat(p.DiscoveredRelics.Count, ModelDb.AllRelics.Count()));
        overall.Add(StatEntry("stats_chest", relTop, ls.GetFormattedText()));

        ls = L("ENTRY_POTION.top");
        ls.Add("Amount", StringHelper.RatioFormat(save.GetTotalUnlockedPotions(), SaveManager.GetUnlockablePotionCount()));
        string potTop = ls.GetFormattedText();
        ls = L("ENTRY_POTION.bottom");
        ls.Add("Amount", ModelDb.AllPotions.Count());
        overall.Add(StatEntry("stats_potions_seen", potTop, ls.GetFormattedText()));

        ls = L("ENTRY_EVENTS.top");
        ls.Add("Amount", "N/A");
        string evTop = ls.GetFormattedText();
        ls = L("ENTRY_EVENTS.bottom");
        ls.Add("Amount", StringHelper.RatioFormat(eventsSeen, events.Count));
        overall.Add(StatEntry("stats_questionmark", evTop, ls.GetFormattedText()));

        ls = L("ENTRY_STREAK.top");
        ls.Add("Amount", p.BestWinStreak);
        overall.Add(StatEntry("stats_chain", ls.GetFormattedText()));

        var characters = new List<Dictionary<string, object?>>();
        foreach (var model in new CharacterModel[] { ModelDb.Character<Ironclad>(), ModelDb.Character<Silent>(),
                     ModelDb.Character<Regent>(), ModelDb.Character<Necrobinder>(), ModelDb.Character<Defect>() })
        {
            var cs = p.GetStatsForCharacter(model.Id);
            if (cs == null) continue;
            ls = L("ENTRY_CHAR_PLAYTIME.top");
            ls.Add("Playtime", TimeFormatting.Format(cs.Playtime));
            string cTop = ls.GetFormattedText();
            string? cBottom = null;
            if (cs.FastestWinTime >= 0)
            {
                ls = L("ENTRY_CHAR_PLAYTIME.bottom");
                ls.Add("FastestWin", TimeFormatting.Format(cs.FastestWinTime));
                cBottom = ls.GetFormattedText();
            }
            string? ascTop = null;
            if (cs.MaxAscension > 0)
            {
                ls = L("ENTRY_CHAR_WIN_LOSS.top");
                ls.Add("Amount", cs.MaxAscension);
                ascTop = "[red]" + ls.GetFormattedText() + "[/red]";
            }
            ls = L("ENTRY_CHAR_WIN_LOSS.bottom");
            ls.Add("Wins", cs.TotalWins);
            ls.Add("Losses", cs.TotalLosses);
            string wl = ls.GetFormattedText();
            ls = L("ENTRY_CHAR_STREAK.top");
            ls.Add("Amount", cs.CurrentWinStreak);
            string sTop = ls.GetFormattedText();
            ls = L("ENTRY_CHAR_STREAK.bottom");
            ls.Add("Amount", cs.BestWinStreak);
            var c = model.NameColor;
            characters.Add(new Dictionary<string, object?>
            {
                ["id"] = model.Id.Entry.ToLowerInvariant(),
                ["name"] = model.Title.GetRawText(),
                ["name_color"] = new[] { c.R, c.G, c.B, c.A },
                ["entries"] = new List<Dictionary<string, object?>>
                {
                    StatEntry("stats_clock", cTop, cBottom),
                    StatEntry("stats_swords", ascTop, wl),
                    StatEntry("stats_chain", sTop, ls.GetFormattedText()),
                },
            });
        }
        return new Dictionary<string, object?>
        {
            ["type"] = "statistics",
            ["stats_tab"] = new LocString("stats_screen", "TAB_STATS.header").GetFormattedText(),
            ["achievements_tab"] = new LocString("stats_screen", "TAB_ACHIEVEMENT.header").GetFormattedText(),
            ["overall_header"] = new LocString("main_menu_ui", "STATISTICS.OVERALL.title").GetFormattedText(),
            ["character_header"] = new LocString("main_menu_ui", "STATISTICS.title").GetFormattedText(),
            ["overall"] = overall,
            ["characters"] = characters,
        };
    }
}
