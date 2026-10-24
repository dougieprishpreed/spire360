// The presentation ledger (tools/presentation_audit.py): with SPIRE_LEDGER
// set to a file, what the headless logic leaves undone, one JSON line per new
// fact (and the counts when the process ends):
//   ui        a call into the UI layer (the 'ui' redirect): its id, the game
//             method making it, the model whose code was running, and what
//             the stand-ins answered ("null" or the answer's type)
//   headless  a stripped UI body that ran after all (its NotSupportedException):
//             a call the redirect does not see (a delegate, a struct's method
//             in the UI's namespace, UI code reached through a virtual call)
//   exception an exception game code threw, with where (the ones it catches
//             itself hide presentation the game would have shown)
// Not used by the game's flow; off unless the variable is set.
using System.Diagnostics;
using System.Reflection;
using System.Text.Json;

namespace Spire.Logic;

internal static class Ledger
{
    private static StreamWriter? _out;
    private static readonly Dictionary<string, int> Counts = new();
    private static readonly Assembly Game = typeof(MegaCrit.Sts2.Core.Models.CardModel).Assembly;

    [ThreadStatic] private static bool _inside;

    public static bool On => _out != null;

    public static void Open()
    {
        var path = Environment.GetEnvironmentVariable("SPIRE_LEDGER");
        if (string.IsNullOrEmpty(path)) return;
        _out = new StreamWriter(path, append: true) { AutoFlush = true };
        AppDomain.CurrentDomain.FirstChanceException += (_, e) => Exception(e.Exception);
        AppDomain.CurrentDomain.ProcessExit += (_, _) => Close();
    }

    public static void Close()
    {
        var o = _out;
        if (o == null) return;
        lock (Counts)
        {
            o.WriteLine(JsonSerializer.Serialize(new Dictionary<string, object?> { ["k"] = "counts", ["counts"] = Counts }));
            o.Flush();
        }
    }

    /// <summary>A 'ui' call and the stand-ins' answer.</summary>
    public static void Ui(string id, object? answer)
    {
        if (_out == null || _inside) return;
        _inside = true;
        try
        {
            var (caller, model) = Callers(2);
            var call = id[(id.IndexOf('|') + 1)..];
            string kind = answer == null ? "null" : answer.GetType().Name;
            Write("ui|" + call + "|" + caller + "|" + kind, () => new Dictionary<string, object?>
            {
                ["k"] = "ui", ["call"] = call, ["caller"] = caller, ["model"] = model, ["answer"] = kind,
            });
        }
        finally { _inside = false; }
    }

    /// <summary>Something the adapter passed over that the client never
    /// shows (an event without options, a shop without an inventory).</summary>
    public static void Skip(string what, string detail)
    {
        if (_out == null || _inside) return;
        Write("skip|" + what + "|" + detail, () => new Dictionary<string, object?>
        {
            ["k"] = "skip", ["what"] = what, ["detail"] = detail,
        });
    }

    /// <summary>A cue a hook recorded itself ("Type.Method" for a UI call,
    /// "t:kind" for the others), with the game method it came from.</summary>
    public static void Cue(string call)
    {
        if (_out == null || _inside) return;
        _inside = true;
        try
        {
            var (caller, model) = Callers(1);
            Write("cue|" + call + "|" + caller, () => new Dictionary<string, object?>
            {
                ["k"] = "cue", ["call"] = call, ["caller"] = caller, ["model"] = model,
            });
        }
        finally { _inside = false; }
    }

    private static void Exception(Exception ex)
    {
        if (_out == null || _inside) return;
        if (ex is OperationCanceledException) return;
        _inside = true;
        try
        {
            var msg = ex.Message ?? "";
            if (ex is NotSupportedException && msg.StartsWith("headless: ", StringComparison.Ordinal))
            {
                var call = msg[10..];
                var (caller, model) = Callers(1, skipUi: true);
                Write("headless|" + call + "|" + caller, () => new Dictionary<string, object?>
                {
                    ["k"] = "headless", ["call"] = call, ["caller"] = caller, ["model"] = model,
                });
                return;
            }
            var frames = new StackTrace(1, false).GetFrames();
            string site = "?", where = "?";
            var stack = new List<string>();
            foreach (var f in frames)
            {
                var m = f.GetMethod();
                var t = m?.DeclaringType;
                var top = t;
                while (top?.DeclaringType != null) top = top.DeclaringType;
                if (t == null || top == typeof(Ledger)) continue;
                if (site == "?") site = Owner(m!);
                bool ours = t.Assembly == Game || t.Assembly == typeof(Ledger).Assembly;
                if (ours && where == "?") where = Owner(m!);
                if (ours && stack.Count < 8) stack.Add(Owner(m!));
            }
            var first = msg.Split('\n')[0];
            if (first.Length > 160) first = first[..160];
            Write("exception|" + ex.GetType().Name + "|" + where, () => new Dictionary<string, object?>
            {
                ["k"] = "exception", ["type"] = ex.GetType().Name, ["message"] = first, ["site"] = site, ["where"] = where,
                ["stack"] = stack,
            });
        }
        catch
        {
            // the ledger never changes what the game does
        }
        finally { _inside = false; }
    }

    private static void Write(string key, Func<Dictionary<string, object?>> line)
    {
        var o = _out;
        if (o == null) return;
        lock (Counts)
        {
            Counts.TryGetValue(key, out var n);
            Counts[key] = n + 1;
            if (n == 0) o.WriteLine(JsonSerializer.Serialize(line()));
        }
    }

    /// <summary>The first game method on the stack (outside the UI layer when
    /// skipUi), and the first content model's.</summary>
    private static (string caller, string? model) Callers(int skip, bool skipUi = false)
    {
        string caller = "?";
        string? model = null;
        foreach (var f in new StackTrace(skip + 1, false).GetFrames())
        {
            var m = f.GetMethod();
            var t = m?.DeclaringType;
            if (t == null || t.Assembly != Game) continue;
            var outer = t;
            while (outer.IsNested) outer = outer.DeclaringType!;
            var ns = outer.Namespace ?? "";
            if (skipUi && (ns.StartsWith("MegaCrit.Sts2.Core.Nodes", StringComparison.Ordinal) ||
                           ns.StartsWith("MegaCrit.sts2.Core.Nodes", StringComparison.Ordinal) ||
                           ns.StartsWith("MegaCrit.Sts2.Core.Bindings.MegaSpine", StringComparison.Ordinal)))
                continue;
            if (caller == "?") caller = Owner(m!);
            if (ns.StartsWith("MegaCrit.Sts2.Core.Models.", StringComparison.Ordinal))
            {
                model = Owner(m!);
                break;
            }
        }
        return (caller, model);
    }

    /// <summary>A method as the presentation dump names it: the source method
    /// of a compiler-made body (state machine, lambda).</summary>
    private static string Owner(MethodBase m)
    {
        var t = m.DeclaringType!;
        if (t.IsConstructedGenericType) t = t.GetGenericTypeDefinition();
        var name = Plain(m.Name);
        while (t.IsNested && t.Name.StartsWith("<", StringComparison.Ordinal))
        {
            if (!t.Name.StartsWith("<>", StringComparison.Ordinal)) name = Plain(t.Name);
            t = t.DeclaringType!;
            if (t.IsConstructedGenericType) t = t.GetGenericTypeDefinition();
        }
        return (t.FullName ?? t.Name).Replace('+', '/') + "::" + name;
    }

    private static string Plain(string name)
    {
        int lt = name.IndexOf('<'), gt = name.IndexOf('>');
        return lt == 0 && gt > 1 ? name[1..gt] : name;
    }
}
