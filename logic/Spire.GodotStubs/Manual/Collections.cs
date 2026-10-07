// Forked from sts2-cli (https://github.com/Garglemesh/sts2-cli, MIT, (c) 2025 Hao Wu).
// See THIRD_PARTY.md. Local changes are marked "Spire360:".
namespace Godot.Collections;

// Godot Array<T> wrapper
public partial class Array<T> : List<T>
{
    public Array() { }
    public Array(IEnumerable<T> items) : base(items) { }
}

// Godot Dictionary
public partial class Dictionary<TKey, TValue> : System.Collections.Generic.Dictionary<TKey, TValue>
    where TKey : notnull
{
    public Dictionary() { }
}

// Non-generic Array
public partial class Array : List<Variant>
{
    public Array() { }
}
