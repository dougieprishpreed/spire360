// LocLookup and LocManager setup ported from sts2-cli's RunSimulator.cs
// (https://github.com/Garglemesh/sts2-cli, MIT, (c) 2025 Hao Wu).
//
// Spire360 changes: tables are read from the data directory the converter
// fills from the player's own game files (data/localization/<lang>/*.json),
// any game language can be primary with English as the fallback, and the
// Harmony fallbacks are IL-patch handlers (Hooks.cs) that look text up for
// real instead of always echoing the key.
using System.Reflection;
using System.Text.RegularExpressions;
using MegaCrit.Sts2.Core.Localization;

namespace Spire.Logic;

/// <summary>Where the adapter's data lives (localization tables, saves).</summary>
public static class DataDir
{
    private static string? _root;

    /// <summary>
    /// SPIRE_DATA, else the first existing of: ./data, &lt;exe dir&gt;/data, /data.
    /// In the wasm build the host mounts the console's data folder as "/".
    /// </summary>
    public static string Root
    {
        get
        {
            if (_root != null) return _root;
            var env = Environment.GetEnvironmentVariable("SPIRE_DATA");
            var candidates = new List<string>();
            if (!string.IsNullOrWhiteSpace(env)) candidates.Add(env.Trim());
            candidates.Add(Path.Combine(Directory.GetCurrentDirectory(), "data"));
            candidates.Add(Path.Combine(AppContext.BaseDirectory, "data"));
            candidates.Add("/data");
            _root = candidates.FirstOrDefault(Directory.Exists) ?? candidates[0];
            return _root;
        }
    }

    /// <summary>Folder of one language's tables, accepting the converter's and sts2-cli's layouts.</summary>
    public static string? LocalizationDir(string gameLang)
    {
        foreach (var dir in new[]
                 {
                     Path.Combine(Root, "localization", gameLang),
                     Path.Combine(Root, "localization_" + gameLang),
                 })
        {
            if (Directory.Exists(dir)) return dir;
        }
        return null;
    }
}

/// <summary>
/// Display-name lookup used by the state export. Lang uses sts2-cli's protocol
/// codes ("en", "zh") or the game's own folder names ("eng", "zhs", "deu", ...).
/// </summary>
internal class LocLookup
{
    private readonly Dictionary<string, Dictionary<string, Dictionary<string, string>>> _byLang = new();
    private string _lang = "eng";

    public LocLookup()
    {
        Load("eng");
    }

    private static string GameLang(string protocolLang) => protocolLang switch
    {
        "en" => "eng",
        "zh" => "zhs",
        _ => protocolLang,
    };

    private Dictionary<string, Dictionary<string, string>> Load(string gameLang)
    {
        if (_byLang.TryGetValue(gameLang, out var have)) return have;
        var tables = new Dictionary<string, Dictionary<string, string>>();
        var dir = DataDir.LocalizationDir(gameLang);
        if (dir != null)
        {
            foreach (var file in Directory.GetFiles(dir, "*.json"))
            {
                try
                {
                    var data = ParseTable(File.ReadAllText(file));
                    if (data != null) tables[Path.GetFileNameWithoutExtension(file)] = data;
                }
                catch (Exception ex)
                {
                    Log.Warn($"loc table {file}: {ex.Message}");
                }
            }
        }
        _byLang[gameLang] = tables;
        return tables;
    }

    /// <summary>Flat "key": "text" JSON object, parsed without reflection.</summary>
    internal static Dictionary<string, string>? ParseTable(string json)
    {
        using var doc = System.Text.Json.JsonDocument.Parse(json);
        if (doc.RootElement.ValueKind != System.Text.Json.JsonValueKind.Object) return null;
        var d = new Dictionary<string, string>();
        foreach (var p in doc.RootElement.EnumerateObject())
            d[p.Name] = p.Value.ValueKind == System.Text.Json.JsonValueKind.String ? p.Value.GetString()! : p.Value.ToString();
        return d;
    }

    /// <summary>All tables for the current language (falls back to English per key elsewhere).</summary>
    public IReadOnlyDictionary<string, Dictionary<string, string>> Tables => Load(_lang);
    public IReadOnlyDictionary<string, Dictionary<string, string>> EnglishTables => Load("eng");

    public string Lang
    {
        get => _lang == "zhs" ? "zh" : _lang == "eng" ? "en" : _lang;
        set => _lang = GameLang(value ?? "en");
    }

    public string GameLanguage => _lang;

    private string? Raw(string table, string key) =>
        Load(_lang).GetValueOrDefault(table)?.GetValueOrDefault(key)
        ?? Load("eng").GetValueOrDefault(table)?.GetValueOrDefault(key);

    /// <summary>Strip BBCode tags like [gold], [/blue], [b], [sine], etc.</summary>
    internal static string StripBBCode(string text) =>
        Regex.Replace(text, @"\[/?[a-zA-Z_][a-zA-Z0-9_=]*\]", "");

    /// <summary>Localized text for the protocol, BBCode stripped; the key if missing.</summary>
    public string Bilingual(string table, string key)
    {
        var text = Raw(table, key);
        return text != null ? StripBBCode(text) : key;
    }

    /// <summary>Localized text with BBCode kept (the presentation layer renders it).</summary>
    public string? Rich(string table, string key) => Raw(table, key);

    public string Name(string table, string key) => Bilingual(table, key);

    public string Card(string entry) => Bilingual("cards", entry + ".title");

    public string Monster(string entry)
    {
        var key = entry + ".name";
        var result = Bilingual("monsters", key);
        // If no dedicated entry, fall back to the base segment key (e.g. DECIMILLIPEDE_SEGMENT_FRONT → DECIMILLIPEDE_SEGMENT)
        if (result == key)
        {
            var lastUnderscore = entry.LastIndexOf('_');
            if (lastUnderscore > 0)
            {
                var baseKey = entry[..lastUnderscore] + ".name";
                var baseResult = Bilingual("monsters", baseKey);
                if (baseResult != baseKey) return baseResult;
            }
        }
        return result;
    }

    public string Relic(string entry) => Bilingual("relics", entry + ".title");
    public string Potion(string entry) => Bilingual("potions", entry + ".title");
    public string Power(string entry) => Bilingual("powers", entry + ".title");
    public string Event(string entry) => Bilingual("events", entry + ".title");
    public string Act(string entry) => Bilingual("acts", entry + ".title");

    /// <summary>Resolve a full loc key like "TABLE.KEY.SUB" by searching all tables.</summary>
    public string BilingualFromKey(string locKey)
    {
        foreach (var tables in new[] { Load(_lang), Load("eng") })
        {
            foreach (var t in tables.Values)
            {
                if (t.TryGetValue(locKey, out var v)) return v;
            }
        }
        return locKey;
    }

    public bool IsLoaded => Load("eng").Count > 0;
}

/// <summary>
/// Gives the game a working LocManager without its normal Initialize()
/// (which needs PlatformUtil). Ported from sts2-cli's InitLocManager.
/// </summary>
internal static class LocSetup
{
    private static readonly BindingFlags Inst = BindingFlags.Instance | BindingFlags.NonPublic | BindingFlags.Public;
    private static readonly BindingFlags Stat = BindingFlags.Static | BindingFlags.NonPublic | BindingFlags.Public;

    /// <summary>The game's LocTable objects by table name, for the GetRawText hook.</summary>
    internal static readonly Dictionary<string, Dictionary<string, string>> RawByTable = new();

    private static readonly string[] KnownTables =
    {
        "achievements","acts","afflictions","ancients","ascension",
        "bestiary","card_keywords","card_library","card_reward_ui",
        "card_selection","cards","characters","combat_messages",
        "credits","enchantments","encounters","epochs","eras",
        "events","ftues","game_over_screen","gameplay_ui",
        "inspect_relic_screen","intents","main_menu_ui","map",
        "merchant_room","modifiers","monsters","orbs","potion_lab",
        "potions","powers","relic_collection","relics","rest_site_ui",
        "run_history","settings_ui","static_hover_tips","stats_screen",
        "timeline","vfx",
    };

    public static void InitLocManager(LocLookup loc)
    {
        try
        {
            var instanceProp = typeof(LocManager).GetProperty("Instance", BindingFlags.Static | BindingFlags.Public);
            var instance = System.Runtime.CompilerServices.RuntimeHelpers.GetUninitializedObject(typeof(LocManager));
            instanceProp!.SetValue(null, instance);

            var tablesField = typeof(LocManager).GetField("_tables", Inst);
            var tables = new Dictionary<string, LocTable>();
            foreach (var (name, data) in loc.Tables)
            {
                // English fills keys the selected language lacks.
                var merged = new Dictionary<string, string>(data);
                if (loc.EnglishTables.TryGetValue(name, out var eng))
                    foreach (var kv in eng) merged.TryAdd(kv.Key, kv.Value);
                tables[name] = new LocTable(name, merged);
                RawByTable[name] = merged;
            }
            foreach (var (name, data) in loc.EnglishTables)
            {
                if (tables.ContainsKey(name)) continue;
                tables[name] = new LocTable(name, data);
                RawByTable[name] = data;
            }
            if (tables.Count == 0)
            {
                Log.Warn($"no localization tables under {DataDir.Root}; text will show keys");
                foreach (var name in KnownTables)
                    tables[name] = new LocTable(name, new Dictionary<string, string>());
            }
            else
            {
                Log.Info($"Loaded {tables.Count} localization tables ({loc.GameLanguage}) from {DataDir.Root}");
            }
            tablesField!.SetValue(instance, tables);

            try { typeof(LocManager).GetProperty("Language", Inst)?.SetValue(instance, loc.GameLanguage); } catch { }
            // A named culture, so SmartFormat picks the right plural rules (the
            // invariant culture reports language "iv", which it does not know).
            try { typeof(LocManager).GetProperty("CultureInfo", Inst)?.SetValue(instance, CultureFor(loc.GameLanguage)); } catch { }

            InitSmartFormatter(instance);
            MapInvariantPluralRule(loc.GameLanguage);

            try { typeof(LocManager).GetField("_engTables", Inst)?.SetValue(instance, tables); } catch { }
            Log.Info("LocManager initialized");
        }
        catch (Exception ex)
        {
            Log.Warn($"InitLocManager failed: {ex.Message}");
        }
    }

    /// <summary>
    /// With InvariantGlobalization every culture reports its language as "iv",
    /// which SmartFormat has no plural rule for. Point "iv" at the rule of the
    /// language actually being shown.
    /// </summary>
    private static void MapInvariantPluralRule(string gameLang)
    {
        try
        {
            var rules = SmartFormat.Utilities.PluralRules.IsoLangToDelegate;
            var iso = CultureFor(gameLang).Name.Split('-')[0];
            if (rules.TryGetValue(iso, out var rule) || rules.TryGetValue("en", out rule))
                rules["iv"] = rule;
        }
        catch (Exception ex)
        {
            Log.Warn($"plural rules: {ex.Message}");
        }
    }

    /// <summary>The game's language folder name to a .NET culture.</summary>
    internal static System.Globalization.CultureInfo CultureFor(string gameLang)
    {
        var name = gameLang switch
        {
            "eng" => "en-US", "zhs" => "zh-CN", "zht" => "zh-TW", "deu" => "de-DE", "fra" => "fr-FR",
            "ita" => "it-IT", "jpn" => "ja-JP", "kor" => "ko-KR", "pol" => "pl-PL", "ptb" => "pt-BR",
            "rus" => "ru-RU", "spa" => "es-ES", "esp" => "es-419", "tha" => "th-TH", "tur" => "tr-TR",
            _ => "en-US",
        };
        try { return System.Globalization.CultureInfo.GetCultureInfo(name); }
        catch { return System.Globalization.CultureInfo.InvariantCulture; }
    }

    private static void InitSmartFormatter(object instance)
    {
        try
        {
            var sfField = typeof(LocManager).GetField("_smartFormatter", Stat);
            if (sfField == null)
            {
                Log.Warn("_smartFormatter field not found in LocManager");
                return;
            }
            // The game's own LoadLocFormatters registers its SmartFormat extensions.
            var loadMethod = typeof(LocManager).GetMethod("LoadLocFormatters", Inst);
            if (loadMethod != null)
            {
                try
                {
                    loadMethod.Invoke(instance, null);
                    Log.Info("SmartFormatter initialized via LoadLocFormatters");
                    return;
                }
                catch (Exception ex)
                {
                    Log.Warn($"LoadLocFormatters failed: {ex.InnerException?.Message ?? ex.Message}");
                }
            }
            var ctor = sfField.FieldType.GetConstructors(Inst).OrderBy(c => c.GetParameters().Length).First();
            var args = ctor.GetParameters().Select(p =>
                p.HasDefaultValue ? p.DefaultValue :
                p.ParameterType.IsValueType ? Activator.CreateInstance(p.ParameterType) : null).ToArray();
            sfField.SetValue(null, ctor.Invoke(args));
            Log.Info("SmartFormatter set (fallback constructor)");
        }
        catch (Exception ex)
        {
            Log.Warn($"SmartFormatter init: {ex.GetType().Name}: {ex.Message}");
        }
    }
}
