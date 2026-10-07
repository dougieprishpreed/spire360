// Forked from sts2-cli (https://github.com/Garglemesh/sts2-cli, MIT, (c) 2025 Hao Wu).
// See THIRD_PARTY.md. Local changes are marked "Spire360:".
namespace Godot.Bridge;

public static partial class ScriptManagerBridge
{
    public static void FrameworkGetGodotMethodList(IntPtr handle) { }
}

public static partial class CSharpInstanceBridge { }

// Spire360: moved here from Godot (the real type is Godot.Bridge.GodotSerializationInfo).
public partial class GodotSerializationInfo
{
    public void AddProperty(string name, Variant value) { }
    public bool TryGetProperty(string name, out Variant value) { value = default; return false; }
}
