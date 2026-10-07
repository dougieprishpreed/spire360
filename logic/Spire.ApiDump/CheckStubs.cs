// Spire.ApiDump --check-stubs <stub GodotSharp.dll> <real GodotSharp.dll>
//
// Finds stub declarations that compile and link but do not match the real
// GodotSharp the game was built against, in ways no signature check sees:
//
// - an enum's underlying type (Godot's are long; call sites push an int64,
//   which NativeAOT-LLVM rejects as invalid IL while the JIT tolerates it)
// - an enum member's value (the game passes the real constant)
// - a type's kind (struct vs class vs enum vs interface)
// - a class's base: the nearest real ancestor the stubs define must be the
//   stub's base (the game passes an InputEventMouseMotion where an
//   InputEventMouse is expected)
// - interfaces the real type implements (IDisposable for `using`)
// - generic method constraints (a stub without `where T : class` makes the
//   game's own constrained calls unverifiable)
//
// Exit code 1 when anything differs; build_logic.py runs it after building
// the stubs.
using Mono.Cecil;

static class CheckStubs
{
    public static int Run(string stubPath, string realPath)
    {
        var stub = ModuleDefinition.ReadModule(stubPath);
        var real = ModuleDefinition.ReadModule(realPath);
        var realTypes = new Dictionary<string, TypeDefinition>();
        foreach (var t in All(real.Types)) realTypes[t.FullName] = t;

        int problems = 0, checkedTypes = 0;
        void Report(string msg) { problems++; Console.WriteLine(msg); }

        foreach (var s in All(stub.Types))
        {
            if (!s.FullName.StartsWith("Godot")) continue;
            if (!realTypes.TryGetValue(s.FullName, out var r)) continue;
            checkedTypes++;
            string Kind(TypeDefinition t) => t.IsEnum ? "enum" : t.IsInterface ? "interface" : t.IsValueType ? "struct" : "class";
            if (Kind(s) != Kind(r))
            {
                Report($"{s.FullName}: stub is a {Kind(s)}, real is a {Kind(r)}");
                continue;
            }
            if (!s.IsEnum && !s.IsInterface)
            {
                var stubNames = new HashSet<string>(All(stub.Types).Select(t => t.FullName));
                // Every real ancestor the stubs define must be in the stub's chain.
                var stubChain = new HashSet<string>(Chain(s));
                foreach (var b in Chain(r).Skip(1))
                    if (stubNames.Contains(b) && !stubChain.Contains(b))
                        Report($"{s.FullName}: does not derive from {b} (stub base {s.BaseType?.FullName})");
                var stubIfaces = new HashSet<string>(Interfaces(s));
                foreach (var i in Interfaces(r))
                    if ((stubNames.Contains(i) || !i.StartsWith("Godot")) && !stubIfaces.Contains(i))
                        Report($"{s.FullName}: does not implement {i}");
                foreach (var sm in s.Methods.Where(m => m.HasGenericParameters))
                {
                    var rm = r.Methods.FirstOrDefault(m => m.Name == sm.Name && m.GenericParameters.Count == sm.GenericParameters.Count
                                                           && m.Parameters.Count == sm.Parameters.Count);
                    if (rm == null) continue;
                    for (int g = 0; g < sm.GenericParameters.Count; g++)
                    {
                        var a = sm.GenericParameters[g];
                        var b = rm.GenericParameters[g];
                        var ac = string.Join(",", a.Constraints.Select(c => c.ConstraintType.FullName).OrderBy(x => x));
                        var bc = string.Join(",", b.Constraints.Select(c => c.ConstraintType.FullName).OrderBy(x => x));
                        if (a.Attributes != b.Attributes || ac != bc)
                            Report($"{s.FullName}::{sm.Name}<{a.Name}>: constraints [{a.Attributes} {ac}], real [{b.Attributes} {bc}]");
                    }
                }
            }
            if (!s.IsEnum) continue;
            var su = Underlying(s);
            var ru = Underlying(r);
            if (su != ru)
                Report($"{s.FullName}: underlying type {su}, real {ru}");
            var realValues = r.Fields.Where(f => f.IsStatic && f.HasConstant)
                .ToDictionary(f => f.Name, f => Convert.ToInt64(f.Constant));
            foreach (var f in s.Fields.Where(f => f.IsStatic && f.HasConstant))
            {
                var v = Convert.ToInt64(f.Constant);
                if (!realValues.TryGetValue(f.Name, out var rv))
                    Report($"{s.FullName}.{f.Name}: not in the real enum");
                else if (rv != v)
                    Report($"{s.FullName}.{f.Name} = {v}, real {rv}");
            }
        }
        Console.WriteLine($"check-stubs: {checkedTypes} types compared, {problems} problem(s)");
        return problems == 0 ? 0 : 1;
    }

    /// <summary>All interfaces of a type and its bases (as far as they resolve).</summary>
    private static IEnumerable<string> Interfaces(TypeDefinition t)
    {
        for (var d = t; d != null; d = d.BaseType?.Resolve())
            foreach (var i in d.Interfaces)
            {
                yield return i.InterfaceType.GetElementType().FullName;
                var id = i.InterfaceType.Resolve();
                if (id != null)
                    foreach (var j in Interfaces(id)) yield return j;
            }
    }

    /// <summary>The type and its bases, open generic names.</summary>
    private static IEnumerable<string> Chain(TypeDefinition t)
    {
        for (TypeReference? d = t; d != null; d = d.Resolve()?.BaseType)
        {
            yield return d.GetElementType().FullName;
            if (d.Resolve() == null) yield break;
        }
    }

    private static string Underlying(TypeDefinition e) =>
        e.Fields.First(f => !f.IsStatic && f.Name == "value__").FieldType.FullName;

    private static IEnumerable<TypeDefinition> All(IEnumerable<TypeDefinition> types)
    {
        foreach (var t in types)
        {
            yield return t;
            foreach (var n in All(t.NestedTypes)) yield return n;
        }
    }
}

// Spire.ApiDump --print-enums <real GodotSharp.dll> <Full.Name[/Nested]>...
// Prints C# declarations of the real enums (for Manual/ stubs).
static class PrintEnums
{
    public static int Run(string realPath, IEnumerable<string> names)
    {
        var real = ModuleDefinition.ReadModule(realPath);
        var all = new Dictionary<string, TypeDefinition>();
        void Add(IEnumerable<TypeDefinition> ts) { foreach (var t in ts) { all[t.FullName] = t; Add(t.NestedTypes); } }
        Add(real.Types);
        foreach (var n in names)
        {
            if (!all.TryGetValue(n, out var e) || !e.IsEnum) { Console.Error.WriteLine($"not an enum: {n}"); return 1; }
            var u = e.Fields.First(f => f.Name == "value__").FieldType;
            string ut = u.FullName switch
            {
                "System.Int64" => "long", "System.Int32" => "int", "System.Byte" => "byte", "System.UInt32" => "uint",
                "System.UInt64" => "ulong", "System.Int16" => "short", "System.UInt16" => "ushort", "System.SByte" => "sbyte",
                _ => u.FullName,
            };
            Console.WriteLine($"@@ {n}");
            if (e.CustomAttributes.Any(a => a.AttributeType.Name == "FlagsAttribute")) Console.WriteLine("[System.Flags]");
            Console.WriteLine($"public enum {e.Name} : {ut}");
            Console.WriteLine("{");
            foreach (var f in e.Fields.Where(f => f.IsStatic && f.HasConstant))
                Console.WriteLine($"    {f.Name} = {f.Constant},");
            Console.WriteLine("}");
        }
        return 0;
    }
}

// Spire.ApiDump --bases <manual GodotSharp.dll> <real GodotSharp.dll>
// For each class in the hand-written stubs whose base differs from the real
// direct base (a Godot type): "Full/Name<TAB>manual base<TAB>real base (C#)".
static class Bases
{
    public static int Run(string manualPath, string realPath)
    {
        var manual = ModuleDefinition.ReadModule(manualPath);
        var real = ModuleDefinition.ReadModule(realPath);
        var realTypes = new Dictionary<string, TypeDefinition>();
        void Add(IEnumerable<TypeDefinition> ts) { foreach (var t in ts) { realTypes[t.FullName] = t; Add(t.NestedTypes); } }
        Add(real.Types);
        void Walk(IEnumerable<TypeDefinition> ts)
        {
            foreach (var m in ts)
            {
                Walk(m.NestedTypes);
                if (m.IsValueType || m.IsInterface || (m.IsAbstract && m.IsSealed)) continue;
                if (!realTypes.TryGetValue(m.FullName, out var r) || r.BaseType == null) continue;
                if (!r.BaseType.FullName.StartsWith("Godot")) continue;
                if (m.BaseType?.FullName == r.BaseType.FullName) continue;
                Console.WriteLine($"{m.FullName}\t{m.BaseType?.FullName}\tglobal::{r.BaseType.FullName.Replace('/', '.')}");
            }
        }
        Walk(manual.Types);
        return 0;
    }
}

// Spire.ApiDump --real-bases <real GodotSharp.dll>
// "Full/Name<TAB>base full name" for every class in the real assembly.
static class RealBases
{
    public static int Run(string realPath)
    {
        var real = ModuleDefinition.ReadModule(realPath);
        void Walk(IEnumerable<TypeDefinition> ts)
        {
            foreach (var t in ts)
            {
                if (t.IsClass && !t.IsValueType && !t.IsInterface && t.BaseType != null)
                    Console.WriteLine($"{t.FullName}\t{t.BaseType.FullName}");
                Walk(t.NestedTypes);
            }
        }
        Walk(real.Types);
        return 0;
    }
}

