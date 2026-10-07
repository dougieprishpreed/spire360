// The Godot.StringExtensions helpers the game uses, with Godot 4's
// semantics (they are pure string functions in real Godot too).
using System.Text;

namespace Godot;

public static partial class StringExtensions
{
    private static int SchemeEnd(string s)
    {
        var i = s.IndexOf("://", StringComparison.Ordinal);
        return i >= 0 ? i + 3 : 0;
    }

    /// <summary>"res://a/b.png" -> "res://a"; "user://x" -> "user://"; "/a" -> "/".</summary>
    public static string GetBaseDir(this string instance)
    {
        int root = SchemeEnd(instance);
        if (root == 0 && instance.StartsWith('/')) root = 1;
        var rest = instance[root..];
        var sep = Math.Max(rest.LastIndexOf('/'), rest.LastIndexOf('\\'));
        return sep < 0 ? instance[..root] : instance[..root] + rest[..sep];
    }

    /// <summary>"a/b.png" -> "b.png".</summary>
    public static string GetFile(this string instance)
    {
        var sep = Math.Max(instance.LastIndexOf('/'), instance.LastIndexOf('\\'));
        return sep < 0 ? instance : instance[(sep + 1)..];
    }

    /// <summary>"a".PathJoin("b") -> "a/b" (no doubled separator).</summary>
    public static string PathJoin(this string instance, string file)
    {
        if (instance.Length == 0) return file;
        if (instance.EndsWith('/') || (file.Length > 0 && file[0] == '/')) return instance + file;
        return instance + "/" + file;
    }

    /// <summary>"someName" / "some_name" -> "some_name".</summary>
    public static string ToSnakeCase(this string instance)
    {
        var sb = new StringBuilder(instance.Length + 8);
        for (int i = 0; i < instance.Length; i++)
        {
            char c = instance[i];
            if (char.IsUpper(c))
            {
                bool prevLowerOrDigit = i > 0 && (char.IsLower(instance[i - 1]) || char.IsDigit(instance[i - 1]));
                bool nextLower = i + 1 < instance.Length && char.IsLower(instance[i + 1]);
                bool prevUpper = i > 0 && char.IsUpper(instance[i - 1]);
                if (i > 0 && instance[i - 1] != '_' && (prevLowerOrDigit || (prevUpper && nextLower)))
                    sb.Append('_');
                sb.Append(char.ToLowerInvariant(c));
            }
            else if (c == ' ' || c == '-')
            {
                sb.Append('_');
            }
            else
            {
                sb.Append(c);
            }
        }
        return sb.ToString();
    }

    /// <summary>"some_name" / "someName" -> "Some Name".</summary>
    public static string Capitalize(this string instance)
    {
        var words = instance.ToSnakeCase().Replace('_', ' ').Trim()
            .Split(' ', StringSplitOptions.RemoveEmptyEntries);
        return string.Join(" ", words.Select(w => char.ToUpperInvariant(w[0]) + w[1..].ToLowerInvariant()));
    }
}
