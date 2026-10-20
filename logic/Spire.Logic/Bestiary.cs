// The compendium's Bestiary (NBestiary): its entries as the game lists them
// (the discovered acts' monsters, then the event encounters'), which are
// discovered, each monster's moves (GenerateBestiaryMoveList) and the
// statistics mode's kills / deaths and quotes per character.

using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Entities.Creatures;
using MegaCrit.Sts2.Core.Localization;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Acts;
using MegaCrit.Sts2.Core.Models.Characters;
using MegaCrit.Sts2.Core.Models.Encounters;
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

    /// <summary>
    /// NBestiaryLayout.Setup for one entry: the monsters it shows (a mutable
    /// copy set up for combat, as the layouts make it), their spine setup and
    /// the moves the buttons play (GenerateBestiaryMoveList; the skeleton's
    /// revive / hurt / die animations are the client's to add, with these
    /// sounds).
    /// </summary>
    public Dictionary<string, object?> BestiaryEntry(string? monsterId, string encounterId)
    {
        EnsureModelDbInitialized();
        var d = new Dictionary<string, object?> { ["type"] = "bestiary_entry", ["monster"] = monsterId, ["encounter"] = encounterId };
        var encounter = ModelDb.AllEncounters.FirstOrDefault(e => e.Id.Entry == encounterId);
        var monsters = new List<MonsterModel>();
        if (monsterId != null)
        {
            var m = ModelDb.Monsters.FirstOrDefault(x => x.Id.Entry == monsterId);
            if (m != null) monsters.Add(m);
        }
        else if (encounter != null)
        {
            monsters.AddRange(encounter.AllPossibleMonsters);
        }
        var views = new List<Dictionary<string, object?>>();
        foreach (var canonical in monsters)
        {
            var view = new Dictionary<string, object?> { ["id"] = canonical.Id.Entry, ["title"] = canonical.Title.GetFormattedText() };
            try
            {
                var monster = canonical.ToMutable();
                monster.Rng = Rng.Chaotic;
                monster.RunRng = new MegaCrit.Sts2.Core.Runs.RunRngSet(Rng.Chaotic);
                monster.SetUpForCombat();
                var creature = new Creature(monster, CombatSide.Enemy, null) { CombatState = NullCombatState.Instance };
                var ops = SpineRecorder.Ops(creature);
                if (ops.Count > 0) view["spine_ops"] = ops;
                if (SpineRecorder.HasPhobiaSkin(creature)) view["phobia_skin"] = true;
                var moves = new List<Dictionary<string, object?>>();
                foreach (var mv in monster.GenerateBestiaryMoveList(null))
                {
                    moves.Add(new Dictionary<string, object?>
                    {
                        ["name"] = mv.displayName,
                        ["state"] = mv.stateId,
                        ["anim"] = mv.animId,
                        ["sfx"] = mv.sfx,
                        ["stop_sfx"] = mv.stopSfxLoops,
                        ["custom"] = mv.nonStateMove != null || mv.action != null,
                    });
                }
                view["moves"] = moves;
                view["hurt_sfx"] = monster.TakeDamageSfx;
                view["death_sfx"] = monster.DeathSfx;
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"[WARN] bestiary {canonical.Id.Entry}: {ex.Message}");
            }
            views.Add(view);
        }
        d["monsters"] = views;
        return d;
    }

    /// <summary>
    /// NBestiary.PlayMoveAnim for a move the logic runs: the state's move
    /// (SetMoveImmediate + PerformMove) or the monster's own action, on a
    /// fresh copy outside any fight (NullCombatState, as the layouts make
    /// it). What it shows comes back as the response's steps (its cues).
    /// </summary>
    public Dictionary<string, object?> BestiaryMove(string? monsterId, int index)
    {
        EnsureModelDbInitialized();
        var d = new Dictionary<string, object?> { ["type"] = "bestiary_move", ["monster"] = monsterId, ["index"] = index, ["ok"] = false };
        var canonical = ModelDb.Monsters.FirstOrDefault(x => x.Id.Entry == monsterId);
        if (canonical == null) return d;
        try
        {
            var monster = canonical.ToMutable();
            monster.Rng = Rng.Chaotic;
            monster.RunRng = new MegaCrit.Sts2.Core.Runs.RunRngSet(Rng.Chaotic);
            monster.SetUpForCombat();
            var creature = new Creature(monster, CombatSide.Enemy, null) { CombatState = NullCombatState.Instance };
            var moves = monster.GenerateBestiaryMoveList(null);
            if (index < 0 || index >= moves.Count) return d;
            var mv = moves[index];
            d["who"] = Steps.Who(creature);
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
            Steps.Capture();
            d["ok"] = true;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"[WARN] bestiary move {monsterId} {index}: {ex.Message}");
        }
        return d;
    }

    private static string ColorHex(Godot.Color c)
    {
        int B(float v) => (int)Math.Round(Math.Clamp(v, 0f, 1f) * 255f);
        return $"#{B(c.R):x2}{B(c.G):x2}{B(c.B):x2}{B(c.A):x2}";
    }
}
