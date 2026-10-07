// Forked from sts2-cli (https://github.com/Garglemesh/sts2-cli, MIT, (c) 2025 Hao Wu).
// See THIRD_PARTY.md. Local changes are marked "Spire360:".
namespace Godot.NativeInterop;

using Godot;

public static partial class VariantUtils
{
    public static T ConvertTo<T>(Variant v) => default!;
    public static Variant CreateFrom<T>(T value) => new Variant(value);
    public static Variant CreateFromString(string s) => new Variant(s);
    public static Variant CreateFromFloat(float f) => new Variant(f);
    public static Variant CreateFromDouble(double d) => new Variant(d);
    public static Variant CreateFromInt(int i) => new Variant(i);
    public static Variant CreateFromBool(bool b) => new Variant(b);
    public static Variant CreateFromStringName(StringName s) => new Variant(s);
}

// Low-level native interop types used in generated bridge code
public partial struct godot_string_name { }
public partial struct godot_variant { }
public enum godot_bool : byte
{
    True = 1,
    False = 0,
}
public partial struct NativeGodotVariant { }
public partial struct NativeGodotString { }
public partial struct NativeGodotStringName { }

// NativeVariantPtrArgs - used in signal dispatch bridge
public readonly partial struct NativeVariantPtrArgs
{
    public readonly ref godot_variant this[int index] => ref System.Runtime.CompilerServices.Unsafe.NullRef<godot_variant>();
    public int Count => 0;
}
