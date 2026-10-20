// The Timeline (NTimelineScreen, NEpochSlot, NEpochInspectScreen): the
// epochs of the current profile and their reveal, which is how a profile
// unlocks characters, cards, relics, potions, acts and modes.
//
// Opening the screen obtains the Neow epoch when it has no slot yet
// (OnSubmenuOpened); a profile that has not revealed Neow sees the tutorial
// first, whose end opens Neow's slot (SpawnFirstTimeTimeline). Revealing an
// obtained epoch (NEpochSlot.RevealEpoch, then the inspect screen's
// UnlockAnimation) marks it revealed and runs its QueueUnlocks: the unlock
// screens it queues on NTimelineScreen (cards, relics, potions, a character,
// text, new slots) reach the UI patch, which hands them to the driver here.
using MegaCrit.Sts2.Core.Helpers;
using MegaCrit.Sts2.Core.Localization;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Saves;
using MegaCrit.Sts2.Core.Timeline;
using MegaCrit.Sts2.Core.Timeline.Epochs;

namespace Spire.Logic;

public partial class RunDriver
{
    /// <summary>Unlock screens queued while an epoch's unlocks run.</summary>
    private List<Dictionary<string, object?>>? _unlockQueue;

    /// <summary>NTimelineScreen.Queue*Unlock calls, while an epoch is revealed.</summary>
    private void TimelineUiCall(string id, object?[] args)
    {
        if (_unlockQueue == null) return;
        var sep = id.LastIndexOf("::", StringComparison.Ordinal);
        if (sep < 0 || !id[..sep].EndsWith("NTimelineScreen", StringComparison.Ordinal)) return;
        var method = id[(sep + 2)..];
        var screen = new Dictionary<string, object?>();
        switch (method)
        {
            case "QueueMiscUnlock":
                screen["kind"] = "misc";
                screen["text"] = args.OfType<string>().FirstOrDefault();
                break;
            case "QueueCharacterUnlock":
            {
                // NUnlockCharacterScreen._Ready: the epoch's texts.
                var epoch = args.OfType<EpochModel>().FirstOrDefault();
                screen["kind"] = "character";
                screen["epoch"] = epoch?.Id;
                if (epoch != null)
                {
                    screen["top_text"] = Text(() => new LocString("epochs", epoch.Id + ".unlock").GetFormattedText());
                    screen["bottom_text"] = Text(() => new LocString("epochs", epoch.Id + ".unlockText").GetFormattedText());
                }
                break;
            }
            case "QueueCardUnlock":
                screen["kind"] = "cards";
                screen["cards"] = args.OfType<IEnumerable<CardModel>>().FirstOrDefault()?.Select(c => CardSummary(c)).ToList();
                break;
            case "QueueRelicUnlock":
                screen["kind"] = "relics";
                screen["relics"] = args.OfType<IEnumerable<RelicModel>>().FirstOrDefault()?.Select(r => new Dictionary<string, object?>
                {
                    ["id"] = r.Id.Entry,
                    ["name"] = Text(() => r.Title.GetFormattedText()),
                    ["description"] = Text(() => r.DynamicDescription.GetFormattedText()),
                    ["rarity"] = r.Rarity.ToString(),
                }).ToList();
                break;
            case "QueuePotionUnlock":
                screen["kind"] = "potions";
                screen["potions"] = args.OfType<IEnumerable<PotionModel>>().FirstOrDefault()?.Select(p => new Dictionary<string, object?>
                {
                    ["id"] = p.Id.Entry,
                    ["name"] = _loc.Potion(p.Id.Entry),
                    ["description"] = _loc.Bilingual("potions", p.Id.Entry + ".description"),
                    ["rarity"] = p.Rarity.ToString(),
                }).ToList();
                break;
            case "QueueTimelineExpansion":
                screen["kind"] = "timeline"; // its slots are filled in by RevealEpoch
                break;
            default:
                return;
        }
        _unlockQueue.Add(screen);
    }

    /// <summary>NUnlockCardsScreen's cards, drawn as NCard does.</summary>
    private Dictionary<string, object?> CardSummary(CardModel c)
    {
        var d = new Dictionary<string, object?>
        {
            ["id"] = c.Id.Entry,
            ["name"] = _loc.Card(c.Id.Entry),
            ["rarity"] = c.Rarity.ToString(),
            ["type"] = c.Type.ToString(),
        };
        try { AddCardView(d, c, MegaCrit.Sts2.Core.Entities.Cards.PileType.None); }
        catch (Exception ex) { Log($"unlock card view {c.Id.Entry}: {ex.Message}"); }
        return d;
    }

    /// <summary>NTimelineScreen.OnSubmenuOpened and InitScreen: the slots.</summary>
    public Dictionary<string, object?> Timeline()
    {
        try
        {
            EnsureModelDbInitialized();
            if (_profileId <= 0) return Error("No profile selected");
            var progress = SaveManager.Instance.Progress;
            var neow = progress.Epochs.FirstOrDefault(e => e.Id == EpochModel.GetId<NeowEpoch>());
            if (neow == null || neow.State == EpochState.NotObtained || neow.State == EpochState.ObtainedNoSlot)
                progress.ObtainEpoch(EpochModel.GetId<NeowEpoch>());
            return TimelineState();
        }
        catch (Exception ex) { return ErrorWithTrace("timeline failed", ex); }
    }

    private Dictionary<string, object?> TimelineState()
    {
        var progress = SaveManager.Instance.Progress;
        var slots = new List<Dictionary<string, object?>>();
        foreach (var e in progress.Epochs)
        {
            if (e.State == EpochState.ObtainedNoSlot) continue;
            slots.Add(EpochData(e.Id, SlotState(e.State)));
        }
        return new Dictionary<string, object?>
        {
            ["type"] = "timeline",
            // FirstTimeLogic: the tutorial, then Neow's slot alone.
            ["first_time"] = SaveManager.Instance.IsNeowDiscovered(),
            ["revealable"] = SaveManager.Instance.GetDiscoveredEpochCount(),
            // NEpochReminderText.
            ["reminder_text"] = Text(() =>
            {
                var l = new LocString("timeline", "REMINDER_TEXT");
                l.Add("RevealableEpochCount", SaveManager.Instance.GetDiscoveredEpochCount());
                return l.GetFormattedText();
            }),
            ["tutorial_text"] = Text(() => new LocString("timeline", "TUTORIAL_TEXT").GetFormattedText()),
            ["slots"] = slots,
        };
    }

    /// <summary>NTimelineScreen.InitScreen's slot states.</summary>
    private static string SlotState(EpochState s) =>
        s >= EpochState.Revealed ? "Complete" : s > EpochState.ObtainedNoSlot ? "Obtained" : "NotObtained";

    private Dictionary<string, object?> EpochData(string id, string state)
    {
        var m = EpochModel.Get(id);
        return new Dictionary<string, object?>
        {
            ["id"] = id,
            ["state"] = state,
            ["era"] = m.Era.ToString(),
            ["era_value"] = (int)m.Era,
            ["era_position"] = m.EraPosition,
            ["title"] = Text(() => m.Title.GetFormattedText()),
            ["description"] = Text(() => m.Description),
            // NEpochSlot.OnFocus's hover tip (IsRevealed: a revealed epoch).
            ["unlock_info"] = Text(() => { var l = m.UnlockInfo; l.Add("IsRevealed", false); return l.GetFormattedText(); }),
            ["unlock_info_revealed"] = Text(() => { var l = m.UnlockInfo; l.Add("IsRevealed", true); return l.GetFormattedText(); }),
            // NEraColumn.Init: NTimelineScreen.GetEraIcon and the eras table.
            ["era_name"] = Text(() => new LocString("eras", StringHelper.Slugify(m.Era.ToString()) + ".name").GetFormattedText()),
            ["era_year"] = Text(() => new LocString("eras", StringHelper.Slugify(m.Era.ToString()) + ".year").GetFormattedText()),
            ["era_icon"] = (int)m.Era >= 0 ? $"images/atlases/era_atlas.sprites/era_{(int)m.Era}.tres"
                : $"images/atlases/era_atlas.sprites/era_minus_{Math.Abs((int)m.Era)}.tres",
            // NEpochInspectScreen.Open: the chapter line.
            ["chapter_text"] = Text(() =>
            {
                if (m.StoryTitle == null) return m.Title.GetFormattedText();
                var l = new LocString("timeline", "EPOCH_INSPECT.chapterFormat");
                l.Add("ChapterIndex", m.ChapterIndex);
                l.Add("ChapterName", m.Title);
                return l.GetFormattedText();
            }),
            ["unlock_text"] = Text(() => m.UnlockText),
            ["story_id"] = m.StoryId,
            ["story_title"] = Text(() => m.StoryTitle),
            ["chapter"] = Num(() => m.ChapterIndex),
            ["portrait"] = "images/atlases/epoch_atlas.sprites/" + id.ToLowerInvariant() + ".tres",
            ["real_portrait"] = "images/timeline/epoch_portraits/" + id.ToLowerInvariant() + ".png",
        };
    }

    private static int Num(Func<int> f)
    {
        try { return f(); }
        catch { return -1; }
    }

    /// <summary>NTimelineScreen.SpawnFirstTimeTimeline, after the tutorial.</summary>
    public Dictionary<string, object?> TimelineFirstTimeDone()
    {
        if (_profileId <= 0) return Error("No profile selected");
        SaveManager.Instance.UnlockSlot(EpochModel.GetId<NeowEpoch>());
        SaveManager.Instance.SaveProgressFile();
        return TimelineState();
    }

    /// <summary>
    /// A click on an obtained slot: NEpochSlot.RevealEpoch, then
    /// NEpochInspectScreen.UnlockAnimation (QueueUnlocks, SaveProgressFile).
    /// </summary>
    public Dictionary<string, object?> RevealEpoch(string id)
    {
        try
        {
            if (_profileId <= 0) return Error("No profile selected");
            var e = SaveManager.Instance.Progress.Epochs.FirstOrDefault(x => x.Id == id);
            if (e == null || e.State != EpochState.Obtained) return Error($"Epoch {id} cannot be revealed ({e?.State.ToString() ?? "no slot"})");
            var model = EpochModel.Get(id);
            var before = SaveManager.Instance.Progress.Epochs.ToDictionary(x => x.Id, x => x.State);
            SaveManager.Instance.RevealEpoch(id);
            _unlockQueue = new List<Dictionary<string, object?>>();
            List<Dictionary<string, object?>> queue;
            try
            {
                model.QueueUnlocks();
            }
            finally
            {
                queue = _unlockQueue;
                _unlockQueue = null;
            }
            SaveManager.Instance.SaveProgressFile();
            // EpochModel.QueueTimelineExpansion: the new slots, Obtained when
            // the epoch was earned before its slot existed.
            var expansion = model.GetTimelineExpansion().Select(x =>
                EpochData(x.Id, before.TryGetValue(x.Id, out var st) && st == EpochState.ObtainedNoSlot ? "Obtained" : "NotObtained")).ToList();
            foreach (var screen in queue)
            {
                if (screen["kind"] as string == "timeline") screen["slots"] = expansion;
                if (screen["kind"] as string == "character")
                {
                    var pending = SaveManager.Instance.Progress.PendingCharacterUnlock;
                    screen["character"] = pending.Entry;
                }
            }
            var d = TimelineState();
            d["revealed"] = EpochData(id, "Complete");
            d["unlocks"] = queue;
            return d;
        }
        catch (Exception ex) { return ErrorWithTrace("reveal_epoch failed", ex); }
    }
}
