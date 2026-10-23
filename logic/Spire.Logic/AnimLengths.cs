using System.Diagnostics.CodeAnalysis;
using System.Reflection;
using System.Text.Json;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Entities.Creatures;

namespace Spire.Logic;

/// <summary>
/// Each creature's Spine animations and their lengths (anim_lengths.json,
/// made by tools/build_logic.py from the game's .pck and embedded here): what
/// the UI answers when the game asks how long an animation runs
/// (NCreature.StartDeathAnim's length, which Hook.AfterDeath passes on to
/// the powers that wait for it).
/// </summary>
internal static class AnimLengths
{
    private static Dictionary<string, Dictionary<string, float>>? _table;

    private static Dictionary<string, Dictionary<string, float>> Table()
    {
        if (_table != null) return _table;
        _table = new Dictionary<string, Dictionary<string, float>>();
        try
        {
            using var s = typeof(AnimLengths).Assembly.GetManifestResourceStream("anim_lengths.json");
            if (s == null) return _table;
            using var doc = JsonDocument.Parse(s);
            foreach (var scene in doc.RootElement.EnumerateObject())
            {
                var anims = new Dictionary<string, float>();
                foreach (var a in scene.Value.EnumerateObject()) anims[a.Name] = a.Value.GetSingle();
                _table[Plain(scene.Name)] = anims;
            }
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"[SIM] animation lengths: {ex.Message}");
        }
        return _table;
    }

    private static string Plain(string path) => path.StartsWith("res://", StringComparison.Ordinal) ? path[6..] : path;

    /// <summary>The creature's visuals scene (MonsterModel / CharacterModel.VisualsPath,
    /// non-public: the declaring class's property, whose getter dispatches to
    /// a monster's override).</summary>
    [DynamicDependency("get_VisualsPath", typeof(MonsterModel))]
    [DynamicDependency("get_VisualsPath", typeof(CharacterModel))]
    private static string? VisualsPath(Creature c)
    {
        object? model = (object?)c.Monster ?? c.Player?.Character;
        for (var t = model?.GetType(); t != null; t = t.BaseType)
        {
            var p = t.GetProperty("VisualsPath", BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.DeclaredOnly);
            if (p != null) return p.GetValue(model) as string;
        }
        return null;
    }

    /// <summary>The length of one of the creature's animations; 0 when
    /// unknown (no skeleton, no such animation).</summary>
    public static float Of(Creature c, string animation)
    {
        var path = VisualsPath(c);
        return path != null && Table().TryGetValue(Plain(path), out var anims) && anims.TryGetValue(animation, out var len) ? len : 0f;
    }

    public static bool HasSkeleton(Creature c)
    {
        var path = VisualsPath(c);
        return path != null && Table().ContainsKey(Plain(path));
    }
}
