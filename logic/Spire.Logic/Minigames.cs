using MegaCrit.Sts2.Core.Events.Custom.CrystalSphereEvent;

namespace Spire.Logic;

/// <summary>
/// Event minigames that the real game runs on their own screen. The game
/// logic asks the UI to show the screen (now a recorded "ui" cue carrying the
/// minigame model) and then waits for the minigame to finish; the adapter
/// exposes the model as a decision instead of the screen.
/// </summary>
public partial class RunDriver
{
    private CrystalSphereMinigame? _crystalSphere;

    /// <summary>Called for every UI cue; picks up minigame models being shown.</summary>
    internal void OnUiCall(string id, object?[] args)
    {
        TimelineUiCall(id, args);
        if (id.EndsWith("NCrystalSphereScreen::ShowScreen", StringComparison.Ordinal))
        {
            foreach (var a in args)
                if (a is CrystalSphereMinigame mg) _crystalSphere = mg;
        }
    }

    /// <summary>A pending minigame decision, or null.</summary>
    private Dictionary<string, object?>? MinigameDecision()
    {
        var mg = _crystalSphere;
        if (mg == null) return null;
        if (mg.IsFinished)
        {
            _crystalSphere = null;
            return null;
        }
        var size = mg.GridSize;
        var cells = new List<Dictionary<string, object?>>();
        for (int x = 0; x < size.X; x++)
        {
            for (int y = 0; y < size.Y; y++)
            {
                var c = mg.cells[x, y];
                var cell = new Dictionary<string, object?> { ["x"] = x, ["y"] = y, ["hidden"] = c.IsHidden };
                if (!c.IsHidden && c.Item != null) cell["item"] = c.Item.GetType().Name;
                cells.Add(cell);
            }
        }
        return new Dictionary<string, object?>
        {
            ["type"] = "decision",
            ["decision"] = "crystal_sphere",
            ["context"] = RunContext(),
            ["width"] = size.X,
            ["height"] = size.Y,
            ["divinations"] = mg.DivinationCount,
            ["tool"] = mg.CrystalSphereTool.ToString(),
            ["tools"] = new List<string> { "Small", "Big" },
            ["cells"] = cells,
            ["items"] = mg.Items?.Select(i => i.GetType().Name).ToList(),
            ["item_views"] = CrystalSphereItemViews(mg),
            ["player"] = PlayerSummary(_runState!.Players[0]),
        };
    }

    /// <summary>NCrystalSphereItem: where each item lies (under the fog),
    /// its picture, and whether its cells are all clear.</summary>
    private static List<Dictionary<string, object?>> CrystalSphereItemViews(CrystalSphereMinigame mg)
    {
        var list = new List<Dictionary<string, object?>>();
        const System.Reflection.BindingFlags Inst = System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.NonPublic;
        foreach (var item in mg.Items ?? new List<CrystalSphereItem>())
        {
            string? texture = null, banner = null, frame = null;
            try { texture = item.GetType().GetProperty("TexturePath", Inst)?.GetValue(item) as string; } catch { }
            try { banner = item.GetType().GetProperty("BannerMaterialPath", Inst)?.GetValue(item) as string; } catch { }
            try
            {
                var owner = item.GetType().GetField("_owner", Inst)?.GetValue(item) as MegaCrit.Sts2.Core.Entities.Players.Player;
                if (banner != null) frame = owner?.Character.CardPool.FrameMaterialPath;
            }
            catch { }
            bool revealed = true;
            for (int i = 0; i < item.Size.X; i++)
                for (int j = 0; j < item.Size.Y; j++)
                    if (mg.cells[item.Position.X + i, item.Position.Y + j].IsHidden) revealed = false;
            list.Add(new Dictionary<string, object?>
            {
                ["type"] = item.GetType().Name,
                ["x"] = item.Position.X,
                ["y"] = item.Position.Y,
                ["w"] = item.Size.X,
                ["h"] = item.Size.Y,
                ["texture"] = ResPath(texture),
                ["good"] = item.IsGood,
                ["revealed"] = revealed,
                ["banner_material"] = ResPath(banner),
                ["frame_material"] = ResPath(frame),
            });
        }
        return list;
    }

    private Dictionary<string, object?> DoCrystalSphereTool(Dictionary<string, object?>? args)
    {
        if (_crystalSphere == null) return Error("No Crystal Sphere in progress");
        var name = args?.GetValueOrDefault("tool") as string ?? "";
        if (!Enum.TryParse<CrystalSphereMinigame.CrystalSphereToolType>(name, out var tool))
            return Error($"Unknown tool '{name}' (Small or Big)");
        Sched.Call(() => _crystalSphere.SetTool(tool));
        return DetectDecisionPoint();
    }

    private Dictionary<string, object?> DoCrystalSphereClick(Dictionary<string, object?>? args)
    {
        var mg = _crystalSphere;
        if (mg == null) return Error("No Crystal Sphere in progress");
        if (args == null || !args.ContainsKey("x") || !args.ContainsKey("y"))
            return Error("crystal_sphere_click requires 'x' and 'y'");
        int x = Convert.ToInt32(args["x"]), y = Convert.ToInt32(args["y"]);
        var size = mg.GridSize;
        if (x < 0 || y < 0 || x >= size.X || y >= size.Y) return Error($"Cell ({x},{y}) is outside the grid");
        if (mg.CrystalSphereTool == CrystalSphereMinigame.CrystalSphereToolType.None)
            Sched.Call(() => mg.SetTool(CrystalSphereMinigame.CrystalSphereToolType.Small));
        Sched.Run(() => mg.CellClicked(mg.cells[x, y]));
        WaitForActionExecutor();
        return DetectDecisionPoint();
    }
}
