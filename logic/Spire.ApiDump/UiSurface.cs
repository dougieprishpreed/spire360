// Spire.ApiDump --ui-surface <sts2.dll> <out.json>
// Where the game's logic meets its Godot UI layer, for the presentation audit
// (tools/ui_audit.py): what the headless logic leaves out and the client has
// to play instead.
//   ui_calls     each UI method (the 'ui' patch's classes: namespaces
//                MegaCrit.Sts2.Core.Nodes.* and Bindings.MegaSpine, or a game
//                class deriving from a Godot class) that game code calls,
//                with the methods calling it
//   godot_calls  Godot engine objects game code drives itself (tweens, nodes,
//                scene trees: null headless, so skipped)
//   test_mode    game methods asking TestMode / NonInteractiveMode (what they
//                skip headless)
//   events       game events UI classes subscribe to, with the handlers
using System.Text.Json;
using Mono.Cecil;
using Mono.Cecil.Cil;

static class UiSurface
{
    static readonly string[] UiNamespaces = { "MegaCrit.Sts2.Core.Nodes", "MegaCrit.sts2.Core.Nodes", "MegaCrit.Sts2.Core.Bindings.MegaSpine" };

    // Godot value types and static helpers: no presentation of their own.
    static readonly HashSet<string> GodotValues = new()
    {
        "Vector2", "Vector2I", "Vector3", "Vector3I", "Vector4", "Color", "Colors", "Rect2", "Rect2I", "Transform2D",
        "Mathf", "GD", "Variant", "StringName", "NodePath", "Callable", "Basis", "Quaternion", "Aabb", "Plane",
        "Projection", "Transform3D", "Time", "OS", "Engine", "FileAccess", "DirAccess", "ProjectSettings", "Json",
        "ResourceLoader", "RandomNumberGenerator", "Marshalls", "Signal", "Error",
    };

    internal static ModuleDefinition _module = null!;
    static readonly Dictionary<TypeDefinition, bool> _godot = new();

    internal static bool InUiNamespace(TypeReference t)
    {
        while (t.IsNested) t = t.DeclaringType;
        var ns = t.Namespace ?? "";
        return UiNamespaces.Any(p => ns == p || ns.StartsWith(p + ".", StringComparison.Ordinal));
    }

    internal static bool DerivesFromGodot(TypeReference t)
    {
        if (t.Module != _module && t.Scope != _module) return false;
        TypeDefinition? def;
        try { def = t.Resolve(); } catch { return false; }
        if (def == null || def.Module != _module) return false;
        if (_godot.TryGetValue(def, out var known)) return known;
        _godot[def] = false;
        bool result = false;
        var b = def.BaseType;
        if (b != null)
        {
            var ns = b.GetElementType().Namespace ?? "";
            result = ns == "Godot" || ns.StartsWith("Godot.", StringComparison.Ordinal) || DerivesFromGodot(b);
        }
        if (!result && def.IsNested) result = DerivesFromGodot(def.DeclaringType);
        _godot[def] = result;
        return result;
    }

    internal static bool IsUi(TypeReference t) => InUiNamespace(t) || DerivesFromGodot(t.GetElementType());

    internal static bool IsGodot(TypeReference t)
    {
        var e = t.GetElementType();
        while (e.IsNested) e = e.DeclaringType;
        return e.Namespace == "Godot" && !GodotValues.Contains(e.Name);
    }

    internal static string Plain(string name)
    {
        int lt = name.IndexOf('<'), gt = name.IndexOf('>');
        return lt == 0 && gt > 1 ? name[1..gt] : name;
    }

    /// <summary>The source method a compiler-generated body belongs to
    /// (state machines <M>d__N, lambdas <M>b__N in <>c / display classes).</summary>
    internal static string Owner(MethodDefinition m)
    {
        var t = m.DeclaringType;
        string name = Plain(m.Name);
        while (t.IsNested && t.Name.StartsWith("<", StringComparison.Ordinal))
        {
            if (!t.Name.StartsWith("<>", StringComparison.Ordinal)) name = Plain(t.Name);
            t = t.DeclaringType;
        }
        return t.FullName + "::" + name;
    }

    internal static string Callee(MethodReference mr)
    {
        var t = mr.DeclaringType.GetElementType();
        return t.FullName + "::" + mr.Name;
    }

    internal static void Add(Dictionary<string, SortedSet<string>> map, string key, string value)
    {
        if (!map.TryGetValue(key, out var set)) map[key] = set = new SortedSet<string>(StringComparer.Ordinal);
        set.Add(value);
    }

    public static int Run(string asm, string outPath)
    {
        var resolver = new DefaultAssemblyResolver();
        resolver.AddSearchDirectory(Path.GetDirectoryName(Path.GetFullPath(asm))!);
        _module = ModuleDefinition.ReadModule(asm, new ReaderParameters { ReadingMode = ReadingMode.Deferred, AssemblyResolver = resolver });
        var uiCalls = new Dictionary<string, SortedSet<string>>();
        var godotCalls = new Dictionary<string, SortedSet<string>>();
        var testMode = new Dictionary<string, SortedSet<string>>();
        var events = new Dictionary<string, SortedSet<string>>();
        foreach (var type in _module.GetTypes())
        {
            bool ui = IsUi(type);
            foreach (var m in type.Methods.Where(x => x.HasBody))
            {
                var ins = m.Body.Instructions;
                for (int i = 0; i < ins.Count; i++)
                {
                    if ((ins[i].OpCode != OpCodes.Call && ins[i].OpCode != OpCodes.Callvirt) || ins[i].Operand is not MethodReference mr)
                        continue;
                    var dt = mr.DeclaringType;
                    if (ui)
                    {
                        // a game event a UI class subscribes to: add_X with the
                        // handler loaded just before
                        if (mr.Name.StartsWith("add_", StringComparison.Ordinal) && !IsUi(dt) && !IsGodot(dt) &&
                            (dt.Namespace ?? "").StartsWith("MegaCrit", StringComparison.Ordinal))
                        {
                            string handler = "?";
                            for (int k = i - 1; k >= 0 && k >= i - 6; k--)
                                if (ins[k].OpCode == OpCodes.Ldftn || ins[k].OpCode == OpCodes.Ldvirtftn)
                                {
                                    var h = (MethodReference)ins[k].Operand;
                                    handler = h.Resolve() is MethodDefinition hd ? Owner(hd) : Callee(h);
                                    break;
                                }
                            Add(events, dt.GetElementType().FullName + "." + mr.Name[4..], Owner(m) + " -> " + handler);
                        }
                        continue;
                    }
                    if (IsUi(dt))
                    {
                        if (mr.Name != ".ctor") Add(uiCalls, Callee(mr), Owner(m));
                    }
                    else if (IsGodot(dt))
                    {
                        Add(godotCalls, Callee(mr), Owner(m));
                    }
                    else if ((dt.FullName == "MegaCrit.Sts2.Core.TestSupport.TestMode" && (mr.Name == "get_IsOn" || mr.Name == "get_IsOff")) ||
                             (dt.Name == "NonInteractiveMode" && mr.Name == "get_IsActive"))
                    {
                        Add(testMode, Owner(m), dt.Name + "." + mr.Name[4..]);
                    }
                }
            }
        }
        static Dictionary<string, string[]> Flat(Dictionary<string, SortedSet<string>> d) =>
            d.OrderBy(kv => kv.Key, StringComparer.Ordinal).ToDictionary(kv => kv.Key, kv => kv.Value.ToArray());
        var doc = new Dictionary<string, object>
        {
            ["ui_calls"] = Flat(uiCalls),
            ["godot_calls"] = Flat(godotCalls),
            ["test_mode"] = Flat(testMode),
            ["events"] = Flat(events),
        };
        File.WriteAllText(outPath, JsonSerializer.Serialize(doc, new JsonSerializerOptions { WriteIndented = true }));
        Console.WriteLine($"ui_calls {uiCalls.Count}, godot_calls {godotCalls.Count}, test_mode {testMode.Count}, events {events.Count}");
        return 0;
    }
}
