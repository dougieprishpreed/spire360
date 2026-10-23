// Spire.ApiDump <assembly.dll> <type-name-or-regex>...
// Prints matching types with base type, methods, properties, fields, events.
// A pattern matches the simple name exactly, or the full name as a regex when
// it starts with '~' (e.g. "~Rooms\.").
using System.Text.RegularExpressions;
using Mono.Cecil;

if (args.Length >= 3 && args[0] == "--calls")
    return Calls.Run(args[1], args[2]);
if (args.Length >= 3 && args[0] == "--ui-surface")
    return UiSurface.Run(args[1], args[2]);
if (args.Length >= 5 && args[0] == "--gen-stubs")
    return StubGen.Run(args[1], args[2], args[3], args[4]);
if (args.Length >= 2 && args[0] == "--real-bases")
    return RealBases.Run(args[1]);
if (args.Length >= 3 && args[0] == "--bases")
    return Bases.Run(args[1], args[2]);
if (args.Length >= 3 && args[0] == "--print-enums")
    return PrintEnums.Run(args[1], args.Skip(2));
if (args.Length >= 3 && args[0] == "--check-stubs")
    return CheckStubs.Run(args[1], args[2]);
if (args.Length >= 3 && args[0] == "--stub-gaps")
    return StubGaps.Run(args[1], args[2], args.Length > 3 ? args[3] : null);
if (args.Length < 2)
{
    Console.Error.WriteLine("usage: Spire.ApiDump <assembly.dll> <TypeName | ~regex>...");
    return 2;
}
var module = ModuleDefinition.ReadModule(args[0], new ReaderParameters { ReadingMode = ReadingMode.Deferred });
var patterns = args.Skip(1).ToList();
bool Match(TypeDefinition t) => patterns.Any(p =>
    p.StartsWith('~') ? Regex.IsMatch(t.FullName, p[1..]) : t.Name == p || t.FullName == p);

string Sig(MethodDefinition m) =>
    $"{(m.IsStatic ? "static " : "")}{(m.IsPublic ? "" : m.IsFamily ? "protected " : "private ")}{(m.IsVirtual ? "virtual " : "")}" +
    $"{m.ReturnType.Name} {m.Name}{(m.HasGenericParameters ? "<" + string.Join(",", m.GenericParameters.Select(g => g.Name)) + ">" : "")}" +
    $"({string.Join(", ", m.Parameters.Select(p => p.ParameterType.Name + " " + p.Name))})";

foreach (var t in module.GetTypes().Where(Match).OrderBy(t => t.FullName))
{
    Console.WriteLine($"\n== {t.FullName} : {t.BaseType?.FullName}" +
        (t.HasInterfaces ? " [" + string.Join(", ", t.Interfaces.Select(i => i.InterfaceType.Name)) + "]" : ""));
    foreach (var p in t.Properties)
        Console.WriteLine($"  P {(p.GetMethod?.IsStatic == true ? "static " : "")}{p.PropertyType.Name} {p.Name}");
    foreach (var f in t.Fields.Where(f => !f.Name.Contains("k__BackingField")))
        Console.WriteLine($"  F {(f.IsStatic ? "static " : "")}{(f.IsPublic ? "" : "private ")}{f.FieldType.Name} {f.Name}");
    foreach (var e in t.Events)
        Console.WriteLine($"  E {e.EventType.Name} {e.Name}");
    foreach (var m in t.Methods.Where(m => !m.IsGetter && !m.IsSetter && !m.IsAddOn && !m.IsRemoveOn).OrderBy(m => m.Name))
        Console.WriteLine($"  M {Sig(m)}");
}
return 0;
