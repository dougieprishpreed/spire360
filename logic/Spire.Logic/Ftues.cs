// The profile's FTUEs (first-time tutorials): which it has yet to see
// (SaveManager.SeenFtue: none while tutorials are off) and the client's
// marks (NFtue screens call MarkFtueAsComplete; NAcceptTutorialsFtue's No
// also SetFtuesEnabled(false)). Presentation only: the run's logic reads
// them only where the game's model code does (CombatManager's
// combat_rules_ftue).

using MegaCrit.Sts2.Core.Saves;

namespace Spire.Logic;

public partial class RunDriver
{
    /// <summary>The single-player FTUEs, by their ids (NxxxFtue.id).</summary>
    private static readonly string[] FtueIds =
    {
        "accept_tutorials_ftue", "combat_rules_ftue", "shuffle_ftue", "can_play_cards_ftue",
        "cannot_play_card_ftue", "obtain_potion_ftue", "merchant_ftue", "rest_site_ftue",
        "obtain_relic_ftue", "combat_reward_ftue", "power_card_ftue", "map_select_ftue",
    };

    /// <summary>The FTUEs not seen yet, null without a profile (test runs:
    /// the client shows none).</summary>
    internal List<object?>? PendingFtues()
    {
        if (_profileId <= 0) return null;
        var list = new List<object?>();
        foreach (var id in FtueIds)
            if (!SaveManager.Instance.SeenFtue(id)) list.Add(id);
        return list;
    }

    public Dictionary<string, object?> Ftues() => new()
    {
        ["type"] = "ftues",
        ["ftues"] = PendingFtues() ?? new List<object?>(),
    };

    /// <summary>NFtue closed: SaveManager.MarkFtueAsComplete.</summary>
    public Dictionary<string, object?> FtueDone(string id)
    {
        if (_profileId <= 0) return Error("No profile selected");
        SaveManager.Instance.MarkFtueAsComplete(id);
        return Ftues();
    }

    /// <summary>NAcceptTutorialsFtue's answer: the ftue marked seen, and
    /// tutorials off for No (SetFtuesEnabled(false)).</summary>
    public Dictionary<string, object?> AcceptTutorials(bool yes)
    {
        if (_profileId <= 0) return Error("No profile selected");
        SaveManager.Instance.MarkFtueAsComplete("accept_tutorials_ftue");
        if (!yes) SaveManager.Instance.SetFtuesEnabled(false);
        return Ftues();
    }
}
