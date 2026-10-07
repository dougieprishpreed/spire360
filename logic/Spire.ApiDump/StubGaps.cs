// Spire.ApiDump --stub-gaps <game.dll> <stub GodotSharp.dll> [<real GodotSharp.dll>]
// Lists every GodotSharp type and member the game references that the stub
// assembly does not define. With the real GodotSharp.dll the declaration is
// printed too, so the stub can be written to match.
using Mono.Cecil;

static class StubGaps
{
    public static int Run(string game, string stubs, string? real)
    {
        var resolver = new DefaultAssemblyResolver();
        resolver.AddSearchDirectory(Path.GetDirectoryName(Path.GetFullPath(game))!);
        var gameMod = ModuleDefinition.ReadModule(game, new ReaderParameters { AssemblyResolver = resolver });
        var stubMod = ModuleDefinition.ReadModule(stubs);
        var realMod = real != null ? ModuleDefinition.ReadModule(real) : null;

        TypeDefinition? Find(ModuleDefinition m, TypeReference t)
        {
            if (t.IsNested)
            {
                var outer = Find(m, t.DeclaringType);
                return outer?.NestedTypes.FirstOrDefault(n => n.Name == t.Name);
            }
            return m.GetType(t.Namespace, t.Name);
        }

        static string Key(MemberReference m) => m switch
        {
            MethodReference mr => $"{mr.Name}({string.Join(",", mr.Parameters.Select(p => p.ParameterType.FullName))})",
            FieldReference fr => fr.Name,
            _ => m.Name,
        };

        bool IsGodot(TypeReference t) => t.Scope?.Name is "GodotSharp" or "GodotSharp.dll";

        var missingTypes = new SortedSet<string>();
        var missingMembers = new SortedDictionary<string, SortedSet<string>>();

        foreach (var tr in gameMod.GetTypeReferences().Where(IsGodot))
            if (Find(stubMod, tr) == null) missingTypes.Add(tr.FullName);

        foreach (var mr in gameMod.GetMemberReferences())
        {
            var decl = mr.DeclaringType.GetElementType();
            if (!IsGodot(decl)) continue;
            var stubType = Find(stubMod, decl);
            if (stubType == null) { missingTypes.Add(decl.FullName); continue; }
            bool found = mr switch
            {
                MethodReference m => stubType.Methods.Any(x => x.Name == m.Name && x.Parameters.Count == m.Parameters.Count
                    && x.Parameters.Select(p => p.ParameterType.Name).SequenceEqual(m.Parameters.Select(p => p.ParameterType.Name))),
                FieldReference f => stubType.Fields.Any(x => x.Name == f.Name),
                _ => true,
            };
            if (!found)
            {
                if (!missingMembers.TryGetValue(decl.FullName, out var set)) missingMembers[decl.FullName] = set = new();
                set.Add(Key(mr));
            }
        }

        Console.WriteLine($"# {missingTypes.Count} missing types");
        foreach (var t in missingTypes)
        {
            var rt = realMod != null ? realMod.GetTypes().FirstOrDefault(x => x.FullName == t.Replace('/', '/')) : null;
            Console.WriteLine($"T {t}" + (rt != null ? $"  ({(rt.IsValueType ? "struct" : rt.IsInterface ? "interface" : rt.IsEnum ? "enum" : "class")} : {rt.BaseType?.FullName})" : ""));
        }
        Console.WriteLine($"# {missingMembers.Sum(kv => kv.Value.Count)} missing members");
        foreach (var (t, ms) in missingMembers)
            foreach (var m in ms) Console.WriteLine($"M {t}::{m}");
        return 0;
    }
}
