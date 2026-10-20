// Spire.Patcher - static IL patches for the player's game assembly.
//
// Replaces the runtime Harmony patches sts2-cli uses (Harmony cannot work
// under NativeAOT) with edits made once, on the player's PC, before the
// assembly is compiled to WebAssembly.
//
//   Spire.Patcher --in <sts2.dll> --out <dir> --config <patches.json>
//                 --hookbridge <Spire.HookBridge.dll> [--search <dir>]...
//                 [--report <report.json>]
//
// Patch actions (see logic/patches.json for the live list):
//   return_default         body = return default(T)
//   return_completed_task  body = return Task.CompletedTask / Task.FromResult(default)
//   return_const           body = return <bool|int|string value>
//   redirect               body = return (T)Hooks.Invoke(id, new object[] { this?, args... })
//   call_after             before every ret: Hooks.After(id, new object[] { this?, args... }, result)
//   replace_calls          every call to the target method anywhere in the assembly
//                          becomes (T)Hooks.Invoke(id, new object[] { args... })
//   strip_bodies           every non-constructor method of Godot-derived types in
//                          the given namespaces throws NotSupportedException("headless: ...")
//   redirect_calls_into    every call from code outside the given namespaces to a
//                          method declared inside them becomes
//                          Hooks.Invoke("<id>|<Type>::<Method>", { this?, args... })
//                          followed by a safe default of the return type (completed
//                          Task, empty sequence, default value). Used to cut the game
//                          logic off from its Godot UI layer while recording what it
//                          asked the UI to do.
//   null_safe_calls_into   every instance call from code outside the given namespaces
//                          to a class method declared inside them checks its receiver:
//                          null -> skip the call and use the same safe default;
//                          otherwise call as before. Used for Godot engine objects
//                          (scenes, tweens, nodes) that do not exist without the engine.
//
// A patch whose target is missing fails the run unless it is "optional".
using System.Text.Json;
using Mono.Cecil;
using Mono.Cecil.Cil;
using Mono.Cecil.Rocks;

namespace Spire.Patcher;

internal sealed class PatchSpec
{
    public string Id = "";
    public string Action = "";
    public string Type = "";
    public string Method = "";
    public List<string>? Params;
    public bool AllOverloads;
    public bool Optional;
    public bool Enabled = true;
    public JsonElement? Value;
    public string? Note;
    public List<string>? NamespacePrefixes;
    public bool GodotSubclasses;
    /// <summary>replace_calls: only call sites in these types (and the types nested in them),
    /// or in "Type::Method" (and that method's compiler-generated state machine or closures).</summary>
    public List<string>? InTypes;

    public static PatchSpec From(JsonElement e)
    {
        var p = new PatchSpec
        {
            Id = e.GetProperty("id").GetString()!,
            Action = e.GetProperty("action").GetString()!,
            Type = e.TryGetProperty("type", out var ty) ? ty.GetString()! : "",
            Method = e.TryGetProperty("method", out var me) ? me.GetString()! : "",
        };
        if (e.TryGetProperty("params", out var ps) && ps.ValueKind == JsonValueKind.Array)
            p.Params = ps.EnumerateArray().Select(x => x.GetString()!).ToList();
        if (e.TryGetProperty("all_overloads", out var ao)) p.AllOverloads = ao.GetBoolean();
        if (e.TryGetProperty("optional", out var op)) p.Optional = op.GetBoolean();
        if (e.TryGetProperty("enabled", out var en)) p.Enabled = en.GetBoolean();
        if (e.TryGetProperty("value", out var v)) p.Value = v.Clone();
        if (e.TryGetProperty("note", out var n)) p.Note = n.GetString();
        if (e.TryGetProperty("godot_subclasses", out var gs)) p.GodotSubclasses = gs.GetBoolean();
        if (e.TryGetProperty("namespaces", out var ns) && ns.ValueKind == JsonValueKind.Array)
            p.NamespacePrefixes = ns.EnumerateArray().Select(x => x.GetString()!).ToList();
        if (e.TryGetProperty("in_types", out var it) && it.ValueKind == JsonValueKind.Array)
            p.InTypes = it.EnumerateArray().Select(x => x.GetString()!.Replace('+', '/')).ToList();
        return p;
    }
}

internal sealed class PatchResult
{
    public string Id = "";
    public string Status = "";   // applied | skipped | missing | disabled | error
    public int Count;
    public List<string> Targets = new();
    public string? Message;
}

internal static class Program
{
    private static int Main(string[] args)
    {
        string? input = null, outDir = null, config = null, hookBridge = null, report = null;
        var search = new List<string>();
        for (int i = 0; i < args.Length; i++)
        {
            string Next() => i + 1 < args.Length ? args[++i] : throw new ArgumentException($"{args[i]} needs a value");
            switch (args[i])
            {
                case "--in": input = Next(); break;
                case "--out": outDir = Next(); break;
                case "--config": config = Next(); break;
                case "--hookbridge": hookBridge = Next(); break;
                case "--search": search.Add(Next()); break;
                case "--report": report = Next(); break;
                default:
                    Console.Error.WriteLine($"unknown argument {args[i]}");
                    return 2;
            }
        }
        if (input == null || outDir == null || config == null || hookBridge == null)
        {
            Console.Error.WriteLine("usage: Spire.Patcher --in sts2.dll --out DIR --config patches.json --hookbridge Spire.HookBridge.dll [--search DIR]... [--report FILE]");
            return 2;
        }

        var specs = JsonDocument.Parse(File.ReadAllText(config)).RootElement
            .GetProperty("patches").EnumerateArray().Select(PatchSpec.From).ToList();

        var resolver = new DefaultAssemblyResolver();
        resolver.AddSearchDirectory(Path.GetDirectoryName(Path.GetFullPath(input))!);
        resolver.AddSearchDirectory(Path.GetDirectoryName(Path.GetFullPath(hookBridge))!);
        foreach (var s in search) resolver.AddSearchDirectory(s);

        using var module = ModuleDefinition.ReadModule(input, new ReaderParameters
        {
            AssemblyResolver = resolver,
            ReadingMode = ReadingMode.Immediate,
            InMemory = true,
        });
        using var bridgeModule = ModuleDefinition.ReadModule(hookBridge);
        var patcher = new Patcher(module, bridgeModule);

        var results = new List<PatchResult>();
        bool failed = false;
        foreach (var spec in specs)
        {
            var r = patcher.Apply(spec);
            results.Add(r);
            var line = $"  [{r.Status,-8}] {spec.Id}: {r.Count} site(s)" + (r.Message != null ? $" - {r.Message}" : "");
            Console.WriteLine(line);
            if (r.Status == "error" || (r.Status == "missing" && !spec.Optional))
                failed = true;
        }

        Directory.CreateDirectory(outDir);
        var outPath = Path.Combine(outDir, Path.GetFileName(input));
        module.Write(outPath);
        Console.WriteLine($"wrote {outPath}");

        if (report != null)
        {
            using var fs = File.Create(report);
            using var w = new Utf8JsonWriter(fs, new JsonWriterOptions { Indented = true });
            w.WriteStartObject();
            w.WriteString("input", Path.GetFullPath(input));
            w.WriteString("input_sha256", Sha256(input));
            w.WriteStartArray("patches");
            foreach (var r in results)
            {
                w.WriteStartObject();
                w.WriteString("id", r.Id);
                w.WriteString("status", r.Status);
                w.WriteNumber("count", r.Count);
                w.WriteStartArray("targets");
                foreach (var t in r.Targets) w.WriteStringValue(t);
                w.WriteEndArray();
                if (r.Message != null) w.WriteString("message", r.Message);
                w.WriteEndObject();
            }
            w.WriteEndArray();
            w.WriteEndObject();
        }
        return failed ? 1 : 0;
    }

    private static string Sha256(string path)
    {
        using var f = File.OpenRead(path);
        return Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(f)).ToLowerInvariant();
    }
}

internal sealed class Patcher
{
    private readonly ModuleDefinition _module;
    private readonly MethodReference _invoke;
    private readonly MethodReference _after;

    public Patcher(ModuleDefinition module, ModuleDefinition bridge)
    {
        _module = module;
        var hooks = bridge.GetType("Spire.HookBridge.Hooks")
            ?? throw new InvalidOperationException("Spire.HookBridge.Hooks not found");
        _invoke = module.ImportReference(hooks.Methods.Single(m => m.Name == "Invoke"));
        _after = module.ImportReference(hooks.Methods.Single(m => m.Name == "After"));
    }

    public PatchResult Apply(PatchSpec spec)
    {
        var result = new PatchResult { Id = spec.Id };
        if (!spec.Enabled)
        {
            result.Status = "disabled";
            result.Message = spec.Note;
            return result;
        }
        try
        {
            if (spec.Action == "replace_calls")
                return ReplaceCalls(spec, result);
            if (spec.Action == "redirect_calls_into")
                return RedirectCallsInto(spec, result);
            if (spec.Action == "null_safe_calls_into")
                return NullSafeCallsInto(spec, result);
            if (spec.Action == "strip_bodies")
                return StripBodies(spec, result);

            var targets = FindMethods(spec, out var why);
            if (targets.Count == 0)
            {
                result.Status = "missing";
                result.Message = why;
                return result;
            }
            foreach (var m in targets)
            {
                if (!m.HasBody)
                    throw new InvalidOperationException($"{m.FullName} has no body");
                switch (spec.Action)
                {
                    case "return_default": ReplaceBody(m, il => EmitDefault(il, m.ReturnType)); break;
                    case "return_completed_task": ReplaceBody(m, il => EmitCompletedTask(il, m.ReturnType)); break;
                    case "return_const": ReplaceBody(m, il => EmitConst(il, m.ReturnType, spec.Value)); break;
                    case "redirect": ReplaceBody(m, il => EmitRedirect(il, m, spec.Id)); break;
                    case "call_after": InsertCallAfter(m, spec.Id); break;
                    default: throw new InvalidOperationException($"unknown action '{spec.Action}'");
                }
                result.Targets.Add(m.FullName);
                result.Count++;
            }
            result.Status = "applied";
        }
        catch (Exception ex)
        {
            result.Status = "error";
            result.Message = ex.Message;
        }
        return result;
    }

    // ---- lookup ----

    private TypeDefinition? FindType(string fullName)
    {
        // Nested types use '/' (Cecil) or '+' (reflection).
        var parts = fullName.Replace('+', '/').Split('/');
        var t = _module.GetType(parts[0]);
        for (int i = 1; t != null && i < parts.Length; i++)
            t = t.NestedTypes.FirstOrDefault(n => n.Name == parts[i]);
        return t;
    }

    private List<MethodDefinition> FindMethods(PatchSpec spec, out string? why)
    {
        why = null;
        if (spec.Type == "*")
        {
            // Any type: used when only the method name is stable across builds.
            var all = _module.GetTypes().SelectMany(t => t.Methods)
                .Where(m => m.Name == spec.Method && (spec.Params == null || ParamsMatch(m, spec.Params)))
                .ToList();
            if (all.Count == 0) why = $"no method named {spec.Method} in any type";
            else if (all.Count > 1 && !spec.AllOverloads)
                throw new InvalidOperationException($"{all.Count} methods named {spec.Method}; add \"all_overloads\": true");
            return all;
        }
        var type = FindType(spec.Type);
        if (type == null)
        {
            why = $"type {spec.Type} not found";
            return new();
        }
        var candidates = type.Methods.Where(m => m.Name == spec.Method).ToList();
        if (spec.Params != null)
            candidates = candidates.Where(m => ParamsMatch(m, spec.Params)).ToList();
        if (candidates.Count == 0)
        {
            var have = string.Join(", ", type.Methods.Where(m => m.Name == spec.Method)
                .Select(m => "(" + string.Join(",", m.Parameters.Select(p => p.ParameterType.FullName)) + ")"));
            why = $"method {spec.Type}::{spec.Method} not found" + (have.Length > 0 ? $"; overloads: {have}" : "");
            return candidates;
        }
        if (candidates.Count > 1 && !spec.AllOverloads)
        {
            why = $"{candidates.Count} overloads match; add \"params\" or \"all_overloads\": true";
            throw new InvalidOperationException(why);
        }
        return candidates;
    }

    private static bool ParamsMatch(MethodReference m, List<string> want)
    {
        if (m.Parameters.Count != want.Count) return false;
        for (int i = 0; i < want.Count; i++)
        {
            var have = m.Parameters[i].ParameterType;
            if (have.FullName != want[i] && have.Name != want[i]) return false;
        }
        return true;
    }

    // ---- body emission ----

    private static void ReplaceBody(MethodDefinition m, Action<ILProcessor> emit)
    {
        var body = m.Body;
        body.Instructions.Clear();
        body.ExceptionHandlers.Clear();
        body.Variables.Clear();
        body.InitLocals = true;
        var il = body.GetILProcessor();
        emit(il);
        il.Emit(OpCodes.Ret);
        body.Optimize();
    }

    private void EmitDefault(ILProcessor il, TypeReference type)
    {
        if (type.MetadataType == MetadataType.Void) return;
        if (!type.IsValueType && !type.IsGenericParameter)
        {
            il.Emit(OpCodes.Ldnull);
            return;
        }
        var v = new VariableDefinition(type);
        il.Body.Variables.Add(v);
        il.Emit(OpCodes.Ldloca, v);
        il.Emit(OpCodes.Initobj, type);
        il.Emit(OpCodes.Ldloc, v);
    }

    private void EmitCompletedTask(ILProcessor il, TypeReference ret)
    {
        var name = ret.Namespace + "." + ret.Name;
        if (name == "System.Threading.Tasks.Task")
        {
            var taskDef = _module.ImportReference(typeof(Task)).Resolve();
            var getter = taskDef.Properties.Single(p => p.Name == "CompletedTask").GetMethod;
            il.Emit(OpCodes.Call, _module.ImportReference(getter));
            return;
        }
        if (name == "System.Threading.Tasks.Task`1" && ret is GenericInstanceType git)
        {
            // Task.FromResult<T>(default(T))
            var taskDef = _module.ImportReference(typeof(Task)).Resolve();
            var fromResult = taskDef.Methods.Single(mm => mm.Name == "FromResult" && mm.HasGenericParameters);
            var inst = new GenericInstanceMethod(_module.ImportReference(fromResult));
            inst.GenericArguments.Add(git.GenericArguments[0]);
            EmitDefault(il, git.GenericArguments[0]);
            il.Emit(OpCodes.Call, inst);
            return;
        }
        if (name == "System.Threading.Tasks.ValueTask" || name == "System.Threading.Tasks.ValueTask`1")
        {
            // default(ValueTask) / default(ValueTask<T>) are completed.
            EmitDefault(il, ret);
            return;
        }
        throw new InvalidOperationException($"return_completed_task on a method returning {ret.FullName}");
    }

    private void EmitConst(ILProcessor il, TypeReference ret, JsonElement? value)
    {
        if (value == null) throw new InvalidOperationException("return_const needs \"value\"");
        var v = value.Value;
        switch (ret.MetadataType)
        {
            case MetadataType.Boolean:
                il.Emit(v.GetBoolean() ? OpCodes.Ldc_I4_1 : OpCodes.Ldc_I4_0);
                break;
            case MetadataType.Int32:
            case MetadataType.UInt32:
            case MetadataType.Int16:
            case MetadataType.UInt16:
            case MetadataType.Byte:
            case MetadataType.SByte:
                il.Emit(OpCodes.Ldc_I4, v.GetInt32());
                break;
            case MetadataType.Int64:
                il.Emit(OpCodes.Ldc_I8, v.GetInt64());
                break;
            case MetadataType.String:
                if (v.ValueKind == JsonValueKind.Null) il.Emit(OpCodes.Ldnull);
                else il.Emit(OpCodes.Ldstr, v.GetString());
                break;
            default:
                throw new InvalidOperationException($"return_const does not support {ret.FullName}");
        }
    }

    /// <summary>Pushes object[] { this?, arg0, ... } with every value boxed.</summary>
    private void EmitArgsArray(ILProcessor il, MethodDefinition m)
    {
        int extra = m.HasThis ? 1 : 0;
        int n = m.Parameters.Count + extra;
        il.Emit(OpCodes.Ldc_I4, n);
        il.Emit(OpCodes.Newarr, _module.TypeSystem.Object);
        for (int i = 0; i < n; i++)
        {
            il.Emit(OpCodes.Dup);
            il.Emit(OpCodes.Ldc_I4, i);
            if (m.HasThis && i == 0)
            {
                il.Emit(OpCodes.Ldarg_0);
                var declaring = SelfType(m);
                if (declaring.IsValueType)
                {
                    il.Emit(OpCodes.Ldobj, declaring);
                    il.Emit(OpCodes.Box, declaring);
                }
            }
            else
            {
                var p = m.Parameters[i - extra];
                if (p.ParameterType.IsByReference)
                    throw new InvalidOperationException($"{m.FullName}: by-ref parameter '{p.Name}' is not supported");
                il.Emit(OpCodes.Ldarg, p);
                EmitBoxIfNeeded(il, p.ParameterType);
            }
            il.Emit(OpCodes.Stelem_Ref);
        }
    }

    private static TypeReference SelfType(MethodDefinition m)
    {
        TypeReference t = m.DeclaringType;
        if (m.DeclaringType.HasGenericParameters)
        {
            var git = new GenericInstanceType(m.DeclaringType);
            foreach (var gp in m.DeclaringType.GenericParameters) git.GenericArguments.Add(gp);
            t = git;
        }
        return t;
    }

    private static void EmitBoxIfNeeded(ILProcessor il, TypeReference t)
    {
        if (t.IsValueType || t.IsGenericParameter)
            il.Emit(OpCodes.Box, t);
    }

    private static void EmitUnboxResult(ILProcessor il, TypeReference ret)
    {
        if (ret.MetadataType == MetadataType.Void)
        {
            il.Emit(OpCodes.Pop);
        }
        else if (ret.IsValueType || ret.IsGenericParameter)
        {
            il.Emit(OpCodes.Unbox_Any, ret);
        }
        else
        {
            il.Emit(OpCodes.Castclass, ret);
        }
    }

    private void EmitRedirect(ILProcessor il, MethodDefinition m, string id)
    {
        il.Emit(OpCodes.Ldstr, id);
        EmitArgsArray(il, m);
        il.Emit(OpCodes.Call, _invoke);
        EmitUnboxResult(il, m.ReturnType);
    }

    private void InsertCallAfter(MethodDefinition m, string id)
    {
        var body = m.Body;
        body.SimplifyMacros();
        var il = body.GetILProcessor();
        var rets = body.Instructions.Where(i => i.OpCode == OpCodes.Ret).ToList();
        bool hasResult = m.ReturnType.MetadataType != MetadataType.Void;
        VariableDefinition? resultVar = null;
        if (hasResult)
        {
            resultVar = new VariableDefinition(m.ReturnType);
            body.Variables.Add(resultVar);
        }
        foreach (var ret in rets)
        {
            // [result on stack] -> stloc r; ldstr id; args[]; ldloc r; box; call After; ldloc r; ret
            var seq = new List<Instruction>();
            if (hasResult) seq.Add(il.Create(OpCodes.Stloc, resultVar!));
            seq.Add(il.Create(OpCodes.Ldstr, id));
            var argsIl = new List<Instruction>();
            var tmp = new MethodBody(m);
            var tmpIl = tmp.GetILProcessor();
            EmitArgsArray(tmpIl, m);
            seq.AddRange(tmp.Instructions);
            if (hasResult)
            {
                seq.Add(il.Create(OpCodes.Ldloc, resultVar!));
                if (m.ReturnType.IsValueType || m.ReturnType.IsGenericParameter)
                    seq.Add(il.Create(OpCodes.Box, m.ReturnType));
            }
            else
            {
                seq.Add(il.Create(OpCodes.Ldnull));
            }
            seq.Add(il.Create(OpCodes.Call, _after));
            if (hasResult) seq.Add(il.Create(OpCodes.Ldloc, resultVar!));

            // Insert before ret; branches that targeted ret must now target the
            // first inserted instruction, so swap the ret instruction's content.
            var first = seq[0];
            var retCopy = il.Create(OpCodes.Ret);
            ret.OpCode = first.OpCode;
            ret.Operand = first.Operand;
            var anchor = ret;
            for (int k = 1; k < seq.Count; k++)
            {
                il.InsertAfter(anchor, seq[k]);
                anchor = seq[k];
            }
            il.InsertAfter(anchor, retCopy);
        }
        body.OptimizeMacros();
    }

    // ---- replace_calls ----

    private PatchResult ReplaceCalls(PatchSpec spec, PatchResult result)
    {
        int count = 0;
        foreach (var type in _module.GetTypes())
        {
            if (spec.InTypes != null && !spec.InTypes.Any(t => TypeInScope(type, t)))
                continue;
            foreach (var method in type.Methods.Where(mm => mm.HasBody))
            {
                if (spec.InTypes != null && !spec.InTypes.Any(t => MethodInScope(type, method, t)))
                    continue;
                var body = method.Body;
                bool changed = false;
                for (int idx = 0; idx < body.Instructions.Count; idx++)
                {
                    var ins = body.Instructions[idx];
                    if ((ins.OpCode != OpCodes.Call && ins.OpCode != OpCodes.Callvirt) || ins.Operand is not MethodReference target)
                        continue;
                    var declName = target.DeclaringType.FullName;
                    if ((declName != spec.Type && target.DeclaringType.Name != spec.Type) || target.Name != spec.Method)
                        continue;
                    if (spec.Params != null && !ParamsMatch(target, spec.Params))
                        continue;
                    if (target.HasThis)
                        throw new InvalidOperationException($"replace_calls only supports static targets ({target.FullName})");
                    if (!changed)
                    {
                        body.SimplifyMacros();
                        changed = true;
                    }
                    ReplaceCallSite(method, ins, target, spec.Id);
                    count++;
                    if (!result.Targets.Contains(method.FullName))
                        result.Targets.Add(method.FullName);
                }
                if (changed) body.OptimizeMacros();
            }
        }
        result.Count = count;
        result.Status = count > 0 ? "applied" : "missing";
        if (count == 0) result.Message = $"no calls to {spec.Type}::{spec.Method}";
        return result;
    }

    // Compiler-generated state machines and closures are nested in the type
    // that declares the method (<Method>d__N, <>c__DisplayClass...), so they
    // belong to its scope; a "Type::Method" scope takes only that method's.
    private static bool TypeInScope(TypeDefinition type, string scope)
    {
        var sep = scope.IndexOf("::", StringComparison.Ordinal);
        var t = sep < 0 ? scope : scope[..sep];
        return type.FullName == t || type.FullName.StartsWith(t + "/", StringComparison.Ordinal);
    }

    private static bool MethodInScope(TypeDefinition type, MethodDefinition method, string scope)
    {
        if (!TypeInScope(type, scope)) return false;
        var sep = scope.IndexOf("::", StringComparison.Ordinal);
        if (sep < 0) return true;
        var t = scope[..sep];
        var m = scope[(sep + 2)..];
        if (type.FullName == t) return method.Name == m;
        return type.FullName.StartsWith(t + "/<" + m + ">", StringComparison.Ordinal);
    }

    private void ReplaceCallSite(MethodDefinition caller, Instruction call, MethodReference target, string id)
    {
        // Arguments are on the stack. Store them to fresh locals (last first),
        // then build the object[] and call Hooks.Invoke.
        var il = caller.Body.GetILProcessor();
        var locals = new List<VariableDefinition>();
        foreach (var p in target.Parameters)
        {
            var pt = ResolveGenericArg(p.ParameterType, target);
            var v = new VariableDefinition(_module.ImportReference(pt));
            caller.Body.Variables.Add(v);
            locals.Add(v);
        }
        var seq = new List<Instruction>();
        for (int i = locals.Count - 1; i >= 0; i--)
            seq.Add(il.Create(OpCodes.Stloc, locals[i]));
        seq.Add(il.Create(OpCodes.Ldstr, id));
        seq.Add(il.Create(OpCodes.Ldc_I4, locals.Count));
        seq.Add(il.Create(OpCodes.Newarr, _module.TypeSystem.Object));
        for (int i = 0; i < locals.Count; i++)
        {
            seq.Add(il.Create(OpCodes.Dup));
            seq.Add(il.Create(OpCodes.Ldc_I4, i));
            seq.Add(il.Create(OpCodes.Ldloc, locals[i]));
            var t = locals[i].VariableType;
            if (t.IsValueType || t.IsGenericParameter) seq.Add(il.Create(OpCodes.Box, t));
            seq.Add(il.Create(OpCodes.Stelem_Ref));
        }
        seq.Add(il.Create(OpCodes.Call, _invoke));
        var ret = _module.ImportReference(ResolveGenericArg(target.ReturnType, target));
        if (ret.MetadataType == MetadataType.Void) seq.Add(il.Create(OpCodes.Pop));
        else if (ret.IsValueType || ret.IsGenericParameter) seq.Add(il.Create(OpCodes.Unbox_Any, ret));
        else seq.Add(il.Create(OpCodes.Castclass, ret));

        // Reuse the call instruction as the first of the sequence so branch
        // targets and exception-handler boundaries stay valid.
        var first = seq[0];
        call.OpCode = first.OpCode;
        call.Operand = first.Operand;
        var anchor = call;
        for (int k = 1; k < seq.Count; k++)
        {
            il.InsertAfter(anchor, seq[k]);
            anchor = seq[k];
        }
    }

    private static bool InNamespaces(TypeReference t, List<string> prefixes)
    {
        while (t.IsNested) t = t.DeclaringType;
        var ns = t.Namespace ?? "";
        return prefixes.Any(p => ns == p || ns.StartsWith(p + ".", StringComparison.Ordinal));
    }

    private readonly Dictionary<TypeDefinition, bool> _godotDerived = new();

    /// <summary>True for classes of this module that derive (transitively) from a Godot class.</summary>
    private bool DerivesFromGodot(TypeReference t)
    {
        if (t.Module != _module && t.Scope != _module) return false;
        TypeDefinition? def;
        try { def = t.Resolve(); } catch { return false; }
        if (def == null || def.Module != _module) return false;
        if (_godotDerived.TryGetValue(def, out var known)) return known;
        _godotDerived[def] = false;
        bool result = false;
        var b = def.BaseType;
        if (b != null)
        {
            var ns = b.GetElementType().Namespace ?? "";
            result = ns == "Godot" || ns.StartsWith("Godot.", StringComparison.Ordinal) || DerivesFromGodot(b);
        }
        if (!result && def.IsNested) result = DerivesFromGodot(def.DeclaringType);
        _godotDerived[def] = result;
        return result;
    }

    private PatchResult RedirectCallsInto(PatchSpec spec, PatchResult result)
    {
        var prefixes = spec.NamespacePrefixes ?? throw new InvalidOperationException("redirect_calls_into needs \"namespaces\"");
        bool IsUi(TypeReference t) => InNamespaces(t, prefixes) || (spec.GodotSubclasses && DerivesFromGodot(t.GetElementType()));
        int count = 0;
        var callers = new HashSet<string>();
        foreach (var type in _module.GetTypes())
        {
            if (IsUi(type)) continue;
            foreach (var method in type.Methods.Where(mm => mm.HasBody))
            {
                var body = method.Body;
                bool changed = false;
                for (int idx = 0; idx < body.Instructions.Count; idx++)
                {
                    var ins = body.Instructions[idx];
                    if ((ins.OpCode != OpCodes.Call && ins.OpCode != OpCodes.Callvirt) || ins.Operand is not MethodReference target)
                        continue;
                    if (!IsUi(target.DeclaringType)) continue;
                    if (target.Name == ".ctor") continue;
                    if (target.DeclaringType.IsValueType) continue;
                    if (ins.Previous != null && ins.Previous.OpCode == OpCodes.Constrained) continue;
                    if (target.Parameters.Any(p => p.ParameterType.IsByReference || p.ParameterType.IsPointer)) continue;
                    if (target.ReturnType.IsByReference || target.ReturnType.IsPointer) continue;
                    if (!changed)
                    {
                        body.SimplifyMacros();
                        changed = true;
                    }
                    var name = spec.Id + "|" + target.DeclaringType.GetElementType().FullName + "::" + target.Name;
                    ReplaceCallWithDefault(method, ins, target, name);
                    count++;
                    callers.Add(method.FullName);
                }
                if (changed) body.OptimizeMacros();
            }
        }
        result.Count = count;
        result.Targets.AddRange(callers.OrderBy(x => x).Take(50));
        if (callers.Count > 50) result.Targets.Add($"... and {callers.Count - 50} more methods");
        result.Status = count > 0 ? "applied" : "missing";
        return result;
    }

    /// <summary>
    /// Every method (constructors aside) of every Godot-derived type in the
    /// given namespaces throws NotSupportedException("headless: Type::Method"). For code the
    /// headless logic never runs (the Godot UI, which the 'ui' patch already cut
    /// off): the AOT builds then compile a throw instead of the body.
    /// </summary>
    private PatchResult StripBodies(PatchSpec spec, PatchResult result)
    {
        var prefixes = spec.NamespacePrefixes ?? throw new InvalidOperationException("strip_bodies needs \"namespaces\"");
        var ctor = _module.ImportReference(typeof(NotSupportedException).GetConstructor(new[] { typeof(string) }));
        int count = 0;
        foreach (var type in _module.GetTypes())
        {
            if (!InNamespaces(type, prefixes)) continue;
            if (type.FullName == "MegaCrit.Sts2.Core.Nodes.Audio.NRunMusicController/MusicSelection") continue;
            // Only Godot objects (nodes, resources) and what is nested in them:
            // plain helpers in these namespaces (EpochComparer, sorting saved
            // epochs) are used by the logic.
            if (!DerivesFromGodot(type)) continue;
            foreach (var m in type.Methods)
            {
                if (!m.HasBody || m.IsConstructor) continue;
                // A pure seeded selection helper, also used by the adapter's
                // presentation export; it never touches a Godot node.
                if (type.FullName == "MegaCrit.Sts2.Core.Nodes.Audio.NRunMusicController" && m.Name == "ResolveMusic") continue;
                var body = m.Body;
                body.Instructions.Clear();
                body.ExceptionHandlers.Clear();
                body.Variables.Clear();
                var il = body.GetILProcessor();
                il.Emit(OpCodes.Ldstr, "headless: " + type.FullName + "::" + m.Name);
                il.Emit(OpCodes.Newobj, ctor);
                il.Emit(OpCodes.Throw);
                count++;
            }
        }
        result.Count = count;
        result.Status = count > 0 ? "applied" : "missing";
        return result;
    }

    private PatchResult NullSafeCallsInto(PatchSpec spec, PatchResult result)
    {
        var prefixes = spec.NamespacePrefixes ?? throw new InvalidOperationException("null_safe_calls_into needs \"namespaces\"");
        int count = 0;
        var callers = new HashSet<string>();
        foreach (var type in _module.GetTypes())
        {
            if (InNamespaces(type, prefixes)) continue;
            foreach (var method in type.Methods.Where(mm => mm.HasBody))
            {
                var body = method.Body;
                bool changed = false;
                for (int idx = 0; idx < body.Instructions.Count; idx++)
                {
                    var ins = body.Instructions[idx];
                    if (ins.OpCode != OpCodes.Callvirt || ins.Operand is not MethodReference target) continue;
                    if (!target.HasThis || !InNamespaces(target.DeclaringType, prefixes)) continue;
                    if (target.DeclaringType.IsValueType) continue;
                    if (ins.Previous != null && ins.Previous.OpCode == OpCodes.Constrained) continue;
                    if (target.Parameters.Any(p => p.ParameterType.IsByReference || p.ParameterType.IsPointer)) continue;
                    if (target.ReturnType.IsByReference || target.ReturnType.IsPointer) continue;
                    if (!changed)
                    {
                        body.SimplifyMacros();
                        changed = true;
                    }
                    // Skip past the inserted sequence (it contains a new callvirt
                    // to the same target, which must not be guarded again).
                    idx += GuardReceiver(method, ins, target) - 1;
                    count++;
                    callers.Add(method.FullName);
                }
                if (changed) body.OptimizeMacros();
            }
        }
        result.Count = count;
        result.Targets.AddRange(callers.OrderBy(x => x).Take(50));
        if (callers.Count > 50) result.Targets.Add($"... and {callers.Count - 50} more methods");
        result.Status = count > 0 ? "applied" : "missing";
        return result;
    }

    /// <summary>
    /// [recv, a0..an] callvirt M  ->  store all; if recv == null: safe default
    /// else: reload and callvirt M. The original instruction becomes the first
    /// store, so branches into the call site still land at its start.
    /// </summary>
    private int GuardReceiver(MethodDefinition caller, Instruction call, MethodReference target)
    {
        var il = caller.Body.GetILProcessor();
        var self = new VariableDefinition(_module.ImportReference(target.DeclaringType));
        caller.Body.Variables.Add(self);
        var locals = new List<VariableDefinition>();
        foreach (var p in target.Parameters)
        {
            var v = new VariableDefinition(_module.ImportReference(Concrete(p.ParameterType, target)));
            caller.Body.Variables.Add(v);
            locals.Add(v);
        }
        var ret = _module.ImportReference(Concrete(target.ReturnType, target));
        var realCall = il.Create(OpCodes.Callvirt, target);
        var end = il.Create(OpCodes.Nop);
        var callPath = il.Create(OpCodes.Ldloc, self);

        var seq = new List<Instruction>();
        for (int i = locals.Count - 1; i >= 0; i--)
            seq.Add(il.Create(OpCodes.Stloc, locals[i]));
        seq.Add(il.Create(OpCodes.Stloc, self));
        seq.Add(il.Create(OpCodes.Ldloc, self));
        seq.Add(il.Create(OpCodes.Brtrue, callPath));
        seq.AddRange(SafeDefault(il, caller, ret));
        seq.Add(il.Create(OpCodes.Br, end));
        seq.Add(callPath);
        foreach (var v in locals) seq.Add(il.Create(OpCodes.Ldloc, v));
        seq.Add(realCall);
        seq.Add(end);

        var first = seq[0];
        call.OpCode = first.OpCode;
        call.Operand = first.Operand;
        var anchor = call;
        for (int k = 1; k < seq.Count; k++)
        {
            il.InsertAfter(anchor, seq[k]);
            anchor = seq[k];
        }
        return seq.Count;
    }

    /// <summary>Resolves !0 / !!0 in a member reference's signature to concrete types.</summary>
    private TypeReference Concrete(TypeReference t, MethodReference m)
    {
        if (t is GenericParameter gp)
        {
            if (gp.Type == GenericParameterType.Method && m is GenericInstanceMethod gim)
                return gim.GenericArguments[gp.Position];
            if (gp.Type == GenericParameterType.Type && m.DeclaringType is GenericInstanceType git)
                return git.GenericArguments[gp.Position];
            return t;
        }
        if (t is GenericInstanceType inst && inst.GenericArguments.Any(a => a.ContainsGenericParameter))
        {
            var copy = new GenericInstanceType(inst.ElementType);
            foreach (var a in inst.GenericArguments) copy.GenericArguments.Add(Concrete(a, m));
            return copy;
        }
        if (t is ArrayType at && at.ElementType.ContainsGenericParameter)
            return new ArrayType(Concrete(at.ElementType, m), at.Rank);
        return t;
    }

    private void ReplaceCallWithDefault(MethodDefinition caller, Instruction call, MethodReference target, string id)
    {
        var il = caller.Body.GetILProcessor();
        var locals = new List<VariableDefinition>();
        if (target.HasThis)
        {
            var self = new VariableDefinition(_module.TypeSystem.Object);
            caller.Body.Variables.Add(self);
            locals.Add(self);
        }
        foreach (var p in target.Parameters)
        {
            var v = new VariableDefinition(_module.ImportReference(Concrete(p.ParameterType, target)));
            caller.Body.Variables.Add(v);
            locals.Add(v);
        }
        var seq = new List<Instruction>();
        for (int i = locals.Count - 1; i >= 0; i--)
            seq.Add(il.Create(OpCodes.Stloc, locals[i]));
        seq.Add(il.Create(OpCodes.Ldstr, id));
        seq.Add(il.Create(OpCodes.Ldc_I4, locals.Count));
        seq.Add(il.Create(OpCodes.Newarr, _module.TypeSystem.Object));
        for (int i = 0; i < locals.Count; i++)
        {
            seq.Add(il.Create(OpCodes.Dup));
            seq.Add(il.Create(OpCodes.Ldc_I4, i));
            seq.Add(il.Create(OpCodes.Ldloc, locals[i]));
            var t = locals[i].VariableType;
            if (t.IsValueType || t.IsGenericParameter) seq.Add(il.Create(OpCodes.Box, t));
            seq.Add(il.Create(OpCodes.Stelem_Ref));
        }
        seq.Add(il.Create(OpCodes.Call, _invoke));
        seq.Add(il.Create(OpCodes.Pop));
        var ret = _module.ImportReference(Concrete(target.ReturnType, target));
        seq.AddRange(SafeDefault(il, caller, ret));

        var first = seq[0];
        call.OpCode = first.OpCode;
        call.Operand = first.Operand;
        var anchor = call;
        for (int k = 1; k < seq.Count; k++)
        {
            il.InsertAfter(anchor, seq[k]);
            anchor = seq[k];
        }
    }

    private static readonly HashSet<string> SequenceInterfaces = new()
    {
        "System.Collections.Generic.IEnumerable`1", "System.Collections.Generic.IReadOnlyList`1",
        "System.Collections.Generic.IReadOnlyCollection`1", "System.Collections.Generic.IList`1",
        "System.Collections.Generic.ICollection`1",
    };

    /// <summary>A value of the given type that callers can use without crashing.</summary>
    private List<Instruction> SafeDefault(ILProcessor il, MethodDefinition caller, TypeReference ret)
    {
        var seq = new List<Instruction>();
        if (ret.MetadataType == MetadataType.Void) return seq;
        var elementName = ret.GetElementType().FullName;
        // Audio singleton receivers must survive ?. guards so the following
        // call reaches the UI bridge. Their constructors only initialize
        // local fields; the redirected calls never execute a UI body.
        if (elementName is "MegaCrit.Sts2.Core.Nodes.Audio.NRunMusicController" or
            "MegaCrit.Sts2.Core.Nodes.Audio.NAudioManager" or
            "MegaCrit.Sts2.Core.Audio.Debug.NDebugAudioManager")
        {
            var ctor = ret.Resolve().Methods.Single(m => m.IsConstructor && !m.IsStatic && m.Parameters.Count == 0);
            seq.Add(il.Create(OpCodes.Newobj, _module.ImportReference(ctor)));
            return seq;
        }
        var taskDef = _module.ImportReference(typeof(Task)).Resolve();
        if (ret.FullName == "System.Threading.Tasks.Task")
        {
            seq.Add(il.Create(OpCodes.Call, _module.ImportReference(taskDef.Properties.Single(p => p.Name == "CompletedTask").GetMethod)));
            return seq;
        }
        if (elementName == "System.Threading.Tasks.Task`1" && ret is GenericInstanceType tgit)
        {
            var fromResult = new GenericInstanceMethod(_module.ImportReference(taskDef.Methods.Single(mm => mm.Name == "FromResult" && mm.HasGenericParameters)));
            fromResult.GenericArguments.Add(tgit.GenericArguments[0]);
            seq.AddRange(PlainDefault(il, caller, tgit.GenericArguments[0]));
            seq.Add(il.Create(OpCodes.Call, fromResult));
            return seq;
        }
        if (ret is ArrayType at && at.Rank == 1)
        {
            seq.Add(il.Create(OpCodes.Ldc_I4_0));
            seq.Add(il.Create(OpCodes.Newarr, at.ElementType));
            return seq;
        }
        if (SequenceInterfaces.Contains(elementName) && ret is GenericInstanceType sgit)
        {
            var arrayDef = _module.ImportReference(typeof(Array)).Resolve();
            var empty = new GenericInstanceMethod(_module.ImportReference(arrayDef.Methods.Single(mm => mm.Name == "Empty")));
            empty.GenericArguments.Add(sgit.GenericArguments[0]);
            seq.Add(il.Create(OpCodes.Call, empty));
            return seq;
        }
        // Awaitables the game awaits (Godot.SignalAwaiter): a fresh one is completed.
        if (!ret.IsValueType && ret.Name.EndsWith("Awaiter", StringComparison.Ordinal))
        {
            var def = ret.Resolve();
            var ctor = def?.Methods.FirstOrDefault(m => m.IsConstructor && !m.IsStatic && m.Parameters.Count == 0 && m.IsPublic);
            if (ctor != null)
            {
                seq.Add(il.Create(OpCodes.Newobj, _module.ImportReference(ctor)));
                return seq;
            }
        }
        seq.AddRange(PlainDefault(il, caller, ret));
        return seq;
    }

    private List<Instruction> PlainDefault(ILProcessor il, MethodDefinition caller, TypeReference t)
    {
        var seq = new List<Instruction>();
        if (t.MetadataType == MetadataType.String)
        {
            seq.Add(il.Create(OpCodes.Ldstr, ""));
            return seq;
        }
        if (!t.IsValueType && !t.IsGenericParameter)
        {
            seq.Add(il.Create(OpCodes.Ldnull));
            return seq;
        }
        var v = new VariableDefinition(t);
        caller.Body.Variables.Add(v);
        seq.Add(il.Create(OpCodes.Ldloca, v));
        seq.Add(il.Create(OpCodes.Initobj, t));
        seq.Add(il.Create(OpCodes.Ldloc, v));
        return seq;
    }

    private static TypeReference ResolveGenericArg(TypeReference t, MethodReference m)
    {
        if (t is GenericParameter gp && gp.Type == GenericParameterType.Method && m is GenericInstanceMethod gim)
            return gim.GenericArguments[gp.Position];
        return t;
    }
}
