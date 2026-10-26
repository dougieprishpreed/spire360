// The rewards screen (NRewardsScreen) as a decision: after a fight (terminal,
// the room's rewards) and when an event offers rewards (RewardsSet.Offer,
// non-terminal). The player takes rewards one at a time in any order; a card
// reward opens the card reward decision; Proceed skips what is left.
using MegaCrit.Sts2.Core.Entities.Cards;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Rewards;
using MegaCrit.Sts2.Core.Rooms;
using MegaCrit.Sts2.Core.Runs;

namespace Spire.Logic;

public partial class RunDriver
{
    // The rewards on the screen, in the set's order; claimed ones leave it.
    private List<Reward>? _rewardList;
    // Rewards the player tried and could not take (potion belt full) or
    // whose card screen they skipped: they stay, marked.
    private readonly HashSet<Reward> _rewardSkipped = new();
    // A reward being taken that waits on a later command (a card choice).
    private Task<bool>? _rewardTask;
    private Reward? _rewardTaking;
    // An event's set (RewardsSet.Offer): the game waits on _rewardSetDone.
    private RewardsSet? _rewardSet;
    private TaskCompletionSource? _rewardSetDone;

    // Set while the driver runs a room's rewards itself (the treasure room
    // until its screen is ported): every reward taken, no decision.
    private bool _autoRewards;

    private static Task OfferRewardsHeadless(RewardsSet set)
    {
        var driver = Hooks.Driver;
        if (driver == null || driver._autoRewards)
            return OfferRewardsAll(set);
        // A fight's own rewards (CombatRoom.OfferRoomEndRewards: a continued
        // run's finished fight, StartPreFinishedCombat): the driver's
        // post-combat rewards screen, terminal until Proceed, not offered
        // again once they are taken
        if (set.Room is CombatRoom room && ReferenceEquals(room, driver._runState?.CurrentRoom))
        {
            // taken through the driver's list: not complete when this returns
            set.ThrowInTestIfRewardsNotTaken = false;
            driver.ClearRewards();
            driver._goldBeforeCombat = set.Player.Gold;
            driver._rewardList = set.Rewards.ToList();
            return Task.CompletedTask;
        }
        return driver.BeginRewardsSet(set);
    }

    // No driver (tests): every reward taken in turn, as the game's test path.
    private static async Task OfferRewardsAll(RewardsSet set)
    {
        var sync = RunManager.Instance.RewardsSetSynchronizer;
        foreach (var reward in set.Rewards.ToList())
            await sync.SelectLocalReward(reward);
        if (!sync.IsRewardsSetCompleted(set))
            sync.SkipLocalRewardsSet();
    }

    private Task BeginRewardsSet(RewardsSet set)
    {
        _rewardSet = set;
        _rewardSetDone = new TaskCompletionSource();
        _rewardList = set.Rewards.ToList();
        _rewardSkipped.Clear();
        Log($"Rewards set offered: {_rewardList.Count} rewards");
        return _rewardSetDone.Task;
    }

    private void ClearRewards()
    {
        _rewardList = null;
        _rewardSkipped.Clear();
        _rewardTask = null;
        _rewardTaking = null;
        _rewardSet = null;
        _rewardSetDone = null;
    }

    /// <summary>
    /// The rewards decision, or null when there is none (an event's set
    /// finished: the game goes on).
    /// </summary>
    private Dictionary<string, object?>? PendingRewardsState()
    {
        if (_rewardList == null) return null;
        if (_rewardTask != null)
        {
            // A reward taken earlier finished (or never will: treated as not taken).
            bool ok = _rewardTask.IsCompletedSuccessfully && _rewardTask.Result;
            if (!_rewardTask.IsCompleted) Log("Reward still pending after its choice; left on the screen");
            if (ok) _rewardList.Remove(_rewardTaking!);
            else _rewardSkipped.Add(_rewardTaking!);
            _rewardTask = null;
            _rewardTaking = null;
        }
        if (_rewardSet != null)
        {
            var sync = RunManager.Instance.RewardsSetSynchronizer;
            if (sync.IsRewardsSetCompleted(_rewardSet))
            {
                FinishRewardsSet();
                return null;
            }
        }
        return RewardsState();
    }

    private void FinishRewardsSet()
    {
        var done = _rewardSetDone;
        ClearRewards();
        done?.TrySetResult();
        Sched.Pump();
        WaitForActionExecutor();
    }

    private Dictionary<string, object?> RewardsState()
    {
        var player = _runState!.Players[0];
        var rewards = _rewardList!.Select((r, i) =>
        {
            var d = new Dictionary<string, object?>
            {
                ["index"] = i,
                ["type"] = RewardTypeName(r),
                ["label"] = SafeText(() => r.Description.GetFormattedText()),
                ["icon"] = RewardIconPath(r),
                ["skipped"] = _rewardSkipped.Contains(r),
            };
            // NRewardButton.OnFocus: the reward's hover tips (its relic's,
            // potion's or card's, a linked set's)
            d["tips"] = Tips(() => r.HoverTips);
            if (r is GoldReward g) d["gold"] = g.Amount;
            if (r is PotionReward p && p.Potion != null) d["potion_id"] = p.Potion.Id.Entry;
            if (r is RelicReward rr && rr.ClaimedRelic == null)
            {
                var relic = RewardRelic(rr);
                if (relic != null)
                {
                    d["relic_id"] = relic.Id.Entry;
                    d["relic_icon_base"] = RelicIconBase(relic);
                }
            }
            return d;
        }).ToList();
        var state = new Dictionary<string, object?>
        {
            ["type"] = "decision",
            ["decision"] = "rewards",
            ["context"] = RunContext(),
            ["terminal"] = _rewardSet == null,
            ["can_skip"] = _rewardSet == null || !_rewardSet.DisallowSkipping,
            ["rewards"] = rewards,
            ["player"] = PlayerSummary(player),
        };
        var sphere = CrystalSphereBehind();
        if (sphere != null) state["crystal_sphere"] = sphere;
        // CombatRoom.StartPreFinishedCombat (a continued run): NCombatRoom in
        // FinishedCombat mode behind the rewards, its background without the
        // enemies; the client has no fight decision to build it from
        if (_runState.CurrentRoom is CombatRoom) state["room_view"] = CombatView();
        // NRewardsScreen's header after a combat in which loot was stolen
        try
        {
            var rs = player.RunState;
            if (rs?.CurrentRoom is MegaCrit.Sts2.Core.Rooms.CombatRoom && rs.CurrentMapPointHistoryEntry?.GetEntry(player.NetId).WasMugged == true)
                state["mugged"] = true;
        }
        catch { }
        return state;
    }

    private static string SafeText(Func<string?> f)
    {
        try { return f() ?? ""; }
        catch { return ""; }
    }

    private static RelicModel? RewardRelic(RelicReward r)
    {
        try { return r.Relic; }
        catch { return null; }
    }

    private static string RewardTypeName(Reward r) => r switch
    {
        GoldReward => "gold",
        PotionReward => "potion",
        RelicReward => "relic",
        SpecialCardReward => "special_card",
        CardReward => "card",
        CardRemovalReward => "card_removal",
        LinkedRewardSet => "linked",
        _ => "other",
    };

    // Reward.IconPath (protected) as the game picks it.
    [System.Diagnostics.CodeAnalysis.DynamicDependency("get_IconPath", typeof(CardReward))]
    private static string? RewardIconPath(Reward r)
    {
        const string dir = "res://images/ui/reward_screen/";
        switch (r)
        {
            case GoldReward: return dir + "reward_icon_money.png";
            case SpecialCardReward: return dir + "reward_icon_special_card.png";
            case CardRemovalReward: return dir + "reward_icon_card_removal.png";
            case CardReward cr:
            {
                // CardReward.IconPath itself (its Options: a combat's reward
                // has creation flags, which ToSerializable refuses)
                try
                {
                    if (typeof(CardReward).GetProperty("IconPath", System.Reflection.BindingFlags.Instance |
                            System.Reflection.BindingFlags.NonPublic)?.GetValue(cr) is string own && own.Length > 0)
                        return own;
                }
                catch { }
                try
                {
                    var s = cr.ToSerializable();
                    if (s.Source == MegaCrit.Sts2.Core.Runs.CardCreationSource.Encounter
                        && s.RarityOdds == MegaCrit.Sts2.Core.Runs.CardRarityOddsType.BossEncounter)
                        return dir + "reward_icon_rare.png";
                    var options = new CardCreationOptions(s.CardPoolIds.Select(ModelDb.GetById<CardPoolModel>), s.Source, s.RarityOdds);
                    if (options.TryGetSingleRarityInPool().HasValue)
                    {
                        var first = cr.Cards.FirstOrDefault();
                        if (first?.Rarity == CardRarity.Rare) return dir + "reward_icon_rare.png";
                        if (first?.Rarity == CardRarity.Uncommon) return dir + "reward_icon_uncommon.png";
                    }
                }
                catch { }
                return dir + "reward_icon_card.png";
            }
        }
        return null;
    }

    private Dictionary<string, object?> DoTakeReward(Player player, Dictionary<string, object?>? args)
    {
        if (_rewardList == null)
            return Error("No rewards to take");
        if (args == null || !args.ContainsKey("reward_index"))
            return Error("take_reward requires 'reward_index'");
        var idx = Convert.ToInt32(args["reward_index"]);
        if (idx < 0 || idx >= _rewardList.Count)
            return Error($"Invalid reward index {idx}, {_rewardList.Count} rewards");
        var reward = _rewardList[idx];
        Log($"Taking reward {RewardTypeName(reward)}");

        _rewardTaking = reward;
        Task<bool>? task = null;
        var sync = RunManager.Instance.RewardsSetSynchronizer;
        Sched.Call(() => task = _rewardSet != null ? sync.SelectLocalReward(reward) : reward.SelectUnsynchronized());
        try { Sched.Finish(task!); }
        catch (Exception ex) { Log($"Take reward: {ex.Message}"); }
        _rewardTask = task;
        WaitForActionExecutor();
        return DetectDecisionPoint();
    }

    /// <summary>Proceed on the rewards screen: what is left is skipped.</summary>
    private Dictionary<string, object?>? ProceedFromRewards(Player player)
    {
        if (_rewardList == null) return null;
        Log($"Leaving the rewards screen ({_rewardList.Count} left)");
        if (_rewardSet != null)
        {
            var sync = RunManager.Instance.RewardsSetSynchronizer;
            try
            {
                if (!sync.IsRewardsSetCompleted(_rewardSet))
                    sync.SkipLocalRewardsSet();
            }
            catch (Exception ex) { Log($"Skip rewards set: {ex.Message}"); }
            FinishRewardsSet();
            return DetectDecisionPoint();
        }
        foreach (var r in _rewardList)
        {
            try { r.OnSkipped(); }
            catch (Exception ex) { Log($"Reward OnSkipped: {ex.Message}"); }
        }
        ClearRewards();
        _rewardsProcessed = true;
        return _runState?.CurrentRoom is CombatRoom room ? DetectPostCombatState(player, room) : DetectDecisionPoint();
    }
}
