// Spire360: the value-type and math members the game calls that the stub
// generator would otherwise emit as `return default` (Vector2I.One was zero,
// so the Crystal Sphere's small gold had no cells, and `==` on Vector2I was
// always false). Same results as GodotSharp's own implementations.
namespace Godot
{
    public partial struct Vector2
    {
        public float DistanceSquaredTo(Vector2 to) => (to - this).LengthSquared();
        public Vector2 LimitLength(float length = 1f)
        {
            Vector2 v = this;
            float l = Length();
            if (l > 0 && length < l)
            {
                v /= l;
                v *= length;
            }
            return v;
        }
        public Vector2 Rotated(float angle)
        {
            float sin = MathF.Sin(angle), cos = MathF.Cos(angle);
            return new Vector2(X * cos - Y * sin, X * sin + Y * cos);
        }
        public bool IsEqualApprox(Vector2 to) => Mathf.IsEqualApprox(X, to.X) && Mathf.IsEqualApprox(Y, to.Y);
        public float Angle() => MathF.Atan2(Y, X);
        public Vector2 Inverse() => new(1 / X, 1 / Y);
    }

    public partial struct Vector2I
    {
        public static Vector2I One { get; } = new(1, 1);
        public static Vector2I operator +(Vector2I a, Vector2I b) => new(a.X + b.X, a.Y + b.Y);
        public static Vector2I operator -(Vector2I a, Vector2I b) => new(a.X - b.X, a.Y - b.Y);
        public static Vector2I operator *(Vector2I a, int b) => new(a.X * b, a.Y * b);
        public static Vector2I operator /(Vector2I a, int b) => new(a.X / b, a.Y / b);
        public static bool operator ==(Vector2I a, Vector2I b) => a.X == b.X && a.Y == b.Y;
        public static bool operator !=(Vector2I a, Vector2I b) => !(a == b);
        public static bool operator >=(Vector2I a, Vector2I b) => a.X == b.X ? a.Y >= b.Y : a.X > b.X;
        public static bool operator <=(Vector2I a, Vector2I b) => a.X == b.X ? a.Y <= b.Y : a.X < b.X;
        public static explicit operator Vector2I(Vector2 v) => new((int)v.X, (int)v.Y);
        public float DistanceTo(Vector2I to) => MathF.Sqrt((float)((to.X - X) * (to.X - X) + (to.Y - Y) * (to.Y - Y)));
        public override bool Equals(object? obj) => obj is Vector2I v && this == v;
        public override int GetHashCode() => HashCode.Combine(X, Y);
        public override string ToString() => $"({X}, {Y})";
    }

    public partial struct Vector3
    {
        public static Vector3 Up { get; } = new(0, 1, 0);
        public static Vector3 operator *(Vector3 v, float f) => new(v.X * f, v.Y * f, v.Z * f);
        public static Vector3 operator +(Vector3 a, Vector3 b) => new(a.X + b.X, a.Y + b.Y, a.Z + b.Z);
        public Vector3 Cross(Vector3 b) => new(Y * b.Z - Z * b.Y, Z * b.X - X * b.Z, X * b.Y - Y * b.X);
    }

    public partial struct Quaternion
    {
        public float X, Y, Z, W;
        public Quaternion(float x, float y, float z, float w) { X = x; Y = y; Z = z; W = w; }
        // YXZ Euler angles, as Godot's.
        public static Quaternion FromEuler(Vector3 e)
        {
            float a1 = e.Y * 0.5f, a2 = e.X * 0.5f, a3 = e.Z * 0.5f;
            float c1 = MathF.Cos(a1), s1 = MathF.Sin(a1);
            float c2 = MathF.Cos(a2), s2 = MathF.Sin(a2);
            float c3 = MathF.Cos(a3), s3 = MathF.Sin(a3);
            return new Quaternion(
                s1 * c2 * s3 + c1 * s2 * c3,
                s1 * c2 * c3 - c1 * s2 * s3,
                -s1 * s2 * c3 + c1 * c2 * s3,
                s1 * s2 * s3 + c1 * c2 * c3);
        }
        public static Vector3 operator *(Quaternion q, Vector3 v)
        {
            var u = new Vector3(q.X, q.Y, q.Z);
            Vector3 uv = u.Cross(v);
            return v + (uv * q.W + u.Cross(uv)) * 2;
        }
    }

    public partial struct Color
    {
        public static Color FromHtml(System.ReadOnlySpan<char> rgba) => new(rgba.ToString());
        public static Color operator *(Color a, Color b) => new(a.R * b.R, a.G * b.G, a.B * b.B, a.A * b.A);
    }

    public static partial class Colors
    {
        public static Color DarkRed { get; } = new(139 / 255f, 0, 0);
        public static Color Gold { get; } = new(1, 215 / 255f, 0);
        public static Color DimGray { get; } = new(105 / 255f, 105 / 255f, 105 / 255f);
    }

    public partial struct Rect2
    {
        public Vector2 End { get => Position + Size; set => Size = value - Position; }
        public Vector2 GetCenter() => Position + Size * 0.5f;
        public bool HasPoint(Vector2 p) =>
            p.X >= Position.X && p.Y >= Position.Y && p.X < Position.X + Size.X && p.Y < Position.Y + Size.Y;
        public bool Intersects(Rect2 b, bool includeBorders = false)
        {
            if (includeBorders)
                return !(Position.X > b.Position.X + b.Size.X || Position.X + Size.X < b.Position.X ||
                         Position.Y > b.Position.Y + b.Size.Y || Position.Y + Size.Y < b.Position.Y);
            return !(Position.X >= b.Position.X + b.Size.X || Position.X + Size.X <= b.Position.X ||
                     Position.Y >= b.Position.Y + b.Size.Y || Position.Y + Size.Y <= b.Position.Y);
        }
        public Rect2 Merge(Rect2 b)
        {
            var pos = new Vector2(MathF.Min(Position.X, b.Position.X), MathF.Min(Position.Y, b.Position.Y));
            var end = new Vector2(MathF.Max(End.X, b.End.X), MathF.Max(End.Y, b.End.Y));
            return new Rect2(pos, end - pos);
        }
    }

    public partial struct Transform2D
    {
        public Transform2D(Vector2 x, Vector2 y, Vector2 origin) { X = x; Y = y; Origin = origin; }
        public Transform2D(float rotation, Vector2 origin)
        {
            float sin = MathF.Sin(rotation), cos = MathF.Cos(rotation);
            X = new Vector2(cos, sin);
            Y = new Vector2(-sin, cos);
            Origin = origin;
        }
        private Vector2 BasisXform(Vector2 v) => new(X.X * v.X + Y.X * v.Y, X.Y * v.X + Y.Y * v.Y);
        private float Determinant() => X.X * Y.Y - X.Y * Y.X;
        public Vector2 Scale => new(X.Length(), MathF.Sign(Determinant()) * Y.Length());
        public float Rotation => MathF.Atan2(X.Y, X.X);
        public Transform2D TranslatedLocal(Vector2 offset) => new(X, Y, Origin + BasisXform(offset));
        public Transform2D RotatedLocal(float angle) => this * new Transform2D(angle, Vector2.Zero);
        public static Vector2 operator *(Transform2D t, Vector2 v) => t.BasisXform(v) + t.Origin;
        public static Transform2D operator *(Transform2D a, Transform2D b) =>
            new(a.BasisXform(b.X), a.BasisXform(b.Y), a * b.Origin);
        public Transform2D Inverse()
        {
            Transform2D inv = this;
            inv.X.Y = Y.X;
            inv.Y.X = X.Y;
            inv.Origin = inv.BasisXform(-inv.Origin);
            return inv;
        }
    }

    public static partial class Mathf
    {
        public static double Sin(double s) => Math.Sin(s);
        public static float Atan2(float y, float x) => MathF.Atan2(y, x);
        public static float LinearToDb(float linear) => MathF.Log(linear) * 8.6858896380650365530225783783321f;
        public static double Max(double a, double b) => Math.Max(a, b);
        public static double Lerp(double from, double to, double weight) => from + (to - from) * weight;
        public static float InverseLerp(float from, float to, float weight) => (weight - from) / (to - from);
        public static float Wrap(float value, float min, float max)
        {
            float range = max - min;
            if (MathF.Abs(range) < Epsilon) return min;
            float result = value - range * MathF.Floor((value - min) / range);
            return IsEqualApprox(result, max) ? min : result;
        }
        public static int Abs(int s) => Math.Abs(s);
        public static int Sign(int s) => Math.Sign(s);
        public static float Log(float s) => MathF.Log(s);
        public static float LerpAngle(float from, float to, float weight)
        {
            float difference = (to - from) % Tau;
            float distance = 2 * difference % Tau - difference;
            return from + distance * weight;
        }
        public static double DegToRad(double deg) => deg * (Math.PI / 180.0);
    }
}
