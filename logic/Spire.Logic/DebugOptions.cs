// Debug options for testing whole runs (the console's spire360.ini puts them
// in the logic's environment; off unless set):
//
//   SPIRE_INVINCIBLE=1  the player keeps their HP (patch debug.invincible:
//                       Creature.LoseHpInternal in CreatureCmd.Damage); the
//                       hits play as usual, damage numbers and all; the
//                       kills that are not damage spare them
//                       (debug.invincible_kill)
//   SPIRE_GODMODE=1     the game's own dev console "godmode"
//                       (GodModeConsoleCmd): Strength, Buffer and Regen
//                       999999999 on the player at each combat's set-up
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Commands;
using MegaCrit.Sts2.Core.Context;
using MegaCrit.Sts2.Core.Entities.Creatures;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.GameActions.Multiplayer;
using MegaCrit.Sts2.Core.Helpers;
using MegaCrit.Sts2.Core.Models.Powers;
using MegaCrit.Sts2.Core.Runs;
using MegaCrit.Sts2.Core.ValueProps;

namespace Spire.Logic;

internal static class DebugOptions
{
    public static readonly bool Invincible = Environment.GetEnvironmentVariable("SPIRE_INVINCIBLE") == "1";
    public static readonly bool GodMode = Environment.GetEnvironmentVariable("SPIRE_GODMODE") == "1";

    public static void Install()
    {
        Spire.HookBridge.Hooks.Register("debug.invincible", a =>
        {
            var creature = (Creature)a[0]!;
            var amount = (decimal)a[1]!;
            var props = (ValueProp)a[2]!;
            if (!Invincible || !creature.IsPlayer)
                return creature.LoseHpInternal(amount, props);
            // The hit as dealt (its number, spark, hurt animation, shake and
            // the HP-loss hooks), then the HP put back: CreatureCmd.Damage
            // kills only a target that is also dead (IsDead).
            int hp = creature.CurrentHp;
            var result = creature.LoseHpInternal(amount, props);
            creature.SetCurrentHpInternal(hp);
            return result;
        });
        // The kills that are not damage (Doom, Sandpit, The Gambit, Tablet of
        // Truth: patch debug.invincible_kill); Give Up's and a won run's are
        // RunManager's own and still happen.
        Spire.HookBridge.Hooks.Register("debug.invincible_kill", a =>
        {
            var creature = (Creature)a[0]!;
            if (!Invincible || !creature.IsPlayer)
                return CreatureCmd.Kill(creature, (bool)a[1]!);
            // Sandpit hid the body before its kill (Visuals.Visible = false):
            // it shows again (a debug cue; the game has no such call)
            Steps.Cue(new Dictionary<string, object?>
            {
                ["t"] = "ui",
                ["call"] = "NCreature.ShowVisuals",
                ["args"] = new List<object?> { Steps.Who(creature) },
            });
            return Task.CompletedTask;
        });
        if (GodMode)
            CombatManager.Instance.CombatSetUp += OnCombatSetUp;
        if (Invincible || GodMode)
            Log.Warn($"debug options: invincible {(Invincible ? "on" : "off")}, godmode {(GodMode ? "on" : "off")}");
    }

    /// <summary>GodModeConsoleCmd.OnCombatSetUp.</summary>
    private static void OnCombatSetUp(CombatState combatState)
    {
        if (!RunManager.Instance.IsInProgress) return;
        var me = LocalContext.GetMe(combatState.RunState);
        if (me != null)
            TaskHelper.RunSafely(EnableGodMode(me));
    }

    /// <summary>GodModeConsoleCmd.EnableGodMode.</summary>
    private static async Task EnableGodMode(Player player)
    {
        Creature playerCreature = player.Creature;
        await PowerCmd.Apply<StrengthPower>(new ThrowingPlayerChoiceContext(), playerCreature, 999999999m, playerCreature, null);
        await PowerCmd.Apply<BufferPower>(new ThrowingPlayerChoiceContext(), playerCreature, 999999999m, playerCreature, null);
        await PowerCmd.Apply<RegenPower>(new ThrowingPlayerChoiceContext(), playerCreature, 999999999m, playerCreature, null);
    }
}
