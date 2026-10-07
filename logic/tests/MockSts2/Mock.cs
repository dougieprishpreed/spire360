// Shapes that mirror the game methods logic/patches.json targets.
namespace MegaCrit.Sts2.Core.Commands
{
    public static class Cmd
    {
        public static int WaitCalls;
        public static async Task Wait(float seconds)
        {
            WaitCalls++;
            await Task.Delay(TimeSpan.FromSeconds(seconds));
        }
    }

    public class NSpeechBubbleVfx { }

    public static class TalkCmd
    {
        public static NSpeechBubbleVfx? Play(string line, object speaker) => throw new NullReferenceException("vfx");
    }
}

namespace MegaCrit.Sts2.Core.Localization
{
    public class LocTable
    {
        private readonly string _name;
        private readonly Dictionary<string, string> _entries;
        public LocTable(string name, Dictionary<string, string> entries) { _name = name; _entries = entries; }
        public string GetRawText(string key) => _entries.TryGetValue(key, out var v) ? v : throw new KeyNotFoundException(key);
        public bool HasEntry(string key) => _entries.ContainsKey(key);
        public int Count => _entries.Count;
    }
}

namespace MegaCrit.Sts2.Core.GameActions
{
    public struct Coord
    {
        public int X, Y;
        public Coord(int x, int y) { X = x; Y = y; }
        public int Sum() => X + Y;
    }

    public class GameAction
    {
        public string Name = "";
        public int Executions;
    }

    public class ActionExecutor
    {
        public int Execute(GameAction action)
        {
            action.Executions++;
            if (action.Name == "early") return -1;
            return action.Executions * 10;
        }

        public void WaitUntilQueueIsEmptyOrWaitingOnNonPlayerDrivenAction() => throw new InvalidOperationException("would block");

        public static T Echo<T>(T value) => value;

        public static async Task<int> Animate(int ms)
        {
            await Task.Delay(ms);
            return ms;
        }
    }
}
