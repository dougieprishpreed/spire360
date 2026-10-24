// Spire.ApiDump --presentation <sts2.dll> <out.json>
// The game's whole presentation surface, per source method, for the
// presentation audit (tools/presentation_audit.py): what each method of the
// game asks of its UI layer and its assets, and which game methods it calls
// (so a content model's obligations can be followed through the helpers it
// uses).
//   methods.<Owner>   ui      UI methods it calls (getters included) and UI
//                             types it constructs ("Type::.ctor")
//                     ftn     UI methods it binds as delegates (ldftn)
//                     godot   Godot objects it drives
//                     test    TestMode / NonInteractiveMode reads
//                     assets  string literals naming assets (scenes, sounds,
//                             textures, materials, animations' resources)
//                     calls   game methods it calls (source methods)
//                     guards  UI / Godot values it tests for null (the call,
//                             or field, the value came from): what it skips
//                             when the stand-ins answer null
//                     ui_type its type is a UI class
//   types.<FullName>  base, ui, scene (literal scene paths its methods name)
using System.Text.Json;
using System.Text.RegularExpressions;
using Mono.Cecil;
using Mono.Cecil.Cil;
using Mono.Collections.Generic;

static class Presentation
{
    static readonly Regex AssetLike = new(
        @"^(res://|event:/|snapshot:/|bus:/|vca:/)|\.(tscn|tres|png|jpg|webp|mp3|ogg|wav|gdshader|material|atlas|skel|json)$|^(scenes|images|vfx|sfx|ui|animations|shaders|materials)/",
        RegexOptions.Compiled);

    sealed class M
    {
        public SortedSet<string> ui = new(StringComparer.Ordinal), ftn = new(StringComparer.Ordinal),
            godot = new(StringComparer.Ordinal), test = new(StringComparer.Ordinal),
            assets = new(StringComparer.Ordinal), calls = new(StringComparer.Ordinal),
            guards = new(StringComparer.Ordinal);
        public bool ui_type;
    }

    public static int Run(string asm, string outPath)
    {
        var resolver = new DefaultAssemblyResolver();
        resolver.AddSearchDirectory(Path.GetDirectoryName(Path.GetFullPath(asm))!);
        var module = ModuleDefinition.ReadModule(asm, new ReaderParameters { ReadingMode = ReadingMode.Deferred, AssemblyResolver = resolver });
        UiSurface._module = module;
        var methods = new SortedDictionary<string, M>(StringComparer.Ordinal);
        var types = new SortedDictionary<string, Dictionary<string, object?>>(StringComparer.Ordinal);
        foreach (var type in module.GetTypes())
        {
            bool ui = UiSurface.IsUi(type);
            var outer = type;
            while (outer.IsNested && outer.Name.StartsWith("<", StringComparison.Ordinal)) outer = outer.DeclaringType;
            if (outer == type)
                types[type.FullName] = new Dictionary<string, object?>
                {
                    ["base"] = type.BaseType?.GetElementType().FullName,
                    ["ui"] = ui,
                    ["abstract"] = type.IsAbstract,
                };
            foreach (var m in type.Methods.Where(x => x.HasBody))
            {
                var owner = UiSurface.Owner(m);
                if (!methods.TryGetValue(owner, out var rec)) methods[owner] = rec = new M { ui_type = ui };
                if (!ui) Guards(m, rec.guards);
                foreach (var ins in m.Body.Instructions)
                {
                    if (ins.OpCode == OpCodes.Ldstr && ins.Operand is string str)
                    {
                        if (str.Length < 200 && AssetLike.IsMatch(str)) rec.assets.Add(str);
                        continue;
                    }
                    if (ins.Operand is not MethodReference mr) continue;
                    var dt = mr.DeclaringType;
                    bool call = ins.OpCode == OpCodes.Call || ins.OpCode == OpCodes.Callvirt;
                    bool ctor = ins.OpCode == OpCodes.Newobj;
                    bool ftn = ins.OpCode == OpCodes.Ldftn || ins.OpCode == OpCodes.Ldvirtftn;
                    if (!call && !ctor && !ftn) continue;
                    if (UiSurface.IsUi(dt) && !ui)
                    {
                        if (ftn) rec.ftn.Add(UiSurface.Callee(mr));
                        else rec.ui.Add(UiSurface.Callee(mr));
                        continue;
                    }
                    if (UiSurface.IsGodot(dt))
                    {
                        rec.godot.Add(UiSurface.Callee(mr));
                        continue;
                    }
                    if ((dt.FullName == "MegaCrit.Sts2.Core.TestSupport.TestMode" && (mr.Name == "get_IsOn" || mr.Name == "get_IsOff")) ||
                        (dt.Name == "NonInteractiveMode" && mr.Name == "get_IsActive"))
                    {
                        rec.test.Add(dt.Name + "." + mr.Name[4..]);
                        continue;
                    }
                    if (dt.Scope == module || dt.Module == module)
                    {
                        MethodDefinition? def = null;
                        try { def = mr.Resolve(); } catch { }
                        if (def != null && def.Module == module)
                        {
                            var target = UiSurface.Owner(def);
                            if (target != owner) rec.calls.Add(target);
                        }
                    }
                }
            }
        }
        var doc = new Dictionary<string, object>
        {
            ["methods"] = methods.ToDictionary(kv => kv.Key, kv =>
            {
                var r = kv.Value;
                var d = new Dictionary<string, object>();
                if (r.ui_type) d["ui_type"] = true;
                foreach (var (name, set) in new[] { ("ui", r.ui), ("ftn", r.ftn), ("godot", r.godot), ("test", r.test), ("assets", r.assets), ("calls", r.calls), ("guards", r.guards) })
                    if (set.Count > 0) d[name] = set.ToArray();
                return (object)d;
            }),
            ["types"] = types,
        };
        File.WriteAllText(outPath, JsonSerializer.Serialize(doc, new JsonSerializerOptions { WriteIndented = false }));
        Console.WriteLine($"methods {methods.Count}, types {types.Count}");
        return 0;
    }

    static bool UiValued(TypeReference t) =>
        !t.IsGenericParameter && (UiSurface.IsUi(t) || UiSurface.IsGodot(t)) && !t.IsValueType;

    /// <summary>The value a null test reads: a call's result (its callee) or a
    /// field (its name), through dup / a local stored just before.</summary>
    static string? Producer(Collection<Instruction> ins, int i)
    {
        for (int hops = 0; i >= 0 && hops < 4; hops++)
        {
            var p = ins[i];
            if (p.OpCode == OpCodes.Dup) { i--; continue; }
            int local = LocalIndex(p, load: true);
            if (local >= 0)
            {
                int k = i - 1;
                while (k >= 0 && LocalIndex(ins[k], load: false) != local) k--;
                if (k < 1) return null;
                i = k - 1;
                continue;
            }
            if ((p.OpCode == OpCodes.Call || p.OpCode == OpCodes.Callvirt) && p.Operand is MethodReference mr)
            {
                var rt = mr.ReturnType;
                if (rt is GenericInstanceType git && git.ElementType.Name == "Task`1") rt = git.GenericArguments[0];
                return UiValued(rt) || UiSurface.IsUi(mr.DeclaringType) ? UiSurface.Callee(mr) : null;
            }
            if ((p.OpCode == OpCodes.Ldfld || p.OpCode == OpCodes.Ldsfld) && p.Operand is FieldReference fr)
                return UiValued(fr.FieldType) ? fr.DeclaringType.GetElementType().FullName + "::" + fr.Name : null;
            if (p.OpCode == OpCodes.Isinst && p.Operand is TypeReference tr)
                return UiValued(tr) ? "isinst " + tr.FullName : null;
            return null;
        }
        return null;
    }

    static int LocalIndex(Instruction p, bool load)
    {
        var c = p.OpCode.Code;
        if (load)
        {
            if (c >= Code.Ldloc_0 && c <= Code.Ldloc_3) return c - Code.Ldloc_0;
            if ((c == Code.Ldloc || c == Code.Ldloc_S) && p.Operand is VariableDefinition v) return v.Index;
        }
        else
        {
            if (c >= Code.Stloc_0 && c <= Code.Stloc_3) return c - Code.Stloc_0;
            if ((c == Code.Stloc || c == Code.Stloc_S) && p.Operand is VariableDefinition v) return v.Index;
        }
        return -1;
    }

    static void Guards(MethodDefinition m, SortedSet<string> into)
    {
        var ins = m.Body.Instructions;
        for (int i = 1; i < ins.Count; i++)
        {
            var c = ins[i].OpCode.Code;
            string? g = null;
            if (c is Code.Brtrue or Code.Brtrue_S or Code.Brfalse or Code.Brfalse_S)
                g = Producer(ins, i - 1);
            else if ((c is Code.Ceq or Code.Cgt_Un) && ins[i - 1].OpCode.Code == Code.Ldnull && i >= 2)
                g = Producer(ins, i - 2);
            if (g != null) into.Add(g);
        }
    }
}
