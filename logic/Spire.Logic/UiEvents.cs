// Game events the UI layer subscribes to (Spire.ApiDump --ui-surface's
// "events"; the presentation audit's "event" items), subscribed to here as
// the UI would and recorded as "ui_event" cues for the client's ports of the
// handlers: what the decisions' state cannot tell it (the end turn button
// hiding as the enemies' turn comes, the combat UI animating in and out).
// Events whose handlers only show state the decisions carry are left out.
using MegaCrit.Sts2.Core.Combat;

namespace Spire.Logic;

internal static class UiEvents
{
    public static void Install()
    {
        var cm = CombatManager.Instance;
        // NCombatUi.Activate (AnimIn), NPotionContainer.OnCombatSetUp
        // (ShinePotions), NOrbManager.OnCombatSetup (AddSlotAnim)
        cm.CombatSetUp += _ => Ev("CombatSetUp");
        // NCombatUi.OnCombatEnded (AnimOut), NCreature.OnCombatEnded
        // (AnimHideIntent, ClearOrbs)
        cm.CombatEnded += _ => Ev("CombatEnded");
        // NEndTurnButton: Hidden as the enemies' turn comes; disabled once the
        // player ended the turn; back in at the player's turn
        cm.AboutToSwitchToEnemyTurn += _ => Ev("AboutToSwitchToEnemyTurn");
        cm.PlayerEndedTurn += (_, _) => Ev("PlayerEndedTurn");
        cm.TurnStarted += s =>
        {
            if (s.CurrentSide == CombatSide.Player && cm.IsInProgress) Ev("TurnStarted");
        };
    }

    private static void Ev(string name) =>
        Steps.Cue(new Dictionary<string, object?> { ["t"] = "ui_event", ["ev"] = name });
}
