// The compendium's Bestiary (NBestiary): its entries as the game lists them
// (the discovered acts' monsters, then the event encounters'), which are
// discovered, each monster's moves (GenerateBestiaryMoveList) and the
// statistics mode's kills / deaths and quotes per character.

using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Commands;
using MegaCrit.Sts2.Core.Entities.Creatures;
using MegaCrit.Sts2.Core.Localization;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Acts;
using MegaCrit.Sts2.Core.Models.Characters;
using MegaCrit.Sts2.Core.Models.Encounters;
using MegaCrit.Sts2.Core.Models.Monsters;
using MegaCrit.Sts2.Core.Nodes.Vfx.Backgrounds;
using MegaCrit.Sts2.Core.Random;
using MegaCrit.Sts2.Core.Rooms;
using MegaCrit.Sts2.Core.Saves;

namespace Spire.Logic;

public partial class RunDriver
{
    // CreateFilters: All, then each character
    private static CharacterModel?[] BestiaryFilters => new CharacterModel?[]
    {
        null, ModelDb.Character<Ironclad>(), ModelDb.Character<Silent>(), ModelDb.Character<Regent>(),
        ModelDb.Character<Necrobinder>(), ModelDb.Character<Defect>(),
    };

    public Dictionary<string, object?> Bestiary()
    {
        EnsureModelDbInitialized();
        var progress = SaveManager.Instance.Progress;
        var monsters = progress.EnemyStats.Values.Where(e => e.TotalWins > 0).Select(e => e.Id).ToHashSet();
        var encounters = progress.EncounterStats.Values.Where(e => e.TotalWins > 0).Select(e => e.Id).ToHashSet();
        var sections = new List<Dictionary<string, object?>>();

        // AddAct for each discovered act, then AddEvents
        foreach (var act in ModelDb.Acts)
        {
            if (!progress.DiscoveredActs.Contains(act.Id)) continue;
            var seen = new HashSet<ModelId>();
            var list = new List<(MonsterModel? m, EncounterModel e, RoomType room)>();
            foreach (var enc in act.AllEncounters)
                foreach (var m in enc.AllPossibleMonsters)
                    if (seen.Add(m.Id) && m.ShouldShowInCompendium)
                        list.Add((m, enc, enc.RoomType));
            if (act is Hive) list.Add((null, ModelDb.Encounter<DecimillipedeElite>(), RoomType.Elite));
            sections.Add(BestiarySection(act.Title.GetFormattedText(), list, monsters, encounters));
        }
        {
            var seen = new HashSet<ModelId>();
            var list = new List<(MonsterModel? m, EncounterModel e, RoomType room)>();
            foreach (var enc in ModelDb.EventEncounters)
                foreach (var m in enc.AllPossibleMonsters)
                    if (seen.Add(m.Id) && m.ShouldShowInCompendium)
                        list.Add((m, enc, enc.RoomType));
            sections.Add(BestiarySection(new LocString("bestiary", "EVENTS.title").GetFormattedText(), list, monsters, encounters));
        }

        var filters = BestiaryFilters.Select(c => new Dictionary<string, object?>
        {
            ["character"] = c?.Id.Entry.ToLowerInvariant(),
            ["seen_quote"] = c != null ? Safe(() => c.BestiarySeenQuote.GetFormattedText()) : "",
            ["kill_quote"] = c != null ? Safe(() => c.BestiaryKillQuote?.GetFormattedText()) : null,
            ["dialogue_color"] = c != null ? ColorHex(c.DialogueColor) : null,
            ["thought"] = c is Silent,
        }).ToList();

        return new Dictionary<string, object?>
        {
            ["type"] = "bestiary",
            ["sections"] = sections,
            ["filters"] = filters,
            ["stats_mode"] = !SaveManager.Instance.PrefsSave.IsBestiaryActionsPreferred,
            ["placeholder_quote"] = new LocString("bestiary", "QUOTE_PLACEHOLDER").GetFormattedText(),
        };
    }

    private Dictionary<string, object?> BestiarySection(string title, List<(MonsterModel? m, EncounterModel e, RoomType room)> list,
        HashSet<ModelId> monsters, HashSet<ModelId> encounters)
    {
        string EncounterTitle((MonsterModel? m, EncounterModel e, RoomType room) x) => x.e.Title.GetFormattedText();
        string EntryTitle((MonsterModel? m, EncounterModel e, RoomType room) x) => x.m != null ? x.m.Title.GetFormattedText() : EncounterTitle(x);
        // AddEntries' order: room type, a boss's encounter, the title
        list.Sort((a, b) =>
        {
            if (a.room != b.room) return a.room.CompareTo(b.room);
            if (a.room == RoomType.Boss)
            {
                int n = string.Compare(EncounterTitle(a), EncounterTitle(b), StringComparison.CurrentCulture);
                if (n != 0) return n;
            }
            return string.Compare(EntryTitle(a), EntryTitle(b), StringComparison.CurrentCulture);
        });
        var entries = new List<Dictionary<string, object?>>();
        foreach (var x in list)
        {
            bool discovered = x.m != null ? monsters.Contains(x.m.Id) : encounters.Contains(x.e.Id);
            var d = new Dictionary<string, object?>
            {
                ["title"] = discovered ? EntryTitle(x) : new LocString("bestiary", "UNSEEN.monsterName").GetRawText(),
                ["discovered"] = discovered,
                ["room_type"] = x.room.ToString(),
                ["encounter"] = x.e.Id.Entry,
                ["monster"] = x.m?.Id.Entry,
            };
            if (discovered) d["stats"] = BestiaryStats(x.m, x.e);
            entries.Add(d);
        }
        return new Dictionary<string, object?> { ["title"] = title, ["entries"] = entries };
    }

    /// <summary>RefreshStatisticsText per filter: the kills and deaths and
    /// DisplayCharacterData's STATS.layout text.</summary>
    private static List<Dictionary<string, object?>> BestiaryStats(MonsterModel? m, EncounterModel e)
    {
        var progress = SaveManager.Instance.Progress;
        var result = new List<Dictionary<string, object?>>();
        var enemy = m != null ? progress.EnemyStats.Values.FirstOrDefault(s => s.Id == m.Id) : null;
        var encounter = m == null ? progress.EncounterStats.Values.FirstOrDefault(s => s.Id == e.Id) : null;
        foreach (var c in BestiaryFilters)
        {
            int kills = 0, deaths = 0;
            if (enemy != null)
            {
                if (c != null)
                {
                    var f = enemy.FightStats.FirstOrDefault(s => s.Character == c.Id);
                    kills = f?.Wins ?? 0;
                    deaths = f?.Losses ?? 0;
                }
                else
                {
                    kills = enemy.TotalWins;
                    deaths = enemy.TotalLosses;
                }
            }
            else if (encounter != null)
            {
                if (c != null)
                {
                    var f = encounter.FightStats.FirstOrDefault(s => s.Character == c.Id);
                    kills = f?.Wins ?? 0;
                    deaths = f?.Losses ?? 0;
                }
                else
                {
                    kills = encounter.TotalWins;
                    deaths = encounter.TotalLosses;
                }
            }
            int total = kills + deaths;
            var text = new LocString("bestiary", "STATS.layout");
            if (total == 0)
            {
                text.Add("total", 0m);
                text.Add("kills", 0m);
                text.Add("deaths", 0m);
                text.Add("winrate", "--");
            }
            else
            {
                // NBestiaryCharacterFilter.WinRate
                double rate = (double)kills / total * 100.0;
                text.Add("total", total);
                text.Add("kills", kills);
                text.Add("deaths", deaths);
                text.Add("winrate", rate % 1.0 == 0.0 ? $"{rate:F0}" : $"{rate:F1}");
            }
            result.Add(new Dictionary<string, object?> { ["kills"] = kills, ["deaths"] = deaths, ["text"] = text.GetFormattedText() });
        }
        return result;
    }

    /// <summary>BestiaryEntry.CreateLayoutNode: which NBestiaryLayout shows the entry.</summary>
    private static string BestiaryLayout(string encounterId) => encounterId switch
    {
        "KAISER_CRAB_BOSS" => "kaiser_crab",
        "DECIMILLIPEDE_ELITE" => "decimillipede",
        _ => "default",
    };

    /// <summary>
    /// The monsters an NBestiaryLayout's Setup makes: a mutable copy of the
    /// entry's monster set up for combat outside any fight (NullCombatState),
    /// or the Decimillipede's segments in their encounter slots.
    /// </summary>
    private static List<Creature> BestiaryCreatures(string? monsterId, string encounterId)
    {
        var result = new List<Creature>();
        if (BestiaryLayout(encounterId) == "decimillipede")
        {
            var encounter = ModelDb.AllEncounters.First(e => e.Id.Entry == encounterId).ToMutable();
            encounter.GenerateMonstersWithSlots(MegaCrit.Sts2.Core.Runs.NullRunState.Instance);
            foreach (var (m, slot) in encounter.MonstersWithSlots)
            {
                m.Rng = Rng.Chaotic;
                m.SetUpForCombat();
                result.Add(new Creature(m, CombatSide.Enemy, slot) { CombatState = NullCombatState.Instance });
            }
            return result;
        }
        var canonical = ModelDb.Monsters.FirstOrDefault(x => x.Id.Entry == monsterId);
        if (canonical == null) return result;
        var monster = canonical.ToMutable();
        monster.Rng = Rng.Chaotic;
        monster.RunRng = new MegaCrit.Sts2.Core.Runs.RunRngSet(Rng.Chaotic);
        monster.SetUpForCombat();
        result.Add(new Creature(monster, CombatSide.Enemy, null) { CombatState = NullCombatState.Instance });
        return result;
    }

    /// <summary>One of NBestiary's move buttons (BestiaryMonsterMove).</summary>
    private struct BestiaryButton
    {
        public string Name;
        public BestiaryMonsterMove? Move;  // the monster's own
        public string? Layout;             // the layout's action: hurt, die, writhe, reattach, dead
    }

    /// <summary>
    /// The layout's Setup list: GenerateBestiaryMoveList, given the creature's
    /// visuals as far as it reads them (the animations its skeleton has, from
    /// the client), plus the Kaiser Crab's hurt / die arm; the Decimillipede's
    /// own three.
    /// </summary>
    private static List<BestiaryButton> BestiaryButtons(string layout, List<Creature> creatures, HashSet<string> anims)
    {
        var buttons = new List<BestiaryButton>();
        if (layout == "decimillipede")
        {
            foreach (var (key, act) in new[] { ("WRITHE", "writhe"), ("REATTACH", "reattach"), ("DEAD", "dead") })
                buttons.Add(new BestiaryButton
                {
                    Name = new LocString("monsters", ModelDb.GetId<DecimillipedeSegment>().Entry + ".moves." + key + ".title").GetFormattedText(),
                    Layout = act,
                });
            return buttons;
        }
        if (creatures.Count == 0) return buttons;
        var visuals = (MegaCrit.Sts2.Core.Nodes.Combat.NCreatureVisuals)System.Runtime.CompilerServices.RuntimeHelpers
            .GetUninitializedObject(typeof(MegaCrit.Sts2.Core.Nodes.Combat.NCreatureVisuals));
        // (the skeleton's questions are not cues)
        var before = Steps.Intercept;
        Steps.Intercept = (_, _) => { };
        StandIns.SkeletonAnims = anims;
        try
        {
            foreach (var mv in creatures[0].Monster!.GenerateBestiaryMoveList(visuals))
                buttons.Add(new BestiaryButton { Name = mv.displayName, Move = mv });
        }
        finally
        {
            StandIns.SkeletonAnims = null;
            Steps.Intercept = before;
        }
        if (layout == "kaiser_crab")
        {
            buttons.Add(new BestiaryButton { Name = new LocString("bestiary", "ACTION_NAME.hurt").GetFormattedText(), Layout = "hurt" });
            buttons.Add(new BestiaryButton { Name = new LocString("bestiary", "ACTION_NAME.die").GetFormattedText(), Layout = "die" });
        }
        return buttons;
    }

    /// <summary>
    /// NBestiaryLayout.Setup for one entry: the layout, its monsters (their
    /// spine setup, slots) and its move buttons. anims: the animations the
    /// monster's skeleton has (revive / hurt / die).
    /// </summary>
    public Dictionary<string, object?> BestiaryEntry(string? monsterId, string encounterId, HashSet<string> anims)
    {
        EnsureModelDbInitialized();
        string layout = BestiaryLayout(encounterId);
        var d = new Dictionary<string, object?>
        {
            ["type"] = "bestiary_entry", ["monster"] = monsterId, ["encounter"] = encounterId, ["layout"] = layout,
        };
        StandIns.InBestiary = true;
        try
        {
            var creatures = BestiaryCreatures(monsterId, encounterId);
            d["monsters"] = creatures.Select(c =>
            {
                var view = new Dictionary<string, object?> { ["id"] = c.Monster!.Id.Entry, ["slot"] = c.SlotName };
                var ops = SpineRecorder.Ops(c);
                if (ops.Count > 0) view["spine_ops"] = ops;
                if (SpineRecorder.HasPhobiaSkin(c)) view["phobia_skin"] = true;
                return view;
            }).ToList();
            // NBestiaryLayoutKaiserCrab: the arm this crab part is
            if (layout == "kaiser_crab" && creatures.Count > 0)
                d["arm"] = creatures[0].Monster is Crusher ? "left" : "right";
            d["moves"] = BestiaryButtons(layout, creatures, anims).Select(b => new Dictionary<string, object?>
            {
                ["name"] = b.Name,
                ["anim"] = b.Move?.animId,
                ["sfx"] = b.Move?.sfx,
                ["stop_sfx"] = b.Move?.stopSfxLoops ?? false,
                ["logic"] = b.Layout != null || b.Move?.animId == null,
                ["layout"] = b.Layout,
            }).ToList();
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"[WARN] bestiary {monsterId ?? encounterId}: {ex.Message}");
        }
        finally
        {
            StandIns.InBestiary = false;
        }
        return d;
    }

    /// <summary>
    /// NBestiary.PlayMoveAnim (and the layouts' own actions) for a move the
    /// logic plays: the state's move (SetMoveImmediate + PerformMove), the
    /// monster's own action, the Kaiser Crab's arm or the Decimillipede's
    /// segments, on fresh copies outside any fight (as the layouts make them).
    /// What it shows comes back as the response's steps; who: the creatures
    /// in the entry's order.
    /// </summary>
    public Dictionary<string, object?> BestiaryMove(string? monsterId, string encounterId, int index, HashSet<string> anims)
    {
        EnsureModelDbInitialized();
        string layout = BestiaryLayout(encounterId);
        var d = new Dictionary<string, object?> { ["type"] = "bestiary_move", ["monster"] = monsterId, ["index"] = index, ["ok"] = false };
        StandIns.InBestiary = true;
        try
        {
            var creatures = BestiaryCreatures(monsterId, encounterId);
            var buttons = BestiaryButtons(layout, creatures, anims);
            if (index < 0 || index >= buttons.Count || creatures.Count == 0) return d;
            d["who"] = creatures.Select(c => Steps.Who(c)).ToList();
            var b = buttons[index];
            var monster = creatures[0].Monster!;
            var side = monster is Crusher ? NKaiserCrabBossBackground.ArmSide.Left : NKaiserCrabBossBackground.ArmSide.Right;
            NKaiserCrabBossBackground Crab() => (NKaiserCrabBossBackground)System.Runtime.CompilerServices.RuntimeHelpers
                .GetUninitializedObject(typeof(NKaiserCrabBossBackground));
            switch (b.Layout)
            {
                case "hurt":
                    Crab().PlayHurtAnim(side);
                    break;
                case "die":
                    SfxCmd.Play(monster.DeathSfx);
                    Crab().PlayArmDeathAnim(side);
                    break;
                case "writhe":
                    foreach (var c in creatures) ((DecimillipedeSegment)c.Monster!).SegmentAttack();
                    // the rocks at the middle of the screen, then Cmd.Wait(0.5)
                    Steps.Cue(new Dictionary<string, object?> { ["t"] = "vfx", ["path"] = "vfx/vfx_decimillipede_rocks", ["at"] = "screen" });
                    Steps.Wait(500, "cmd");
                    break;
                case "reattach":
                    Sched.Run(async () =>
                    {
                        foreach (var c in creatures) await CreatureCmd.TriggerAnim(c, "Revive", 0.15f);
                    });
                    break;
                case "dead":
                    Sched.Run(async () =>
                    {
                        foreach (var c in creatures) await CreatureCmd.TriggerAnim(c, "Dead", 0.15f);
                    });
                    break;
                default:
                    var mv = b.Move!.Value;
                    if (mv.stateId != null)
                    {
                        monster.SetMoveImmediate((MegaCrit.Sts2.Core.MonsterMoves.MonsterMoveStateMachine.MoveState)monster.MoveStateMachine.States[mv.stateId], forceTransition: true);
                        Sched.Run(() => monster.PerformMove());
                    }
                    else if (mv.nonStateMove != null)
                    {
                        var run = mv.nonStateMove;
                        Sched.Run(() => run(Array.Empty<Creature>()));
                    }
                    else if (mv.action != null)
                    {
                        var run = mv.action;
                        Sched.Run(() => run());
                    }
                    break;
            }
            Steps.Capture();
            d["ok"] = true;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"[WARN] bestiary move {monsterId ?? encounterId} {index}: {ex.Message}");
        }
        finally
        {
            StandIns.InBestiary = false;
        }
        return d;
    }

    private static string ColorHex(Godot.Color c)
    {
        int B(float v) => (int)Math.Round(Math.Clamp(v, 0f, 1f) * 255f);
        return $"#{B(c.R):x2}{B(c.G):x2}{B(c.B):x2}{B(c.A):x2}";
    }
}
