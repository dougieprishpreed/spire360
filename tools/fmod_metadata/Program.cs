// Offline bank metadata for the asset converter. No game DLLs are loaded.
// Outputs are game data and belong only in the converter's ignored cache.
using System.Collections;
using System.Buffers.Binary;
using System.Text.Json;
using System.Text.Json.Serialization;
using FModBankParser;
using FModBankParser.Objects;
using Fmod5Sharp;

if (args.Length == 3 && args[0] == "--fsb")
{
    var bank = FsbLoader.LoadFsbFromByteArray(File.ReadAllBytes(args[1]));
    Directory.CreateDirectory(args[2]);
    var files = new List<object>();
    for (int i = 0; i < bank.Samples.Count; i++)
    {
        if (!bank.Samples[i].RebuildAsStandardFileFormat(out var bytes, out var extension))
            throw new InvalidDataException($"Cannot rebuild FSB sample {i}");
        var name = $"{i}.{extension}";
        var output = Path.Combine(args[2], name);
        File.WriteAllBytes(output + ".tmp", bytes);
        File.Move(output + ".tmp", output, true);
        files.Add(new { index = i, file = name, bytes = bytes.Length });
    }
    var manifest = Path.Combine(args[2], "files.json");
    File.WriteAllText(manifest + ".tmp", JsonSerializer.Serialize(files));
    File.Move(manifest + ".tmp", manifest, true);
    Console.Error.WriteLine($"FSB: {bank.Samples.Count} samples rebuilt without re-encoding");
    return 0;
}

if (args.Length != 2)
{
    Console.Error.WriteLine("usage: FmodMetadata <bank directory> <output.json>");
    Console.Error.WriteLine("       FmodMetadata --fsb <input.fsb> <output directory>");
    return 2;
}

var options = new JsonSerializerOptions
{
    IncludeFields = true,
    PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
    Converters = { new GuidConverter(), new JsonStringEnumConverter() }
};
var paths = new Dictionary<string, string>();
var banks = new List<object>();
foreach (var file in Directory.GetFiles(args[0], "*.bank").Order(StringComparer.Ordinal))
{
    var metadata = File.ReadAllBytes(file);
    MetadataOnly.HideAudio(metadata);
    using var input = new BinaryReader(new MemoryStream(metadata, false));
    var reader = new FModReader(input, Path.GetFileName(file));
    if (reader.StringTable?.RadixTree is { } strings)
        foreach (var guid in strings.Guids)
            if (strings.TryGetString(guid, out var path)) paths[guid.ToString()] = path;

    // Keep the graph, including trigger conditions, playlist modes, curves
    // and buses. A flat event->sample list loses adaptive music and layering.
    var graph = new Dictionary<string, object>();
    foreach (var field in typeof(FModReader).GetFields())
    {
        if (field.IsStatic || field.GetValue(reader) is not IDictionary nodes) continue;
        var entries = new Dictionary<string, object>();
        foreach (DictionaryEntry entry in nodes)
            if (entry.Value is { } node)
                entries[entry.Key.ToString()!] = new
                {
                    type = node.GetType().Name,
                    data = JsonSerializer.SerializeToElement(node, node.GetType(), options)
                };
        graph[JsonNamingPolicy.CamelCase.ConvertName(field.Name)] = entries;
    }
    banks.Add(new { file = Path.GetFileName(file), version = FModReader.Version, graph });
    Console.Error.WriteLine($"{Path.GetFileName(file)}: {reader.EventNodes.Count} events");
}
File.WriteAllText(args[1] + ".tmp", JsonSerializer.Serialize(new { version = 1, paths, banks }, options));
File.Move(args[1] + ".tmp", args[1], true);
return 0;

sealed class GuidConverter : JsonConverter<FModGuid>
{
    public override FModGuid Read(ref Utf8JsonReader reader, Type type, JsonSerializerOptions options)
        => new(reader.GetString()!);
    public override void Write(Utf8JsonWriter writer, FModGuid value, JsonSerializerOptions options)
        => writer.WriteStringValue(value.ToString());
    public override void WriteAsPropertyName(Utf8JsonWriter writer, FModGuid value, JsonSerializerOptions options)
        => writer.WritePropertyName(value.ToString());
}

static class MetadataOnly
{
    // The audio decoder reads the original banks separately. Mask SND chunk
    // tags in this in-memory copy so the metadata parser does not decode FSB
    // payloads or depend on its unpublished FSB decoder extension.
    public static void HideAudio(byte[] data)
    {
        if (data.Length < 12 || !data.AsSpan(0, 4).SequenceEqual("RIFF"u8) ||
            !data.AsSpan(8, 4).SequenceEqual("FEV "u8))
            throw new InvalidDataException("Not an FMOD Studio bank");
        Walk(data, 12, checked((int)BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(4)) + 8));
    }
    private static void Walk(byte[] data, int start, int end)
    {
        if (end > data.Length) throw new InvalidDataException("Truncated bank");
        for (int p = start; p + 8 <= end;)
        {
            if (data[p] == 0) p++; // odd-sized RIFF chunk padding
            if (p + 8 > end) break;
            var tag = data.AsSpan(p, 4);
            int next = checked(p + 8 + (int)BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(p + 4)));
            if (next > end || next < p + 8) throw new InvalidDataException("Invalid bank chunk");
            if (tag.SequenceEqual("SND "u8)) "JUNK"u8.CopyTo(tag);
            else if (tag.SequenceEqual("LIST"u8) && next >= p + 12) Walk(data, p + 12, next);
            p = next;
        }
    }
}
