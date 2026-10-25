// Presentation the game plays on Godot nodes directly (positions, tweens,
// visibility) or in branches TestMode skips, which the headless logic
// never runs: the same moments recorded as cues for the client (the
// audit's list: notes/UI_AUDIT.md). The patches are in patches.json under
// the ids registered here.
using System.Diagnostics.CodeAnalysis;
using System.Reflection;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Commands;
using MegaCrit.Sts2.Core.Entities.Cards;
using MegaCrit.Sts2.Core.Entities.Creatures;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.GameActions.Multiplayer;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Powers;
using MegaCrit.Sts2.Core.ValueProps;

namespace Spire.Logic;

internal static partial class Hooks
{
    // Monster moves that aim a node of theirs before the attack plays: the
    // node's global x = the target's x + k * the creature's Visuals.Scale.X
    // (the leftmost target, or the first: the local player's for the
    // Thieving Hopper). TestMode-off branches, except the Hopper's.
    private static readonly (string Id, string Node, float K, bool Leftmost)[] Aims =
    {
        ("bowlbug_silk.spit", "Visuals/SpineBoneNode", 0f, true),
        ("bygone_effigy.slash", "Visuals/SpineBoneNode", 1500f, true),
        ("kin_follower.boomerang", "Visuals/AttackDistanceControl", 400f, true),
        ("leaf_slime_m.sticky_shot", "Visuals/SpitTarget", 0f, true),
        ("twig_slime_m.sticky_shot", "Visuals/SpitTarget", 0f, true),
        ("magi_knight.magic_bomb", "Visuals/AttackDistanceControl", 600f, true),
        ("slumbering_beetle.rollout", "Visuals/SpineBoneNode", 750f, true),
        ("tunneler.below", "Visuals/SpineBoneNode", 400f, false),
        ("thieving_hopper.thievery", "Visuals/SpineBoneNode", 900f, false),
        ("torch_head_amalgam.soul_beam", "Visuals/LaserControlBone", 400f, false),
    };

    private static void SpecialCall(string call, Creature c, string node, params object?[] args)
    {
        var list = new List<object?> { Steps.Who(c) + "#" + node };
        list.AddRange(args);
        Steps.Cue(new Dictionary<string, object?> { ["t"] = "ui", ["call"] = call, ["args"] = list });
    }

    private static void CreatureCall(string call, params object?[] args) =>
        Steps.Cue(new Dictionary<string, object?> { ["t"] = "ui", ["call"] = call, ["args"] = args.ToList() });

    /// <summary>Sandpit's AfterRemoved TestMode-off steps, in the order its
    /// IsOff reads come (the pull, then each affected creature's body
    /// hidden).</summary>
    private static readonly Queue<Action> _sandpitSteps = new();

    /// <summary>The UI call a stripped body names ("headless: Type::Method").</summary>
    private static bool HeadlessCall(Exception ex, out string call)
    {
        call = "";
        const string mark = "headless: ";
        for (var e = ex; e != null; e = e.InnerException)
        {
            int at = e.Message.IndexOf(mark, StringComparison.Ordinal);
            if (e is NotSupportedException && at >= 0)
            {
                call = e.Message[(at + mark.Length)..].TrimEnd(')', '.');
                return true;
            }
        }
        return false;
    }

    private static void RandomizeCostCue(CardModel card)
    {
        if (card.Pile?.Type == PileType.Hand && Steps.InCombat)
            Steps.Cue(new Dictionary<string, object?> { ["t"] = "card_randomize_cost", ["card"] = Steps.IdOf(card) });
    }

    private static readonly string[] FlashMethods =
    {
        "set_BaseReplayCount", "set_BaseStarCost", "InvokeEnergyCostChanged", "AddTemporaryStarCost", "AddKeyword",
        "RemoveKeyword", "AfflictInternal", "ClearAfflictionInternal", "UpgradeInternal", "DowngradeInternal",
    };

    private static readonly (string Id, string Type, string Method)[] SpineAccessCalls =
    {
        ("get_state", "MegaSprite", "GetAnimationState"), ("set_animation", "MegaAnimationState", "SetAnimation"),
        ("add_animation", "MegaAnimationState", "AddAnimation"), ("get_current", "MegaAnimationState", "GetCurrent"),
        ("current_name", "MegaAnimationState", "GetCurrentAnimationName"),
        ("current_duration", "MegaAnimationState", "GetCurrentAnimationDuration"),
        ("time_scale", "MegaAnimationState", "SetTimeScale"),
    };

    private static MethodInfo? _powerFlash;

    /// <summary>RollingBoulderPower.AfterPlayerTurnStart as the game plays it
    /// outside TestMode: the boulder (NRollingBoulderVfx) rolls over the
    /// enemies and its HitCreature signal damages each as it passes, each
    /// its own damage (TestMode damages them all at once). The client times
    /// each boulder_hit mark by where the boulder meets that enemy (the
    /// logic cannot see where they stand), and boulder_done when it has
    /// rolled out (all hit, then 1 s).</summary>
    [DynamicDependency("Flash", typeof(PowerModel))]
    private static async Task RollingBoulderTurn(RollingBoulderPower p, PlayerChoiceContext ctx, Player player)
    {
        if (player != p.Owner.Player) return;
        _powerFlash ??= typeof(PowerModel).GetMethod("Flash", BindingFlags.Instance | BindingFlags.NonPublic, null, Type.EmptyTypes, null);
        _powerFlash?.Invoke(p, null);
        var targets = p.Owner.CombatState?.HittableEnemies.ToList() ?? new List<Creature>();
        CreatureCall("NRollingBoulderVfx.Create", targets.Select(c => (object?)Steps.Who(c)).ToList(), p.Amount);
        foreach (var t in targets)
        {
            Steps.Cue(new Dictionary<string, object?> { ["t"] = "boulder_hit", ["who"] = Steps.Who(t) });
            await CreatureCmd.Damage(ctx, new[] { t }, p.Amount, ValueProp.Unpowered, p.Owner);
        }
        Steps.Cue(new Dictionary<string, object?> { ["t"] = "boulder_done" });
        p.SetAmount(p.Amount + p.DynamicVars.Damage.IntValue);
    }

    private static void InstallPresentation()
    {
        Bridge.Register("rolling_boulder.turn_start", a =>
            RollingBoulderTurn((RollingBoulderPower)a[0]!, (PlayerChoiceContext)a[1]!, (Player)a[2]!));
        // SpineAnimationAccess's calls on a creature's stand-in sprite and
        // animation state: through the 'ui' hook as if redirected (the
        // state's calls recorded with the creature as their receiver).
        foreach (var (id, type, method) in SpineAccessCalls)
        {
            var uiId = "ui|MegaCrit.Sts2.Core.Bindings.MegaSpine." + type + "::" + method;
            Bridge.Register("spine.access." + id, a =>
            {
                if (type == "MegaAnimationState") Steps.Ui(uiId, a);
                return StandIns.Answer(uiId, a);
            });
        }
        // AncientEventModel.BeforeEventStarted: at Neow (not pre-finished),
        // the top bar's HP from 0 up to what the heal left.
        Spire.HookBridge.Hooks.RegisterAfter("neow.hp_lerp", (a, r) =>
        {
            if (a.Length > 1 && a[0]?.GetType().Name == "Neow" && a[1] is false)
                CreatureCall("NTopBarHp.LerpAtNeow");
        });
        // NHandCardHolder.Flash: a hand card whose cost, keywords, replays,
        // affliction or upgrade changed.
        foreach (var m in FlashMethods)
            Spire.HookBridge.Hooks.RegisterAfter("card.flash." + m, (a, r) =>
            {
                if (a.Length > 0 && a[0] is CardModel card && card.Pile?.Type == PileType.Hand && Steps.InCombat)
                    Steps.Cue(new Dictionary<string, object?> { ["t"] = "card_flash", ["card"] = Steps.IdOf(card) });
            });
        // NCard.PlayRandomizeCostAnim: Confused (the owner's drawn cards with
        // a cost), Slither (its card), Snecko Oil (the hand's costed cards).
        Spire.HookBridge.Hooks.RegisterAfter("card.randomize_cost.confused", (a, r) =>
        {
            if (a.Length > 2 && a[0] is PowerModel p && a[2] is CardModel card && card.Owner == p.Owner.Player && card.EnergyCost.Canonical >= 0)
                RandomizeCostCue(card);
        });
        Spire.HookBridge.Hooks.RegisterAfter("card.randomize_cost.slither", (a, r) =>
        {
            if (a.Length > 2 && a[0] is EnchantmentModel e && a[2] is CardModel card && card == e.Card) RandomizeCostCue(card);
        });
        Spire.HookBridge.Hooks.RegisterAfter("card.randomize_cost.snecko_oil", (a, r) =>
        {
            if (a.Length < 3 || a[2] is not Creature target || target.Player == null) return;
            foreach (var card in PileType.Hand.GetPile(target.Player).Cards)
                if (!card.EnergyCost.CostsX && card.EnergyCost.Canonical >= 0) RandomizeCostCue(card);
        });
        // AttackCommand's custom effects: invoked as the game does; a UI
        // method bound as the delegate is recorded as the call it makes.
        Bridge.Register("attack.hit_vfx_node", a =>
        {
            var attack = (MegaCrit.Sts2.Core.Commands.Builders.AttackCommand)a[0]!;
            if (a.Length < 2 || a[1] is not Func<Creature, Godot.Node2D?> make) return attack;
            return attack.WithHitVfxNode(target =>
            {
                try { return make(target); }
                catch (Exception ex)
                {
                    // presentation only: an effect that cannot be made
                    // headless is no reason to stop the attack
                    if (HeadlessCall(ex, out var call)) Steps.Ui("ui|" + call, new object?[] { target });
                    else Console.Error.WriteLine($"[SIM] hit effect skipped: {ex.GetType().Name}: {ex.Message}");
                    return null;
                }
            });
        });
        Bridge.Register("attack.attacker_vfx_node", a =>
        {
            var attack = (MegaCrit.Sts2.Core.Commands.Builders.AttackCommand)a[0]!;
            if (a.Length < 2 || a[1] is not Func<Godot.Node2D?> make) return attack;
            return attack.WithAttackerFx(() =>
            {
                try { return make(); }
                catch (Exception ex)
                {
                    if (HeadlessCall(ex, out var call)) Steps.Ui("ui|" + call, Array.Empty<object?>());
                    else Console.Error.WriteLine($"[SIM] attacker effect skipped: {ex.GetType().Name}: {ex.Message}");
                    return null;
                }
            });
        });
        // VfxCmd.GetSideCenter(side, combatState) in the Mecha Knight's hit
        // effects: the side's first creature's centre.
        Bridge.Register("vfx.side_center", a =>
        {
            if (a.Length < 2 || a[0] is not MegaCrit.Sts2.Core.Combat.CombatSide side || a[1] is not ICombatState state) return null;
            var c = side == MegaCrit.Sts2.Core.Combat.CombatSide.Player ? state.PlayerCreatures.FirstOrDefault()
                                                                       : state.Enemies.FirstOrDefault(e => e.IsAlive);
            return c == null ? null : (Godot.Vector2?)StandIns.CenterOf(c);
        });
        // VfxCmd.GetSideCenterFloor(side, combatState) in End of Days: the
        // side's floor, resolved by the client
        Bridge.Register("vfx.side_center_floor", a =>
        {
            if (a.Length < 2 || a[0] is not MegaCrit.Sts2.Core.Combat.CombatSide side || a[1] is not ICombatState state) return null;
            var c = side == MegaCrit.Sts2.Core.Combat.CombatSide.Player ? state.PlayerCreatures.FirstOrDefault()
                                                                       : state.Enemies.FirstOrDefault(e => e.IsAlive);
            return c == null ? null : (Godot.Vector2?)StandIns.SideFloorOf(c);
        });
        foreach (var (id, node, k, leftmost) in Aims)
        {
            Spire.HookBridge.Hooks.RegisterBefore("aim." + id, a =>
            {
                if (a.Length < 2 || a[0] is not MonsterModel m || a[1] is not IReadOnlyList<Creature> targets) return;
                if (!Steps.InCombat || m.Creature.IsDead) return;
                if (targets.Count == 0)
                {
                    // Tunneler.BelowMove with no target: Position = Left * 400;
                    // TorchHeadAmalgam.SoulBeamMove: Left * 3000
                    if (id == "tunneler.below") SpecialCall("SpecialNode.SetPosition", m.Creature, node, -400f, 0f);
                    if (id == "torch_head_amalgam.soul_beam") SpecialCall("SpecialNode.SetPosition", m.Creature, node, -3000f, 0f);
                    return;
                }
                IEnumerable<Creature> aimAt = targets;
                if (!leftmost)
                {
                    var me = id == "thieving_hopper.thievery" ? targets.FirstOrDefault(t => t.IsPlayer) : null;
                    aimAt = new[] { me ?? targets[0] };
                }
                SpecialCall("SpecialNode.AimAt", m.Creature, node, aimAt.Select(t => (object?)Steps.Who(t)).ToList(), k);
            });
        }
        // TestSubject.SetColor: %CanvasGroup.SetSelfModulate(color)
        Spire.HookBridge.Hooks.RegisterBefore("test_subject.set_color", a =>
        {
            if (a.Length < 2 || a[0] is not MonsterModel m || a[1] is not Godot.Color c || !Steps.InCombat) return;
            static int B(float v) => (int)Math.Round(Math.Clamp(v, 0f, 1f) * 255f);
            SpecialCall("CanvasItem.SetSelfModulate", m.Creature, "%CanvasGroup", $"#{B(c.R):x2}{B(c.G):x2}{B(c.B):x2}{B(c.A):x2}");
        });
        // FuzzyWurmCrawler.AcidGoop: Position = Left * (its x - the first target's x)
        Spire.HookBridge.Hooks.RegisterBefore("aim.fuzzy_wurm_crawler.acid_goop", a =>
        {
            if (a.Length < 2 || a[0] is not MonsterModel m || a[1] is not IReadOnlyList<Creature> targets || targets.Count == 0) return;
            if (Steps.InCombat)
                SpecialCall("SpecialNode.ReachTo", m.Creature, "Visuals/SpineBoneNode", Steps.Who(targets[0]));
        });
        // TorchHeadAmalgam.OnDieToDoom: its torches' lights go out.
        Spire.HookBridge.Hooks.RegisterBefore("torch_head.die_to_doom", a =>
        {
            if (a.Length < 1 || a[0] is not MonsterModel m || !Steps.InCombat) return;
            for (int i = 1; i <= 3; i++)
                SpecialCall("CanvasItem.SetVisible", m.Creature, $"Visuals/torch{i}Slot/fire{i}_small_green/light_small", false);
        });
        // The Fabricator's bots (AfterAddedToRoom: FabricatorNormal.SetBotFallPosition):
        // Visuals/FallControl.Position = Down * ((125 - node.Position.Y) / body.Scale.Y).
        foreach (var bot in new[] { "guardbot", "noisebot", "stabbot", "zapbot" })
            Spire.HookBridge.Hooks.RegisterBefore("bot_fall." + bot, a =>
            {
                if (a.Length > 0 && a[0] is MonsterModel m && Steps.InCombat)
                    SpecialCall("SpecialNode.BotFall", m.Creature, "Visuals/FallControl");
            });
        // Vantom.ScaleTo(scale, duration) (ShrinkPower): Visuals/ScalingBone's
        // scale tweens there (Sine InOut).
        Spire.HookBridge.Hooks.RegisterBefore("vantom.scale_to", a =>
        {
            if (a.Length > 2 && a[0] is MonsterModel m && a[1] is float scale && a[2] is float duration && Steps.InCombat)
                SpecialCall("SpecialNode.ScaleTo", m.Creature, "Visuals/ScalingBone", scale, duration);
        });
        // SurroundedPower.FaceDirection(direction): the owner's and its pets'
        // bodies and form effects face that way; the music follows.
        Spire.HookBridge.Hooks.RegisterBefore("surrounded.face", a =>
        {
            if (a.Length < 2 || a[0] is not PowerModel power || a[1] is not Enum direction || power.Owner == null) return;
            if (!Steps.InCombat) return;
            bool left = direction.ToString() == "Left";
            var who = new List<object?> { Steps.Who(power.Owner) };
            who.AddRange(power.Owner.Pets.Select(p => (object?)Steps.Who(p)));
            CreatureCall("NCreature.FaceDirection", who, left);
            if (power.Owner.IsPlayer)
                CreatureCall("NRunMusicController.UpdateMusicParameter", "kaiser_crab_direction", left ? 1 : 2);
        });
        // OstyCmd.Summon (a new Osty from a card): its node fades in, 0.35 s
        // after 0.1 s (the tween made on the node).
        Bridge.Register("osty.fade_in", a =>
        {
            if (a.Length > 0 && StandIns.TryCreature(a[0], out var osty) && Steps.InCombat)
                CreatureCall("NCreature.FadeIn", Steps.Who(osty), 0.35f, 0.1f);
            return new Godot.Tween();
        });
        // CreatureCmd.Escape: the node is taken out of the room and hidden
        // (no death).
        Spire.HookBridge.Hooks.RegisterBefore("creature.escape", a =>
        {
            if (a.Length < 2 || a[0] is not Creature c || a[1] is not true || c.IsDead) return;
            if (c.CombatState == null || !Steps.InCombat) return;
            Steps.Cue(new Dictionary<string, object?> { ["t"] = "escape", ["who"] = Steps.Who(c) });
        });
        // VfxCmd.PlayVfx(position, path, container) at a creature's point
        // (Hand of Greed's coins: its target's VfxSpawnPosition).
        Spire.HookBridge.Hooks.RegisterAfter("vfx.at_position", (a, r) =>
        {
            if (a.Length < 2 || a[0] is not Godot.Vector2 at || a[1] is not string path) return;
            var place = StandIns.Describe(at);
            if (place == null) return;
            int sep = place.IndexOf('@');
            var anchor = place[(sep + 1)..];
            int sign = anchor.IndexOfAny(new[] { '+', '-' });
            if (sign >= 0) anchor = anchor[..sign];
            Steps.Cue(new Dictionary<string, object?> { ["t"] = "vfx", ["path"] = path, ["who"] = place[..sep], ["at"] = anchor == "bottom" ? "feet" : anchor });
        });
        // Nightmare.OnPlay (TestMode off): a purple vignette and the hands
        // over the screen, then CustomScaledWait(0.1, 0.25). The branch's
        // own code stays skipped (it reaches into NGame's run node).
        Bridge.Register("testmode.nightmare.is_off", _ =>
        {
            if (Steps.InCombat)
            {
                CreatureCall("NSmokyVignetteVfx.Create", "#cc4dcca8", "#0000ff54");
                CreatureCall("NNightmareHandsVfx.Create");
                ScaledWait(0.1f, 0.25f);
            }
            return false;
        });
        // DollRoom.BeforeEventStarted (TestMode off): its ambience, played by
        // the game itself: NDebugAudioManager.Play's ui cue gets a handle
        // (Hooks), which the event keeps, so StopAudio stops it when a doll
        // is taken or the event ends.
        Bridge.Register("testmode.doll_room.is_off", _ => true);
        // SandpitPower (The Insatiable): the player is drawn towards it as the
        // power counts down (AfterApplied keeps where they stood;
        // UpdateCreaturePositions tweens them, 0.25 s Cubic Out), and pulled
        // in when it runs out.
        Spire.HookBridge.Hooks.RegisterBefore("sandpit.applied", a =>
        {
            if (a.Length > 0 && a[0] is SandpitPower p && p.Target != null && Steps.InCombat)
                CreatureCall("Sandpit.Begin", Steps.Who(p.Owner), Steps.Who(p.Target), p.Amount);
        });
        Spire.HookBridge.Hooks.RegisterBefore("sandpit.update_positions", a =>
        {
            if (a.Length < 1 || a[0] is not SandpitPower p || p.Target == null || !Steps.InCombat) return;
            var player = p.Target.Player;
            var affected = SandpitAffected(p).Where(c => c.IsAlive).Select(c => (object?)Steps.Who(c)).ToList();
            var osty = player != null && player.IsOstyAlive ? Steps.Who(player.Osty!) : null;
            CreatureCall("Sandpit.Update", Steps.Who(p.Owner), Steps.Who(p.Target), p.Amount, affected, osty);
            Steps.Wait(250, "sandpit");
        });
        Spire.HookBridge.Hooks.RegisterBefore("sandpit.removed", a =>
        {
            _sandpitSteps.Clear();
            if (a.Length < 2 || a[0] is not SandpitPower p || a[1] is not Creature oldOwner || p.Target == null) return;
            if (oldOwner.IsDead || p.Target.IsDead || !Steps.InCombat) return;
            var affected = SandpitAffected(p);
            var owner = Steps.Who(oldOwner);
            _sandpitSteps.Enqueue(() => CreatureCall("Sandpit.Pull", owner, affected.Select(c => (object?)Steps.Who(c)).ToList()));
            foreach (var c in affected)
            {
                var who = Steps.Who(c);
                _sandpitSteps.Enqueue(() => CreatureCall("NCreature.HideVisuals", who));
            }
        });
        Bridge.Register("testmode.sandpit_removed.is_off", _ =>
        {
            if (_sandpitSteps.Count > 0) _sandpitSteps.Dequeue()();
            return false;
        });
    }

    /// <summary>SandpitPower.AllAffectedCreatures: its target's player and pets.</summary>
    private static List<Creature> SandpitAffected(SandpitPower p)
    {
        var list = new List<Creature>();
        var main = p.Target?.Player?.Creature ?? p.Target;
        if (main != null) list.Add(main);
        if (p.Target != null) list.AddRange(p.Target.Pets);
        return list;
    }
}
