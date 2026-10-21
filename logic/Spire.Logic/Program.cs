// Command router ported from sts2-cli's Program.cs
// (https://github.com/Garglemesh/sts2-cli, MIT, (c) 2025 Hao Wu).
//
// Spire360 changes: a synchronous command loop (NativeAOT-LLVM on wasip1 has
// no event loop and no blocking waits; Sched drives all async game code),
// every response gets the animation "steps" recorded while it
// was computed, nested decisions for synchronous game prompts, and no
// assembly-resolve hook (the game assemblies are referenced directly).
using System.Text.Json;
using MegaCrit.Sts2.Core.Entities.Players;

namespace Spire.Logic;

internal static class Program
{
    public const string Version = "0.1.0";

    private static int Main(string[] args)
    {
        AppDomain.CurrentDomain.UnhandledException += (_, e) =>
            Log.Error($"Unhandled: {e.ExceptionObject}");
        TaskScheduler.UnobservedTaskException += (_, e) =>
        {
            Log.Warn($"Unobserved task exception: {e.Exception?.Message}");
            e.SetObserved();
        };

        // IL-patch handlers must exist before any game code runs.
        Hooks.Install();

        // UTF-8 both ways whatever the platform's console code page is
        // (Windows would otherwise use the OEM code page for redirected pipes).
        var utf8 = new System.Text.UTF8Encoding(false);
        var proto = new Protocol(
            new StreamReader(Console.OpenStandardInput(), utf8),
            new StreamWriter(Console.OpenStandardOutput(), utf8));
        var driver = new RunDriver();
        Hooks.Driver = driver;
        Steps.Attach(driver.LocalPlayer);
        RunDriver.NestedDecisionLoop = pending => NestedLoop(proto, driver, pending);

        proto.Write(new Dictionary<string, object?>
        {
            ["type"] = "ready",
            ["version"] = "0.2.0",           // sts2-cli protocol version this follows
            ["spire_version"] = Version,
            ["data_dir"] = DataDir.Root,
        });

        while (true)
        {
            var cmd = proto.ReadCommand();
            if (cmd == null) break;

            Dictionary<string, object?>? result;
            try
            {
                if (Protocol.Str(cmd.Value, "cmd") == "selfplay")
                {
                    result = SelfPlay.Run(driver,
                        Protocol.Str(cmd.Value, "character", "Ironclad")!,
                        Protocol.Str(cmd.Value, "seed", "selfplay")!,
                        Protocol.Int(cmd.Value, "steps", 3000),
                        cmd.Value.TryGetProperty("tank", out var tk) && tk.ValueKind == JsonValueKind.True);
                    proto.Write(result);
                    continue;
                }
                result = HandleCommand(driver, cmd.Value);
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine("[ERROR] " + ex);
                result = new Dictionary<string, object?> { ["type"] = "error", ["message"] = $"{ex.GetType().Name}: {ex.Message}" };
            }

            // Let thread-pool work and timers that left the queue finish; if
            // that changed the game, the decision point may have moved on.
            long afterHandle = Sched.CallbacksRun;
            Sched.Settle();
            if (Sched.CallbacksRun != afterHandle && result != null
                && result.TryGetValue("type", out var t) && (t as string) == "decision")
            {
                result = driver.CurrentDecision();
            }

            if (result == null) continue;
            AttachSteps(result);
            proto.Write(result);
            if (result.TryGetValue("type", out var rt) && (rt as string) == "quit_result")
                break;
        }
        return 0;
    }

    private static void AttachSteps(Dictionary<string, object?> response)
    {
        var steps = Steps.Drain();
        if (steps != null) response["steps"] = steps;
    }

    /// <summary>
    /// Runs while the game is inside a synchronous prompt (event card rewards):
    /// reports the decision, then serves commands until one resolves it. The
    /// resolving command gets its response when the outer command finishes.
    /// </summary>
    private static void NestedLoop(Protocol proto, RunDriver driver, Func<bool> pending)
    {
        var decision = driver.CurrentDecision();
        AttachSteps(decision);
        proto.Write(decision);
        while (pending())
        {
            var cmd = proto.ReadCommand();
            if (cmd == null)
            {
                driver.SkipNestedReward();
                return;
            }
            if (driver.TryResolveNested(cmd.Value))
                return;
            var cmdType = Protocol.Str(cmd.Value, "cmd") ?? "";
            Dictionary<string, object?> reply = cmdType switch
            {
                "get_map" => driver.GetFullMap(),
                "view_pile" => driver.ViewPile(Protocol.Str(cmd.Value, "pile", "deck")!),
                "view_map" => driver.ViewMap(),
                "settings" => driver.Settings(),
                "library" => driver.Library(Protocol.Str(cmd.Value, "pool", "ironclad")!),
                "relic_collection" => driver.RelicCollection(),
                "potion_lab" => driver.PotionLab(),
                "statistics" => driver.Statistics(),
                "compendium" => driver.CompendiumState(),
                "bestiary" => driver.Bestiary(),
                "bestiary_entry" => driver.BestiaryEntry(Protocol.Str(cmd.Value, "monster"), Protocol.Str(cmd.Value, "encounter") ?? "", Anims(cmd.Value)),
                "bestiary_move" => driver.BestiaryMove(Protocol.Str(cmd.Value, "monster"), Protocol.Str(cmd.Value, "encounter") ?? "",
                    cmd.Value.TryGetProperty("index", out var nestedMoveIndex) ? nestedMoveIndex.GetInt32() : 0, Anims(cmd.Value)),
                "run_history" => driver.RunHistoryView(cmd.Value.TryGetProperty("index", out var nestedRunIndex) ? nestedRunIndex.GetInt32() : 0),
                "get_state" => driver.CurrentDecision(),
                _ => new Dictionary<string, object?>
                {
                    ["type"] = "error",
                    ["message"] = "A card reward choice is pending: send select_card_reward or skip_card_reward",
                },
            };
            proto.Write(reply);
        }
    }

    /// <summary>A bestiary command's "anims": the skeleton's animations the client has.</summary>
    private static HashSet<string> Anims(JsonElement cmd)
    {
        var set = new HashSet<string>();
        if (cmd.TryGetProperty("anims", out var a) && a.ValueKind == JsonValueKind.Array)
            foreach (var x in a.EnumerateArray())
                if (x.ValueKind == JsonValueKind.String) set.Add(x.GetString()!);
        return set;
    }

    private static Dictionary<string, object?>? HandleCommand(RunDriver sim, JsonElement cmd)
    {
        var cmdType = Protocol.Str(cmd, "cmd") ?? "";
        switch (cmdType)
        {
            case "start_run":
                return sim.StartRun(
                    Protocol.Str(cmd, "character", "Ironclad")!,
                    Protocol.Int(cmd, "ascension", 0),
                    Protocol.Str(cmd, "seed"),
                    Protocol.Str(cmd, "lang"));

            case "start_game_run":
                return sim.StartGameRun(
                    Protocol.Str(cmd, "character", "Ironclad")!,
                    Protocol.Int(cmd, "ascension", 0),
                    Protocol.Str(cmd, "seed"),
                    Protocol.Str(cmd, "lang"));

            case "profiles":
                return sim.Profiles();

            case "select_profile":
                return sim.SelectProfile(Protocol.Int(cmd, "profile", 0), Protocol.Str(cmd, "lang"));

            case "main_menu":
                return sim.MainMenu();

            case "set_unlock_all":
                return sim.SetUnlockAll(cmd.TryGetProperty("on", out var on) && on.ValueKind == JsonValueKind.True);

            case "continue_run":
                return sim.ContinueRun(Protocol.Str(cmd, "lang"));

            case "abandon_run":
                return sim.AbandonSavedRun();

            case "give_up":
                return sim.GiveUp();

            case "return_to_menu":
                return sim.ReturnToMenu();

            case "content":
                return sim.Content();

            case "obtain":
                return sim.Obtain(cmd);

            case "add_to_hand":
                return sim.AddToHand(cmd);

            case "clear_pending_unlock":
                return sim.ClearPendingUnlock();

            case "timeline":
                return sim.Timeline();

            case "timeline_first_time_done":
                return sim.TimelineFirstTimeDone();

            case "reveal_epoch":
                return sim.RevealEpoch(Protocol.Str(cmd, "epoch") ?? "");

            case "characters":
                return sim.Characters(Protocol.Str(cmd, "lang"));

            case "action":
                return sim.ExecuteAction(Protocol.Str(cmd, "action") ?? "", Protocol.Args(cmd));

            case "get_state":
                return sim.HasRun ? sim.CurrentDecision() : new Dictionary<string, object?> { ["type"] = "error", ["message"] = "No run in progress" };

            case "load_save":
            {
                var savePath = Protocol.Str(cmd, "path");
                var saveJson = Protocol.Str(cmd, "json");
                if (saveJson == null && savePath != null)
                {
                    if (!File.Exists(savePath))
                        return new Dictionary<string, object?> { ["type"] = "error", ["message"] = $"Save file not found: {savePath}" };
                    saveJson = File.ReadAllText(savePath);
                }
                if (saveJson == null)
                    return new Dictionary<string, object?> { ["type"] = "error", ["message"] = "Provide 'path' or 'json' for load_save" };
                return sim.LoadSave(saveJson, Protocol.Str(cmd, "lang"));
            }

            case "get_map":
                return sim.GetFullMap();

            case "view_pile":
                return sim.ViewPile(Protocol.Str(cmd, "pile", "deck")!);

            case "view_map":
                return sim.ViewMap();

            case "settings":
                return sim.Settings();

            case "library":
                return sim.Library(Protocol.Str(cmd, "pool", "ironclad")!);

            case "relic_collection":
                return sim.RelicCollection();

            case "potion_lab":
                return sim.PotionLab();

            case "statistics":
                return sim.Statistics();

            case "compendium":
                return sim.CompendiumState();

            case "bestiary":
                return sim.Bestiary();

            case "bestiary_entry":
                return sim.BestiaryEntry(Protocol.Str(cmd, "monster"), Protocol.Str(cmd, "encounter") ?? "", Anims(cmd));

            case "bestiary_move":
                return sim.BestiaryMove(Protocol.Str(cmd, "monster"), Protocol.Str(cmd, "encounter") ?? "",
                    cmd.TryGetProperty("index", out var moveIndex) ? moveIndex.GetInt32() : 0, Anims(cmd));

            case "run_history":
                return sim.RunHistoryView(cmd.TryGetProperty("index", out var runIndex) ? runIndex.GetInt32() : 0);

            case "set_setting":
                return cmd.TryGetProperty("value", out var settingValue)
                    ? sim.SetSetting(Protocol.Str(cmd, "name") ?? "", settingValue)
                    : new Dictionary<string, object?> { ["type"] = "error", ["message"] = "set_setting needs a value" };

            case "reset_gameplay_settings":
                return sim.ResetGameplaySettings();

            case "set_player":
            {
                var setArgs = new Dictionary<string, JsonElement>();
                foreach (var prop in cmd.EnumerateObject())
                    if (prop.Name != "cmd") setArgs[prop.Name] = prop.Value;
                return sim.SetPlayer(setArgs);
            }

            case "enter_room":
                return sim.EnterRoom(Protocol.Str(cmd, "type", "")!, Protocol.Str(cmd, "encounter"), Protocol.Str(cmd, "event"));

            case "set_draw_order":
            {
                var cards = new List<string>();
                if (cmd.TryGetProperty("cards", out var cardsArr) && cardsArr.ValueKind == JsonValueKind.Array)
                    foreach (var c in cardsArr.EnumerateArray())
                        cards.Add(c.GetString() ?? "");
                return sim.SetDrawOrder(cards);
            }

            case "write_continue_save":
                return sim.SaveCheckpoint(Protocol.Str(cmd, "path"));

            case "quit":
            {
                var outputPath = Protocol.Str(cmd, "path");
                if (!string.IsNullOrEmpty(outputPath))
                {
                    var saveResult = sim.SaveCheckpoint(outputPath);
                    bool saveOk = saveResult.TryGetValue("success", out var sObj) && sObj is bool b && b;
                    if (!saveOk)
                    {
                        // Save failed — do NOT clean up so the caller can retry with a different path.
                        return new Dictionary<string, object?> { ["type"] = "save_error", ["save"] = saveResult };
                    }
                    sim.CleanUp();
                    return new Dictionary<string, object?> { ["type"] = "quit_result", ["success"] = true, ["save"] = saveResult };
                }
                sim.CleanUp();
                return new Dictionary<string, object?> { ["type"] = "quit_result", ["success"] = true, ["save"] = null };
            }

            case "ping":
                return new Dictionary<string, object?>
                {
                    ["type"] = "pong",
                    ["callbacks_run"] = Sched.CallbacksRun,
                    ["gc_heap_bytes"] = GC.GetTotalMemory(false),
                    ["pool_work_seen"] = Sched.PoolWorkSeen,
                    ["pool_work_completed"] = ThreadPool.CompletedWorkItemCount,
                };

            default:
                return new Dictionary<string, object?> { ["type"] = "error", ["message"] = $"Unknown command: {cmdType}" };
        }
    }
}

public partial class RunDriver
{
    internal Player? LocalPlayer() =>
        _runState != null && _runState.Players.Count > 0 ? _runState.Players[0] : null;

    /// <summary>Handles the reply to a nested card reward; true if it resolved it.</summary>
    internal bool TryResolveNested(JsonElement cmd)
    {
        if (!_cardSelector.HasPendingReward) return false;
        if (Protocol.Str(cmd, "cmd") != "action") return false;
        var action = Protocol.Str(cmd, "action");
        if (action == "skip_card_reward")
        {
            Log("Skipping event card reward");
            _cardSelector.SkipReward();
            return true;
        }
        if (action == "select_card_reward")
        {
            var args = Protocol.Args(cmd);
            if (args == null || !args.TryGetValue("card_index", out var idx) || idx == null) return false;
            Log($"Resolving event card reward: index {idx}");
            _cardSelector.ResolveReward(Convert.ToInt32(idx));
            return true;
        }
        return false;
    }

    /// <summary>Test aid for selfplay --tank.</summary>
    internal void SetPlayerHp(int hp, int? maxHp)
    {
        var player = LocalPlayer();
        if (player?.Creature == null) return;
        if (maxHp != null) SetField(player.Creature, "_maxHp", maxHp.Value);
        SetField(player.Creature, "_currentHp", hp);
    }

    internal void SkipNestedReward()
    {
        if (_cardSelector.HasPendingReward) _cardSelector.SkipReward();
    }
}
