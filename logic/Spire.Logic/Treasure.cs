using System;
using System.Collections.Generic;
using System.Linq;
using MegaCrit.Sts2.Core.Commands;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.Rooms;
using MegaCrit.Sts2.Core.Runs;

namespace Spire.Logic;

/// <summary>
/// The treasure room as NTreasureRoom plays it: the chest closed (open_chest
/// opens it: the gold and any extra rewards, OpenChest's DoNormalRewards /
/// DoExtraRewardsIfNeeded), then its relic offered (pick_relic takes it;
/// proceed skips it, SkipRelicLocally, and goes to the map), then done
/// (proceed goes to the map).
/// </summary>
public partial class RunDriver
{
    private const int TreasureClosed = 0, TreasureOpen = 1, TreasureDone = 2;
    private int _treasureStage;
    private int _treasureGold;

    private void ResetTreasure()
    {
        _treasureStage = TreasureClosed;
        _treasureGold = 0;
    }

    private Dictionary<string, object?> TreasureState(TreasureRoom treasureRoom)
    {
        WaitForActionExecutor();
        Sched.Pump();
        var player = _runState!.Players[0];
        var relics = new List<Dictionary<string, object?>>();
        if (_treasureStage == TreasureOpen)
        {
            var current = RunManager.Instance.TreasureRoomRelicSynchronizer?.CurrentRelics;
            if (current != null)
            {
                for (int i = 0; i < current.Count; i++)
                {
                    var r = current[i];
                    relics.Add(new Dictionary<string, object?>
                    {
                        ["index"] = i,
                        ["id"] = r.Id.Entry,
                        ["icon_base"] = RelicIconBase(r),
                        ["title"] = SafeText(() => r.Title.GetFormattedText()),
                        ["description"] = SafeText(() => r.DynamicDescription.GetFormattedText()),
                        ["rarity"] = r.Rarity.ToString(),
                    });
                }
            }
        }
        // The act's chest (NTreasureButton.Setup): its skeleton and skins.
        Dictionary<string, object?>? chest = null;
        try
        {
            var act = _runState.Act;
            chest = new Dictionary<string, object?>
            {
                ["skeleton"] = act.ChestSpineResourcePath,
                ["skin"] = act.ChestSpineSkinNameNormal,
                ["skin_outline"] = act.ChestSpineSkinNameStroke,
            };
        }
        catch { }
        return new Dictionary<string, object?>
        {
            ["type"] = "decision",
            ["decision"] = "treasure",
            ["chest"] = chest,
            ["context"] = RunContext(),
            ["stage"] = _treasureStage == TreasureClosed ? "closed" : _treasureStage == TreasureOpen ? "open" : "done",
            ["relics"] = relics,
            ["gold"] = _treasureGold,
            ["player"] = PlayerSummary(player),
        };
    }

    private Dictionary<string, object?> DoOpenChest(Player player)
    {
        if (_runState?.CurrentRoom is not TreasureRoom treasureRoom)
            return Error("open_chest: not in a treasure room");
        if (_treasureStage != TreasureClosed)
            return TreasureState(treasureRoom);
        Log("Treasure room: opening the chest");
        WaitForActionExecutor();
        Sched.Pump();
        int goldBefore = player.Gold;
        // The chest's gold, and the extra rewards a relic may add (taken at
        // once: the treasure room has no rewards screen of its own here).
        _autoRewards = true;
        try
        {
            Sched.Run(() => treasureRoom.DoNormalRewards());
            Sched.Pump();
            Sched.Run(() => treasureRoom.DoExtraRewardsIfNeeded());
            Sched.Pump();
        }
        catch (Exception ex) { Log($"Treasure rewards: {ex.Message}"); }
        finally { _autoRewards = false; }
        WaitForActionExecutor();
        Sched.Pump();
        _treasureGold = Math.Max(0, player.Gold - goldBefore);
        _treasureStage = TreasureOpen;
        var relicSync = RunManager.Instance.TreasureRoomRelicSynchronizer;
        if (relicSync?.CurrentRelics == null || relicSync.CurrentRelics.Count == 0)
        {
            // Nothing offered: the session closed with no relic.
            try
            {
                relicSync?.CompleteWithNoRelics();
                Sched.Pump();
                WaitForActionExecutor();
            }
            catch (Exception ex) { Log($"Treasure: {ex.Message}"); }
            _treasureStage = TreasureDone;
        }
        return TreasureState(treasureRoom);
    }

    private Dictionary<string, object?> DoPickRelic(Player player, Dictionary<string, object?>? args)
    {
        if (_runState?.CurrentRoom is not TreasureRoom treasureRoom)
            return Error("pick_relic: not in a treasure room");
        if (_treasureStage == TreasureClosed)
            DoOpenChest(player);
        if (_treasureStage != TreasureOpen)
            return TreasureState(treasureRoom);
        int index = args != null && args.ContainsKey("relic_index") ? Convert.ToInt32(args["relic_index"]) : 0;
        PickTreasureRelic(index);
        _treasureStage = TreasureDone;
        return TreasureState(treasureRoom);
    }

    /// <summary>Proceed / leave_room in a treasure room: an offered relic is
    /// skipped, then to the map.</summary>
    private Dictionary<string, object?> LeaveTreasure()
    {
        if (_treasureStage != TreasureDone)
        {
            Log("Treasure room: relic skipped");
            PickTreasureRelic(null);
        }
        _treasureStage = TreasureDone;
        ForceToMap();
        return MapSelectState();
    }

    /// <summary>
    /// The relic-picking session TreasureRoom.EnterInternal opened (BeginRelicPicking):
    /// a pick (or a skip, null) ends it; otherwise the next treasure room's
    /// BeginRelicPicking throws "relic picking session while one was already
    /// occurring!" (issue #56). The grant normally lives in the UI node's
    /// RelicsAwarded handler (RelicCmd.Obtain per result), absent headless:
    /// the results are captured and granted after the pick resolves (granting
    /// inside the event, mid action-execution, risks re-entrancy).
    /// </summary>
    private void PickTreasureRelic(int? index)
    {
        try
        {
            var relicSync = RunManager.Instance.TreasureRoomRelicSynchronizer;
            if (relicSync?.CurrentRelics == null) return;
            List<MegaCrit.Sts2.Core.Entities.TreasureRelicPicking.RelicPickingResult>? awarded = null;
            Action<List<MegaCrit.Sts2.Core.Entities.TreasureRelicPicking.RelicPickingResult>> capture = r => awarded = r;
            relicSync.RelicsAwarded += capture;
            try
            {
                if (relicSync.CurrentRelics.Count == 0)
                {
                    relicSync.CompleteWithNoRelics();
                }
                else
                {
                    if (index.HasValue && (index.Value < 0 || index.Value >= relicSync.CurrentRelics.Count))
                        index = 0;
                    Log(index.HasValue
                        ? $"Picking treasure relic {index.Value} of {relicSync.CurrentRelics.Count}"
                        : "Skipping the treasure relic");
                    relicSync.PickRelicLocally(index);
                }
                Sched.Pump();
                WaitForActionExecutor();
                Sched.Pump();
            }
            finally { relicSync.RelicsAwarded -= capture; }

            if (awarded != null)
            {
                foreach (var res in awarded)
                {
                    if (res.relic != null && res.player != null)
                        Sched.Run(() => RelicCmd.Obtain(res.relic.ToMutable(), res.player));
                }
                Sched.Pump();
                WaitForActionExecutor();
                Sched.Pump();
            }
        }
        catch (Exception ex) { Log($"Treasure relic pick: {ex.Message}"); }
    }
}
