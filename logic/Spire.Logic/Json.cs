using System.Collections;
using System.Globalization;
using System.Text;
using System.Text.Encodings.Web;
using System.Text.Json;

namespace Spire.Logic;

/// <summary>
/// Writes the protocol's object graphs (dictionaries, lists, primitives) as
/// compact JSON without reflection-based serialisation, so it behaves the same
/// under JIT, NativeAOT and NativeAOT-LLVM. Null-valued dictionary entries are
/// omitted, matching sts2-cli's WhenWritingNull output.
/// </summary>
public static class Json
{
    private static readonly JsonWriterOptions Options = new()
    {
        Indented = false,
        // Keep non-ASCII text (card names in other languages) readable.
        Encoder = JavaScriptEncoder.UnsafeRelaxedJsonEscaping,
    };

    public static string Serialize(object? value)
    {
        using var ms = new MemoryStream();
        using (var w = new Utf8JsonWriter(ms, Options))
            Write(w, value, 0);
        return Encoding.UTF8.GetString(ms.GetBuffer(), 0, (int)ms.Length);
    }

    private static void Write(Utf8JsonWriter w, object? value, int depth)
    {
        if (depth > 64)
        {
            w.WriteStringValue("<depth limit>");
            return;
        }
        switch (value)
        {
            case null: w.WriteNullValue(); return;
            case string s: w.WriteStringValue(s); return;
            case bool b: w.WriteBooleanValue(b); return;
            case int i: w.WriteNumberValue(i); return;
            case long l: w.WriteNumberValue(l); return;
            case uint ui: w.WriteNumberValue(ui); return;
            case ulong ul: w.WriteNumberValue(ul); return;
            case short sh: w.WriteNumberValue(sh); return;
            case ushort us: w.WriteNumberValue(us); return;
            case byte by: w.WriteNumberValue(by); return;
            case sbyte sb: w.WriteNumberValue(sb); return;
            case float f: WriteDouble(w, f); return;
            case double d: WriteDouble(w, d); return;
            case decimal m: w.WriteNumberValue(m); return;
            case Enum e: w.WriteStringValue(e.ToString()); return;
            case JsonElement je: je.WriteTo(w); return;
            case IDictionary<string, object?> dict:
                w.WriteStartObject();
                foreach (var kv in dict)
                {
                    if (kv.Value == null) continue;
                    w.WritePropertyName(kv.Key);
                    Write(w, kv.Value, depth + 1);
                }
                w.WriteEndObject();
                return;
            case IDictionary nonGeneric:
                w.WriteStartObject();
                foreach (DictionaryEntry kv in nonGeneric)
                {
                    if (kv.Value == null) continue;
                    w.WritePropertyName(Convert.ToString(kv.Key, CultureInfo.InvariantCulture) ?? "");
                    Write(w, kv.Value, depth + 1);
                }
                w.WriteEndObject();
                return;
            case IEnumerable seq:
                w.WriteStartArray();
                foreach (var item in seq)
                    Write(w, item, depth + 1);
                w.WriteEndArray();
                return;
            default:
                w.WriteStringValue(Convert.ToString(value, CultureInfo.InvariantCulture));
                return;
        }
    }

    private static void WriteDouble(Utf8JsonWriter w, double d)
    {
        if (double.IsNaN(d) || double.IsInfinity(d)) w.WriteNullValue();
        else w.WriteNumberValue(d);
    }

    /// <summary>Converts a parsed JSON value to the loose types the action handlers expect.</summary>
    public static object? ToLoose(JsonElement e) => e.ValueKind switch
    {
        JsonValueKind.Number => e.TryGetInt32(out var i) ? i : e.GetDouble(),
        JsonValueKind.String => e.GetString(),
        JsonValueKind.True => true,
        JsonValueKind.False => false,
        JsonValueKind.Null => null,
        _ => e.ToString(),
    };
}
