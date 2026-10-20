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
            ["player"] = PlayerSummary(_runState!.Players[0]),
        };
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
