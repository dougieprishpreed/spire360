// Spire.ApiDump --calls <assembly.dll> <Type::Method>  (also matches compiler-generated
// state machines of async methods) - lists the methods it calls, in order.
using Mono.Cecil;
using Mono.Cecil.Cil;

static class Calls
{
    public static int Run(string asm, string target)
    {
        var parts = target.Split("::");
        var module = ModuleDefinition.ReadModule(asm, new ReaderParameters { ReadingMode = ReadingMode.Deferred });
        foreach (var t in module.GetTypes().Where(t => t.Name == parts[0] || t.FullName == parts[0]))
        {
            var methods = t.Methods.Where(m => m.Name == parts[1]).ToList();
            // async state machines: <Method>d__N.MoveNext
            methods.AddRange(t.NestedTypes.Where(n => n.Name.StartsWith("<" + parts[1] + ">"))
                .SelectMany(n => n.Methods.Where(m => m.Name == "MoveNext")));
            foreach (var m in methods.Where(m => m.HasBody))
            {
                Console.WriteLine($"== {m.FullName}");
                foreach (var ins in m.Body.Instructions)
                {
                    if (ins.Operand is MethodReference mr && (ins.OpCode == OpCodes.Call || ins.OpCode == OpCodes.Callvirt || ins.OpCode == OpCodes.Newobj))
                        Console.WriteLine($"  {ins.OpCode,-8} {mr.DeclaringType.FullName}::{mr.Name}");
                    else if (ins.OpCode == OpCodes.Ldstr)
                        Console.WriteLine($"  ldstr    \"{ins.Operand}\"");
                    else if (ins.Operand is FieldReference fr && (ins.OpCode == OpCodes.Ldsfld || ins.OpCode == OpCodes.Ldfld))
                        Console.WriteLine($"  {ins.OpCode,-8} {fr.DeclaringType.Name}::{fr.Name}");
                }
            }
        }
        return 0;
    }
}
