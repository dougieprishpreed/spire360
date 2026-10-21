// The settings screen's values (NSettingsScreen): the profile's prefs
// (PrefsSave) and the game's settings (SettingsSave), read and written
// through SaveManager as the screen's controls do. Presentation only: the
// run's logic never reads them back here.

using MegaCrit.Sts2.Core.Localization;
using MegaCrit.Sts2.Core.Saves;
using MegaCrit.Sts2.Core.Settings;

namespace Spire.Logic;

public partial class RunDriver
{
    private bool _settingsLoaded;

    /// <summary>The language the logic speaks: a command's "lang", else the
    /// settings' (SettingsSave.Language; null: English). LocManager is set up
    /// again when it changes (LocManager.SetLanguage, without Godot's files).</summary>
    private void ApplyLanguage(string? lang)
    {
        if (lang == null)
        {
            EnsureSettings();
            lang = SaveManager.Instance.SettingsSave?.Language;
            if (lang == null || !LocManager.Languages.Contains(lang)) lang = "eng";
        }
        string before = _loc.GameLanguage;
        _loc.Lang = lang;
        if (_loc.GameLanguage != before) LocSetup.InitLocManager(_loc);
    }

    private void EnsureSettings()
    {
        EnsureModelDbInitialized(); // the save store (FileStore) first
        if (_settingsLoaded) return;
        _settingsLoaded = true;
        try
        {
            if (SaveManager.Instance.SettingsSave == null) SaveManager.Instance.InitSettingsData();
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"[SIM] settings: {ex.Message}");
        }
        try
        {
            if (SaveManager.Instance.PrefsSave == null) SaveManager.Instance.InitPrefsData();
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"[SIM] prefs: {ex.Message}");
        }
    }

    /// <summary>The settings screen's values.</summary>
    public Dictionary<string, object?> Settings()
    {
        EnsureSettings();
        var prefs = SaveManager.Instance.PrefsSave;
        var settings = SaveManager.Instance.SettingsSave;
        return new Dictionary<string, object?>
        {
            ["type"] = "settings",
            ["in_run"] = HasRun,
            ["fast_mode"] = prefs != null && prefs.FastMode != FastModeType.Normal,
            ["phobia_mode"] = prefs?.PhobiaMode ?? false,
            ["screenshake"] = prefs?.ScreenShakeOptionIndex ?? 2,
            ["run_timer"] = prefs?.ShowRunTimer ?? false,
            ["card_indices"] = prefs?.ShowCardIndices ?? false,
            ["long_press"] = prefs?.IsLongPressEnabled ?? false,
            ["text_effects"] = prefs?.TextEffectsEnabled ?? true,
            ["skip_intro"] = settings?.SkipIntroLogo ?? false,
            // NLanguageDropdown: LocManager.Languages, the current one
            ["language"] = _loc.GameLanguage,
            ["languages"] = LocManager.Languages,
            ["master"] = settings?.VolumeMaster ?? 0.5f,
            ["bgm"] = settings?.VolumeBgm ?? 0.5f,
            ["sfx"] = settings?.VolumeSfx ?? 0.5f,
            ["ambience"] = settings?.VolumeAmbience ?? 0.5f,
        };
    }

    /// <summary>One control changed (its OnTick / OnUntick / OnValueChanged):
    /// the value saved, the values answered.</summary>
    public Dictionary<string, object?> SetSetting(string name, System.Text.Json.JsonElement value)
    {
        EnsureSettings();
        var prefs = SaveManager.Instance.PrefsSave;
        var settings = SaveManager.Instance.SettingsSave;
        bool b = value.ValueKind == System.Text.Json.JsonValueKind.True;
        float f = value.ValueKind == System.Text.Json.JsonValueKind.Number ? value.GetSingle() : 0f;
        bool pref = true;
        switch (name)
        {
            case "fast_mode": if (prefs != null) prefs.FastMode = b ? FastModeType.Fast : FastModeType.Normal; break;
            case "phobia_mode": if (prefs != null) prefs.PhobiaMode = b; break;
            case "screenshake": if (prefs != null) prefs.ScreenShakeOptionIndex = Math.Clamp((int)f, 0, 4); break;
            case "run_timer": if (prefs != null) prefs.ShowRunTimer = b; break;
            case "card_indices": if (prefs != null) prefs.ShowCardIndices = b; break;
            case "long_press": if (prefs != null) prefs.IsLongPressEnabled = b; break;
            // NBestiary.OnSubmenuClosed: the mode it was left in
            case "bestiary_actions": if (prefs != null) prefs.IsBestiaryActionsPreferred = b; break;
            case "text_effects": if (prefs != null) prefs.TextEffectsEnabled = b; break;
            case "skip_intro": pref = false; if (settings != null) settings.SkipIntroLogo = b; break;
            case "language":
            {
                // NLanguageDropdown.OnDropdownItemSelected (not in a run: the
                // dropdown is greyed there)
                pref = false;
                string? code = value.ValueKind == System.Text.Json.JsonValueKind.String ? value.GetString() : null;
                if (code == null || !LocManager.Languages.Contains(code)) return Error("Unknown language " + code);
                if (settings != null) settings.Language = code;
                ApplyLanguage(code);
                break;
            }
            case "master": pref = false; if (settings != null) settings.VolumeMaster = Math.Clamp(f, 0f, 1f); break;
            case "bgm": pref = false; if (settings != null) settings.VolumeBgm = Math.Clamp(f, 0f, 1f); break;
            case "sfx": pref = false; if (settings != null) settings.VolumeSfx = Math.Clamp(f, 0f, 1f); break;
            case "ambience": pref = false; if (settings != null) settings.VolumeAmbience = Math.Clamp(f, 0f, 1f); break;
            default: return Error("Unknown setting " + name);
        }
        try
        {
            if (pref) SaveManager.Instance.SavePrefsFile();
            else SaveManager.Instance.SaveSettings();
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"[SIM] saving settings: {ex.Message}");
        }
        return Settings();
    }

    /// <summary>NResetGameplayButton (after its confirmation): the gameplay
    /// settings back to the values it sets.</summary>
    public Dictionary<string, object?> ResetGameplaySettings()
    {
        EnsureSettings();
        var settings = SaveManager.Instance.SettingsSave;
        if (settings != null)
        {
            settings.LimitFpsInBackground = true;
            settings.SkipIntroLogo = false;
        }
        var prefs = SaveManager.Instance.PrefsSave;
        if (prefs != null)
        {
            prefs.ScreenShakeOptionIndex = 2;
            prefs.FastMode = FastModeType.Normal;
            prefs.ShowRunTimer = false;
            prefs.ShowCardIndices = false;
            prefs.PhobiaMode = false;
            prefs.IsLongPressEnabled = false;
            prefs.UploadData = true;
            prefs.TextEffectsEnabled = true;
            prefs.ShowMultiplayerDrawings = true;
        }
        try
        {
            SaveManager.Instance.SaveSettings();
            SaveManager.Instance.SavePrefsFile();
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"[SIM] saving settings: {ex.Message}");
        }
        return Settings();
    }
}
