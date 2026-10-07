// Spire360: interfaces the real GodotSharp types implement. Callers compiled
// against the real API rely on them (`using` on a Variant, IEquatable<T>
// constrained calls); see Spire.ApiDump --check-stubs.
namespace Godot
{
    public partial struct Variant : System.IDisposable
    {
        public void Dispose() { }
    }

    public partial struct Vector2 : System.IEquatable<Vector2>
    {
        public bool Equals(Vector2 other) => this == other;
    }

    public partial struct Vector2I : System.IEquatable<Vector2I>
    {
        public bool Equals(Vector2I other) => X == other.X && Y == other.Y;
    }

    public partial struct Vector3 : System.IEquatable<Vector3>
    {
        public bool Equals(Vector3 other) => X == other.X && Y == other.Y && Z == other.Z;
    }

    public partial struct Color : System.IEquatable<Color>
    {
        public bool Equals(Color other) => this == other;
    }

    public partial struct Rect2 : System.IEquatable<Rect2>
    {
        public bool Equals(Rect2 other) => Position == other.Position && Size == other.Size;
    }

    public partial struct Transform2D : System.IEquatable<Transform2D>
    {
        public bool Equals(Transform2D other) => X == other.X && Y == other.Y && Origin == other.Origin;
    }

    public partial struct Quaternion : System.IEquatable<Quaternion>
    {
        public bool Equals(Quaternion other) => base.Equals(other);
    }

    public partial struct Rid : System.IEquatable<Rid>
    {
        public bool Equals(Rid other) => base.Equals(other);
    }

    public sealed partial class StringName : System.IEquatable<StringName>
    {
        public bool Equals(StringName? other) => this == other;
    }

    public sealed partial class NodePath : System.IEquatable<NodePath>
    {
        public bool Equals(NodePath? other) => other is not null && (string)this == (string)other;
        public override bool Equals(object? obj) => obj is NodePath p && Equals(p);
        public override int GetHashCode() => ((string)this).GetHashCode();
    }

}

namespace Godot.Bridge
{
    public partial class GodotSerializationInfo : System.IDisposable
    {
        public void Dispose() { }
    }
}

namespace Godot.Collections
{
    public partial class Array : System.IDisposable
    {
        public void Dispose() { }
    }

    public sealed partial class Dictionary : System.Collections.Generic.Dictionary<Variant, Variant>, System.IDisposable
    {
        public void Dispose() { }
        public new System.Collections.Generic.IEnumerator<System.Collections.Generic.KeyValuePair<Variant, Variant>> GetEnumerator() => base.GetEnumerator();
    }
}
