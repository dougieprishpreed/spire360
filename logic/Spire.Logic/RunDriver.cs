// Ported from sts2-cli's RunSimulator.cs (https://github.com/Garglemesh/sts2-cli,
// MIT, (c) 2025 Hao Wu). See THIRD_PARTY.md.
//
// Spire360 changes: single-threaded (no Task.Run, Thread.Sleep or blocking
// waits; see Sched.cs), Harmony patches replaced by static IL patches
// (logic/patches.json + Hooks.cs), configurable data directory, and
// animation steps recorded for the presentation layer (Steps.cs).
using System.Reflection;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Commands;
using MegaCrit.Sts2.Core.Context;
using MegaCrit.Sts2.Core.Events;
using MegaCrit.Sts2.Core.Entities.Ascension;
using MegaCrit.Sts2.Core.Entities.Cards;
using MegaCrit.Sts2.Core.Entities.Creatures;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.GameActions;
using MegaCrit.Sts2.Core.Helpers;
using MegaCrit.Sts2.Core.Map;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Characters;
using MegaCrit.Sts2.Core.Multiplayer;
using MegaCrit.Sts2.Core.CardSelection;
using MegaCrit.Sts2.Core.Entities.CardRewardAlternatives;
using MegaCrit.Sts2.Core.Entities.Merchant;
using MegaCrit.Sts2.Core.Entities.RestSite;
using MegaCrit.Sts2.Core.Rewards;
using MegaCrit.Sts2.Core.Rooms;
using MegaCrit.Sts2.Core.Models.Powers;
using MegaCrit.Sts2.Core.GameActions.Multiplayer;
using MegaCrit.Sts2.Core.Runs;
using MegaCrit.Sts2.Core.TestSupport;
using MegaCrit.Sts2.Core.Localization;
using MegaCrit.Sts2.Core.Multiplayer.Serialization;
using MegaCrit.Sts2.Core.Saves;
using MegaCrit.Sts2.Core.Unlocks;

namespace Spire.Logic;

/// <summary>
/// Full run simulator — manages the game lifecycle from character selection
/// through map navigation, combat, events, rest sites, shops, and act transitions.
/// Drives the engine forward until it hits a "decision point" requiring external input.
/// </summary>
public partial class RunDriver
{
    private static int? _expectedSaveSchemaVersion;
    private static bool _expectedSaveSchemaVersionReady;
    private static readonly object _expectedSaveSchemaVersionLock = new();

    private RunState? _runState;
    private static bool _modelDbInitialized;
    private bool _turnStarted;
    private bool _combatEnded;
    private static readonly LocLookup _loc = new();
    private bool _eventOptionChosen;
    private int _lastEventOptionCount;

    // The card reward screen open (a fight's, from the rewards decision)
    private CardReward? _pendingCardReward;
    private bool _rewardsProcessed;
    private bool _restSiteDone; // the rest site's choice is made: only Proceed is left
    private int _goldBeforeCombat;
    private int _lastKnownHp;
    private readonly HeadlessCardSelector _cardSelector = new();
    private IDisposable? _selectorScope; // the game's selector stack holds _cardSelector while a run is on

    internal void AnnotateCardSelection(string kind, object?[] args, object? result) =>
        _cardSelector.Annotate(kind, args, result);
    // Pending bundle selection (Scroll Boxes: pick 1 of N packs)
    private IReadOnlyList<IReadOnlyList<CardModel>>? _pendingBundles;
    private TaskCompletionSource<IEnumerable<CardModel>>? _pendingBundleTcs;

    public Dictionary<string, object?> StartRun(string character, int ascension = 0, string? seed = null, string lang = "en")
    {
        try
        {
            _loc.Lang = lang;
            EnsureModelDbInitialized();
            if (_runState != null)
                EndRun();

            var player = CreatePlayer(character);
            if (player == null)
                return Error($"Unknown character: {character}");

            var seedStr = seed ?? "headless_" + DateTimeOffset.UtcNow.ToUnixTimeSeconds();
            Log($"Creating RunState with seed={seedStr}");

            // Use CreateForTest which properly handles mutable copies internally
            _runState = RunState.CreateForTest(
                players: new[] { player },
                ascensionLevel: ascension,
                seed: seedStr
            );

            // Set up RunManager with test mode
            var netService = new NetSingleplayerGameService();
            RunManager.Instance.SetUpTest(_runState, netService);
            LocalContext.NetId = netService.NetId;

            // Force Neow event (blessing selection at start)
            _runState.ExtraFields.StartedWithNeow = true;

            // Generate rooms for all acts
            RunManager.Instance.GenerateRooms();
            Log("Rooms generated");

            // Launch the run
            RunManager.Instance.Launch();
            Log("Run launched");

            // Register event handlers for combat turn transitions
            CombatManager.Instance.TurnStarted += _ => _turnStarted = true;
            CombatManager.Instance.CombatEnded += _ => _combatEnded = true;

            // Finalize starting relics
            Sched.Run(() => RunManager.Instance.FinalizeStartingRelics());
            Log("Starting relics finalized");

            // Enter first act (generates map)
            Sched.Run(() => RunManager.Instance.EnterAct(0, doTransition: false));
            Log("Entered Act 0");

            // Rewards an event offers (Neow's Lost Coffer...): the rewards
            // decision until the player proceeds (Rewards.cs).
            RewardsSet.testSelector = OfferRewardsHeadless;

            // Register card selector for cards that need player choice
            _selectorScope?.Dispose();
            _selectorScope = CardSelectCmd.UseSelector(_cardSelector);
            Hooks.Driver = this;

            // Now we should be at the map — detect decision point
            return DetectDecisionPoint();
        }
        catch (Exception ex)
        {
            return ErrorWithTrace("StartRun failed", ex);
        }
    }

    /// <summary>
    /// A new run in the same process (the console's main menu after a run):
    /// the game's own clean-up (RunManager.CleanUp, as quitting to the menu
    /// does), then the driver's run state.
    /// </summary>
    private void EndRun()
    {
        try { RunManager.Instance.CleanUp(); }
        catch (Exception ex) { Log($"EndRun: {ex.Message}"); }
        _runState = null;
        _turnStarted = _combatEnded = _eventOptionChosen = false;
        _lastEventOptionCount = 0;
        ClearRewards();
        _pendingCardReward = null;
        _rewardsProcessed = false;
        _restSiteDone = false;
        ResetTreasure();
        _pendingBundles = null;
        _pendingBundleTcs = null;
        _combatView = null;
        _combatViewFor = null;
        _ancientDialogueShown.Clear();
        _cardSelector.CancelPending();
        _selectorScope?.Dispose();
        _selectorScope = null;
    }

    // ─── Test/Debug commands ───

    private static readonly System.Reflection.BindingFlags NonPublic =
        System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.NonPublic;

    /// <summary>Get the backing List&lt;T&gt; behind an IReadOnlyList property via reflection.</summary>
    private static List<T>? GetBackingList<T>(object obj, string fieldName)
    {
        var field = obj.GetType().GetField(fieldName, NonPublic);
        return field?.GetValue(obj) as List<T>;
    }

    private static void SetField(object obj, string fieldName, object? value)
    {
        var field = obj.GetType().GetField(fieldName, NonPublic);
        field?.SetValue(obj, value);
    }

    public Dictionary<string, object?> SetPlayer(Dictionary<string, System.Text.Json.JsonElement> args)
    {
        try
        {
            if (_runState == null) return Error("No run in progress");
            var player = _runState.Players[0];

            if (args.TryGetValue("hp", out var hpEl) && player.Creature != null)
                SetField(player.Creature, "_currentHp", hpEl.GetInt32());
            if (args.TryGetValue("max_hp", out var mhpEl) && player.Creature != null)
                SetField(player.Creature, "_maxHp", mhpEl.GetInt32());
            if (args.TryGetValue("gold", out var goldEl))
                player.Gold = goldEl.GetInt32();

            if (args.TryGetValue("relics", out var relicsEl))
            {
                var list = GetBackingList<RelicModel>(player, "_relics");
                if (list != null)
                {
                    list.Clear();
                    foreach (var rEl in relicsEl.EnumerateArray())
                    {
                        var id = rEl.GetString();
                        if (id == null) continue;
                        var model = ModelDb.GetById<RelicModel>(new ModelId("RELIC", id));
                        if (model != null) list.Add(model.ToMutable());
                    }
                }
            }
            if (args.TryGetValue("deck", out var deckEl))
            {
                // Remove existing cards from RunState tracking
                foreach (var c in player.Deck.Cards.ToList())
                    _runState.RemoveCard(c);
                player.Deck.Clear(silent: true);
                // Add new cards via RunState.CreateCard (sets Owner + registers)
                foreach (var cEl in deckEl.EnumerateArray())
                {
                    var id = cEl.GetString();
                    if (id == null) continue;
                    var canonical = ModelDb.GetById<CardModel>(new ModelId("CARD", id));
                    if (canonical != null)
                    {
                        var card = _runState.CreateCard(canonical, player);
                        player.Deck.AddInternal(card, silent: true);
                    }
                }
            }
            if (args.TryGetValue("potions", out var potionsEl))
            {
                var slots = GetBackingList<PotionModel>(player, "_potionSlots")
                         ?? GetBackingList<PotionModel?>(player, "_potionSlots") as System.Collections.IList;
                if (slots != null)
                {
                    for (int i = 0; i < slots.Count; i++) slots[i] = null;
                    int idx = 0;
                    foreach (var pEl in potionsEl.EnumerateArray())
                    {
                        if (idx >= slots.Count) break;
                        var id = pEl.GetString();
                        if (id != null)
                        {
                            var model = ModelDb.GetById<PotionModel>(new ModelId("POTION", id));
                            // Inject a mutable instance (not the canonical model — that throws
                            // CanonicalModelException when the game reads potion.Owner) and set its
                            // Owner, or UsePotionAction fails with "without an owner!".
                            if (model != null)
                            {
                                var mutable = model.ToMutable();
                                mutable.Owner = player;
                                slots[idx] = mutable;
                            }
                        }
                        idx++;
                    }
                }
            }

            Log($"SetPlayer: hp={player.Creature?.CurrentHp} gold={player.Gold} relics={player.Relics.Count} deck={player.Deck?.Cards?.Count}");
            return new Dictionary<string, object?>
            {
                ["type"] = "ok",
                ["player"] = PlayerSummary(player),
            };
        }
        catch (Exception ex) { return ErrorWithTrace("SetPlayer failed", ex); }
    }

    public Dictionary<string, object?> EnterRoom(string roomType, string? encounter, string? eventId)
    {
        try
        {
            if (_runState == null) return Error("No run in progress");
            var runState = _runState;
            Log($"EnterRoom: type={roomType} encounter={encounter} event={eventId}");

            AbstractRoom room;
            switch (roomType.ToLowerInvariant())
            {
                case "combat":
                case "monster":
                case "elite":
                {
                    if (string.IsNullOrEmpty(encounter))
                        encounter = "SHRINKER_BEETLE_WEAK"; // default encounter
                    var encModel = ModelDb.GetById<EncounterModel>(new ModelId("ENCOUNTER", encounter));
                    if (encModel == null) return Error($"Unknown encounter: {encounter}");
                    room = new CombatRoom(encModel.ToMutable(), runState);
                    break;
                }
                case "shop":
                    room = new MerchantRoom();
                    break;
                case "rest":
                case "rest_site":
                    room = new RestSiteRoom();
                    break;
                case "event":
                {
                    if (string.IsNullOrEmpty(eventId))
                        return Error("event requires 'event' parameter (e.g. CHANGELING_GROVE)");
                    var evModel = ModelDb.GetById<EventModel>(new ModelId("EVENT", eventId));
                    if (evModel == null) return Error($"Unknown event: {eventId}");
                    room = new EventRoom(evModel);
                    break;
                }
                case "treasure":
                    room = new TreasureRoom(_runState.CurrentActIndex);
                    break;
                default:
                    return Error($"Unknown room type: {roomType}");
            }

            Sched.Run(() => RunManager.Instance.EnterRoom(room));
            Sched.Pump();
            WaitForActionExecutor();
            return DetectDecisionPoint();
        }
        catch (Exception ex) { return ErrorWithTrace("EnterRoom failed", ex); }
    }

    public Dictionary<string, object?> SetDrawOrder(List<string> cardIds)
    {
        try
        {
            if (_runState == null) return Error("No run in progress");
            var player = _runState.Players[0];
            var pcs = player.PlayerCombatState;
            if (pcs?.DrawPile == null) return Error("Not in combat");

            var drawList = GetBackingList<CardModel>(pcs.DrawPile, "_cards");
            if (drawList == null) return Error("Cannot access draw pile");

            var newOrder = new List<CardModel>();
            var available = new List<CardModel>(drawList);
            foreach (var cardId in cardIds)
            {
                var match = available.FirstOrDefault(c =>
                    c.Id.Entry.Equals(cardId, StringComparison.OrdinalIgnoreCase));
                if (match != null)
                {
                    newOrder.Add(match);
                    available.Remove(match);
                }
            }
            newOrder.AddRange(available);

            drawList.Clear();
            drawList.AddRange(newOrder);

            Log($"SetDrawOrder: {newOrder.Count} cards, top={newOrder.FirstOrDefault()?.Id.Entry}");
            return new Dictionary<string, object?>
            {
                ["type"] = "ok",
                ["draw_pile_count"] = drawList.Count,
                ["top_cards"] = newOrder.Take(5).Select(c => _loc.Card(c.Id.Entry)).ToList(),
            };
        }
        catch (Exception ex) { return ErrorWithTrace("SetDrawOrder failed", ex); }
    }

    // ─── Game actions ───
    public Dictionary<string, object?> LoadSave(string saveJson, string lang = "en")
    {
        try
        {
            _loc.Lang = lang;
            EnsureModelDbInitialized();

            Log("Loading save file...");

            if (!ValidateSaveSchemaVersion(saveJson, out var schemaError))
                return Error($"Save schema mismatch: {schemaError}");

            var readResult = SaveManager.FromJson<SerializableRun>(saveJson);
            if (!readResult.Success || readResult.SaveData == null)
                return Error($"Failed to parse save file: {readResult.Status} {readResult.ErrorMessage}");

            var save = readResult.SaveData;
            Log($"Save loaded: seed={save.SerializableRng?.Seed}, act={save.CurrentActIndex}, ascension={save.Ascension}");

            _runState = RunState.FromSerializable(save);
            if (_runState == null)
                return Error("Failed to create RunState from save");

            Log($"RunState created, players={_runState.Players?.Count}");

            var netService = new NetSingleplayerGameService();
            RunManager.Instance.SetUpSavedSingleplayer(_runState, save);
            LocalContext.NetId = netService.NetId;

            CombatManager.Instance.TurnStarted += _ => _turnStarted = true;
            CombatManager.Instance.CombatEnded += _ => _combatEnded = true;
            _selectorScope?.Dispose();
            _selectorScope = CardSelectCmd.UseSelector(_cardSelector);
            Hooks.Driver = this;

            var savedRoom = _runState.CurrentRoom;

            // Save visited coords before Launch (EnterAct will clear them)
            var savedVisitedCoords = _runState.VisitedMapCoords?.ToList() ?? new List<MapCoord>();
            var shouldResumeInitialNeow = IsInitialNeowSave(saveJson);
            Log($"Save has {savedVisitedCoords.Count} visited coords");

            RunManager.Instance.Launch();
            Log("Run launched");

            if (savedRoom is MapRoom || savedRoom == null)
            {
                // Preserve Neow for saves created before the first blessing choice.
                // Once the run has visited at least one map node, re-entering Act 1
                // should not send the player back through the Ancient start node.
                if (_runState.CurrentActIndex == 0 && savedVisitedCoords.Count > 0)
                    _runState.ExtraFields.StartedWithNeow = false;
                Sched.Run(() => RunManager.Instance.EnterAct(_runState.CurrentActIndex, doTransition: false));
                Sched.Pump();
                Log($"Entered Act {_runState.CurrentActIndex}");

                if (shouldResumeInitialNeow && _runState.Map?.StartingMapPoint != null)
                {
                    Log("Restoring initial Neow event");
                    Sched.Run(() => RunManager.Instance.EnterMapCoord(_runState.Map.StartingMapPoint.coord));
                    Sched.Pump();
                }

                // EnterAct clears visited coords and ActFloor — restore them from save
                if (savedVisitedCoords.Count > 0)
                {
                    if (_runState.VisitedMapCoords == null || _runState.VisitedMapCoords.Count == 0)
                    {
                        foreach (var coord in savedVisitedCoords)
                            _runState.AddVisitedMapCoord(coord);
                    }
                    _runState.ActFloor = savedVisitedCoords.Count;
                    var last = savedVisitedCoords[^1];
                    Log($"Restored map position: floor={_runState.ActFloor}, coord=({last.col},{last.row})");
                }
            }
            else
            {
                Log($"Preserving saved room: {savedRoom.GetType().Name}");
            }

            return DetectDecisionPoint();
        }
        catch (Exception ex)
        {
            return ErrorWithTrace("LoadSave failed", ex);
        }
    }

    /// <summary>
    /// Expected run save <c>schema_version</c> (lazy: first load_save only, so StartRun never fails on reflection).
    /// Order: <c>STS2_SAVE_SCHEMA_VERSION</c> env → reflect sts2.dll → unknown, defer to SaveManager.
    /// </summary>
    private static int? GetExpectedSaveSchemaVersion()
    {
        if (_expectedSaveSchemaVersionReady)
            return _expectedSaveSchemaVersion;
        lock (_expectedSaveSchemaVersionLock)
        {
            if (_expectedSaveSchemaVersionReady)
                return _expectedSaveSchemaVersion;
            _expectedSaveSchemaVersion = ResolveExpectedSaveSchemaVersion();
            _expectedSaveSchemaVersionReady = true;
            return _expectedSaveSchemaVersion;
        }
    }

    private static int? ResolveExpectedSaveSchemaVersion()
    {
        var env = Environment.GetEnvironmentVariable("STS2_SAVE_SCHEMA_VERSION");
        if (!string.IsNullOrWhiteSpace(env) && int.TryParse(env.Trim(), out var envVer))
            return envVer;

        var reflected = TryReflectLatestSaveSchemaVersion();
        if (reflected.HasValue)
            return reflected.Value;

        Console.Error.WriteLine(
            "[Sts2Headless] Could not read save schema from sts2.dll; deferring schema compatibility " +
            "to SaveManager.FromJson. Set STS2_SAVE_SCHEMA_VERSION to enforce a specific version.");
        return null;
    }

    /// <summary>Find static parameterless GetLatestSchemaVersion (or close) on sts2; supports int/uint/long.</summary>
    private static int? TryReflectLatestSaveSchemaVersion()
    {
        var asm = typeof(SerializableRun).Assembly;
        Type[] types;
        try
        {
            types = asm.GetTypes();
        }
        catch (ReflectionTypeLoadException ex)
        {
            types = ex.Types.Where(t => t != null).Cast<Type>().ToArray();
        }

        var candidates = new List<(int score, string typeName, int value)>();
        foreach (var t in types)
        {
            MethodInfo? m;
            try
            {
                foreach (var name in new[] { "GetLatestSchemaVersion", "GetLatestVersion" })
                {
                    m = t.GetMethod(name, BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static,
                        null, Type.EmptyTypes, null);
                    if (m == null) continue;
                    var tn = t.FullName ?? "";
                    // Avoid unrelated static GetLatestVersion() elsewhere in the assembly.
                    if (name == "GetLatestVersion" && !tn.Contains("Saves", StringComparison.Ordinal))
                        continue;

                    var conv = TryConvertSchemaNumber(m.Invoke(null, null));
                    if (!conv.HasValue) continue;

                    var score = name == "GetLatestSchemaVersion" ? 100 : 0;
                    if (tn.Contains("Saves", StringComparison.Ordinal)) score += 50;
                    if (tn.Contains("Schema", StringComparison.Ordinal) || tn.Contains("Migration", StringComparison.Ordinal))
                        score += 25;
                    candidates.Add((score, tn, conv.Value));
                }
            }
            catch
            {
                // type may not support full reflection on this runtime
            }
        }

        if (candidates.Count == 0)
            return null;

        var best = candidates.OrderByDescending(c => c.score).ThenBy(c => c.typeName).First();
        return best.value;
    }

    private static int? TryConvertSchemaNumber(object? value) => value switch
    {
        int i => i,
        uint u => u <= int.MaxValue ? (int)u : null,
        long l => l >= int.MinValue && l <= int.MaxValue ? (int)l : null,
        short s => s,
        ushort us => us,
        byte b => b,
        _ => null,
    };

    private static bool ValidateSaveSchemaVersion(string saveJson, out string error)
    {
        error = "";
        try
        {
            using var doc = System.Text.Json.JsonDocument.Parse(saveJson);
            var root = doc.RootElement;

            if (!root.TryGetProperty("schema_version", out var versionElem))
            {
                error = "missing schema_version";
                return false;
            }

            if (versionElem.ValueKind != System.Text.Json.JsonValueKind.Number ||
                !versionElem.TryGetInt32(out var schemaVersion))
            {
                error = "schema_version is not a valid integer";
                return false;
            }

            var expected = GetExpectedSaveSchemaVersion();
            if (expected.HasValue && schemaVersion != expected.Value)
            {
                error = $"expected v{expected.Value}, got v{schemaVersion}";
                return false;
            }

            return true;
        }
        catch (Exception ex)
        {
            error = $"could not inspect save: {ex.Message}";
            return false;
        }
    }

    private static bool TrySetPropertyValue(object target, string propertyName, object? value)
    {
        var prop = target.GetType().GetProperty(propertyName);
        if (prop?.CanWrite != true)
            return false;
        prop.SetValue(target, value);
        return true;
    }

    private static bool IsInitialNeowSave(string saveJson)
    {
        try
        {
            using var doc = System.Text.Json.JsonDocument.Parse(saveJson);
            var root = doc.RootElement;

            if (!root.TryGetProperty("current_act_index", out var actIndexElem) || actIndexElem.GetInt32() != 0)
                return false;

            var hasVisitedCoords = root.TryGetProperty("visited_map_coords", out var visitedElem)
                                && visitedElem.ValueKind == System.Text.Json.JsonValueKind.Array
                                && visitedElem.GetArrayLength() > 0;
            if (hasVisitedCoords)
                return false;

            return root.TryGetProperty("extra_fields", out var extraFieldsElem)
                && extraFieldsElem.ValueKind == System.Text.Json.JsonValueKind.Object
                && extraFieldsElem.TryGetProperty("started_with_neow", out var startedElem)
                && startedElem.ValueKind == System.Text.Json.JsonValueKind.True;
        }
        catch
        {
            return false;
        }
    }

    private static bool TryRollbackSerializedSaveToPreRoom(SerializableRun serializableRun, out string error)
    {
        error = "";

        var saveType = serializableRun.GetType();
        var visitedProp = saveType.GetProperty("VisitedMapCoords");
        if (visitedProp == null)
        {
            error = "Save data is missing VisitedMapCoords";
            return false;
        }

        var visitedValue = visitedProp.GetValue(serializableRun);
        var visitedItems = new List<object?>();
        if (visitedValue is System.Collections.IEnumerable visitedEnumerable)
        {
            foreach (var item in visitedEnumerable)
                visitedItems.Add(item);
        }

        if (visitedItems.Count == 0)
        {
            error = "Cannot roll back save before the first room";
            return false;
        }

        visitedItems.RemoveAt(visitedItems.Count - 1);

        var visitedType = visitedProp.PropertyType;
        if (visitedType.IsArray)
        {
            var elementType = visitedType.GetElementType()!;
            var array = Array.CreateInstance(elementType, visitedItems.Count);
            for (int i = 0; i < visitedItems.Count; i++)
                array.SetValue(visitedItems[i], i);
            visitedProp.SetValue(serializableRun, array);
        }
        else if (visitedType.IsGenericType)
        {
            var elementType = visitedType.GetGenericArguments()[0];
            var listType = typeof(List<>).MakeGenericType(elementType);
            var list = (System.Collections.IList)Activator.CreateInstance(listType)!;
            foreach (var item in visitedItems)
                list.Add(item);
            visitedProp.SetValue(serializableRun, list);
        }
        else
        {
            error = $"Unsupported VisitedMapCoords type: {visitedType.Name}";
            return false;
        }

        TrySetPropertyValue(serializableRun, "ActFloor", visitedItems.Count);
        TrySetPropertyValue(serializableRun, "CurrentMapCoord", visitedItems.Count > 0 ? visitedItems[^1] : null);
        TrySetPropertyValue(serializableRun, "PreFinishedRoom", null);
        TrySetPropertyValue(serializableRun, "CurrentRoom", null);
        return true;
    }

    public Dictionary<string, object?> SaveCheckpoint(string? outputPath)
    {
        try
        {
            if (_runState == null)
                return Error("No active run to save");

            if (string.IsNullOrEmpty(outputPath))
                return Error("No output path specified for quit save");

            var currentRoom = _runState.CurrentRoom;
            SerializableRun serializableRun;

            if (currentRoom is MapRoom || currentRoom == null)
            {
                Log($"Saving map checkpoint (room={currentRoom?.GetType().Name ?? "null"}, outputPath={outputPath})...");
                serializableRun = RunManager.Instance.ToSave(currentRoom);
            }
            else
            {
                Log($"Saving pre-room checkpoint from {currentRoom.GetType().Name} (outputPath={outputPath})...");
                serializableRun = RunManager.Instance.ToSave(new MapRoom());
                if (!TryRollbackSerializedSaveToPreRoom(serializableRun, out var rollbackError))
                    return Error($"Cannot save checkpoint: {rollbackError}");
            }

            var saveJson = SaveManager.ToJson(serializableRun);
            Log($"Serialized save: {saveJson.Length} chars");

            var dir = System.IO.Path.GetDirectoryName(outputPath);
            if (!string.IsNullOrEmpty(dir))
                System.IO.Directory.CreateDirectory(dir);
            System.IO.File.WriteAllText(outputPath, saveJson);
            Log($"Save written to: {outputPath}");

            return new Dictionary<string, object?>
            {
                ["type"] = "save_result",
                ["success"] = true,
                ["path"] = outputPath,
                ["size"] = saveJson.Length,
                ["room_type"] = currentRoom?.GetType().Name,
            };
        }
        catch (Exception ex)
        {
            return ErrorWithTrace("SaveCheckpoint failed", ex);
        }
    }
    public Dictionary<string, object?> ExecuteAction(string action, Dictionary<string, object?>? args)
    {
        try
        {
            if (_runState == null)
                return Error("No run in progress");

            var player = _runState.Players[0];

            switch (action)
            {
                case "select_map_node":
                    return DoMapSelect(player, args);
                case "play_card":
                    return DoPlayCard(player, args);
                case "end_turn":
                    return DoEndTurn(player);
                case "choose_option":
                    return DoChooseOption(player, args);
                case "select_card_reward":
                    return DoSelectCardReward(player, args);
                case "skip_card_reward":
                    return DoSkipCardReward(player);
                case "take_reward":
                    return DoTakeReward(player, args);
                case "buy_card":
                    return DoBuyCard(player, args);
                case "buy_relic":
                    return DoBuyRelic(player, args);
                case "buy_potion":
                    return DoBuyPotion(player, args);
                case "remove_card":
                    return DoRemoveCard(player);
                case "select_bundle":
                    return DoSelectBundle(player, args);
                case "select_cards":
                    return DoSelectCards(player, args);
                case "skip_select":
                    return DoSkipSelect(player);
                case "use_potion":
                    return DoUsePotion(player, args);
                case "discard_potion":
                    return DoDiscardPotion(player, args);
                case "open_chest":
                    return DoOpenChest(player);
                case "pick_relic":
                    return DoPickRelic(player, args);
                case "leave_room":
                    return DoLeaveRoom(player);
                case "proceed":
                    return DoProceed(player);
                case "crystal_sphere_tool":
                    return DoCrystalSphereTool(args);
                case "crystal_sphere_click":
                    return DoCrystalSphereClick(args);
                default:
                    return Error($"Unknown action: {action}");
            }
        }
        catch (Exception ex)
        {
            return ErrorWithTrace($"Action '{action}' failed", ex);
        }
    }

    #region Actions

    private Dictionary<string, object?> DoMapSelect(Player player, Dictionary<string, object?>? args)
    {
        if (args == null || !args.ContainsKey("col") || !args.ContainsKey("row"))
            return Error("select_map_node requires 'col' and 'row'");

        // Reset tracking for new room
        _rewardsProcessed = false;
        _restSiteDone = false;
        ResetTreasure();
        _pendingCardReward = null;
        _eventOptionChosen = false;
        _lastEventOptionCount = 0;
        ClearRewards();
        _lastKnownHp = player.Creature?.CurrentHp ?? 0;

        var col = Convert.ToInt32(args["col"]);
        var row = Convert.ToInt32(args["row"]);
        var coord = new MapCoord((byte)col, (byte)row);

        Log($"Moving to map coord ({col},{row})");

        // BUG-013: Wait for any pending actions (relic sessions, etc.) to complete before entering new room
        WaitForActionExecutor();
        Sched.Pump();

        // Call EnterMapCoord directly (same as what MoveToMapCoordAction does in TestMode)
        // This avoids the action executor which can swallow errors silently.
        Sched.Run(() => RunManager.Instance.EnterMapCoord(coord));
        Sched.Pump();
        WaitForActionExecutor();

        return DetectDecisionPoint();
    }

    private Dictionary<string, object?> DoPlayCard(Player player, Dictionary<string, object?>? args)
    {
        if (args == null || !args.ContainsKey("card_index"))
            return Error("play_card requires 'card_index'");

        var cardIndex = Convert.ToInt32(args["card_index"]);
        var pcs = player.PlayerCombatState;
        if (pcs == null)
            return Error("Not in combat");

        var hand = pcs.Hand.Cards;
        if (cardIndex < 0 || cardIndex >= hand.Count)
            return Error($"Invalid card index {cardIndex}, hand has {hand.Count} cards");

        var card = hand[cardIndex];

        // Determine target based on card's TargetType first
        // Self/None/All cards: target = null (game handles internally)
        // AnyEnemy cards: use target_index or auto-pick first alive enemy
        Creature? target = null;
        var cardTargetType = card.TargetType;
        if (cardTargetType == TargetType.AnyEnemy)
        {
            // Use caller's target_index if provided
            if (args.TryGetValue("target_index", out var targetObj) && targetObj != null)
            {
                var targetIndex = Convert.ToInt32(targetObj);
                var state = CombatManager.Instance.DebugOnlyGetState();
                if (state != null)
                {
                    var enemies = state.Enemies.Where(e => e != null && e.IsAlive).ToList();
                    if (targetIndex >= 0 && targetIndex < enemies.Count)
                        target = enemies[targetIndex];
                }
            }
            // No target_index given: only auto-target when the choice is unambiguous
            // (a single alive enemy). With multiple enemies, picking one is a real game
            // decision — return an error instead of silently targeting enemy 0 (#79).
            if (target == null)
            {
                var state = CombatManager.Instance.DebugOnlyGetState();
                var alive = state?.Enemies?.Where(e => e != null && e.IsAlive).ToList() ?? new();
                if (alive.Count == 1)
                    target = alive[0];
                else if (alive.Count > 1)
                    return Error($"Card {card.Id.Entry} targets a single enemy (AnyEnemy); " +
                                 $"'target_index' is required when multiple enemies are alive ({alive.Count}).");
            }
        }
        // All other target types (None, All, etc.) → leave target as null

        // Check if card can be played
        if (!card.CanPlay(out var reason, out var _))
        {
            return Error($"Cannot play card {card.GetType().Name}: {reason}");
        }

        Log($"Playing card {card.GetType().Name} (index {cardIndex}) targeting {(target != null ? target.Monster?.GetType().Name ?? "creature" : "none")}");

        var handCountBefore = hand.Count;

        var playAction = new PlayCardAction(card, target);
        RunManager.Instance.ActionQueueSet.EnqueueWithoutSynchronizing(playAction);
        WaitForActionExecutor();

        // Check if card play had no effect (hand unchanged, same card still at same index).
        // Exception: some cards (e.g. Particle Wall) are 0-cost and intentionally return
        // themselves to Hand after a successful play, so "still in hand" is expected and NOT
        // a failure — flagging it killed otherwise-valid runs.
        var handAfter = pcs.Hand.Cards;
        if (handAfter.Count == handCountBefore && cardIndex < handAfter.Count && handAfter[cardIndex] == card
            && !ReturnsToHandOnPlay(card))
        {
            return Error($"Card could not be played (still in hand after action): {card.GetType().Name} [{card.Id}]");
        }

        return DetectDecisionPoint();
    }

    // STS2 build 23372702 removed CombatManager.IsPlayPhase (global) in favor of a
    // per-player PlayerCombatState.Phase. Headless is single-player, so the local
    // player (Players[0]) being in the Play phase is the equivalent signal.
    private bool IsPlayPhase()
    {
        var p = (_runState != null && _runState.Players.Count > 0) ? _runState.Players[0] : null;
        return p?.PlayerCombatState?.Phase == MegaCrit.Sts2.Core.Combat.PlayerTurnPhase.Play;
    }

    private Dictionary<string, object?> DoEndTurn(Player player)
    {
        // A pending card / card-reward / bundle selection is an unresolved prompt; ending
        // the turn here would silently mutate combat instead. Surface the prompt unchanged
        // and let the caller resolve it first (#61).
        if (_cardSelector.HasPending || _cardSelector.HasPendingReward || _pendingBundles != null)
        {
            Log("end_turn ignored: a card selection is pending");
            return DetectDecisionPoint();
        }

        Sched.Pump();
        if (!IsPlayPhase())
            return DetectDecisionPoint();

        // Ensure no actions are still running before ending turn
        WaitForActionExecutor();

        Log($"Ending turn (round={CombatManager.Instance.DebugOnlyGetState()?.RoundNumber ?? 0})");
        _turnStarted = false;
        _combatEnded = false;

        // Single-threaded, the whole enemy turn runs as queued continuations: pumping
        // until the queue is empty reaches the next player turn, ends the combat, or
        // stops at a pending decision (e.g. a start-of-turn card selection).
        // sts2-cli's cancel/retry/"nuclear" fallbacks existed because continuations
        // raced across threads; with one queue there is nothing to race.
        Sched.Call(() => PlayerCmd.EndTurn(player, canBackOut: false));
        WaitForActionExecutor();

        if (CombatManager.Instance.IsInProgress && !IsPlayPhase() && !player.Creature.IsDead
            && !_cardSelector.HasPending && !_cardSelector.HasPendingReward && _pendingBundles == null)
        {
            var state = CombatManager.Instance.DebugOnlyGetState();
            Log($"EndTurn did not reach the next play phase yet (round={state?.RoundNumber}, " +
                $"executor running={RunManager.Instance.ActionExecutor.IsRunning}, queued={Sched.Context.Count}, " +
                $"turnStarted={_turnStarted}, combatEnded={_combatEnded})");
        }

        return DetectDecisionPoint();
    }

    private Dictionary<string, object?> DoSelectCardReward(Player player, Dictionary<string, object?>? args)
    {
        // Handle event-triggered card reward (blocking GetSelectedCardReward)
        if (_cardSelector.HasPendingReward)
        {
            if (args == null || !args.ContainsKey("card_index"))
                return Error("select_card_reward requires 'card_index'");
            var idx = Convert.ToInt32(args["card_index"]);
            Log($"Resolving event card reward: index {idx}");
            _cardSelector.ResolveReward(idx);
            Sched.Pump();
            Sched.Pump();
            WaitForActionExecutor();
            return DetectDecisionPoint();
        }

        if (_pendingCardReward == null)
            return Error("No pending card reward");
        if (args == null || !args.ContainsKey("card_index"))
            return Error("select_card_reward requires 'card_index'");

        var cardIndex = Convert.ToInt32(args["card_index"]);
        var cards = _pendingCardReward.Cards.ToList();
        if (cardIndex < 0 || cardIndex >= cards.Count)
            return Error($"Invalid card index {cardIndex}, {cards.Count} cards available");

        var card = cards[cardIndex];
        Log($"Selected card reward: {card.GetType().Name}");

        // Add card to deck
        try
        {
            Sched.Run(() => MegaCrit.Sts2.Core.Commands.CardPileCmd
                .Add(card, MegaCrit.Sts2.Core.Entities.Cards.PileType.Deck));
            Sched.Pump();
            RunManager.Instance.RewardSynchronizer.SyncLocalObtainedCard(card);
        }
        catch (Exception ex) { Log($"Add card to deck: {ex.Message}"); }

        if (_cardFromList) _rewardList?.Remove(_pendingCardReward);
        _cardFromList = false;
        _pendingCardReward = null;
        // Back to the rewards screen
        return DetectDecisionPoint();
    }

    private Dictionary<string, object?> DoSkipCardReward(Player player)
    {
        if (_cardSelector.HasPendingReward)
        {
            Log("Skipping event card reward");
            _cardSelector.SkipReward();
            Sched.Pump();
            Sched.Pump();
            WaitForActionExecutor();
            return DetectDecisionPoint();
        }
        if (_pendingCardReward != null)
        {
            Log("Skipping card reward");
            // From the rewards screen: it stays there, marked; Proceed skips it.
            if (_cardFromList) _rewardSkipped.Add(_pendingCardReward);
            else _pendingCardReward.OnSkipped();
            _cardFromList = false;
            _pendingCardReward = null;
        }
        return DetectDecisionPoint();
    }

    private Dictionary<string, object?> DoBuyCard(Player player, Dictionary<string, object?>? args)
    {
        if (_runState?.CurrentRoom is not MerchantRoom merchantRoom)
            return Error("Not in a shop");
        if (args == null || !args.ContainsKey("card_index"))
            return Error("buy_card requires 'card_index'");

        var idx = Convert.ToInt32(args["card_index"]);
        var allEntries = merchantRoom.GetLocalInventory().CharacterCardEntries
            .Concat(merchantRoom.GetLocalInventory().ColorlessCardEntries).ToList();
        if (idx < 0 || idx >= allEntries.Count)
            return Error($"Invalid card index {idx}");

        var entry = allEntries[idx];
        if (!entry.IsStocked) return Error("Card already purchased");
        if (player.Gold < entry.Cost) return Error("Not enough gold");

        try
        {
            Sched.Run(() => entry.OnTryPurchaseWrapper(merchantRoom.GetLocalInventory()));
            Sched.Pump();
            Log($"Bought card: {entry.CreationResult?.Card?.GetType().Name ?? "?"} for {entry.Cost}g");
        }
        catch (Exception ex) { return ErrorWithTrace("Buy card failed", ex); }

        return DetectDecisionPoint();
    }

    private Dictionary<string, object?> DoBuyRelic(Player player, Dictionary<string, object?>? args)
    {
        if (_runState?.CurrentRoom is not MerchantRoom merchantRoom)
            return Error("Not in a shop");
        if (args == null || !args.ContainsKey("relic_index"))
            return Error("buy_relic requires 'relic_index'");

        var idx = Convert.ToInt32(args["relic_index"]);
        var entries = merchantRoom.GetLocalInventory().RelicEntries;
        if (idx < 0 || idx >= entries.Count) return Error($"Invalid relic index {idx}");

        var entry = entries[idx];
        if (!entry.IsStocked) return Error("Relic already purchased");
        if (player.Gold < entry.Cost) return Error("Not enough gold");

        try
        {
            // The pickup effect can open a card_select (e.g. KIFUDA → enchant up to 3 with
            // Adroit, #80). The purchase runs until it awaits the selector's TCS; it then
            // resumes when select_cards completes that TCS.
            var inv = merchantRoom.GetLocalInventory();
            // The entry forgets its model once bought; name it first.
            var relicName = entry.Model?.GetType().Name ?? "?";
            var cost = entry.Cost;
            // Single-threaded: runs until it completes or waits on a selection.
            Sched.Run(() => entry.OnTryPurchaseWrapper(inv));
            if (_cardSelector.HasPending || _cardSelector.HasPendingReward || _pendingBundles != null)
            {
                Log($"Buy relic {relicName}: yielded for pending selection");
                return DetectDecisionPoint();
            }
            Sched.Pump();
            Log($"Bought relic: {relicName} for {cost}g");
        }
        catch (Exception ex) { return ErrorWithTrace("Buy relic failed", ex); }

        return DetectDecisionPoint();
    }

    private Dictionary<string, object?> DoBuyPotion(Player player, Dictionary<string, object?>? args)
    {
        if (_runState?.CurrentRoom is not MerchantRoom merchantRoom)
            return Error("Not in a shop");
        if (args == null || !args.ContainsKey("potion_index"))
            return Error("buy_potion requires 'potion_index'");

        var idx = Convert.ToInt32(args["potion_index"]);
        var entries = merchantRoom.GetLocalInventory().PotionEntries;
        if (idx < 0 || idx >= entries.Count) return Error($"Invalid potion index {idx}");

        var entry = entries[idx];
        if (!entry.IsStocked) return Error("Potion already purchased");
        if (player.Gold < entry.Cost) return Error("Not enough gold");

        try
        {
            Sched.Run(() => entry.OnTryPurchaseWrapper(merchantRoom.GetLocalInventory()));
            Sched.Pump();
            Log($"Bought potion: {entry.Model.GetType().Name} for {entry.Cost}g");
        }
        catch (Exception ex)
        {
            // Potion purchase sometimes NullRefs in headless (missing potion slot UI)
            Log($"Buy potion failed: {ex.Message}");
        }

        return DetectDecisionPoint();
    }

    private Dictionary<string, object?> DoRemoveCard(Player player)
    {
        if (_runState?.CurrentRoom is not MerchantRoom merchantRoom)
            return Error("Not in a shop");

        var removal = merchantRoom.GetLocalInventory().CardRemovalEntry;
        if (removal == null) return Error("No card removal available");
        if (player.Gold < removal.Cost) return Error("Not enough gold");

        try
        {
            // Single-threaded: runs until it completes or waits on a selection.
            Sched.Run(() => removal.OnTryPurchaseWrapper(merchantRoom.GetLocalInventory()));
            if (_cardSelector.HasPending)
            {
                WaitForActionExecutor();
                return DetectDecisionPoint();
            }
            Sched.Pump();
            Log($"Removed card for {removal.Cost}g");
        }
        catch (Exception ex) { return ErrorWithTrace("Remove card failed", ex); }

        return DetectDecisionPoint();
    }

    private Dictionary<string, object?> DoSelectBundle(Player player, Dictionary<string, object?>? args)
    {
        if (_pendingBundleTcs == null || _pendingBundles == null)
            return Error("No pending bundle selection");
        if (args == null || !args.ContainsKey("bundle_index"))
            return Error("select_bundle requires 'bundle_index'");

        var idx = Convert.ToInt32(args["bundle_index"]);
        Log($"Bundle selection: pack {idx}");
        var bundles = _pendingBundles;
        var tcs = _pendingBundleTcs;
        _pendingBundles = null;
        _pendingBundleTcs = null;

        // Set result directly (no ContinueWith/ThreadPool)
        var selected = (idx >= 0 && idx < bundles.Count) ? bundles[idx] : bundles[0];
        tcs.TrySetResult(selected);

        Sched.Pump();
        WaitForActionExecutor();
        return DetectDecisionPoint();
    }

    private Dictionary<string, object?> DoSelectCards(Player player, Dictionary<string, object?>? args)
    {
        if (!_cardSelector.HasPending)
            return Error("No pending card selection");
        if (args == null || !args.ContainsKey("indices"))
            return Error("select_cards requires 'indices' (comma-separated card indices)");

        var indicesStr = args["indices"]?.ToString() ?? "";
        var indices = indicesStr.Split(',')
            .Select(s => int.TryParse(s.Trim(), out var v) ? v : -1)
            .Where(i => i >= 0)
            .ToArray();

        Log($"Card selection: indices [{string.Join(",", indices)}]");
        _cardSelector.ResolvePendingByIndices(indices);
        Sched.Pump();
        WaitForActionExecutor();

        // Extra wait for rest-site SMITH: the background ChooseLocalOption task
        // needs time to complete the upgrade after card selection resolves.
        if (_runState?.CurrentRoom is RestSiteRoom)
        {
            Sched.Pump();
            Sched.Pump();
            WaitForActionExecutor();
            // SMITH done: the room stays with its proceed button (as after HEAL).
            Log("Card selection in rest site (SMITH) done");
            _restSiteDone = true;
            return DetectDecisionPoint();
        }

        // Extra wait for shop card removal: the purchase task needs to finish
        if (_runState?.CurrentRoom is MerchantRoom)
        {
            Sched.Pump();
            Sched.Pump();
            WaitForActionExecutor();
            Log("Card selection in shop (card removal), refreshing shop state");
        }

        return DetectDecisionPoint();
    }

    private Dictionary<string, object?> DoSkipSelect(Player player)
    {
        if (_cardSelector.HasPending)
        {
            Log("Skipping card selection");
            _cardSelector.CancelPending();
            Sched.Pump();
            WaitForActionExecutor();
        }
        return DetectDecisionPoint();
    }

    private Dictionary<string, object?> DoUsePotion(Player player, Dictionary<string, object?>? args)
    {
        if (args == null || !args.ContainsKey("potion_index"))
            return Error("use_potion requires 'potion_index'");

        var idx = Convert.ToInt32(args["potion_index"]);
        var potionsList = player.Potions?.ToList() ?? new();
        if (idx < 0 || idx >= potionsList.Count) return Error($"Invalid potion index {idx}");
        var potion = potionsList[idx];
        if (potion == null) return Error($"No potion at index {idx}");

        // Determine target based on potion's TargetType first, then fall back to target_index
        Creature? target = null;
        var potionTargetType = potion.TargetType;

        // Self-targeting potions (Flex, Fortifier, etc.) ALWAYS target the player
        // regardless of any target_index the caller provides
        if (potionTargetType == TargetType.Self || potionTargetType == TargetType.TargetedNoCreature)
        {
            target = player.Creature;
        }
        else if (potionTargetType == TargetType.AnyEnemy)
        {
            // Use caller's target_index if provided, otherwise pick first alive enemy
            if (args.TryGetValue("target_index", out var tObj) && tObj != null)
            {
                var targetIdx = Convert.ToInt32(tObj);
                var combatState = CombatManager.Instance.DebugOnlyGetState();
                if (combatState != null)
                {
                    var enemies = combatState.Enemies.Where(e => e != null && e.IsAlive).ToList();
                    if (targetIdx >= 0 && targetIdx < enemies.Count)
                        target = enemies[targetIdx];
                }
            }
            // Same single-enemy rule as play_card: only auto-target when unambiguous (#79).
            if (target == null && CombatManager.Instance.IsInProgress)
            {
                var combatState = CombatManager.Instance.DebugOnlyGetState();
                var alive = combatState?.Enemies?.Where(e => e != null && e.IsAlive).ToList() ?? new();
                if (alive.Count == 1)
                    target = alive[0];
                else if (alive.Count > 1)
                    return Error($"Potion {potion.Id.Entry} targets a single enemy (AnyEnemy); " +
                                 $"'target_index' is required when multiple enemies are alive ({alive.Count}).");
            }
        }
        // All other target types (None, All, etc.) → leave target as null

        Log($"Using potion: {potion.GetType().Name} at slot {idx} target={target?.GetType().Name ?? "none"}");
        try
        {
            var action = new MegaCrit.Sts2.Core.GameActions.UsePotionAction(potion, target, CombatManager.Instance.IsInProgress);
            RunManager.Instance.ActionQueueSet.EnqueueWithoutSynchronizing(action);
            WaitForActionExecutor();
            Sched.Pump();

            // Effect may require card_select before the potion slot clears — do not discard as "stuck".
            if (_cardSelector.HasPending || _cardSelector.HasPendingReward)
                return DetectDecisionPoint();

            // Verify potion was consumed
            var afterPotions = player.Potions?.ToList() ?? new();
            if (afterPotions.Contains(potion))
            {
                // Potion wasn't consumed — manually discard it
                Log("Potion not consumed by action, manually discarding");
                Sched.Run(() => MegaCrit.Sts2.Core.Commands.PotionCmd.Discard(potion));
                Sched.Pump();
            }
        }
        catch (Exception ex)
        {
            Log($"Use potion failed: {ex.Message}");
            // Try manual discard as fallback
            try { Sched.Run(() => MegaCrit.Sts2.Core.Commands.PotionCmd.Discard(potion)); } catch { }
        }

        return DetectDecisionPoint();
    }

    private Dictionary<string, object?> DoDiscardPotion(Player player, Dictionary<string, object?>? args)
    {
        if (args == null || !args.ContainsKey("potion_index"))
            return Error("discard_potion requires 'potion_index'");

        var idx = Convert.ToInt32(args["potion_index"]);
        var potionsList = player.Potions?.ToList() ?? new();
        if (idx < 0 || idx >= potionsList.Count) return Error($"Invalid potion index {idx}");
        var potion = potionsList[idx];
        if (potion == null) return Error($"No potion at index {idx}");

        Sched.Run(() => MegaCrit.Sts2.Core.Commands.PotionCmd.Discard(potion));
        Sched.Pump();
        return DetectDecisionPoint();
    }

    private Dictionary<string, object?> DoChooseOption(Player player, Dictionary<string, object?>? args)
    {
        if (args == null || !args.ContainsKey("option_index"))
            return Error("choose_option requires 'option_index'");

        var optionIndex = Convert.ToInt32(args["option_index"]);
        Log($"Choosing option {optionIndex}");

        // Dispatch based on ROOM TYPE (not event state) to avoid cross-contamination
        if (_runState?.CurrentRoom is RestSiteRoom restSiteRoom)
        {
            Log($"Rest site: choosing option {optionIndex}");
            try
            {
                // Single-threaded: runs until it completes or waits on a selection.
                Sched.Run(() => RunManager.Instance.RestSiteSynchronizer.ChooseLocalOption(optionIndex));
                if (_cardSelector.HasPending)
                {
                    WaitForActionExecutor();
                    return DetectDecisionPoint();
                }
                Sched.Pump();
            }
            catch (Exception ex)
            {
                Log($"Rest site ChooseLocalOption failed: {ex}");
            }

            // After non-Smith rest site options (HEAL, etc.), the options may not clear.
            // Wait for the action to complete (heal/dig), then force transition to map.
            if (!_cardSelector.HasPending)
            {
                Log("Rest site: option chosen (non-Smith), waiting for action then forcing to map");
                // Give the action time to complete (heal HP, dig for relic, etc.)
                WaitForActionExecutor();
                Sched.Pump();
                Sched.Pump();
                Sched.Pump();
                WaitForActionExecutor();
                // The room stays (its proceed button); rewards it offered first.
                _restSiteDone = true;
                if (_rewardList != null)
                    return DetectDecisionPoint();
                return RestSiteState(restSiteRoom);
            }
        }
        // For events — use EventSynchronizer
        else if (_runState?.CurrentRoom is EventRoom)
        {
            var eventSync = RunManager.Instance.EventSynchronizer;
            var localEvent = eventSync?.GetLocalEvent();
            if (localEvent != null && !localEvent.IsFinished)
            {
                var options = localEvent.CurrentOptions;
                var optCountBefore = options?.Count ?? 0;
                if (options != null && optionIndex >= 0 && optionIndex < options.Count)
                {
                    try
                    {
                        _eventOptionChosen = true;
                        _lastEventOptionCount = options.Count;
                        // Single-threaded: runs until it completes or waits on a selection.
                        Sched.Run(() => options[optionIndex].Chosen());
                        if (_cardSelector.HasPending || _cardSelector.HasPendingReward || _pendingBundles != null)
                        {
                            WaitForActionExecutor();
                            return DetectDecisionPoint();
                        }
                        Sched.Pump();
                    }
                    catch (Exception ex) { Log($"Event choose: {ex}"); }
                }

                // Note: do NOT force-finish on `optCountAfter == optCountBefore`. Events can
                // legitimately stay interactive with the same option count (Slippery Bridge
                // Hold On loops until Overcome is chosen, #59). Trust IsFinished and let the
                // next DetectDecisionPoint return the current options for the next choice.
            }
        }

        WaitForActionExecutor();
        return DetectDecisionPoint();
    }

    private Dictionary<string, object?> DoLeaveRoom(Player player)
    {
        if (_runState?.CurrentRoom is TreasureRoom)
            return LeaveTreasure();
        Log("Leaving room");
        try { Sched.Run(() => RunManager.Instance.ProceedFromTerminalRewardsScreen()); }
        catch { }
        Sched.Pump();
        WaitForActionExecutor();

        // If still in a non-combat room, force to map
        var room = _runState?.CurrentRoom;
        if (room is RestSiteRoom || room is MerchantRoom || room is EventRoom || room is TreasureRoom)
        {
            Log("Force leaving non-combat room to map");
            try
            {
                Sched.Run(() => RunManager.Instance.EnterRoom(new MapRoom()));
                Sched.Pump();
                WaitForActionExecutor();
            }
            catch (Exception ex) { Log($"Force leave: {ex.Message}"); }
        }
        return DetectDecisionPoint();
    }

    /// <summary>
    /// Headless mode skips Godot transition callbacks that normally trigger between-act healing
    /// (AncientEventModel.BeforeEventStarted). Replicates the original sts2.dll formula:
    ///   healAmount = MaxHp - CurrentHp  (i.e. heal to full)
    ///   if Ascension >= 2: healAmount *= 0.8
    /// No-op if the engine already healed (missingHp &lt;= 0), so this is safe alongside any
    /// future engine path that does fire the callback.
    /// Adapted from PR #83 commit cf75bec by @tianyumyum.
    /// </summary>
    private void HealBetweenActs()
    {
        if (_runState == null) return;
        var player = _runState.Players[0];
        if (player.Creature == null) return;

        var currentHp = player.Creature.CurrentHp;
        var maxHp = player.Creature.MaxHp;
        var missingHp = maxHp - currentHp;
        if (missingHp <= 0) return;

        decimal healAmount = missingHp;
        if (RunManager.Instance.HasAscension((AscensionLevel)2))
            healAmount *= 0.8m;

        var newHp = currentHp + (int)Math.Ceiling(healAmount);
        if (newHp > maxHp) newHp = maxHp;
        SetField(player.Creature, "_currentHp", newHp);
        Log($"Between-act heal: {currentHp} → {newHp} (missing={missingHp}, ascension2+={RunManager.Instance.HasAscension((AscensionLevel)2)})");
    }

    private Dictionary<string, object?> DoProceed(Player player)
    {
        Log("Proceeding");

        if (_runState?.CurrentRoom is TreasureRoom)
            return LeaveTreasure();

        var fromRewards = ProceedFromRewards(player);
        if (fromRewards != null) return fromRewards;

        // Check if we need to move to next act (boss defeated)
        var room = _runState?.CurrentRoom;
        if (room is CombatRoom combatRoom && combatRoom.RoomType == RoomType.Boss)
        {
            if (combatRoom.IsPreFinished || !CombatManager.Instance.IsInProgress)
            {
                // Final act boss → victory (same rule as DetectPostCombatState, #81).
                if (_runState != null && _runState.CurrentActIndex >= 2)
                {
                    Log($"Final boss defeated via Proceed (Act {_runState.CurrentActIndex + 1}), reporting victory");
                    return GameOverState(true);
                }
                Sched.Run(() => RunManager.Instance.EnterNextAct());
                WaitForActionExecutor();
                HealBetweenActs();
                return DetectDecisionPoint();
            }
        }

        Sched.Run(() => RunManager.Instance.ProceedFromTerminalRewardsScreen());
        WaitForActionExecutor();
        return DetectDecisionPoint();
    }

    #endregion

    #region Decision Point Detection

    /// <summary>Current decision point (used after Sched.Settle ran more work).</summary>
    public Dictionary<string, object?> CurrentDecision()
    {
        try { return DetectDecisionPoint(); }
        catch (Exception ex) { return ErrorWithTrace("DetectDecisionPoint failed", ex); }
    }

    public bool HasRun => _runState != null;

    private Dictionary<string, object?> DetectDecisionPoint()
    {
        if (_runState == null)
            return Error("No run in progress");

        var player = _runState.Players[0];

        // Check game over (death)
        if (player.Creature != null && player.Creature.IsDead)
        {
            return GameOverState(false);
        }

        // Event minigames (Crystal Sphere) run inside an event option.
        var minigame = MinigameDecision();
        if (minigame != null)
            return minigame;

        // Check if there's a pending bundle selection (Scroll Boxes: pick 1 of N packs)
        if (_pendingBundles != null && _pendingBundleTcs != null && !_pendingBundleTcs.Task.IsCompleted)
        {
            var bundles = _pendingBundles.Select((bundle, i) => new Dictionary<string, object?>
            {
                ["index"] = i,
                ["cards"] = bundle.Select(card =>
                {
                    var stats = new Dictionary<string, object?>();
                    try { foreach (var dv in card.DynamicVars.Values) stats[dv.Name.ToLowerInvariant()] = (int)dv.BaseValue; } catch { }
                    var bkws = card.Keywords?.Where(k => k != CardKeyword.None).Select(k => k.ToString()).ToList();
                    return WithCardView(card, PileType.None, new Dictionary<string, object?>
                    {
                        ["name"] = _loc.Card(card.Id.Entry),
                        ["cost"] = card.EnergyCost?.GetResolved() ?? 0,
                        ["type"] = card.Type.ToString(),
                        ["rarity"] = card.Rarity.ToString(),
                        ["description"] = _loc.Bilingual("cards", card.Id.Entry + ".description"),
                        ["stats"] = stats.Count > 0 ? stats : null,
                        ["keywords"] = bkws?.Count > 0 ? bkws : null,
                    });
                }).ToList(),
            }).ToList();

            return new Dictionary<string, object?>
            {
                ["type"] = "decision",
                ["decision"] = "bundle_select",
                ["context"] = RunContext(),
                ["bundles"] = bundles,
                ["player"] = PlayerSummary(player),
            };
        }

        // Check if there's a pending card reward from event (GetSelectedCardReward blocking)
        if (_cardSelector.HasPendingReward)
        {
            var rewardCards = _cardSelector.PendingRewardCards!;
            var cards = rewardCards.Select((cr, i) =>
            {
                var stats = new Dictionary<string, object?>();
                try { foreach (var dv in cr.Card.DynamicVars.Values) stats[dv.Name.ToLowerInvariant()] = (int)dv.BaseValue; } catch { }
                var rrkws = cr.Card.Keywords?.Where(k => k != CardKeyword.None).Select(k => k.ToString()).ToList();
                return WithCardView(cr.Card, PileType.None, new Dictionary<string, object?>
                {
                    ["index"] = i,
                    ["id"] = cr.Card.Id.ToString(),
                    ["name"] = _loc.Card(cr.Card.Id.Entry),
                    ["cost"] = cr.Card.EnergyCost?.GetResolved() ?? 0,
                    ["type"] = cr.Card.Type.ToString(),
                    ["rarity"] = cr.Card.Rarity.ToString(),
                    ["description"] = _loc.Bilingual("cards", cr.Card.Id.Entry + ".description"),
                    ["stats"] = stats.Count > 0 ? stats : null,
                    ["keywords"] = rrkws?.Count > 0 ? rrkws : null,
                    ["after_upgrade"] = GetUpgradedInfo(cr.Card),
                });
            }).ToList();

            return new Dictionary<string, object?>
            {
                ["type"] = "decision",
                ["decision"] = "card_reward",
                ["context"] = RunContext(),
                ["cards"] = cards,
                ["can_skip"] = true,
                ["from_event"] = true,
                ["player"] = PlayerSummary(_runState!.Players[0]),
            };
        }

        // Check if there's a pending card selection (upgrade, remove, transform, start-of-turn powers)
        checkCardSelect:
        if (_cardSelector.HasPending && _cardSelector.PendingOptions != null)
        {
            var opts = _cardSelector.PendingOptions.Select((card, i) =>
            {
                var stats = new Dictionary<string, object?>();
                try { foreach (var dv in card.DynamicVars.Values) stats[dv.Name.ToLowerInvariant()] = (int)dv.BaseValue; } catch { }
                var selkws = card.Keywords?.Where(k => k != CardKeyword.None).Select(k => k.ToString()).ToList();
                return WithCardView(card, card.Pile?.Type ?? PileType.None, new Dictionary<string, object?>
                {
                    ["index"] = i,
                    ["id"] = card.Id.ToString(),
                    ["name"] = _loc.Card(card.Id.Entry),
                    ["cost"] = card.EnergyCost?.GetResolved() ?? 0,
                    ["type"] = card.Type.ToString(),
                    ["rarity"] = card.Rarity.ToString(),
                    ["upgraded"] = card.IsUpgraded,
                    ["stats"] = stats.Count > 0 ? stats : null,
                    ["description"] = _loc.Bilingual("cards", card.Id.Entry + ".description"),
                    ["keywords"] = selkws?.Count > 0 ? selkws : null,
                    ["after_upgrade"] = GetUpgradedInfo(card),
                    ["upgrade_preview"] = _cardSelector.PendingKind == "upgrade" ? UpgradePreviewView(card) : null,
                });
            }).ToList();

            return new Dictionary<string, object?>
            {
                ["type"] = "decision",
                ["decision"] = "card_select",
                ["context"] = RunContext(),
                ["cards"] = opts,
                ["min_select"] = _cardSelector.PendingMinSelect,
                ["max_select"] = _cardSelector.PendingMaxSelect,
                ["kind"] = _cardSelector.PendingKind,
                ["prompt"] = _cardSelector.PendingPromptText,
                ["cancelable"] = _cardSelector.PendingCancelable,
                ["manual_confirm"] = _cardSelector.PendingManualConfirm,
                ["player"] = PlayerSummary(player),
            };
        }

        // Check if there's a pending card reward
        if (_pendingCardReward != null)
        {
            return CardRewardState(player, _runState.CurrentRoom as CombatRoom);
        }

        // The rewards screen (a fight's or an event's)
        var rewardsState = PendingRewardsState();
        if (rewardsState != null) return rewardsState;

        // Check if RunManager reports game over (victory)
        if (RunManager.Instance.IsGameOver)
        {
            return GameOverState(true);
        }

        var room = _runState.CurrentRoom;

        // Map room — need to select a node
        if (room is MapRoom || room == null)
        {
            return MapSelectState();
        }

        // Combat room
        if (room is CombatRoom combatRoom)
        {
            // With Task.Yield() patched, combat init should be synchronous
            Sched.Pump();
            WaitForActionExecutor();

            // Re-check for pending card selections AFTER pump (BUG-024: start-of-turn effects
            // like Tools of Trade create card selections during Pump, AFTER the initial HasPending check)
            if (_cardSelector.HasPending && _cardSelector.PendingOptions != null)
            {
                goto checkCardSelect;  // Jump back to card_select handling
            }

            if (CombatManager.Instance.IsInProgress && IsPlayPhase())
            {
                return CombatPlayState(player);
            }
            if (!CombatManager.Instance.IsInProgress || (player.Creature != null && player.Creature.IsDead))
            {
                return DetectPostCombatState(player, combatRoom);
            }
            // Fallback: brief wait
            for (int i = 0; i < 20; i++)
            {
                Sched.Pump();
                Sched.Pump();
                if (IsPlayPhase()) return CombatPlayState(player);
                if (!CombatManager.Instance.IsInProgress) return DetectPostCombatState(player, combatRoom);
            }
            return CombatPlayState(player);
        }

        // Event room
        if (room is EventRoom eventRoom)
        {
            return EventChoiceState(eventRoom);
        }

        // Rest site
        if (room is RestSiteRoom restRoom)
        {
            return RestSiteState(restRoom);
        }

        // Merchant/Shop
        if (room is MerchantRoom merchantRoom)
        {
            return ShopState(merchantRoom, player);
        }

        // Treasure room
        if (room is TreasureRoom treasureRoom)
        {
            return TreasureState(treasureRoom);
        }

        // Fallback
        return new Dictionary<string, object?>
        {
            ["type"] = "decision",
            ["decision"] = "unknown",
            ["context"] = RunContext(),
            ["room_type"] = room?.GetType().Name,
            ["message"] = "Unknown room type or state",
        };
    }

    // The whole act map for the map screen: every point (type, children),
    // the start and boss points, the grid size and the path taken so far.
    private Dictionary<string, object?>? MapView(ActMap? map)
    {
        if (map == null) return null;
        try
        {
            Dictionary<string, object?> Point(MapPoint p) => new()
            {
                ["col"] = (int)p.coord.col,
                ["row"] = (int)p.coord.row,
                ["type"] = p.PointType.ToString(),
                ["children"] = (p.Children ?? Enumerable.Empty<MapPoint>())
                    .Select(c => new List<int> { (int)c.coord.col, (int)c.coord.row }).ToList(),
            };
            var cur = _runState?.CurrentMapCoord;
            // NMapScreen.SetMap's jitter: the run seed's "map_jitter_<act>"
            // stream, two floats per point in GetAllMapPoints order.
            var points = map.GetAllMapPoints().Select(Point).ToList();
            try
            {
                if (_runState != null)
                {
                    var rng = new MegaCrit.Sts2.Core.Random.Rng(_runState.Rng.Seed, $"map_jitter_{_runState.CurrentActIndex}");
                    foreach (var pd in points)
                    {
                        float jx = rng.NextFloat(-21f, 21f);
                        float jy = rng.NextFloat(-25f, 25f);
                        pd["jitter"] = new[] { jx, jy };
                    }
                    // NNormalMapPoint.UpdateIcon: a visited "?" shows the room it was.
                    var hist = _runState.MapPointHistory;
                    if (hist.Count > _runState.CurrentActIndex)
                    {
                        var entries = hist[_runState.CurrentActIndex];
                        foreach (var pd in points)
                        {
                            int row = (int)pd["row"]!;
                            if ((string?)pd["type"] == "Unknown" && row < entries.Count &&
                                (_runState.VisitedMapCoords?.Any(c => c.col == (int)pd["col"]! && c.row == row) ?? false))
                                pd["room"] = entries[row].Rooms.FirstOrDefault()?.RoomType.ToString();
                        }
                    }
                }
            }
            catch (Exception ex) { Log($"MapView jitter: {ex.Message}"); }
            Dictionary<string, object?>? look = null;
            try
            {
                var act = _runState?.Act;
                static float[] Rgba(Godot.Color c) => new[] { c.R, c.G, c.B, c.A };
                static string Res(string p) => p.StartsWith("res://") ? p.Substring(6) : p;
                if (act != null)
                    look = new Dictionary<string, object?>
                    {
                        ["bg"] = new[] { Res(act.MapTopBgPath), Res(act.MapMidBgPath), Res(act.MapBotBgPath) },
                        ["bg_color"] = Rgba(act.MapBgColor),
                        ["traveled_color"] = Rgba(act.MapTraveledColor),
                        ["untraveled_color"] = Rgba(act.MapUntraveledColor),
                    };
            }
            catch (Exception ex) { Log($"MapView look: {ex.Message}"); }
            return new Dictionary<string, object?>
            {
                ["cols"] = map.GetColumnCount(),
                ["rows"] = map.GetRowCount(),
                ["look"] = look,
                ["points"] = points,
                ["start"] = Point(map.StartingMapPoint),
                ["boss"] = Point(map.BossMapPoint),
                ["second_boss"] = map.SecondBossMapPoint != null ? Point(map.SecondBossMapPoint) : null,
                ["visited"] = (_runState?.VisitedMapCoords ?? new List<MapCoord>())
                    .Select(c => new List<int> { (int)c.col, (int)c.row }).ToList(),
                ["current"] = cur.HasValue ? new List<int> { (int)cur.Value.col, (int)cur.Value.row } : null,
            };
        }
        catch (Exception ex)
        {
            Log($"MapView: {ex.Message}");
            return null;
        }
    }

    private Dictionary<string, object?> MapSelectState()
    {
        var map = _runState?.Map;
        if (map == null)
        {
            Log("Map is null, generating...");
            try
            {
                Sched.Run(() => RunManager.Instance.GenerateMap());
                Sched.Pump();
                map = _runState?.Map;
            }
            catch (Exception ex)
            {
                Log($"GenerateMap failed: {ex.Message}");
            }
            if (map == null)
                return Error("No map available");
        }
        var currentCoord = _runState!.CurrentMapCoord;

        List<Dictionary<string, object?>> choices;
        if (currentCoord.HasValue)
        {
            var currentPoint = map.GetPoint(currentCoord.Value);
            if (currentPoint == null)
            {
                Log($"GetPoint returned null for coord ({currentCoord.Value.col},{currentCoord.Value.row}), falling back to start");
                // Current coord is invalid (stale after forced room transition); treat as no position
                choices = new List<Dictionary<string, object?>>();
                var sp = map.StartingMapPoint;
                if (sp?.Children != null)
                {
                    foreach (var child in sp.Children)
                    {
                        choices.Add(new Dictionary<string, object?>
                        {
                            ["col"] = (int)child.coord.col,
                            ["row"] = (int)child.coord.row,
                            ["type"] = child.PointType.ToString(),
                        });
                    }
                }
            }
            else
            {
                choices = (currentPoint.Children ?? Enumerable.Empty<MapPoint>())
                    .Select(child => new Dictionary<string, object?>
                    {
                        ["col"] = (int)child.coord.col,
                        ["row"] = (int)child.coord.row,
                        ["type"] = child.PointType.ToString(),
                    })
                    .ToList();
            }
        }
        else
        {
            // Starting point — pick from starting row
            var startPoint = map.StartingMapPoint;
            choices = new List<Dictionary<string, object?>>
            {
                new()
                {
                    ["col"] = (int)startPoint.coord.col,
                    ["row"] = (int)startPoint.coord.row,
                    ["type"] = startPoint.PointType.ToString(),
                }
            };
            // Add all children of start point as well since we can travel to them
            if (startPoint.Children != null)
            {
                foreach (var child in startPoint.Children)
                {
                    choices.Add(new Dictionary<string, object?>
                    {
                        ["col"] = (int)child.coord.col,
                        ["row"] = (int)child.coord.row,
                        ["type"] = child.PointType.ToString(),
                    });
                }
            }
        }

        return new Dictionary<string, object?>
        {
            ["type"] = "decision",
            ["decision"] = "map_select",
            ["context"] = RunContext(),
            ["choices"] = choices,
            ["map"] = MapView(map),
            ["player"] = PlayerSummary(_runState!.Players[0]),
            ["act"] = _runState.CurrentActIndex + 1,
            ["act_name"] = _loc.Act(_runState.Act?.Id.Entry ?? "OVERGROWTH"),
            ["floor"] = _runState.ActFloor,
        };
    }

    // res:// folder listings for the game code that reads folders (the
    // combat background picker): data/res_dirs.json, written by the converter.
    private static Dictionary<string, string[]>? _resDirs;

    private static void EnsureResFiles()
    {
        if (Godot.DirAccess.ResFiles != null) return;
        Godot.DirAccess.ResFiles = dir =>
        {
            if (_resDirs == null)
            {
                _resDirs = new Dictionary<string, string[]>();
                try
                {
                    var path = Path.Combine(DataDir.Root, "res_dirs.json");
                    using var doc = System.Text.Json.JsonDocument.Parse(File.Exists(path) ? File.ReadAllText(path) : "{}");
                    foreach (var kv in doc.RootElement.EnumerateObject())
                        if (kv.Value.ValueKind == System.Text.Json.JsonValueKind.Array)
                            _resDirs[kv.Name] = kv.Value.EnumerateArray().Select(x => x.GetString() ?? "").ToArray();
                }
                catch (Exception ex) { Log($"res_dirs.json: {ex.Message}"); }
            }
            return _resDirs.TryGetValue(dir, out var files) ? files : null;
        };
    }

    // What the combat screen needs beyond the decision: the encounter, its
    // camera, and the background the game picks for this map point (the
    // game's own picker, with the room's seed: same layers as on PC).
    private Dictionary<string, object?>? _combatView;
    // Presentation inputs (engine/game/combat_screen.cpp), from the game's
    // own view code so the console draws what the game would.

    // NCard.UpdateVisuals for a card in a pile: what the card shows (title,
    // the formatted description, portrait, frame and energy orb, cost
    // colours, the hand's glow).
    /// <summary>
    /// A model's hover tips (NHoverTipSet's text tips: title, description,
    /// debuff look), duplicates removed as the game does; card previews are
    /// not included. Null when there are none.
    /// </summary>
    private static List<Dictionary<string, object?>>? Tips(Func<IEnumerable<MegaCrit.Sts2.Core.HoverTips.IHoverTip>> get)
    {
        try
        {
            var list = new List<Dictionary<string, object?>>();
            foreach (var t in MegaCrit.Sts2.Core.HoverTips.IHoverTip.RemoveDupes(get()))
            {
                if (t is not MegaCrit.Sts2.Core.HoverTips.HoverTip h) continue;
                list.Add(new Dictionary<string, object?>
                {
                    ["title"] = h.Title,
                    ["description"] = h.Description,
                    ["debuff"] = h.IsDebuff ? true : null,
                    ["power"] = (h.CanonicalModel as PowerModel)?.Id.Entry,
                });
            }
            return list.Count > 0 ? list : null;
        }
        catch { return null; }
    }

    private static void AddCardView(Dictionary<string, object?> d, CardModel c, PileType pile)
    {
        d["tips"] = Tips(() => c.HoverTips);
        static string Res(string p) => p.StartsWith("res://") ? p.Substring(6) : p;
        static float[] Rgba(Godot.Color col) => new[] { col.R, col.G, col.B, col.A };
        try
        {
            d["title"] = c.Title;
            d["upgrade_level"] = c.CurrentUpgradeLevel;
            d["portrait"] = Res(c.PortraitPath);
            d["pool"] = c.Pool.Title;
            d["energy_outline"] = Rgba(c.Pool.EnergyOutlineColor);
            d["frame_material"] = c.VisualCardPool.CardFrameMaterialPath;
            d["energy_icon"] = c.VisualCardPool.EnergyColorName;
            d["cost_display"] = c.EnergyCost.CostsX ? "X" : c.EnergyCost.GetWithModifiers(CostModifiers.All).ToString();
            d["cost_just_upgraded"] = c.EnergyCost.WasJustUpgraded;
            d["star_display"] = c.HasStarCostX ? "X" : c.GetStarCostWithModifiers().ToString();
            d["star_just_upgraded"] = c.WasStarCostJustUpgraded;
            if (c.CombatState != null)
            {
                d["cost_color"] = MegaCrit.Sts2.Core.Helpers.Models.CardCostHelper.GetEnergyCostColor(c, c.CombatState).ToString();
                d["star_color"] = MegaCrit.Sts2.Core.Helpers.Models.CardCostHelper.GetStarCostColor(c, c.CombatState).ToString();
            }
            if (pile == PileType.Hand)
            {
                bool canPlay = c.CanPlay(out var reason, out _);
                // UnplayableReasonExtensions.HasResourceCostReason (internal)
                bool costReason = reason.HasFlag(UnplayableReason.EnergyCostTooHigh) || reason.HasFlag(UnplayableReason.StarCostTooHigh);
                d["unplayable_icon"] = !canPlay && !costReason;
                bool play = c.Owner?.PlayerCombatState?.Phase == MegaCrit.Sts2.Core.Combat.PlayerTurnPhase.Play;
                d["glow"] = c.ShouldGlowRed && play ? "red"
                          : canPlay && c.ShouldGlowGold && play ? "gold"
                          : canPlay ? "playable" : "none";
            }
            c.DynamicVars.ClearPreview();
            c.UpdateDynamicVarPreview(MegaCrit.Sts2.Core.Entities.Cards.CardPreviewMode.Normal, c.CurrentTarget, c.DynamicVars);
            if (c.Enchantment != null)
            {
                c.Enchantment.DynamicVars.ClearPreview();
                c.UpdateDynamicVarPreview(MegaCrit.Sts2.Core.Entities.Cards.CardPreviewMode.Normal, c.CurrentTarget, c.Enchantment.DynamicVars);
            }
            d["description_bbcode"] = c.GetDescriptionForPile(pile, c.CurrentTarget);
            if (pile == PileType.Hand && c.CombatState != null)
            {
                // NCard.SetPreviewTarget while the pad aims the card: the text
                // against each enemy (the decision's enemies, in order), and
                // ShowMultiCreatureTargetingVisuals' preview for all-enemy cards.
                var enemies = c.CombatState.Enemies.Where(e => e != null && e.IsAlive).ToList();
                string Preview(CardPreviewMode mode, Creature? target)
                {
                    c.DynamicVars.ClearPreview();
                    c.UpdateDynamicVarPreview(mode, target, c.DynamicVars);
                    if (c.Enchantment != null)
                    {
                        c.Enchantment.DynamicVars.ClearPreview();
                        c.UpdateDynamicVarPreview(mode, target, c.Enchantment.DynamicVars);
                    }
                    return c.GetDescriptionForPile(pile, target);
                }
                if (c.TargetType == TargetType.AnyEnemy)
                    d["description_by_target"] = enemies.Select(e => Preview(CardPreviewMode.Normal, e)).ToList();
                else if (c.TargetType == TargetType.AllEnemies || c.TargetType == TargetType.RandomEnemy)
                {
                    var hittable = c.CombatState.HittableEnemies;
                    d["description_multi"] = Preview(CardPreviewMode.MultiCreatureTargeting,
                                                     hittable.Count == 1 ? hittable[0] : c.CurrentTarget);
                }
            }
        }
        catch (Exception ex) { Log($"CardView {c.Id.Entry}: {(ex.InnerException ?? ex).Message}"); }
        finally
        {
            // As the stats above: leave the live card without a preview.
            try
            {
                c.DynamicVars.ClearPreview();
                c.Enchantment?.DynamicVars.ClearPreview();
            }
            catch { }
        }
    }

    // The card's view fields (AddCardView) on a decision's card entry.
    private static Dictionary<string, object?> WithCardView(CardModel? c, PileType pile, Dictionary<string, object?> d)
    {
        if (c != null) AddCardView(d, c, pile);
        return d;
    }

    // NPower.RefreshAmount: the icon's label.
    private Dictionary<string, object?> PowerView(PowerModel pw)
    {
        var d = new Dictionary<string, object?>
        {
            ["id"] = pw.Id.Entry,
            ["name"] = _loc.Power(pw.Id.Entry),
            ["description"] = _loc.Bilingual("powers", pw.Id.Entry + ".description"),
            ["amount"] = pw.Amount,
            ["tips"] = Tips(() => pw.HoverTips),
        };
        try
        {
            d["visible"] = pw.IsVisible;
            d["counter"] = pw.StackType == MegaCrit.Sts2.Core.Entities.Powers.PowerStackType.Counter;
            d["display_amount"] = pw.DisplayAmount;
            var c = pw.AmountLabelColor;
            d["amount_color"] = new[] { c.R, c.G, c.B, c.A };
        }
        catch (Exception ex) { Log($"PowerView {pw.Id.Entry}: {ex.Message}"); }
        return d;
    }

    // NIntent.UpdateVisuals: the animation (IntentAnimData), the particle
    // texture and the value label.
    [System.Diagnostics.CodeAnalysis.DynamicDependency("get_SpritePath", typeof(MegaCrit.Sts2.Core.MonsterMoves.Intents.AbstractIntent))]
    private static void AddIntentView(Dictionary<string, object?> d, MegaCrit.Sts2.Core.MonsterMoves.Intents.AbstractIntent intent,
                                      IEnumerable<Creature> targets, Creature owner)
    {
        try
        {
            var anim = intent.GetAnimation(targets, owner);
            d["animation"] = anim;
            string? sprite;
            if (intent is MegaCrit.Sts2.Core.MonsterMoves.Intents.AttackIntent)
            {
                // AttackIntent.GetTexture picks the same tier as GetAnimation.
                sprite = "atlases/intent_atlas.sprites/attack/intent_" + anim + ".tres";
            }
            else
            {
                var prop = typeof(MegaCrit.Sts2.Core.MonsterMoves.Intents.AbstractIntent)
                    .GetProperty("SpritePath", BindingFlags.NonPublic | BindingFlags.Instance);
                sprite = prop?.GetValue(intent) as string;
            }
            d["sprite"] = sprite != null ? "images/" + sprite.TrimStart('/') : null;
            if (intent is MegaCrit.Sts2.Core.MonsterMoves.Intents.AttackIntent || intent is MegaCrit.Sts2.Core.MonsterMoves.Intents.StatusIntent)
                d["label"] = intent.GetIntentLabel(targets, owner).GetFormattedText() ?? "";
        }
        catch (Exception ex) { Log($"IntentView: {(ex.InnerException ?? ex).Message}"); }
    }

    private EncounterModel? _combatViewFor;

    [System.Diagnostics.CodeAnalysis.DynamicDependency("GetBackgroundAssets", typeof(EncounterModel))]
    private Dictionary<string, object?>? CombatView()
    {
        try
        {
            var enc = (_runState?.CurrentRoom as CombatRoom)?.Encounter;
            var act = _runState?.Act;
            if (enc == null || act == null) return null;
            if (ReferenceEquals(enc, _combatViewFor)) return _combatView;
            EnsureResFiles();
            Dictionary<string, object?>? bg = null;
            try
            {
                uint mix = 0;
                var coord = _runState!.CurrentMapCoord;
                if (coord.HasValue) mix = (uint)(coord.Value.row + coord.Value.col * 747);
                var rng = new MegaCrit.Sts2.Core.Random.Rng(_runState.Rng.Seed + mix);
                var get = typeof(EncounterModel).GetMethod("GetBackgroundAssets", BindingFlags.NonPublic | BindingFlags.Instance);
                if (get?.Invoke(enc, new object[] { act, rng }) is BackgroundAssets assets)
                {
                    static string Res(string? p) => p == null ? "" : p.StartsWith("res://") ? p.Substring(6) : p;
                    bg = new Dictionary<string, object?>
                    {
                        ["scene"] = Res(assets.BackgroundScenePath),
                        ["layers"] = assets.BgLayers.Select(Res).ToList(),
                        ["fg"] = assets.FgLayer != null ? Res(assets.FgLayer) : null,
                    };
                }
            }
            catch (Exception ex) { Log($"CombatView background: {(ex.InnerException ?? ex).Message}"); }
            var off = enc.GetCameraOffset();
            _combatViewFor = enc;
            _combatView = new Dictionary<string, object?>
            {
                ["encounter"] = enc.Id.Entry,
                ["camera_scaling"] = enc.GetCameraScaling(),
                ["camera_offset"] = new List<float> { off.X, off.Y },
                ["has_scene"] = enc.HasScene,
                ["fully_center_players"] = enc.FullyCenterPlayers,
                ["background"] = bg,
            };
            return _combatView;
        }
        catch (Exception ex)
        {
            Log($"CombatView: {ex.Message}");
            return null;
        }
    }

    private Dictionary<string, object?> CombatPlayState(Player player)
    {
        var pcs = player.PlayerCombatState;
        var combatState = CombatManager.Instance.DebugOnlyGetState();

        // Track last known HP for accurate game_over reporting (BUG-005)
        if (player.Creature != null && player.Creature.CurrentHp > 0)
            _lastKnownHp = player.Creature.CurrentHp;

        // Alive enemies in the same order play_card's AnyEnemy targeting uses, so
        // damage_by_target[i].target_index aligns with the target_index clients pass.
        var aliveEnemiesForTargeting = combatState?.Enemies?
            .Where(e => e != null && e.IsAlive).ToList() ?? new();

        var hand = pcs?.Hand?.Cards?.Select((c, i) =>
        {
            // Export the *currently resolved* stat values, not the card base: refresh the
            // DynamicVar previews (mirrors NCard.UpdateVisuals) so damage reflects Strength/Weak,
            // block reflects Frail, calculateddamage reflects current Block, etc. ClearPreview
            // resets PreviewValue to BaseValue, and only damage/block/calculated vars override it,
            // so reading PreviewValue uniformly is safe. Issues #65 #69 #70 #71 #74 #75.
            var stats = new Dictionary<string, object?>();
            try
            {
                c.DynamicVars.ClearPreview();
                c.UpdateDynamicVarPreview(
                    MegaCrit.Sts2.Core.Entities.Cards.CardPreviewMode.Normal,
                    c.CurrentTarget, c.DynamicVars);
                foreach (var dv in c.DynamicVars.Values)
                {
                    stats[dv.Name.ToLowerInvariant()] = (int)dv.PreviewValue;
                }
                // Restore the live card to base state. UpdateDynamicVarPreview mutates the
                // card's preview (and for self-cost cards like Momentum Strike, leaving it in
                // preview state corrupts the subsequent PlayCardAction — card stays in hand).
                c.DynamicVars.ClearPreview();
            }
            catch { }

            // Per-target resolved damage for attack cards: the scalar `stats` above use the
            // card's CurrentTarget, but a single value can't capture target-specific modifiers
            // (Vulnerable #60, Slow #77) or conditional hit counts (Dismantle #78, X-cost
            // Whirlwind #82). Re-run the preview per enemy via MultiCreatureTargeting, the same
            // path the game uses to draw multi-target previews, and read the resolved vars.
            List<Dictionary<string, object?>>? damageByTarget = null;
            if (c.Type == CardType.Attack && aliveEnemiesForTargeting.Count > 0
                && (c.TargetType == TargetType.AnyEnemy || c.TargetType == TargetType.AllEnemies))
            {
                damageByTarget = new List<Dictionary<string, object?>>();
                for (int ti = 0; ti < aliveEnemiesForTargeting.Count; ti++)
                {
                    var tgt = aliveEnemiesForTargeting[ti];
                    try
                    {
                        c.DynamicVars.ClearPreview();
                        c.UpdateDynamicVarPreview(
                            MegaCrit.Sts2.Core.Entities.Cards.CardPreviewMode.MultiCreatureTargeting,
                            tgt, c.DynamicVars);
                        var tstats = new Dictionary<string, object?>();
                        foreach (var dv in c.DynamicVars.Values)
                            tstats[dv.Name.ToLowerInvariant()] = (int)dv.PreviewValue;

                        // Per-hit damage = calculateddamage (override cards) or damage.
                        int? perHit = tstats.TryGetValue("calculateddamage", out var cdv) && cdv is int cdi && cdi > 0
                            ? cdi
                            : (tstats.TryGetValue("damage", out var dv2) && dv2 is int di ? di : (int?)null);
                        // Hit count: explicit `repeat` var if present, else for X-cost attacks
                        // the hit count is the current X (= available energy), e.g. Whirlwind (#82).
                        int repeat = tstats.TryGetValue("repeat", out var rv) && rv is int ri && ri > 0 ? ri : 1;
                        if (repeat == 1 && c.EnergyCost?.CostsX == true && pcs != null)
                            repeat = pcs.Energy;
                        // Dismantle hits twice when the target is Vulnerable (#78). The doubled
                        // hit count lives in Dismantle.OnPlay, not in any DynamicVar preview or
                        // the Hook.ModifyAttackHitCount path (which needs an AttackCommand we
                        // don't have at preview time), so it's special-cased by card entry.
                        if (repeat == 1 && c.Id.Entry == "DISMANTLE" && tgt.Powers != null
                            && tgt.Powers.Any(p => p?.Id.Entry == "VULNERABLE_POWER"))
                            repeat = 2;

                        var row = new Dictionary<string, object?>
                        {
                            ["target_index"] = ti,
                            ["id"] = tgt.Monster?.Id.Entry,
                            ["name"] = _loc.Monster(tgt.Monster?.Id.Entry ?? "UNKNOWN"),
                        };
                        if (perHit != null) row["damage"] = perHit;
                        if (repeat > 1)
                        {
                            row["repeat"] = repeat;
                            if (perHit != null) row["total_damage"] = perHit * repeat;
                        }
                        damageByTarget.Add(row);
                    }
                    catch { }
                    finally { c.DynamicVars.ClearPreview(); }
                }
            }

            // Use CurrentStarCost (combat-modified) for UI/can_play; BaseStarCost ignores temporary reductions.
            var starCost = c.CurrentStarCost;
            var cardInfo = new Dictionary<string, object?>
            {
                ["index"] = i,
                // NPlayerHand retains holders by card instance across decisions.
                ["instance_id"] = Steps.IdOf(c),
                ["id"] = c.Id.ToString(),
                ["name"] = _loc.Card(c.Id.Entry),
                ["cost"] = c.EnergyCost?.GetResolved() ?? 0,
                ["type"] = c.Type.ToString(),
                ["rarity"] = c.Rarity.ToString(),
                // The UI cannot aim a single-target card at nothing (e.g. while
                // Test Subject waits to revive), so it is not playable then.
                ["can_play"] = c.CanPlay(out _, out _)
                    && !(c.TargetType == TargetType.AnyEnemy && aliveEnemiesForTargeting.Count == 0),
                ["target_type"] = c.TargetType.ToString(),
                ["stats"] = stats.Count > 0 ? stats : null,
                ["description"] = _loc.Bilingual("cards", c.Id.Entry + ".description"),
            };
            if (starCost > 0)
            {
                cardInfo["star_cost"] = starCost;
                // BUG-007: Override can_play for star-cost cards when player lacks stars
                if (pcs != null && pcs.Stars < starCost)
                    cardInfo["can_play"] = false;
            }
            var kws = c.Keywords?.Where(k => k != CardKeyword.None).Select(k => k.ToString()).ToList();
            if (kws?.Count > 0) cardInfo["keywords"] = kws;
            if (c.Enchantment != null)
            {
                cardInfo["enchantment"] = _loc.Bilingual("enchantments", c.Enchantment.Id.Entry + ".title");
                try { if (c.Enchantment.Amount != 0) cardInfo["enchantment_amount"] = c.Enchantment.Amount; } catch { }
            }
            if (c.Affliction != null)
            {
                cardInfo["affliction"] = _loc.Bilingual("afflictions", c.Affliction.Id.Entry + ".title");
                try { if (c.Affliction.Amount != 0) cardInfo["affliction_amount"] = c.Affliction.Amount; } catch { }
            }
            if (damageByTarget != null && damageByTarget.Count > 0)
                cardInfo["damage_by_target"] = damageByTarget;
            AddCardView(cardInfo, c, PileType.Hand);
            return cardInfo;
        }).ToList() ?? new();

        var playerCreatures = combatState?.PlayerCreatures?.ToList();

        var enemies = combatState?.Enemies?
            .Where(e => e != null && e.IsAlive)
            .Select((e, i) =>
            {
                // Extract detailed intent info
                var intents = new List<Dictionary<string, object?>>();
                try
                {
                    if (e.Monster?.NextMove?.Intents != null)
                    {
                        foreach (var intent in e.Monster.NextMove.Intents)
                        {
                            var intentInfo = new Dictionary<string, object?>
                            {
                                ["type"] = intent.IntentType.ToString(),
                            };
                            if (playerCreatures != null) AddIntentView(intentInfo, intent, playerCreatures, e);
                            // Get damage for attack intents
                            if (intent is MegaCrit.Sts2.Core.MonsterMoves.Intents.AttackIntent atk && playerCreatures != null)
                            {
                                try
                                {
                                    // For multi-hit attacks, expose per-hit `damage` (matching the
                                    // game's intent description, which pairs GetSingleDamage with
                                    // Repeat) plus an explicit `total_damage`. Reporting the total in
                                    // `damage` while also reporting `hits` let clients compute
                                    // damage*hits and double-count incoming damage (#67).
                                    var hits = atk.Repeats;
                                    if (hits > 1)
                                    {
                                        intentInfo["damage"] = atk.GetSingleDamage(playerCreatures, e);
                                        intentInfo["hits"] = hits;
                                        intentInfo["total_damage"] = atk.GetTotalDamage(playerCreatures, e);
                                    }
                                    else
                                    {
                                        intentInfo["damage"] = atk.GetTotalDamage(playerCreatures, e);
                                    }
                                }
                                catch { }
                            }
                            intents.Add(intentInfo);
                        }
                    }
                }
                catch { }

                // Enemy powers
                var ePowers = e.Powers?.Select(PowerView).ToList();

                return new Dictionary<string, object?>
                {
                    ["index"] = i,
                    // View ids: the monster (its visuals are
                    // scenes/creature_visuals/<id lowercased>.tscn) and the
                    // creature, stable for the whole combat.
                    ["id"] = e.Monster?.Id.Entry,
                    ["combat_id"] = e.CombatId,
                    ["slot"] = string.IsNullOrEmpty(e.SlotName) ? null : e.SlotName,
                    ["name"] = _loc.Monster(e.Monster?.Id.Entry ?? "UNKNOWN"),
                    ["hp"] = e.CurrentHp,
                    ["max_hp"] = e.MaxHp,
                    ["block"] = e.Block,
                    ["intents"] = intents.Count > 0 ? intents : null,
                    ["intends_attack"] = e.Monster?.IntendsToAttack ?? false,
                    ["powers"] = ePowers?.Count > 0 ? ePowers : null,
                }.WithCreatureView(e);
            }).ToList() ?? new();

        // Player powers/buffs
        var playerPowers = player.Creature?.Powers?.Select(PowerView).ToList();

        var result = new Dictionary<string, object?>
        {
            ["type"] = "decision",
            ["decision"] = "combat_play",
            ["context"] = RunContext(),
            ["round"] = combatState?.RoundNumber ?? 0,
            ["energy"] = pcs?.Energy ?? 0,
            ["max_energy"] = pcs?.MaxEnergy ?? 0,
            ["hand"] = hand,
            ["enemies"] = enemies,
            ["player"] = player.Creature != null ? PlayerSummary(player).WithCreatureView(player.Creature) : PlayerSummary(player),
            ["player_powers"] = playerPowers?.Count > 0 ? playerPowers : null,
            ["draw_pile_count"] = pcs?.DrawPile?.Cards?.Count ?? 0,
            ["discard_pile_count"] = pcs?.DiscardPile?.Cards?.Count ?? 0,
            // The combat UI (NCombatUi): exhaust pile button, star counter,
            // "End Turn {n}".
            ["exhaust_pile_count"] = pcs?.ExhaustPile?.Cards?.Count ?? 0,
            ["stars"] = pcs?.Stars ?? 0,
            ["always_show_stars"] = player.Character?.ShouldAlwaysShowStarCounter ?? false,
            ["turn_number"] = pcs?.TurnNumber ?? 0,
            ["view"] = CombatView(),
        };

        // Character-specific mechanics
        try
        {
            // Defect: Orbs
            var orbQueue = pcs?.OrbQueue;
            if (orbQueue != null && (orbQueue.Orbs?.Count > 0 || orbQueue.Capacity > 0))
            {
                result["orbs"] = orbQueue.Orbs.Select((orb, i) => new Dictionary<string, object?>
                {
                    ["index"] = i,
                    ["id"] = orb.Id.Entry,
                    ["instance_id"] = Steps.IdOf(orb),
                    ["name"] = _loc.Bilingual("orbs", orb.Id.Entry + ".title"),
                    ["type"] = orb.GetType().Name.Replace("Orb", ""),
                    ["passive"] = (int)orb.PassiveVal,
                    ["evoke"] = (int)orb.EvokeVal,
                }).ToList();
                result["orb_slots"] = orbQueue.Capacity;
            }

            // Necrobinder: Osty (minion)
            var osty = player.Osty;
            if (osty != null)
            {
                result["osty"] = new Dictionary<string, object?>
                {
                    ["id"] = osty.Monster?.Id.Entry,
                    ["combat_id"] = osty.CombatId,
                    ["name"] = _loc.Monster(osty.Monster?.Id.Entry ?? "OSTY"),
                    ["hp"] = osty.CurrentHp,
                    ["max_hp"] = osty.MaxHp,
                    ["block"] = osty.Block,
                    ["alive"] = osty.IsAlive,
                };
            }
            else if (player.Character?.Id.Entry == "NECROBINDER")
            {
                result["osty"] = new Dictionary<string, object?> { ["alive"] = false };
            }
        }
        catch (Exception ex)
        {
            Log($"Character-specific data: {ex.Message}");
        }

        return result;
    }

    private Dictionary<string, object?> DetectPostCombatState(Player player, CombatRoom combatRoom)
    {
        Log($"Post-combat: RoomType={combatRoom.RoomType}, IsPreFinished={combatRoom.IsPreFinished}");
        Sched.Pump();

        // The room's rewards (the rewards screen) until the player proceeds
        if (_rewardList == null && !_rewardsProcessed)
        {
            _goldBeforeCombat = player.Gold;
            try
            {
                var rewardsSet = new RewardsSet(player).WithRewardsFromRoom(combatRoom);
                // build 23372702: GenerateWithoutOffering() now returns Task (void);
                // generated rewards live on rewardsSet.Rewards afterwards.
                Sched.Run(() => rewardsSet.GenerateWithoutOffering());
                Sched.Pump();
                ClearRewards();
                _rewardList = rewardsSet.Rewards.ToList();
                if (_rewardList.Count > 0)
                    return RewardsState();
                ClearRewards();
            }
            catch (Exception ex) { Log($"Generate rewards: {ex.Message}"); }
        }
        if (_rewardList != null)
            return RewardsState();

        // Rewards done — proceed
        _pendingCardReward = null;
        _rewardsProcessed = true;

        // Boss → next act, OR final victory after the last act's boss (#81). Act index is
        // 0-based and STS2 has 3 acts (0/1/2); killing the Act-3 (index 2) boss has no next
        // act — EnterNextAct NREs and DetectDecisionPoint falls through to an empty
        // map_select. Report victory directly in that case.
        if (combatRoom.RoomType == RoomType.Boss)
        {
            if (_runState != null && _runState.CurrentActIndex >= 2)
            {
                Log($"Final boss defeated (Act {_runState.CurrentActIndex + 1}), reporting victory");
                return GameOverState(true);
            }
            Log("Boss defeated, entering next act");
            try
            {
                Sched.Run(() => RunManager.Instance.EnterNextAct());
                Sched.Pump();
                WaitForActionExecutor();
                HealBetweenActs();
            }
            catch (Exception ex) { Log($"EnterNextAct: {ex.Message}"); }
            return DetectDecisionPoint();
        }

        // Normal → go to map
        ForceToMap();
        return MapSelectState();
    }

    private Dictionary<string, object?> CardRewardState(Player player, CombatRoom? combatRoom)
    {
        if (_pendingCardReward == null)
            return DetectPostCombatState(player, combatRoom ?? (_runState?.CurrentRoom as CombatRoom)!);

        var cards = _pendingCardReward.Cards.Select((c, i) =>
        {
            var stats = new Dictionary<string, object?>();
            try { foreach (var dv in c.DynamicVars.Values) stats[dv.Name.ToLowerInvariant()] = (int)dv.BaseValue; } catch { }
            var crkws = c.Keywords?.Where(k => k != CardKeyword.None).Select(k => k.ToString()).ToList();
            return WithCardView(c, PileType.None, new Dictionary<string, object?>
            {
                ["index"] = i,
                ["id"] = c.Id.ToString(),
                ["name"] = _loc.Card(c.Id.Entry),
                ["cost"] = c.EnergyCost?.GetResolved() ?? 0,
                ["type"] = c.Type.ToString(),
                ["rarity"] = c.Rarity.ToString(),
                ["description"] = _loc.Bilingual("cards", c.Id.Entry + ".description"),
                ["stats"] = stats.Count > 0 ? stats : null,
                ["keywords"] = crkws?.Count > 0 ? crkws : null,
                ["after_upgrade"] = GetUpgradedInfo(c),
            });
        }).ToList();

        return new Dictionary<string, object?>
        {
            ["type"] = "decision",
            ["decision"] = "card_reward",
            ["context"] = RunContext(),
            ["cards"] = cards,
            ["can_skip"] = _pendingCardReward.CanSkip,
            ["gold_earned"] = _runState!.Players[0].Gold - _goldBeforeCombat,
            ["player"] = PlayerSummary(_runState!.Players[0]),
        };
    }

    private void ForceToMap()
    {
        try
        {
            Sched.Run(() => RunManager.Instance.ProceedFromTerminalRewardsScreen());
            Sched.Pump();
        }
        catch { }

        if (_runState?.CurrentRoom is not MapRoom)
        {
            try { Sched.Run(() => RunManager.Instance.EnterRoom(new MapRoom())); Sched.Pump(); }
            catch (Exception ex) { Log($"ForceToMap: {ex.Message}"); }
        }
    }

    // Event rooms whose ancient has had its say (its dialogue goes with the
    // first page only).
    private readonly HashSet<EventRoom> _ancientDialogueShown = new HashSet<EventRoom>();

    private Dictionary<string, object?> EventChoiceState(EventRoom eventRoom)
    {
        var localEvent = RunManager.Instance.EventSynchronizer?.GetLocalEvent();
        Sched.Pump();

        // Reset the choice-tracking flag once we re-export the event state. Earlier this
        // block force-finished events whose option count was unchanged, but that incorrectly
        // killed legitimate loops like Slippery Bridge Hold On (#59). Rely on IsFinished
        // instead and let DetectDecisionPoint show the current options for the next choice.
        if (_eventOptionChosen) _eventOptionChosen = false;

        // If event is finished, proceed to map
        if (localEvent == null || localEvent.IsFinished)
        {
            Log($"Event {localEvent?.GetType().Name ?? "null"} finished, proceeding");
            try
            {
                Sched.Run(() => RunManager.Instance.ProceedFromTerminalRewardsScreen());
                Sched.Pump();
            }
            catch { }
            // Force to map if still in event room
            if (_runState?.CurrentRoom is EventRoom)
            {
                try { Sched.Run(() => RunManager.Instance.EnterRoom(new MapRoom())); Sched.Pump(); }
                catch { }
            }
            return _runState?.CurrentRoom is MapRoom ? MapSelectState() : DetectDecisionPoint();
        }

        var currentOptions = localEvent.CurrentOptions;
        if (currentOptions == null || currentOptions.Count == 0)
        {
            Log($"Event {localEvent.GetType().Name} has no options, auto-skipping");
            try { Sched.Run(() => RunManager.Instance.EnterRoom(new MapRoom())); Sched.Pump(); }
            catch { }
            return MapSelectState();
        }

        var options = currentOptions
            .Select((opt, i) =>
            {
                // Try to resolve title via loc tables
                string? title = null;
                if (opt.Title != null)
                {
                    var t = _loc.Bilingual(opt.Title.LocTable, opt.Title.LocEntryKey);
                    // Check if we actually found a translation (not just the key echoed back)
                    if (t != opt.Title.LocEntryKey)
                        title = t;
                }
                // Fallback: try to extract option ID from the key and look up as relic/card/potion
                if (title == null && opt.TextKey != null)
                {
                    // TextKey like "NEOW.pages.INITIAL.options.STONE_HUMIDIFIER" → extract "STONE_HUMIDIFIER"
                    var parts = opt.TextKey.Split('.');
                    var optionId = parts.Length > 0 ? parts[^1] : opt.TextKey;
                    // Try relic, then card, then just use the optionId
                    var relic = _loc.Relic(optionId);
                    if (relic != optionId + ".title")
                        title = relic;
                    else
                    {
                        var card = _loc.Card(optionId);
                        if (card != optionId + ".title")
                            title = card;
                        else
                            title = optionId.Replace("_", " ");
                    }
                }
                title ??= $"option_{i}";

                // Description: try loc table first
                string? optDesc = null;
                if (opt.Description != null && !string.IsNullOrEmpty(opt.Description.LocEntryKey))
                {
                    var d = _loc.Bilingual(opt.Description.LocTable, opt.Description.LocEntryKey);
                    if (d != opt.Description.LocEntryKey)
                        optDesc = d;
                }
                // Fallback: try relic/card description
                if (optDesc == null && opt.TextKey != null)
                {
                    var parts = opt.TextKey.Split('.');
                    var optionId = parts.Length > 0 ? parts[^1] : opt.TextKey;
                    var rd = _loc.Bilingual("relics", optionId + ".description");
                    if (rd != optionId + ".description")
                        optDesc = rd;
                }

                // Extract vars: try event's own DynamicVars first, then relic
                Dictionary<string, object?>? optVars = null;
                try
                {
                    // Event's DynamicVars (covers Gold, HpLoss, Heal, etc.)
                    if (localEvent.DynamicVars?.Values != null)
                    {
                        optVars = new Dictionary<string, object?>();
                        foreach (var dv in localEvent.DynamicVars.Values)
                            optVars[dv.Name] = (int)dv.BaseValue;
                    }
                }
                catch { }
                // Also try relic vars (for Neow options)
                if (opt.TextKey != null)
                {
                    try
                    {
                        var parts = opt.TextKey.Split('.');
                        var optionId = parts.Length > 0 ? parts[^1] : opt.TextKey;
                        var relicModel = ModelDb.GetById<RelicModel>(new ModelId("RELIC", optionId));
                        if (relicModel != null)
                        {
                            optVars ??= new Dictionary<string, object?>();
                            var mutable = relicModel.ToMutable();
                            foreach (var dv in mutable.DynamicVars.Values)
                                optVars[dv.Name] = (int)dv.BaseValue;
                        }
                    }
                    catch { }
                }

                // `RandomCard` (Slippery Bridge / Overcome) carries a *deck index*, but the
                // description template `{RandomCard}` should render that deck card's name. Resolve
                // it to the localized name so clients can substitute it (#58). Scoped to this var
                // name on purpose: other card vars (e.g. Wood Carvings' BirdCard/ToricCard) index
                // a transform-target pool, not the deck, so a generic rule would mis-resolve them.
                if (optVars != null && optVars.TryGetValue("RandomCard", out var rcVal) && rcVal is int rcIdx)
                {
                    try
                    {
                        var deck = _runState?.Players?[0]?.Deck?.Cards;
                        if (deck != null && rcIdx >= 0 && rcIdx < deck.Count && deck[rcIdx] != null)
                            optVars["RandomCard"] = _loc.Card(deck[rcIdx].Id.Entry);
                    }
                    catch { }
                }

                // The button's text as NEventOptionButton._Ready writes it:
                // the event's vars in, title and description formatted.
                string? textBbcode = null;
                try
                {
                    if (opt.Description != null) localEvent.DynamicVars.AddTo(opt.Description);
                    if (opt.Title != null) localEvent.DynamicVars.AddTo(opt.Title);
                    var ft = opt.Title?.GetFormattedText();
                    var fd = opt.Description?.GetFormattedText() ?? "";
                    var tag = opt.IsLocked ? "red" : "gold";
                    textBbcode = string.IsNullOrEmpty(ft) ? fd : $"[{tag}][b]{ft}[/b][/{tag}]\n{fd}";
                }
                catch (Exception ex) { Log($"event option text: {ex.Message}"); }
                bool willKill = false;
                try { willKill = localEvent.Owner != null && (opt.WillKillPlayer?.Invoke(localEvent.Owner) ?? false); }
                catch { }

                return new Dictionary<string, object?>
                {
                    ["index"] = i,
                    ["title"] = title,
                    ["description"] = optDesc,
                    ["text_key"] = opt.TextKey,
                    ["is_locked"] = opt.IsLocked,
                    ["vars"] = optVars?.Count > 0 ? optVars : null,
                    ["text_bbcode"] = textBbcode,
                    ["relic"] = opt.Relic?.Id.Entry.ToLowerInvariant(),
                    ["will_kill"] = willKill,
                };
            }).ToList();

        // Resolve event name — try ancients table first (for Neow), then events
        var eventEntry = localEvent.Id?.Entry ?? localEvent.GetType().Name.ToUpperInvariant();
        var eventName = _loc.Bilingual("ancients", eventEntry + ".title");
        if (eventName == eventEntry + ".title")
            eventName = _loc.Event(eventEntry);

        // Resolve event description, suppress if key not found
        string? eventDesc = null;
        if (localEvent.Description != null)
        {
            var d = _loc.Bilingual(localEvent.Description.LocTable, localEvent.Description.LocEntryKey);
            if (d != localEvent.Description.LocEntryKey)
                eventDesc = d;
        }

        // What NEventRoom shows: the layout, the portrait's id, the title and
        // the description formatted as SetDescription does; for an ancient,
        // its banner and dialogue.
        static float[] Rgba(Godot.Color col) => new[] { col.R, col.G, col.B, col.A };
        string? titleText = null, descBbcode = null;
        try
        {
            titleText = localEvent.Title.GetFormattedText();
            var desc = localEvent.Description;
            if (desc != null && desc.Exists())
            {
                localEvent.Owner?.Character.AddDetailsTo(desc);
                desc.Add("IsMultiplayer", false);
                localEvent.DynamicVars.AddTo(desc);
                descBbcode = desc.GetFormattedText();
            }
        }
        catch (Exception ex) { Log($"event text: {ex.Message}"); }
        Dictionary<string, object?>? ancient = null;
        if (localEvent is AncientEventModel ancientModel)
        {
            ancient = new Dictionary<string, object?>();
            try
            {
                ancient["title"] = ancientModel.Title.GetFormattedText();
                ancient["epithet"] = ancientModel.Epithet.GetFormattedText();
                ancient["dialogue_color"] = Rgba(ancientModel.DialogueColor);
                var character = ancientModel.Owner?.Character ?? _runState!.Players[0].Character;
                ancient["character_dialogue_color"] = Rgba(character.DialogueColor);
                // A first visit's dialogue (the game picks one of the valid ones
                // with the chaotic rng and the player's visit counts); the
                // ancient talks only while its first page is open.
                if (!_ancientDialogueShown.Contains(eventRoom))
                {
                    var lines = new List<Dictionary<string, object?>>();
                    var dialogue = ancientModel.DialogueSet.GetValidDialogues(character.Id, 0, 0,
                        !ancientModel.AnyCharacterDialogueBlacklist.Contains(character)).FirstOrDefault();
                    if (dialogue != null)
                        foreach (var line in dialogue.Lines)
                        {
                            string? text = null;
                            if (line.LineText != null)
                            {
                                line.LineText.Add("Act1Name", _runState!.Acts[0].Title);
                                character.AddDetailsTo(line.LineText);
                                ancientModel.DynamicVars.AddTo(line.LineText);
                                text = line.LineText.GetFormattedText();
                            }
                            lines.Add(new Dictionary<string, object?>
                            {
                                ["speaker"] = line.Speaker.ToString(),
                                ["text"] = text,
                                ["next"] = line.NextButtonText?.GetFormattedText(),
                            });
                        }
                    ancient["dialogue"] = lines;
                    _ancientDialogueShown.Add(eventRoom);
                }
            }
            catch (Exception ex) { Log($"ancient: {ex.Message}"); }
        }

        return new Dictionary<string, object?>
        {
            ["type"] = "decision",
            ["decision"] = "event_choice",
            ["context"] = RunContext(),
            ["event_name"] = eventName,
            ["description"] = eventDesc,
            ["options"] = options,
            ["player"] = PlayerSummary(_runState!.Players[0]),
            ["event_id"] = localEvent.Id?.Entry.ToLowerInvariant(),
            ["layout"] = localEvent.LayoutType.ToString(),
            ["button_color"] = Rgba(localEvent.ButtonColor),
            ["title_text"] = titleText,
            ["description_bbcode"] = descBbcode,
            ["ancient"] = ancient,
        };
    }

    private Dictionary<string, object?> RestSiteState(RestSiteRoom restRoom)
    {
        var options = restRoom.Options;
        var player = _runState!.Players[0];
        string? background = null;
        try { background = _runState.Act.RestSiteBackgroundPath; } catch { }

        if (_restSiteDone || options == null || options.Count == 0)
        {
            // The choice is made (the synchronizer cleared the options): the
            // room stays with its proceed button (NRestSiteRoom.ShowProceedButton);
            // leave_room goes to the map.
            Log("Rest site: option done, waiting to proceed");
            _restSiteDone = true;
            return new Dictionary<string, object?>
            {
                ["type"] = "decision",
                ["decision"] = "rest_site",
                ["context"] = RunContext(),
                ["options"] = new List<Dictionary<string, object?>>(),
                ["done"] = true,
                ["background"] = background,
                ["player"] = PlayerSummary(player),
            };
        }

        var optionList = options.Select((opt, i) => new Dictionary<string, object?>
        {
            ["index"] = i,
            ["option_id"] = opt.OptionId,
            ["name"] = opt.GetType().Name,
            ["is_enabled"] = opt.IsEnabled,
            ["title"] = SafeText(() => opt.Title.GetFormattedText()),
            ["description"] = SafeText(() => opt.Description.GetFormattedText()),
        }).ToList();

        return new Dictionary<string, object?>
        {
            ["type"] = "decision",
            ["decision"] = "rest_site",
            ["context"] = RunContext(),
            ["options"] = optionList,
            ["background"] = background,
            ["player"] = PlayerSummary(player),
        };
    }

    private Dictionary<string, object?> ShopState(MerchantRoom merchantRoom, Player player)
    {
        var inv = merchantRoom.GetLocalInventory();
        if (inv == null) { ForceToMap(); return MapSelectState(); }

        int characterCards = inv.CharacterCardEntries.Count;
        var cards = inv.CharacterCardEntries.Concat(inv.ColorlessCardEntries)
            .Select((e, i) =>
            {
                var card = e.CreationResult?.Card;
                var entry = card?.Id.Entry ?? "?";
                var stats = new Dictionary<string, object?>();
                int cardCost = 0;
                try
                {
                    if (card != null)
                    {
                        cardCost = card.EnergyCost?.GetResolved() ?? 0;
                        // The shop entry's card can have uninitialized DynamicVars (stats: null
                        // while after_upgrade is populated, #68). Read base stats from a fresh
                        // ModelDb clone at the card's current upgrade level, like GetUpgradedInfo.
                        var fresh = ModelDb.GetById<CardModel>(card.Id).ToMutable();
                        for (int u = 0; u < card.CurrentUpgradeLevel; u++)
                        {
                            fresh.UpgradeInternal();
                            fresh.FinalizeUpgradeInternal();
                        }
                        foreach (var dv in fresh.DynamicVars.Values)
                            stats[dv.Name.ToLowerInvariant()] = (int)dv.BaseValue;
                    }
                }
                catch { }
                var shopkws = card?.Keywords?.Where(k => k != CardKeyword.None).Select(k => k.ToString()).ToList();
                return WithCardView(card, PileType.None, new Dictionary<string, object?>
                {
                    ["index"] = i,
                    ["name"] = _loc.Card(entry),
                    ["type"] = card?.Type.ToString() ?? "?",
                    ["rarity"] = card?.Rarity.ToString() ?? "?",
                    ["card_cost"] = cardCost,
                    ["description"] = _loc.Bilingual("cards", entry + ".description"),
                    ["stats"] = stats.Count > 0 ? stats : null,
                    ["keywords"] = shopkws?.Count > 0 ? shopkws : null,
                    ["after_upgrade"] = card != null ? GetUpgradedInfo(card) : null,
                    ["cost"] = e.Cost,
                    ["is_stocked"] = e.IsStocked,
                    ["on_sale"] = e.IsOnSale,
                    ["colorless"] = i >= characterCards,
                });
            }).ToList();

        var relics = inv.RelicEntries.Select((e, i) => new Dictionary<string, object?>
        {
            ["index"] = i,
            ["id"] = e.Model?.Id.Entry,
            ["name"] = _loc.Relic(e.Model?.Id.Entry ?? "?"),
            ["description"] = _loc.Bilingual("relics", (e.Model?.Id.Entry ?? "?") + ".description"),
            ["cost"] = e.Cost,
            ["is_stocked"] = e.IsStocked,
        }).ToList();

        var potions = inv.PotionEntries.Select((e, i) => new Dictionary<string, object?>
        {
            ["index"] = i,
            ["id"] = e.Model?.Id.Entry,
            ["name"] = _loc.Potion(e.Model?.Id.Entry ?? "?"),
            ["description"] = _loc.Bilingual("potions", (e.Model?.Id.Entry ?? "?") + ".description"),
            ["cost"] = e.Cost,
            ["is_stocked"] = e.IsStocked,
        }).ToList();

        var removal = merchantRoom.GetLocalInventory().CardRemovalEntry;

        return new Dictionary<string, object?>
        {
            ["type"] = "decision",
            ["decision"] = "shop",
            ["context"] = RunContext(),
            ["cards"] = cards,
            ["relics"] = relics,
            ["potions"] = potions,
            ["card_removal_cost"] = removal?.Cost,
            ["card_removal_used"] = removal?.Used ?? true,
            ["player"] = PlayerSummary(player),
        };
    }

    private Dictionary<string, object?> GameOverState(bool isVictory)
    {
        var player = _runState!.Players[0];
        var summary = PlayerSummary(player);
        // BUG-005: When player died, the engine resets HP to max. Use last known HP instead.
        if (!isVictory)
            summary["hp"] = _lastKnownHp > 0 ? 0 : (player.Creature?.CurrentHp ?? 0);
        var d = new Dictionary<string, object?>
        {
            ["type"] = "decision",
            ["decision"] = "game_over",
            ["context"] = RunContext(),
            ["victory"] = isVictory,
            ["player"] = summary,
            ["act"] = _runState.CurrentActIndex + 1,
            ["floor"] = _runState.ActFloor,
        };
        AddGameOverDetails(d, isVictory);
        return d;
    }

    #endregion

    #region Helpers

    private void WaitForActionExecutor()
    {
        // Pump until the executor is idle, a decision is pending, or nothing more can
        // run on the queue (the outer loop's Sched.Settle then lets thread-pool work
        // and timers run, and the decision point is recomputed).
        try
        {
            for (int i = 0; i < 10_000; i++)
            {
                int ran = Sched.Pump();
                if (_cardSelector.HasPending || _cardSelector.HasPendingReward) return;
                if (!RunManager.Instance.ActionExecutor.IsRunning) return;
                if (ran == 0) return;
            }
            Log("WaitForActionExecutor: executor still running after 10000 pumps");
        }
        catch (Exception ex)
        {
            Log($"WaitForActionExecutor exception: {ex.Message}");
        }
    }

    private void SpinWaitForCombatStable()
    {
        int maxIterations = 200;
        for (int i = 0; i < maxIterations; i++)
        {
            Sched.Pump();
            if (!CombatManager.Instance.IsInProgress) return;
            if (IsPlayPhase()) return;
            WaitForActionExecutor();
            if (IsPlayPhase() || !CombatManager.Instance.IsInProgress) return;
            Sched.Pump();
        }
    }

    /// <summary>Compute what a card would look like after upgrading (stats + cost + description).</summary>
    // NUpgradePreview's right-hand card: an upgraded copy shown with
    // NCard.ShowUpgradePreview (the changed values in green). The copy comes
    // from the canonical card, outside any card scope, as GetUpgradedInfo's.
    private static Dictionary<string, object?>? UpgradePreviewView(CardModel card)
    {
        if (!card.IsUpgradable) return null;
        try
        {
            var clone = ModelDb.GetById<CardModel>(card.Id).ToMutable();
            for (int i = 0; i < card.CurrentUpgradeLevel; i++)
            {
                clone.UpgradeInternal();
                clone.FinalizeUpgradeInternal();
            }
            clone.UpgradeInternal();
            clone.UpgradePreviewType = card.Pile?.IsCombatPile == true
                ? MegaCrit.Sts2.Core.Entities.Cards.CardUpgradePreviewType.Combat
                : MegaCrit.Sts2.Core.Entities.Cards.CardUpgradePreviewType.Deck;
            var d = new Dictionary<string, object?>
            {
                ["type"] = clone.Type.ToString(),
                ["rarity"] = clone.Rarity.ToString(),
            };
            AddCardView(d, clone, PileType.None);
            clone.DynamicVars.ClearPreview();
            clone.UpdateDynamicVarPreview(MegaCrit.Sts2.Core.Entities.Cards.CardPreviewMode.Upgrade, null, clone.DynamicVars);
            d["description_bbcode"] = clone.GetDescriptionForUpgradePreview();
            return d;
        }
        catch (Exception ex)
        {
            Spire.Logic.Log.Warn($"upgrade preview {card.Id}: {ex.Message}");
            return null;
        }
    }

    // NPotionPopup: whether Use and Discard are enabled, and Use's label
    // (drink, or throw for potions aimed at a creature).
    private static void AddPotionUse(Dictionary<string, object?> d, PotionModel p, Player player)
    {
        try
        {
            bool can = player.CanUseOrRemovePotions && !p.IsQueued && !player.Creature.IsDead;
            bool usable = can && p.PassesCustomUsabilityCheck && p.Usage switch
            {
                MegaCrit.Sts2.Core.Entities.Potions.PotionUsage.AnyTime => true,
                MegaCrit.Sts2.Core.Entities.Potions.PotionUsage.CombatOnly => CombatManager.Instance.IsInProgress
                    && player.PlayerCombatState?.Phase == MegaCrit.Sts2.Core.Combat.PlayerTurnPhase.Play,
                _ => false,
            };
            var t = p.TargetType;
            bool aimed = t == TargetType.AnyEnemy || t == TargetType.AnyAlly || t == TargetType.TargetedNoCreature;
            d["usage"] = p.Usage.ToString();
            d["usable"] = usable;
            d["discardable"] = can;
            d["use_label"] = aimed || p.CanThrowAtAlly() ? "throw" : "drink";
        }
        catch (Exception ex) { Spire.Logic.Log.Warn($"potion use {p.Id}: {ex.Message}"); }
    }

    private Dictionary<string, object?>? GetUpgradedInfo(CardModel card)
    {
        if (!card.IsUpgradable) return null;
        try
        {
            var clone = ModelDb.GetById<CardModel>(card.Id).ToMutable();
            // Apply existing upgrades first
            for (int i = 0; i < card.CurrentUpgradeLevel; i++)
            {
                clone.UpgradeInternal();
                clone.FinalizeUpgradeInternal();
            }
            // Apply one more upgrade
            clone.UpgradeInternal();
            clone.FinalizeUpgradeInternal();

            var stats = new Dictionary<string, object?>();
            try { foreach (var dv in clone.DynamicVars.Values) stats[dv.Name.ToLowerInvariant()] = (int)dv.BaseValue; } catch { }

            // Compare keywords before/after upgrade
            var oldKws = card.Keywords?.Where(k => k != CardKeyword.None).Select(k => k.ToString()).ToHashSet() ?? new();
            var newKws = clone.Keywords?.Where(k => k != CardKeyword.None).Select(k => k.ToString()).ToHashSet() ?? new();
            var addedKws = newKws.Except(oldKws).ToList();
            var removedKws = oldKws.Except(newKws).ToList();

            return new Dictionary<string, object?>
            {
                ["cost"] = clone.EnergyCost?.GetResolved() ?? 0,
                ["stats"] = stats.Count > 0 ? stats : null,
                ["description"] = _loc.Bilingual("cards", card.Id.Entry + ".description"),
                ["added_keywords"] = addedKws.Count > 0 ? addedKws : null,
                ["removed_keywords"] = removedKws.Count > 0 ? removedKws : null,
            };
        }
        catch { return null; }
    }

    private Dictionary<string, object?> PlayerSummary(Player player)
    {
        return new Dictionary<string, object?>
        {
            ["character"] = player.Character?.Id.Entry,
            ["combat_id"] = player.Creature?.CombatId,
            ["name"] = _loc.Bilingual("characters", (player.Character?.Id.Entry ?? "IRONCLAD") + ".title"),
            ["hp"] = player.Creature?.CurrentHp ?? 0,
            ["max_hp"] = player.Creature?.MaxHp ?? 0,
            ["block"] = player.Creature?.Block ?? 0,
            ["gold"] = player.Gold,
            ["relics"] = player.Relics?.Select(r =>
            {
                var vars = new Dictionary<string, object?>();
                try { foreach (var dv in r.DynamicVars.Values) vars[dv.Name] = (int)dv.BaseValue; } catch { }
                return new Dictionary<string, object?>
                {
                    ["id"] = r.Id.Entry,
                    ["name"] = _loc.Relic(r.Id.Entry),
                    ["description"] = _loc.Bilingual("relics", r.Id.Entry + ".description"),
                    ["vars"] = vars.Count > 0 ? vars : null,
                    ["tips"] = Tips(() => r.HoverTips),
                };
            }).ToList(),
            ["potions"] = player.Potions?.Select((p, i) =>
            {
                if (p == null) return null;
                var pvars = new Dictionary<string, object?>();
                try { foreach (var dv in p.DynamicVars.Values) pvars[dv.Name] = (int)dv.BaseValue; } catch { }
                var pd = new Dictionary<string, object?>
                {
                    ["index"] = i,
                    ["id"] = p.Id.Entry,
                    ["name"] = _loc.Potion(p.Id.Entry),
                    ["description"] = _loc.Bilingual("potions", p.Id.Entry + ".description"),
                    ["vars"] = pvars.Count > 0 ? pvars : null,
                    ["target_type"] = p.TargetType.ToString(),
                };
                AddPotionUse(pd, p, player);
                return pd;
            }).Where(x => x != null).ToList(),
            ["max_potions"] = player.MaxPotionCount,
            ["ascension"] = _runState?.AscensionLevel ?? 0,
            ["deck_size"] = player.Deck?.Cards?.Count(c => c != null) ?? 0,
            ["deck"] = player.Deck?.Cards?.Where(c => c != null).Select(c =>
            {
                var dstats = new Dictionary<string, object?>();
                try { foreach (var dv in c.DynamicVars.Values) dstats[dv.Name.ToLowerInvariant()] = (int)dv.BaseValue; } catch { }
                var dkws = c.Keywords?.Where(k => k != CardKeyword.None).Select(k => k.ToString()).ToList();
                var dcard = new Dictionary<string, object?>
                {
                    ["id"] = c.Id.ToString(),
                    ["name"] = _loc.Card(c.Id.Entry),
                    ["cost"] = c.EnergyCost?.GetResolved() ?? 0,
                    ["type"] = c.Type.ToString(),
                    ["upgraded"] = c.IsUpgraded,
                    ["description"] = _loc.Bilingual("cards", c.Id.Entry + ".description"),
                    ["stats"] = dstats.Count > 0 ? dstats : null,
                    ["keywords"] = dkws?.Count > 0 ? dkws : null,
                    ["after_upgrade"] = GetUpgradedInfo(c),
                };
                // Enchantment/affliction metadata, matching the combat hand export so clients
                // can see e.g. Slither applied to a deck card after an event (#76).
                if (c.Enchantment != null)
                {
                    dcard["enchantment"] = _loc.Bilingual("enchantments", c.Enchantment.Id.Entry + ".title");
                    try { if (c.Enchantment.Amount != 0) dcard["enchantment_amount"] = c.Enchantment.Amount; } catch { }
                }
                if (c.Affliction != null)
                {
                    dcard["affliction"] = _loc.Bilingual("afflictions", c.Affliction.Id.Entry + ".title");
                    try { if (c.Affliction.Amount != 0) dcard["affliction_amount"] = c.Affliction.Amount; } catch { }
                }
                return dcard;
            }).ToList(),
        };
    }

    /// <summary>Common context added to every decision point.</summary>
    private Dictionary<string, object?> RunContext()
    {
        if (_runState == null) return new();
        var ctx = new Dictionary<string, object?>
        {
            ["act"] = _runState.CurrentActIndex + 1,
            ["act_name"] = _loc.Act(_runState.Act?.Id.Entry ?? "OVERGROWTH"),
            ["floor"] = _runState.ActFloor,
            ["room_type"] = _runState.CurrentRoom?.RoomType.ToString(),
        };

        // Boss encounter info — use BossEncounter?.Id?.Entry
        try
        {
            var bossIdEntry = _runState.Act?.BossEncounter?.Id?.Entry;
            if (!string.IsNullOrEmpty(bossIdEntry))
            {
                var monsterKey = bossIdEntry.EndsWith("_BOSS") ? bossIdEntry[..^5] : bossIdEntry;
                // Handle special mappings
                if (monsterKey == "THE_KIN") monsterKey = "KIN_PRIEST";
                ctx["boss"] = new Dictionary<string, object?>
                {
                    ["id"] = bossIdEntry,
                    ["name"] = _loc.Monster(monsterKey),
                };
            }
        }
        catch { }

        // NTopBar's room, floor and boss icons (NTopBarRoomIcon,
        // NTopBarFloorIcon, NTopBarBossIcon).
        try
        {
            var act = _runState.Act;
            var point = _runState.CurrentMapPoint;
            ctx["point_type"] = point?.PointType.ToString();
            ctx["total_floor"] = _runState.TotalFloor;
            ctx["ancient"] = act?.Ancient?.Id.Entry;
            ctx["second_boss"] = act?.SecondBossEncounter?.Id.Entry;
            ctx["at_second_boss"] = point != null && point == _runState.Map?.SecondBossMapPoint;
            ctx["at_boss"] = point != null && point == _runState.Map?.BossMapPoint;
        }
        catch { }

        // NRunMusicController: ResolveMusic uses a separate seeded bg_music
        // stream, so presentation selection cannot consume the run's RNG.
        if (_runState.Act is { } musicAct)
        {
            var selection = MegaCrit.Sts2.Core.Nodes.Audio.NRunMusicController.ResolveMusic(
                null, musicAct.BgMusicOptions, musicAct.MusicBankPaths, _runState.Rng.Seed);
            var encounter = (_runState.CurrentRoom as CombatRoom)?.Encounter;
            var roomType = _runState.CurrentRoom?.RoomType;
            bool combat = roomType is RoomType.Monster or RoomType.Elite or RoomType.Boss;
            int progress = combat && !CombatManager.Instance.IsInProgress ? 7 : roomType switch
            {
                RoomType.Shop => 2, RoomType.RestSite => 3, RoomType.Treasure => 5,
                RoomType.Monster => 1, RoomType.Elite or RoomType.Boss => 6,
                RoomType.Event when (_runState.CurrentRoom as EventRoom)?.CanonicalEvent is AncientEventModel => 0,
                RoomType.Event => 4, _ => 0,
            };
            ctx["audio"] = new Dictionary<string, object?>
            {
                ["track"] = selection?.Track, ["bank"] = selection?.BankPath,
                ["ambience"] = encounter?.HasAmbientSfx == true ? encounter.AmbientSfx : musicAct.AmbientSfx,
                ["custom"] = CombatManager.Instance.IsInProgress && encounter?.HasBgm == true ? encounter.CustomBgm : null,
                ["progress"] = progress,
            };
        }

        return ctx;
    }

    // CardModel.GetResultPileTypeForCardPlay() is protected/virtual; cache the reflective handle.
    private static System.Reflection.MethodInfo? _getResultPileMethod;

    /// <summary>
    /// True if this card is designed to return itself to the player's Hand after being played
    /// (PileType.Hand == 2), e.g. Particle Wall ("Gain Block. Return this card to your Hand.").
    /// Such cards legitimately remain in hand post-play, so the "still in hand" no-effect guard
    /// must not treat them as failures.
    /// </summary>
    private static bool ReturnsToHandOnPlay(CardModel card)
    {
        try
        {
            _getResultPileMethod ??= typeof(CardModel).GetMethod(
                "GetResultPileTypeForCardPlay",
                BindingFlags.Instance | BindingFlags.NonPublic | BindingFlags.Public);
            var pile = _getResultPileMethod?.Invoke(card, null);  // virtual → dispatches to override
            return pile != null && Convert.ToInt32(pile) == 2;    // PileType.Hand
        }
        catch
        {
            return false;
        }
    }

    /// <summary>
    /// Mark ModManager as finished (with no mods) so ReflectionHelper.ModTypes is usable in
    /// headless mode. v0.107.1 throws from get_ModTypes() until ModManager.State != None.
    /// We set State = Skipped (the "ran, declined to load mods" value) and seed an empty
    /// _modTypes array via reflection, avoiding the real Initialize(fileIo, settings, version)
    /// path which needs Steam/filesystem mod discovery.
    /// </summary>
    private static void EnsureModManagerInitialized()
    {
        const BindingFlags SF = BindingFlags.Static | BindingFlags.NonPublic | BindingFlags.Public;
        try
        {
            var modMgr = typeof(MegaCrit.Sts2.Core.Modding.ModManager);

            // Allow the test/headless init path where the game checks for it.
            modMgr.GetField("_allowInitForTests", SF)?.SetValue(null, true);

            // Ensure an (empty) mod list so GetLoadedMods()/iteration never NREs.
            var modsField = modMgr.GetField("_mods", SF);
            if (modsField != null && modsField.GetValue(null) == null)
                modsField.SetValue(null, Activator.CreateInstance(modsField.FieldType));

            // Flip State None → Skipped via the auto-property backing field.
            var stateField = modMgr.GetField("<State>k__BackingField", SF);
            if (stateField != null)
                stateField.SetValue(null, Enum.Parse(stateField.FieldType, "Skipped"));

            // Seed ReflectionHelper._modTypes so get_ModTypes() returns instead of throwing.
            var reflHelper = typeof(MegaCrit.Sts2.Core.Helpers.ReflectionHelper);
            var modTypesField = reflHelper.GetField("_modTypes", SF);
            if (modTypesField != null && modTypesField.GetValue(null) == null)
                modTypesField.SetValue(null, Array.Empty<Type>());

            Console.Error.WriteLine("[INFO] ModManager marked initialized (headless, no mods)");
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"[WARN] EnsureModManagerInitialized: {ex.Message}");
        }
    }

    private static void EnsureModelDbInitialized()
    {
        if (_modelDbInitialized) return;
        _modelDbInitialized = true;

        TestMode.IsOn = true;

        // v0.107.1 gates ReflectionHelper.ModTypes behind ModManager initialization:
        // RunState.CreateForTest → BadgeModels → GetSubtypesInMods throws
        // "ModManager is not finished initializing!" unless ModManager has run.
        // Headless never loads mods, so short-circuit the gate reflectively.
        EnsureModManagerInitialized();

        // Initialize PlatformServices before anything touches PlatformUtil
        try
        {
            // Try to access PlatformUtil to trigger its static init
            // If it fails, it won't be available but most code checks SteamInitializer.Initialized
            var _ = MegaCrit.Sts2.Core.Platform.PlatformUtil.PrimaryPlatform;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"[WARN] PlatformUtil init: {ex.Message}");
        }

        // Saves go to <data>/user through our own ISaveStore (the game's
        // GodotFileIo needs Godot's FileAccess). forceSynchronous: one thread.
        try
        {
            var store = new FileStore(Path.Combine(DataDir.Root, "user"));
            SaveManager.MockInstanceForTesting(new SaveManager(store, forceSynchronous: true));
            Log($"Saves in {store.Root}");
        }
        catch (Exception ex) { Console.Error.WriteLine($"[WARN] SaveManager store: {ex.Message}"); }

        // Initialize SaveManager with a dummy profile for save/load support
        try { SaveManager.Instance.InitProfileId(0); }
        catch (Exception ex) { Console.Error.WriteLine($"[WARN] SaveManager.InitProfileId: {(Spire.Logic.Log.Verbose ? ex.ToString() : ex.Message)}"); }

        // Initialize PrefsSave (FastMode etc.). build 23372702 reads PrefsSave.FastMode
        // from many gameplay paths (e.g. Slice.OnPlay anim delay); without this the
        // PrefsSave getter returns null and those paths NRE.
        try { SaveManager.Instance.InitPrefsDataForTest(); }
        catch (Exception ex) { Console.Error.WriteLine($"[WARN] SaveManager.InitPrefsDataForTest: {ex.Message}"); }

        // Initialize progress data for epoch/timeline tracking
        try { SaveManager.Instance.InitProgressData(); }
        catch (Exception ex) { Console.Error.WriteLine($"[WARN] InitProgressData: {ex.Message}"); }

        // Cmd.Wait, TalkCmd.Play and the other sts2-cli Harmony patches are now
        // static IL patches (logic/patches.json); their handlers live in Hooks.cs
        // and are registered by Program before any game code runs.



        // Initialize localization system (needed for events, cards, etc.)
        LocSetup.InitLocManager(_loc);

        var subtypes = MegaCrit.Sts2.Core.Models.AbstractModelSubtypes.All;
        int registered = 0, failed = 0;
        for (int i = 0; i < subtypes.Count; i++)
        {
            try
            {
                ModelDb.Inject(subtypes[i]);
                registered++;
            }
            catch (Exception ex)
            {
                failed++;
                // Only log first few failures to reduce noise
                if (failed <= 5)
                    Console.Error.WriteLine($"[WARN] Failed to register {subtypes[i].Name}: {ex.GetType().Name}: {ex.Message}");
            }
        }
        Console.Error.WriteLine($"[INFO] ModelDb: {registered} registered, {failed} failed out of {subtypes.Count}");

        // v0.111.0: ContentSorter (used by the serialization cache) requires
        // AssemblyInfo, which the game initializes during its normal boot.
        try { MegaCrit.Sts2.Core.Modding.AssemblyInfo.Init(); }
        catch (Exception ex) { Console.Error.WriteLine($"[WARN] AssemblyInfo.Init: {ex.Message}"); }

        // v0.111.0: progress data resolves character models, so load it again
        // now that ModelDb is populated (the earlier call fails on a fresh profile).
        try { SaveManager.Instance.InitProgressData(); }
        catch (Exception ex) { Console.Error.WriteLine($"[WARN] InitProgressData (after ModelDb): {(Spire.Logic.Log.Verbose ? ex.ToString() : ex.Message)}"); }

        // Initialize net ID serialization cache (needed for combat actions)
        try
        {
            ModelIdSerializationCache.Init();
            Console.Error.WriteLine("[INFO] ModelIdSerializationCache initialized");
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"[WARN] ModelIdSerializationCache.Init: {ex.Message}");
        }
    }

    private Player? CreatePlayer(string characterName)
    {
        return characterName.ToLowerInvariant() switch
        {
            "ironclad" => Player.CreateForNewRun<Ironclad>(UnlockState.all, 1uL),
            "silent" => Player.CreateForNewRun<Silent>(UnlockState.all, 1uL),
            "defect" => Player.CreateForNewRun<Defect>(UnlockState.all, 1uL),
            "regent" => Player.CreateForNewRun<Regent>(UnlockState.all, 1uL),
            "necrobinder" => Player.CreateForNewRun<Necrobinder>(UnlockState.all, 1uL),
            _ => null
        };
    }

    private static void Log(string message)
    {
        Console.Error.WriteLine($"[SIM] {message}");
    }

    private static Dictionary<string, object?> Error(string message) =>
        new() { ["type"] = "error", ["message"] = message };

    private static Dictionary<string, object?> ErrorWithTrace(string context, Exception ex)
    {
        var inner = ex;
        while (inner.InnerException != null) inner = inner.InnerException;
        return new Dictionary<string, object?>
        {
            ["type"] = "error",
            ["message"] = $"{context}: {inner.GetType().Name}: {inner.Message}",
            ["stack_trace"] = inner.StackTrace,
        };
    }

    public Dictionary<string, object?> GetFullMap()
    {
        if (_runState?.Map == null)
            return Error("No map available");

        var map = _runState.Map;
        var rows = new List<List<Dictionary<string, object?>>>();
        var currentCoord = _runState.CurrentMapCoord;
        var visited = _runState.VisitedMapCoords;

        for (int row = 0; row < map.GetRowCount(); row++)
        {
            var rowNodes = new List<Dictionary<string, object?>>();
            foreach (var point in map.GetPointsInRow(row))
            {
                if (point == null) continue;
                var children = point.Children?.Select(ch => new Dictionary<string, object?>
                {
                    ["col"] = (int)ch.coord.col,
                    ["row"] = (int)ch.coord.row,
                }).ToList();

                var isVisited = visited?.Any(v => v.col == point.coord.col && v.row == point.coord.row) ?? false;
                var isCurrent = currentCoord.HasValue &&
                    currentCoord.Value.col == point.coord.col && currentCoord.Value.row == point.coord.row;

                rowNodes.Add(new Dictionary<string, object?>
                {
                    ["col"] = (int)point.coord.col,
                    ["row"] = (int)point.coord.row,
                    ["type"] = point.PointType.ToString(),
                    ["children"] = children,
                    ["visited"] = isVisited,
                    ["current"] = isCurrent,
                });
            }
            if (rowNodes.Count > 0)
                rows.Add(rowNodes);
        }

        // Boss node
        var bossNode = new Dictionary<string, object?>
        {
            ["col"] = (int)map.BossMapPoint.coord.col,
            ["row"] = (int)map.BossMapPoint.coord.row,
            ["type"] = map.BossMapPoint.PointType.ToString(),
        };

        // Add boss name/id — use BossEncounter?.Id?.Entry
        try
        {
            var bossIdEntry = _runState.Act?.BossEncounter?.Id?.Entry;
            if (!string.IsNullOrEmpty(bossIdEntry))
            {
                var monsterKey = bossIdEntry.EndsWith("_BOSS") ? bossIdEntry[..^5] : bossIdEntry;
                if (monsterKey == "THE_KIN") monsterKey = "KIN_PRIEST";
                bossNode["id"] = bossIdEntry;
                bossNode["name"] = _loc.Monster(monsterKey);
            }
        }
        catch { }

        return new Dictionary<string, object?>
        {
            ["type"] = "map",
            ["context"] = RunContext(),
            ["rows"] = rows,
            ["boss"] = bossNode,
            ["current_coord"] = currentCoord.HasValue ? new Dictionary<string, object?>
            {
                ["col"] = (int)currentCoord.Value.col,
                ["row"] = (int)currentCoord.Value.row,
            } : null,
        };
    }

    public void CleanUp()
    {
        try
        {
            if (RunManager.Instance.IsInProgress)
                RunManager.Instance.CleanUp(graceful: true);
            _runState = null;
        }
        catch (Exception ex)
        {
            Log($"CleanUp exception: {ex.Message}");
        }
    }

    #endregion
}

internal static class CreatureViewExtensions
{
    // NCreatureStateDisplay / NHealthBar inputs beyond hp and block: the
    // HP display mode, the poison preview, the monster's bar width and
    // visibility.
    public static Dictionary<string, object?> WithCreatureView(this Dictionary<string, object?> d, Creature c)
    {
        try
        {
            d["hp_display"] = c.HpDisplay.ToString();
            d["poison_next_turn"] = c.GetPower<PoisonPower>()?.CalculateTotalDamageNextTurn() ?? 0;
            if (c.Monster != null)
            {
                d["hp_bar_reduction"] = c.Monster.HpBarSizeReduction;
                d["health_bar_visible"] = c.Monster.IsHealthBarVisible;
            }
        }
        catch (Exception ex) { Console.Error.WriteLine($"[SIM] CreatureView: {ex.Message}"); }
        return d;
    }
}
