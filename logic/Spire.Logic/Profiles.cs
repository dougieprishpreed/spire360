// Profiles, progress and runs that follow the game's own flow.
//
// Saves keep the PC game's layout and format under <data>/user, which stands
// for %APPDATA%/SlayTheSpire2/steam/<account> on the PC: profile.save, then
// profileN/saves/{progress,prefs,current_run}.save and history/*.run. A PC
// profile copied there (tools/import_save.py) plays on here, and one played
// here can go back to the PC. The game's own SaveManager reads and writes
// them.
//
// Runs started with start_game_run follow the path the game takes from the
// main menu: the unlocks come from the progress save (UnlockState), the
// acts and the ascension from StartRunLobby.BeginRunLocally, the run is set
// up as NGame.StartNewSingleplayerRun does and saves itself as it goes
// (current_run.save); it continues as NMainMenu.OnContinueButtonPressedAsync
// and is abandoned as NMainMenu.AbandonRun / RunManager.Abandon; it ends
// with RunManager.OnEnded (progress, run history, epochs) and the progress
// the game over screen saves (badges, the score bar's epoch). Older runs
// (start_run) keep the test flow the seeded traces were recorded with.
using System.Text.Json;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Commands;
using MegaCrit.Sts2.Core.Context;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.Helpers;
using MegaCrit.Sts2.Core.Localization;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Characters;
using MegaCrit.Sts2.Core.Multiplayer;
using MegaCrit.Sts2.Core.Random;
using MegaCrit.Sts2.Core.Rewards;
using MegaCrit.Sts2.Core.Rooms;
using MegaCrit.Sts2.Core.Runs;
using MegaCrit.Sts2.Core.Saves;
using MegaCrit.Sts2.Core.Saves.Managers;
using MegaCrit.Sts2.Core.Timeline;
using MegaCrit.Sts2.Core.Timeline.Epochs;
using MegaCrit.Sts2.Core.Unlocks;

namespace Spire.Logic;

public partial class RunDriver
{
    /// <summary>The current run follows the game's own flow (saves, progress).</summary>
    internal bool GameFlow { get; private set; }

    /// <summary>RunManager.OnEnded ran for the current run: its outcome and save.</summary>
    private bool? _endedVictory;
    private SerializableRun? _endedRun;
    private Dictionary<string, object?>? _gameOverProgress;

    /// <summary>A profile was chosen (select_profile); 0: the test profile.</summary>
    private int _profileId;

    private static FileStore? _saveStore;

    /// <summary>The highest ascension (AscensionLevel.DoubleBoss).</summary>
    private const int MaxAscensionLevel = 10;

    internal void OnRunEnded(bool victory, object? save)
    {
        if (!GameFlow || _endedVictory != null) return;
        _endedVictory = victory;
        _endedRun = save as SerializableRun;
        Log($"Run ended ({(victory ? "victory" : "defeat")})");
    }

    // ─── Spire360's own per-profile options (spire360.json) ───

    /// <summary>
    /// "Unlock everything": the profile's runs start with every character,
    /// card, relic, event and act (UnlockState.all) and any ascension up to
    /// 10. Progress is still recorded as the game records it.
    /// </summary>
    private bool UnlockAll => _profileId > 0 && ReadOptions().TryGetValue(_profileId.ToString(), out var o) && o;

    private static string OptionsPath => Path.Combine(_saveStore?.Root ?? Path.Combine(DataDir.Root, "user"), "spire360.json");

    private static Dictionary<string, bool> ReadOptions()
    {
        var result = new Dictionary<string, bool>();
        try
        {
            if (!File.Exists(OptionsPath)) return result;
            using var doc = JsonDocument.Parse(File.ReadAllText(OptionsPath));
            if (doc.RootElement.TryGetProperty("unlock_all", out var ua) && ua.ValueKind == JsonValueKind.Object)
                foreach (var p in ua.EnumerateObject())
                    result[p.Name] = p.Value.ValueKind == JsonValueKind.True;
        }
        catch (Exception ex) { Log($"spire360.json: {ex.Message}"); }
        return result;
    }

    public Dictionary<string, object?> SetUnlockAll(bool on)
    {
        if (_profileId <= 0) return Error("No profile selected");
        var options = ReadOptions();
        options[_profileId.ToString()] = on;
        var body = JsonSerializer.Serialize(new Dictionary<string, object> { ["unlock_all"] = options });
        _saveStore?.WriteFile("spire360.json", body);
        return MainMenu();
    }

    // ─── Profiles (NProfileScreen) ───

    /// <summary>NProfileScreen's three buttons (NProfileButton.Initialize).</summary>
    public Dictionary<string, object?> Profiles()
    {
        EnsureModelDbInitialized();
        var list = new List<Dictionary<string, object?>>();
        var current = _profileId > 0 ? _profileId : -1;
        for (int id = 1; id <= 3; id++)
        {
            var dir = Path.Combine(_saveStore!.Root, UserDataPathProvider.GetProfileDir(id), UserDataPathProvider.SavesDir);
            var progress = Path.Combine(dir, ProgressSaveManager.fileName);
            var title = new LocString("main_menu_ui", "PROFILE_SCREEN.BUTTON.title");
            title.Add("Id", id);
            var d = new Dictionary<string, object?>
            {
                ["id"] = id,
                ["title"] = Text(() => title.GetFormattedText()),
                ["exists"] = File.Exists(progress),
                ["has_run"] = File.Exists(Path.Combine(dir, "current_run.save")),
                ["unlock_all"] = ReadOptions().TryGetValue(id.ToString(), out var ua) && ua,
            };
            try
            {
                if (!File.Exists(progress))
                {
                    d["description"] = Text(() => new LocString("main_menu_ui", "PROFILE_SCREEN.BUTTON.empty").GetFormattedText());
                }
                else
                {
                    long? playtime = null;
                    if (id == current) playtime = SaveManager.Instance.Progress.TotalPlaytime;
                    else
                    {
                        using var doc = JsonDocument.Parse(File.ReadAllText(progress));
                        if (doc.RootElement.TryGetProperty("total_playtime", out var pt) && pt.TryGetInt64(out var pl)) playtime = pl;
                    }
                    var updated = File.GetLastWriteTimeUtc(progress);
                    foreach (var f in new[] { "current_run.save", "current_run_mp.save" })
                    {
                        var path = Path.Combine(dir, f);
                        if (File.Exists(path) && File.GetLastWriteTimeUtc(path) > updated) updated = File.GetLastWriteTimeUtc(path);
                    }
                    var desc = new LocString("main_menu_ui", "PROFILE_SCREEN.BUTTON.description");
                    desc.Add("Playtime", playtime != null ? TimeFormatting.Format(playtime.Value) : "???");
                    var local = TimeZoneInfo.ConvertTimeFromUtc(updated, TimeZoneInfo.Local);
                    var format = new LocString("main_menu_ui", "PROFILE_SCREEN.BUTTON.dateFormat").GetFormattedText();
                    desc.Add("LastUpdatedTime", local.ToString(format, LocManager.Instance.CultureInfo.DateTimeFormat));
                    d["description"] = Text(() => desc.GetFormattedText());
                }
            }
            catch (Exception ex) { d["error"] = ex.Message; }
            list.Add(d);
        }
        int last = 1;
        try
        {
            var path = Path.Combine(_saveStore!.Root, "profile.save");
            if (File.Exists(path))
            {
                using var doc = JsonDocument.Parse(File.ReadAllText(path));
                if (doc.RootElement.TryGetProperty("last_profile_id", out var lp) && lp.TryGetInt32(out var lpi)) last = lpi;
            }
        }
        catch (Exception ex) { Log($"profile.save: {ex.Message}"); }
        return new Dictionary<string, object?>
        {
            ["type"] = "profiles", ["last_profile"] = last, ["current"] = _profileId,
            ["message"] = Text(() => new LocString("main_menu_ui", "PROFILE_SCREEN.BUTTON.chooseProfileMessage").GetFormattedText()),
            ["profiles"] = list,
        };
    }

    /// <summary>
    /// NProfileScreen's choice (SaveManager.SwitchProfileId, then the
    /// profile's prefs and progress), or the last profile when id is 0, as
    /// the game does at start-up.
    /// </summary>
    public Dictionary<string, object?> SelectProfile(int id, string lang = "en")
    {
        try
        {
            _loc.Lang = lang;
            EnsureModelDbInitialized();
            if (_runState != null) EndRun();
            if (id <= 0)
            {
                var p = Profiles();
                id = p["last_profile"] is int l && l >= 1 && l <= 3 ? l : 1;
            }
            if (id < 1 || id > 3) return Error($"No profile {id}");
            // The game's start-up reads profile.save (InitProfileId); the
            // adapter starts on its test profile, so read it here first.
            SaveManager.Instance.InitProfileId();
            if (SaveManager.Instance.CurrentProfileId != id) SaveManager.Instance.SwitchProfileId(id);
            _profileId = id;
            SaveManager.Instance.InitPrefsData();
            SaveManager.Instance.InitProgressData();
            Log($"Profile {id}: {SaveManager.Instance.Progress.NumberOfRuns} runs, {SaveManager.Instance.Progress.Epochs.Count} epochs");
            return MainMenu();
        }
        catch (Exception ex) { return ErrorWithTrace("select_profile failed", ex); }
    }

    // ─── The main menu (NMainMenu.RefreshButtons, NContinueRunInfo) ───

    public Dictionary<string, object?> MainMenu()
    {
        try
        {
            EnsureModelDbInitialized();
            if (_profileId <= 0) return Error("No profile selected");
            var sm = SaveManager.Instance;
            var progress = sm.Progress;
            var d = new Dictionary<string, object?>
            {
                ["type"] = "main_menu",
                ["profile"] = _profileId,
                ["unlock_all"] = UnlockAll,
                ["number_of_runs"] = progress.NumberOfRuns,
                ["has_run_save"] = sm.HasRunSave,
            };
            if (sm.HasRunSave)
            {
                var read = sm.LoadRunSave();
                d["run"] = read.Success && read.SaveData != null ? ContinueInfo(read.SaveData)
                    : new Dictionary<string, object?> { ["error"] = $"{read.Status} {read.ErrorMessage}" };
            }
            // UpdateTimelineButtonBehavior: obtained epochs wait on the
            // timeline (and the other buttons wait for them) unless a run
            // is saved.
            int discovered = sm.GetDiscoveredEpochCount();
            bool timelineVisible, timelineEnabled, notify = false, othersEnabled = true;
            if (discovered > 0 && !sm.HasRunSave)
            {
                timelineVisible = timelineEnabled = notify = true;
                othersEnabled = false;
            }
            else if (progress.Epochs.Count > 1 && sm.IsEpochRevealed<NeowEpoch>())
            {
                timelineVisible = true;
                timelineEnabled = discovered == 0;
            }
            else
            {
                timelineVisible = progress.Epochs.Count > 1;
                timelineEnabled = false;
            }
            d["timeline"] = new Dictionary<string, object?>
            {
                ["visible"] = timelineVisible, ["enabled"] = timelineEnabled, ["notification"] = notify,
                ["discovered"] = discovered,
            };
            d["singleplayer_enabled"] = othersEnabled;
            d["compendium_available"] = sm.IsCompendiumAvailable();
            d["characters"] = CharacterProgress();
            // NAscensionPanel.RefreshAscensionText.
            var ascensions = new List<Dictionary<string, object?>>();
            for (int i = 0; i <= MaxAscensionLevel; i++)
            {
                int level = i;
                ascensions.Add(new Dictionary<string, object?>
                {
                    ["title"] = Text(() => AscensionHelper.GetTitle(level).GetFormattedText()),
                    ["description"] = Text(() => AscensionHelper.GetDescription(level).GetFormattedText()),
                });
            }
            d["ascensions"] = ascensions;
            // NCharacterSelectScreen plays this character's unlock once.
            var pending = progress.PendingCharacterUnlock;
            d["pending_unlock"] = pending == ModelId.none ? null : pending.Entry;
            return d;
        }
        catch (Exception ex) { return ErrorWithTrace("main_menu failed", ex); }
    }

    /// <summary>NContinueRunInfo.ShowInfo: its labels' text and the icon.</summary>
    private static Dictionary<string, object?> ContinueInfo(SerializableRun save)
    {
        var info = new Dictionary<string, object?> { ["ascension"] = save.Ascension, ["saved_time"] = save.SaveTime };
        try
        {
            var local = TimeZoneInfo.ConvertTimeFromUtc(DateTimeOffset.FromUnixTimeSeconds(save.SaveTime).UtcDateTime, TimeZoneInfo.Local);
            var format = new LocString("main_menu_ui", "CONTINUE_RUN_INFO.savedTimeFormat").GetRawText();
            var saved = new LocString("main_menu_ui", "CONTINUE_RUN_INFO.saved");
            saved.Add("LastSavedTime", local.ToString(format, LocManager.Instance.CultureInfo.DateTimeFormat));
            info["date_text"] = saved.GetFormattedText();
            if (save.Ascension > 0)
                info["ascension_text"] = $"{new LocString("main_menu_ui", "CONTINUE_RUN_INFO.ascension").GetFormattedText()} {save.Ascension}";
            var act = ModelDb.GetById<ActModel>(save.Acts[save.CurrentActIndex].Id);
            var player = save.Players[0];
            int floor = save.VisitedMapCoords.Count;
            for (int i = 0; i < save.CurrentActIndex; i++)
                floor += ModelDb.GetById<ActModel>(save.Acts[i].Id).GetNumberOfFloors(save.Players.Count > 1);
            info["character"] = player.CharacterId.Entry;
            info["icon"] = "images/ui/top_panel/character_icon_" + player.CharacterId.Entry.ToLowerInvariant() + ".png";
            info["act_title"] = act.Title.GetFormattedText();
            info["floor"] = floor;
            info["progress_text"] = $"{act.Title.GetFormattedText()} [blue]- {new LocString("main_menu_ui", "CONTINUE_RUN_INFO.floor").GetFormattedText()} {floor}[/blue]";
            info["hp"] = player.CurrentHp;
            info["max_hp"] = player.MaxHp;
            info["gold"] = player.Gold;
            info["health_text"] = $"[red]{player.CurrentHp}/{player.MaxHp}[/red]";
            info["gold_text"] = $"[gold]{player.Gold}[/gold]";
        }
        catch (Exception ex) { info["error"] = ex.Message; }
        return info;
    }

    /// <summary>
    /// What the character select screen needs from the progress save: which
    /// characters are unlocked (UnlockState.Characters) and their ascension
    /// panel (StartRunLobby.SetSingleplayerAscensionAfterCharacterChanged).
    /// </summary>
    private List<Dictionary<string, object?>> CharacterProgress()
    {
        var all = UnlockAll;
        var unlocks = all ? UnlockState.all : SaveManager.Instance.GenerateUnlockStateFromProgress();
        var unlocked = unlocks.Characters.Select(c => c.Id).ToHashSet();
        var list = new List<Dictionary<string, object?>>();
        foreach (var c in ModelDb.AllCharacters)
        {
            var stats = SaveManager.Instance.Progress.CharacterStats.TryGetValue(c.Id, out var s) ? s : null;
            int max = MaxAscensionFor(c, all);
            list.Add(new Dictionary<string, object?>
            {
                ["id"] = c.Id.Entry,
                ["unlocked"] = unlocked.Contains(c.Id),
                ["max_ascension"] = max,
                ["ascension"] = Math.Min(stats?.PreferredAscension ?? 0, max),
                ["wins"] = stats?.TotalWins ?? 0,
                ["losses"] = stats?.TotalLosses ?? 0,
                // NCharacterSelectButton's hover tip while locked.
                ["unlock_text"] = Text(() => c.GetUnlockText().GetFormattedText()),
            });
        }
        return list;
    }

    private static int MaxAscensionFor(CharacterModel c, bool unlockAll)
    {
        if (unlockAll) return MaxAscensionLevel;
        var stats = SaveManager.Instance.Progress.CharacterStats.TryGetValue(c.Id, out var s) ? s : null;
        if (stats == null || stats.MaxAscension <= 0 || !IsAscensionEpochRevealed(c)) return 0;
        return stats.MaxAscension;
    }

    /// <summary>StartRunLobby.IsAscensionEpochRevealed.</summary>
    private static bool IsAscensionEpochRevealed(CharacterModel c) => c switch
    {
        Ironclad => SaveManager.Instance.IsEpochRevealed<Ironclad4Epoch>(),
        Silent => SaveManager.Instance.IsEpochRevealed<Silent4Epoch>(),
        Regent => SaveManager.Instance.IsEpochRevealed<Regent4Epoch>(),
        Defect => SaveManager.Instance.IsEpochRevealed<Defect4Epoch>(),
        Necrobinder => SaveManager.Instance.IsEpochRevealed<Necrobinder4Epoch>(),
        _ => true,
    };

    /// <summary>NCharacterSelectScreen.PlayUnlockCharacterAnimation's end.</summary>
    public Dictionary<string, object?> ClearPendingUnlock()
    {
        if (_profileId <= 0) return Error("No profile selected");
        SaveManager.Instance.Progress.PendingCharacterUnlock = ModelId.none;
        SaveManager.Instance.SaveProgressFile();
        return MainMenu();
    }

    // ─── A new run (NCharacterSelectScreen, StartRunLobby, NGame) ───

    public Dictionary<string, object?> StartGameRun(string characterName, int ascension, string? seed, string lang)
    {
        try
        {
            _loc.Lang = lang;
            EnsureModelDbInitialized();
            if (_profileId <= 0) return Error("No profile selected");
            if (_runState != null) EndRun();
            var character = ModelDb.AllCharacters.FirstOrDefault(c =>
                string.Equals(c.Id.Entry, characterName, StringComparison.OrdinalIgnoreCase));
            if (character == null) return Error($"Unknown character: {characterName}");
            bool all = UnlockAll;
            var unlocks = all ? UnlockState.all : SaveManager.Instance.GenerateUnlockStateFromProgress();
            if (!unlocks.Characters.Contains(character)) return Error($"{character.Id.Entry} is locked");
            var seedStr = string.IsNullOrEmpty(seed) ? SeedHelper.GetRandomSeed() : seed!;
            GameFlow = true;

            // StartRunLobby.BeginRunLocally: the acts, then the ascension
            // within the character's maximum; UpdatePreferredAscension.
            var rng = new Rng(StringHelper.GetDeterministicHashCode(seedStr), "act_selection");
            var acts = ActModel.GetRandomList(rng, unlocks, isMultiplayer: false).ToList();
            int max = MaxAscensionFor(character, all);
            ascension = Math.Clamp(ascension, 0, max);
            var stats = SaveManager.Instance.Progress.GetOrCreateCharacterStats(character.Id);
            if (stats.MaxAscension != 0 && stats.PreferredAscension != ascension)
            {
                stats.PreferredAscension = ascension;
                SaveManager.Instance.SaveProgressFile();
            }
            Log($"Embarking on a singleplayer {character.Id.Entry} run. Ascension: {ascension} Seed: {seedStr} " +
                $"Acts: {string.Join(",", acts.Select(a => a.Id.Entry))}");

            // NGame.StartNewSingleplayerRun, then NGame.StartRun.
            var player = Player.CreateForNewRun(character, unlocks, 1uL);
            _runState = RunState.CreateForNewRun(new[] { player }, acts.Select(a => a.ToMutable()).ToList(),
                Array.Empty<ModifierModel>(), GameMode.Standard, ascension, seedStr);
            RunManager.Instance.SetUpNewSingleplayer(_runState, shouldSave: true);
            LocalContext.NetId = RunManager.Instance.NetService.NetId;
            AttachRun();
            Sched.Run(() => RunManager.Instance.FinalizeStartingRelics());
            RunManager.Instance.Launch();
            Sched.Run(() => RunManager.Instance.EnterAct(0, doTransition: false));
            Log("Run launched");
            return DetectDecisionPoint();
        }
        catch (Exception ex) { return ErrorWithTrace("StartGameRun failed", ex); }
    }

    /// <summary>The driver's hooks into a run that has just been set up.</summary>
    private void AttachRun()
    {
        CombatManager.Instance.TurnStarted += _ => _turnStarted = true;
        CombatManager.Instance.CombatEnded += _ => _combatEnded = true;
        RewardsSet.testSelector = OfferRewardsHeadless;
        _selectorScope?.Dispose();
        _selectorScope = CardSelectCmd.UseSelector(_cardSelector);
        Hooks.Driver = this;
    }

    // ─── Continue, abandon, save and quit (NMainMenu, NPauseMenu) ───

    public Dictionary<string, object?> ContinueRun(string lang)
    {
        try
        {
            _loc.Lang = lang;
            EnsureModelDbInitialized();
            if (_profileId <= 0) return Error("No profile selected");
            if (_runState != null) EndRun();
            if (!SaveManager.Instance.HasRunSave) return Error("No saved run");
            var read = SaveManager.Instance.LoadRunSave();
            if (!read.Success || read.SaveData == null)
                return Error($"The saved run cannot be loaded: {read.Status} {read.ErrorMessage}");
            var save = read.SaveData;
            GameFlow = true;
            _runState = RunState.FromSerializable(save);
            Sched.Run(() => RunManager.Instance.SetUpSavedSingleplayer(_runState, save));
            LocalContext.NetId = RunManager.Instance.NetService.NetId;
            AttachRun();
            Log($"Continuing run with character: {save.Players[0].CharacterId}");
            // NGame.LoadRun.
            RunManager.Instance.Launch();
            Sched.Run(() => RunManager.Instance.GenerateMap());
            var preFinished = AbstractRoom.FromSerializable(save.PreFinishedRoom, _runState);
            Sched.Run(() => RunManager.Instance.LoadIntoLatestMapCoord(preFinished));
            WaitForActionExecutor();
            return DetectDecisionPoint();
        }
        catch (Exception ex)
        {
            // OnContinueButtonPressedAsync: clean up, report the bad save.
            try { RunManager.Instance.CleanUp(); } catch { }
            _runState = null;
            GameFlow = false;
            return ErrorWithTrace("The saved run cannot be continued", ex);
        }
    }

    /// <summary>NMainMenu.AbandonRun: the saved run counts as a loss.</summary>
    public Dictionary<string, object?> AbandonSavedRun()
    {
        try
        {
            EnsureModelDbInitialized();
            if (_profileId <= 0) return Error("No profile selected");
            if (_runState != null) EndRun();
            if (!SaveManager.Instance.HasRunSave) return MainMenu();
            var read = SaveManager.Instance.LoadRunSave();
            if (read.Success && read.SaveData != null)
            {
                try
                {
                    Log("Abandoning run from main menu");
                    var save = read.SaveData;
                    SaveManager.Instance.UpdateProgressWithRunData(save, victory: false);
                    RunHistoryUtilities.CreateRunHistoryEntry(save, victory: false, isAbandoned: true, save.PlatformType);
                }
                catch (Exception ex) { Log($"ERROR: Failed to record the abandoned run: {ex}"); }
            }
            else
            {
                Log($"Abandoning run with invalid save (status={read.Status})");
            }
            SaveManager.Instance.DeleteCurrentRun();
            return MainMenu();
        }
        catch (Exception ex) { return ErrorWithTrace("abandon_run failed", ex); }
    }

    /// <summary>NPauseMenu's Give Up (RunManager.Abandon): the player dies.</summary>
    public Dictionary<string, object?> GiveUp()
    {
        if (_runState == null) return Error("No run in progress");
        if (!GameFlow) return Error("Give up needs a run started with start_game_run");
        RunManager.Instance.Abandon();
        Sched.Pump();
        WaitForActionExecutor();
        return DetectDecisionPoint();
    }

    /// <summary>
    /// NPauseMenu's Save and Quit (NGame.ReturnToMainMenu): the run was saved
    /// when its room was entered; it is cleaned up and the menu returns.
    /// </summary>
    public Dictionary<string, object?> ReturnToMenu()
    {
        if (_runState != null) EndRun();
        return _profileId > 0 ? MainMenu() : new Dictionary<string, object?> { ["type"] = "main_menu" };
    }

    // ─── The end of a run (RunManager.OnEnded, NGameOverScreen) ───

    /// <summary>
    /// The death path of CreatureCmd.Kill outside test mode
    /// (RunManager.OnEnded) and what NGameOverScreen saves to the progress:
    /// the badges (AnimateBadges) and the score bar's unlock
    /// (AnimateScoreBar). Once per run.
    /// </summary>
    private Dictionary<string, object?>? GameOverProgress(bool won)
    {
        if (!GameFlow) return null;
        if (_gameOverProgress != null) return _gameOverProgress;
        var d = new Dictionary<string, object?>();
        _gameOverProgress = d;
        try
        {
            SerializableRun? run = _endedRun;
            if (_endedVictory == null || run == null)
                run = RunManager.Instance.OnEnded(won);
            if (run == null) return d;
            var me = run.Players[0];
            var player = _runState!.Players[0];
            int score = ScoreUtility.CalculateScore(run, won);
            d["score"] = score;

            var badges = ScoreUtility.GetBadges(run, me.NetId, won);
            var badgeList = new List<Dictionary<string, object?>>();
            if (!run.GameMode.AreAchievementsAndEpochsLocked()
                && SaveManager.Instance.Progress.CharacterStats.TryGetValue(player.Character.Id, out var characterStats))
            {
                foreach (var badge in badges)
                {
                    var stats = characterStats.Badges.FirstOrDefault(b => b.Id.Equals(badge.Id) && b.Rarity == badge.Rarity);
                    if (stats == null) characterStats.Badges.Add(new BadgeStats { Id = badge.Id, Count = 1, Rarity = badge.Rarity });
                    else stats.Count++;
                }
                SaveManager.Instance.SaveProgressFile();
            }
            foreach (var badge in badges)
                badgeList.Add(new Dictionary<string, object?> { ["id"] = badge.Id, ["rarity"] = badge.Rarity.ToString() });
            d["badges"] = badgeList;

            // AnimateScoreBar.
            int remaining = SaveManager.Instance.GetUnlocksRemaining();
            var bar = new Dictionary<string, object?> { ["unlocks_remaining"] = remaining };
            d["score_bar"] = bar;
            if (remaining > 0)
            {
                int current = SaveManager.Instance.GetCurrentScore();
                int threshold = ScoreThreshold(remaining);
                bar["current"] = current;
                bar["threshold"] = threshold;
                if (current + score >= threshold)
                {
                    current -= threshold;
                    int next = ScoreThreshold(remaining - 1);
                    var epoch = SaveManager.Instance.GrantNextUnlock();
                    if (epoch != null)
                    {
                        player.DiscoveredEpochs.Add(epoch.Id);
                        me.DiscoveredEpochs.Add(epoch.Id);
                        bar["epoch"] = epoch.Id;
                        bar["epoch_title"] = Text(() => epoch.Title.GetFormattedText());
                    }
                    current += score;
                    int after = next == 0 || current == 0 ? 0 : current >= next ? next - 1 : current;
                    SaveManager.Instance.Progress.CurrentScore = after;
                    SaveManager.Instance.SaveProgressFile();
                    bar["unlocked"] = true;
                    bar["next_threshold"] = next;
                    bar["after"] = after;
                }
                else
                {
                    SaveManager.Instance.Progress.CurrentScore += score;
                    SaveManager.Instance.SaveProgressFile();
                    bar["unlocked"] = false;
                    bar["after"] = current + score;
                }
            }
            var epochs = new List<string>();
            foreach (var id in me.DiscoveredEpochs) epochs.Add(id);
            d["discovered_epochs"] = epochs;
        }
        catch (Exception ex) { Log($"Game over progress: {ex}"); d["error"] = ex.Message; }
        return d;
    }

    /// <summary>NGameOverScreen.GetScoreThreshold (a UI class, stripped here).</summary>
    private static int ScoreThreshold(int unlocksRemaining)
    {
        int n = SaveManager.TotalAgnosticUnlocks - unlocksRemaining;
        if (n < 0 || n >= EpochModel.AgnosticUnlocks.Count) return 0;
        return EpochModel.AgnosticUnlocks[n].ScoreThreshold;
    }
}
