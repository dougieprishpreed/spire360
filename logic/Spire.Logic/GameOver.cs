// What the game over screen (NGameOverScreen) shows besides the outcome:
// the encounter's quote (NRunHistory.GetDeathQuote) and the run summary's
// score lines (AnimateScoreLines), the victory text.
using MegaCrit.Sts2.Core.Localization;
using MegaCrit.Sts2.Core.Rooms;
using MegaCrit.Sts2.Core.Runs;

namespace Spire.Logic;

public partial class RunDriver
{
    private void AddGameOverDetails(Dictionary<string, object?> d, bool won)
    {
        var rs = _runState!;
        var player = rs.Players[0];
        var history = rs.MapPointHistory;
        try
        {
            var lines = new List<Dictionary<string, object?>>();
            void Line(string key, string var, int amount, string score, string icon)
            {
                var ls = new LocString("game_over_screen", key);
                ls.Add(var, amount);
                lines.Add(new Dictionary<string, object?>
                {
                    ["text"] = ls.GetFormattedText(),
                    ["score"] = score,
                    ["icon"] = "images/ui/game_over_screen/" + icon + ".png",
                });
            }
            Line("SCORE_LINE.floorsClimbed", "FloorCount", rs.TotalFloor, $"+{ScoreUtility.GetScoreForFloor(history)}", "score_floor");
            int gold = history.SelectMany(a => a).Sum(e => e.GetEntry(player.NetId).GoldGained);
            Line("SCORE_LINE.goldGained", "GoldAmount", gold, $"+{ScoreUtility.GetScoreForGoldGained(history, rs.Players.Count)}", "score_gold");
            int elites = ScoreUtility.GetElitesKilledCount(history);
            if (elites > 0)
                Line("SCORE_LINE.elitesKilled", "EliteCount", elites, $"+{ScoreUtility.GetScoreForElitesKilled(elites)}", "score_elite");
            int bosses = ScoreUtility.GetBossesSlainCount(history, won);
            if (bosses > 0)
                Line("SCORE_LINE.bossesSlain", "BossCount", bosses, $"+{ScoreUtility.GetScoreForBossesSlain(bosses)}", "score_boss");
            int asc = rs.AscensionLevel;
            if (asc > 0)
            {
                int whole = asc / 10 + 1, tenth = asc % 10;
                Line("SCORE_LINE.ascension", "AscensionLevel", asc, "x" + (tenth != 0 ? $"{whole}.{tenth}" : $"{whole}"), "score_ascension");
            }
            d["score_lines"] = lines;
            d["score"] = ScoreUtility.CalculateScore(rs, won);
        }
        catch (Exception ex) { Log($"Score lines: {ex.Message}"); }

        try
        {
            if (won)
            {
                var ls = new LocString("game_over_screen", "VICTORY_DAMAGE_LOCAL");
                ls.Add("PlayerDamage", d.TryGetValue("score", out var s) && s is int si ? si : 0);
                long personal = 0;
                try { personal = MegaCrit.Sts2.Core.Platform.StatsManager.GetPersonalArchitectDamage(); } catch { }
                ls.Add("PersonalDamage", personal);
                d["victory_text"] = ls.GetFormattedText();
            }
            else if (rs.CurrentRoom is CombatRoom room)
            {
                // NRunHistory.GetDeathQuote for a death in a fight.
                var loss = room.Encounter.GetLossMessageFor(player.Character!);
                d["death_quote"] = new LocString("game_over_screen", "ENCOUNTER_QUOTE_LEFT").GetRawText()
                    + loss.GetFormattedText()
                    + new LocString("game_over_screen", "ENCOUNTER_QUOTE_RIGHT").GetRawText();
            }
        }
        catch (Exception ex) { Log($"Game over text: {ex.Message}"); }
    }
}
