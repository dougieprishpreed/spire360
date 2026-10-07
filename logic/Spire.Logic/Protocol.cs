using System.Text.Json;

namespace Spire.Logic;

/// <summary>
/// Line-based JSON over stdin/stdout, compatible with sts2-cli's protocol:
/// one command per line in, one response object per line out.
///
/// The console host feeds stdin and reads stdout through in-memory pipes, so
/// the same binary logic runs under `dotnet run`, wasmtime, the w2c2 PC host
/// and the Xbox 360.
/// </summary>
public sealed class Protocol
{
    private readonly TextReader _in;
    private readonly TextWriter _out;

    public Protocol(TextReader input, TextWriter output)
    {
        _in = input;
        _out = output;
    }

    /// <summary>Next non-empty command, or null at end of input.</summary>
    public JsonElement? ReadCommand()
    {
        string? line;
        while ((line = _in.ReadLine()) != null)
        {
            line = line.Trim();
            if (line.Length == 0) continue;
            try
            {
                using var doc = JsonDocument.Parse(line);
                return doc.RootElement.Clone();
            }
            catch (JsonException ex)
            {
                Write(new Dictionary<string, object?> { ["type"] = "error", ["message"] = $"Invalid JSON: {ex.Message}" });
            }
        }
        return null;
    }

    public void Write(Dictionary<string, object?> response)
    {
        _out.Write(Json.Serialize(response));
        _out.Write('\n');
        _out.Flush();
    }

    public static string? Str(JsonElement cmd, string name, string? fallback = null) =>
        cmd.TryGetProperty(name, out var v) && v.ValueKind == JsonValueKind.String ? v.GetString() : fallback;

    public static int Int(JsonElement cmd, string name, int fallback = 0) =>
        cmd.TryGetProperty(name, out var v) && v.ValueKind == JsonValueKind.Number && v.TryGetInt32(out var i) ? i : fallback;

    public static Dictionary<string, object?>? Args(JsonElement cmd)
    {
        if (!cmd.TryGetProperty("args", out var a) || a.ValueKind != JsonValueKind.Object)
            return null;
        var d = new Dictionary<string, object?>();
        foreach (var p in a.EnumerateObject())
            d[p.Name] = Json.ToLoose(p.Value);
        return d;
    }
}
