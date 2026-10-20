// The character select screen's data (NCharacterSelectScreen.SelectCharacter):
// each character's name, description, starting HP and gold and starting
// relic, as the game shows them. Asked for from the main menu, so the game
// data loads while the player is still choosing.
using MegaCrit.Sts2.Core.Localization;
using MegaCrit.Sts2.Core.Models;

namespace Spire.Logic;

public partial class RunDriver
{
    public Dictionary<string, object?> Characters(string lang = "en")
    {
        try
        {
            _loc.Lang = lang;
            EnsureModelDbInitialized();
            var list = new List<Dictionary<string, object?>>();
            foreach (var c in ModelDb.AllCharacters)
            {
                var d = new Dictionary<string, object?>
                {
                    ["id"] = c.Id.Entry,
                    ["title"] = Text(() => new LocString("characters", c.CharacterSelectTitle).GetFormattedText()),
                    ["description"] = Text(() => new LocString("characters", c.CharacterSelectDesc).GetFormattedText()),
                    ["hp"] = c.StartingHp,
                    ["gold"] = c.StartingGold,
                };
                var relic = c.StartingRelics.FirstOrDefault();
                if (relic != null)
                    d["relic"] = new Dictionary<string, object?>
                    {
                        ["id"] = relic.Id.Entry,
                        ["icon_base"] = RelicIconBase(relic),
                        ["title"] = Text(() => relic.Title.GetFormattedText()),
                        ["description"] = Text(() => relic.DynamicDescription.GetFormattedText()),
                    };
                list.Add(d);
            }
            return new Dictionary<string, object?> { ["type"] = "characters", ["characters"] = list };
        }
        catch (Exception ex)
        {
            return ErrorWithTrace("characters failed", ex);
        }
    }

    private static string Text(Func<string?> f)
    {
        try { return f() ?? ""; }
        catch (Exception ex) { Log($"text: {ex.Message}"); return ""; }
    }
}
