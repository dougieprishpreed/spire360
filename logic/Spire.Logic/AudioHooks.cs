// SfxCmd returns early in NonInteractiveMode. Export its presentation
// commands explicitly while keeping its combat-ending guards and arguments.
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Entities.Creatures;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.Models;

namespace Spire.Logic;

internal static partial class Hooks
{
    private static void InstallSound()
    {
        Bridge.Register("sfx.play", a =>
        {
            if (a.Length > 0 && a[0] is string path && !CombatManager.Instance.IsEnding)
            {
                var cue = Sound("play", path);
                if (a.Length > 3 && a[1] is string parameter && a[2] is float value)
                {
                    cue["params"] = new Dictionary<string, object?> { [parameter] = value };
                    cue["volume"] = a[3];
                }
                else if (a.Length > 1) cue["volume"] = a[1];
                Steps.Cue(cue);
            }
            return null;
        });
        Bridge.Register("sfx.loop", a =>
        {
            Creature? creature = a.Length > 0 ? a[0] as Creature : null;
            int at = creature == null ? 0 : 1;
            if (a.Length > at && a[at] is string path)
            {
                var cue = Sound("loop", path);
                if (creature != null) cue["who"] = Steps.Who(creature);
                cue["loop_param"] = creature != null || a.Length <= 1 || a[1] is not false ? "loop" : null;
                cue["stop_value"] = 1f;
                if (creature != null && a.Length > 3)
                {
                    cue["loop_param"] = a[2]; cue["stop_value"] = a[3];
                }
                Steps.Cue(cue);
            }
            return null;
        });
        Bridge.Register("sfx.stop", a =>
        {
            Creature? creature = a.Length > 0 ? a[0] as Creature : null;
            int at = creature == null ? 0 : 1;
            if (a.Length > at && a[at] is string path)
            {
                var cue = Sound("stop", path);
                if (creature != null) cue["who"] = Steps.Who(creature);
                Steps.Cue(cue);
            }
            return null;
        });
        Bridge.Register("sfx.param", a =>
        {
            if (a.Length > 2 && a[0] is string path && a[1] is string parameter)
            {
                var cue = Sound("param", path);
                cue["params"] = new Dictionary<string, object?> { [parameter] = a[2] };
                Steps.Cue(cue);
            }
            return null;
        });
        Bridge.Register("sfx.damage", a =>
        {
            if (a.Length > 0 && a[0] is MonsterModel monster && !CombatManager.Instance.IsEnding)
            {
                var cue = Sound("play", monster.TakeDamageSfx);
                cue["params"] = new Dictionary<string, object?> { ["EnemyImpact_Intensity"] = 2f };
                Steps.Cue(cue);
            }
            return null;
        });
        Bridge.Register("sfx.death", a =>
        {
            string? path = a.Length > 0 ? a[0] switch
            {
                MonsterModel monster => monster.DeathSfx,
                Player player => player.Character.DeathSfx,
                _ => null
            } : null;
            if (!string.IsNullOrEmpty(path)) Steps.Cue(Sound("play", path));
            return null;
        });
    }
    private static Dictionary<string, object?> Sound(string operation, string? path) => new()
    {
        ["t"] = "sfx", ["op"] = operation, ["path"] = path, ["volume"] = 1f,
    };
}
