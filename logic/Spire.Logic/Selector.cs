// HeadlessCardSelector ported from sts2-cli's RunSimulator.cs
// (https://github.com/Garglemesh/sts2-cli, MIT, (c) 2025 Hao Wu).
//
// Spire360 change: the synchronous GetSelectedCardReward no longer blocks on
// a ManualResetEventSlim (impossible on one thread). It reports the decision
// point and runs a nested command loop until the reply arrives.
using MegaCrit.Sts2.Core.CardSelection;
using MegaCrit.Sts2.Core.Entities.CardRewardAlternatives;
using MegaCrit.Sts2.Core.Entities.Cards;
using MegaCrit.Sts2.Core.Models;

namespace Spire.Logic;

public partial class RunDriver
{
    /// <summary>
    /// Set by Program: emits the current decision point, then processes
    /// commands until the predicate turns false (the pending choice was made).
    /// </summary>
    internal static Action<Func<bool>>? NestedDecisionLoop;

    /// <summary>Opens a bundle_select decision (Scroll Boxes: pick 1 of N packs).</summary>
    internal Task<IEnumerable<CardModel>> BeginBundleSelection(IReadOnlyList<IReadOnlyList<CardModel>> bundles)
    {
        if (bundles.Count == 0)
            return Task.FromResult<IEnumerable<CardModel>>(Array.Empty<CardModel>());
        _pendingBundles = bundles;
        _pendingBundleTcs = new TaskCompletionSource<IEnumerable<CardModel>>();
        Log($"Bundle selection pending: {bundles.Count} packs");
        return _pendingBundleTcs.Task;
    }

    /// <summary>
    /// Card selector that creates a pending selection decision point.
    /// When the game needs the player to choose cards (upgrade, remove, transform),
    /// this stores the options and returns a task the next command completes.
    /// </summary>
    internal class HeadlessCardSelector : MegaCrit.Sts2.Core.TestSupport.ICardSelector
    {
        public List<CardModel>? PendingOptions { get; private set; }
        public int PendingMinSelect { get; private set; }
        public int PendingMaxSelect { get; private set; }
        public string PendingPrompt { get; private set; } = "";
        private TaskCompletionSource<IEnumerable<CardModel>>? _pendingTcs;

        public bool HasPending => _pendingTcs != null && !_pendingTcs.Task.IsCompleted;

        public Task<IEnumerable<CardModel>> GetSelectedCards(
            IEnumerable<CardModel> options, int minSelect, int maxSelect)
        {
            var optList = options.ToList();
            if (optList.Count == 0)
                return Task.FromResult<IEnumerable<CardModel>>(Array.Empty<CardModel>());

            // If only one option and minSelect requires it, auto-select
            if (optList.Count == 1 && minSelect >= 1)
                return Task.FromResult<IEnumerable<CardModel>>(optList);

            PendingOptions = optList;
            PendingMinSelect = minSelect;
            PendingMaxSelect = maxSelect;
            _pendingTcs = new TaskCompletionSource<IEnumerable<CardModel>>();
            Spire.Logic.Log.Info($"Card selection pending: {optList.Count} options, select {minSelect}-{maxSelect}");
            return _pendingTcs.Task;
        }

        public void ResolvePending(IEnumerable<CardModel> selected)
        {
            // Clear first: completing the TCS runs continuations (via the queue)
            // that may open the next selection.
            var tcs = _pendingTcs;
            PendingOptions = null;
            _pendingTcs = null;
            tcs?.TrySetResult(selected);
        }

        public void ResolvePendingByIndices(int[] indices)
        {
            if (PendingOptions == null) return;
            var selected = indices
                .Where(i => i >= 0 && i < PendingOptions.Count)
                .Select(i => PendingOptions[i])
                .ToList();
            ResolvePending(selected);
        }

        public void CancelPending() => ResolvePending(Array.Empty<CardModel>());

        // Pending card reward from events. The game calls this synchronously.
        public List<CardCreationResult>? PendingRewardCards { get; private set; }
        private int _rewardChoice = -1;
        private bool _rewardResolved;

        // NOTE: ICardSelector.GetSelectedCardReward return type has churned across STS2 builds:
        //   build 23372702 → CardRewardSelection struct { CardModel card; CardRewardAlternative alternative }
        //   a later build  → CardModel directly
        //   v0.107.1       → CardRewardSelection struct again (current target)
        // CardReward.OnSelect interprets: non-null card → take that card; null card → skip (no card kept).
        public CardRewardSelection GetSelectedCardReward(
            IReadOnlyList<CardCreationResult> options,
            IReadOnlyList<CardRewardAlternative> alternatives)
        {
            if (options.Count == 0) return default;  // Skip

            PendingRewardCards = options.ToList();
            _rewardChoice = -1;
            _rewardResolved = false;
            Spire.Logic.Log.Info($"Card reward pending: {options.Count} cards (nested decision)");

            if (NestedDecisionLoop != null)
                NestedDecisionLoop(() => !_rewardResolved);
            else
                Spire.Logic.Log.Warn("no nested decision loop; skipping the card reward");

            var choice = _rewardChoice;
            PendingRewardCards = null;

            if (choice >= 0 && choice < options.Count)
                return new CardRewardSelection { card = options[choice].Card };
            return default;  // Skip (no card kept)
        }

        public bool HasPendingReward => PendingRewardCards != null && !_rewardResolved;

        public void ResolveReward(int index)
        {
            _rewardChoice = index;
            _rewardResolved = true;
        }

        public void SkipReward()
        {
            _rewardChoice = -1;
            _rewardResolved = true;
        }
    }
}
