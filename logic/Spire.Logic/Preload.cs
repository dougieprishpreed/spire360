// What the game preloads behind the start of a run and of each act
// (PreloadManager.LoadRunAssets / LoadActAssets: AssetSets.CommonAssets,
// its characters' run set, the act's), as asset paths with the first
// decision of each, for the client to read ahead of their first use
// (SceneLibrary::warm). Headless, TestMode keeps the game's own calls from
// running. Its card art is not in those sets; the console's pack puts every
// portrait on two atlas pages, read at the first hand's draw otherwise: the
// deck's portraits come too.
using System.Diagnostics.CodeAnalysis;
using System.Reflection;
using System.Runtime.CompilerServices;
using MegaCrit.Sts2.Core.Assets;

namespace Spire.Logic;

public partial class RunDriver
{
    // The run and act last warmed (its RunState and act index).
    private string? _preloadKey;

    [DynamicDependency("GetRunAssetPaths", typeof(PreloadManager))]
    public void AttachPreload(Dictionary<string, object?> response)
    {
        if (_runState == null || !response.TryGetValue("type", out var type) || (type as string) != "decision") return;
        string key = $"{RuntimeHelpers.GetHashCode(_runState)}/{_runState.CurrentActIndex}";
        if (key == _preloadKey) return;
        _preloadKey = key;
        try
        {
            var paths = new HashSet<string>(AssetSets.CommonAssets);
            var runPaths = typeof(PreloadManager).GetMethod("GetRunAssetPaths", BindingFlags.NonPublic | BindingFlags.Static);
            var characters = _runState.Players.Select(p => p.Character).ToList();
            if (runPaths?.Invoke(null, new object[] { characters, false }) is IEnumerable<string> run)
                paths.UnionWith(run);
            if (_runState.Act is { } act) paths.UnionWith(act.AssetPaths);
            // the orbs' visuals (OrbModel.AssetPaths): in none of the game's
            // sets, each made at its first channel; on the console a scene,
            // its skeleton and its effects read from the USB drive mid-turn
            foreach (var orb in MegaCrit.Sts2.Core.Models.ModelDb.Orbs.Append(
                         MegaCrit.Sts2.Core.Models.ModelDb.Orb<MegaCrit.Sts2.Core.Models.Orbs.GlassOrb>()))
                paths.UnionWith(orb.AssetPaths);
            foreach (var p in _runState.Players)
                paths.UnionWith(p.Deck.Cards.Select(c => c.PortraitPath));
            response["preload"] = paths.OrderBy(p => p, StringComparer.Ordinal).ToList();
        }
        catch (Exception ex)
        {
            Spire.Logic.Log.Warn($"preload: {ex.GetBaseException().Message}");
        }
    }
}
