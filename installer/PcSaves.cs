using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace Spire360Installer
{
    /// <summary>
    /// An account's Slay the Spire 2 saves on this PC, in the game's own place
    /// (%APPDATA%\SlayTheSpire2\steam\&lt;account&gt;). The console's logic reads
    /// the same files in the same format from spire\data\user, so importing is
    /// a copy (tools/import_save.py does the same by hand): profile.save and
    /// each profile's saves and run history; not the PC's settings.
    /// </summary>
    public sealed class PcSaves
    {
        static readonly string[] SaveFiles = { "progress.save", "prefs.save", "current_run.save", "current_run_mp.save" };

        public string Dir;
        public string Account;
        public bool Steam;
        public int Profiles;
        public int Wins;
        public bool RunInProgress;
        public DateTime LastPlayed;

        public static string GameSaveRoot =>
            Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "SlayTheSpire2");

        /// <summary>"Steam account …8228"</summary>
        public string Name => (Steam ? "Steam account " : "Account ") +
                              (Account.Length > 6 ? "…" + Account.Substring(Account.Length - 4) : Account);

        /// <summary>"3 profiles, 12 wins, a run in progress, last played 22 Oct 2026"</summary>
        public string Summary =>
            $"{Profiles} profile{(Profiles == 1 ? "" : "s")}, {Wins} win{(Wins == 1 ? "" : "s")}" +
            (RunInProgress ? ", a run in progress" : "") + $", last played {LastPlayed:d MMM yyyy}";

        /// <summary>The accounts with saves, the last played first (none when the
        /// game hasn't been played here).</summary>
        public static List<PcSaves> Find()
        {
            var found = new List<PcSaves>();
            foreach (var kind in new[] { "steam", "default" })
            {
                string dir = Path.Combine(GameSaveRoot, kind);
                if (!Directory.Exists(dir)) continue;
                foreach (string account in Directory.GetDirectories(dir))
                {
                    var s = Read(account, kind == "steam");
                    if (s != null) found.Add(s);
                }
            }
            return found.OrderByDescending(s => s.LastPlayed).ToList();
        }

        static PcSaves Read(string dir, bool steam)
        {
            var s = new PcSaves { Dir = dir, Account = Path.GetFileName(dir), Steam = steam };
            var files = s.Files();
            if (!files.Any(f => f.EndsWith("progress.save"))) return null;
            s.LastPlayed = files.Max(f => File.GetLastWriteTime(Path.Combine(dir, f)));
            for (int i = 1; i <= 3; i++)
            {
                string saves = Path.Combine(dir, "profile" + i, "saves");
                if (!File.Exists(Path.Combine(saves, "progress.save"))) continue;
                s.Profiles++;
                s.RunInProgress |= File.Exists(Path.Combine(saves, "current_run.save"));
                try
                {
                    var p = (Dictionary<string, object>)MiniJson.Parse(File.ReadAllText(Path.Combine(saves, "progress.save")));
                    if (p.TryGetValue("character_stats", out var cs) && cs is List<object> stats)
                        foreach (var c in stats.OfType<Dictionary<string, object>>())
                            if (c.TryGetValue("total_wins", out var w) && w is double d) s.Wins += (int)d;
                }
                catch { }
            }
            return s;
        }

        /// <summary>The save files, relative to the account's folder (only what the
        /// game's progress is).</summary>
        public List<string> Files()
        {
            var list = new List<string>();
            foreach (var name in new[] { "profile.save", "profile.save.backup" })
                if (File.Exists(Path.Combine(Dir, name))) list.Add(name);
            for (int i = 1; i <= 3; i++)
            {
                string saves = Path.Combine("profile" + i, "saves");
                foreach (var f in SaveFiles)
                    foreach (var suffix in new[] { "", ".backup" })
                        if (File.Exists(Path.Combine(Dir, saves, f + suffix))) list.Add(Path.Combine(saves, f + suffix));
                string hist = Path.Combine(Dir, saves, "history");
                if (Directory.Exists(hist))
                    list.AddRange(Directory.GetFiles(hist, "*.run").OrderBy(f => f).Select(f => Path.Combine(saves, "history", Path.GetFileName(f))));
            }
            return list;
        }

        /// <summary>The saves copied into user (a console folder's spire\data\user),
        /// which is replaced; a folder there that Setup didn't import moves to
        /// keepDir first. The saves are only read. Returns how many files.</summary>
        public int CopyTo(string user, bool userIsImported, string keepDir)
        {
            var files = Files();
            foreach (var f in files.Where(f => f.EndsWith(".save")))
            {
                try { MiniJson.Parse(File.ReadAllText(Path.Combine(Dir, f))); }
                catch (Exception e) { throw new Exception($"A save there isn't readable ({f}: {e.Message}): nothing was copied."); }
            }
            if (Directory.Exists(user))
            {
                if (userIsImported) Directory.Delete(user, true);
                else
                {
                    Directory.CreateDirectory(Path.GetDirectoryName(keepDir));
                    Directory.Move(user, keepDir);
                }
            }
            foreach (var f in files)
            {
                string target = Path.Combine(user, f);
                Directory.CreateDirectory(Path.GetDirectoryName(target));
                File.Copy(Path.Combine(Dir, f), target);
            }
            return files.Count;
        }
    }
}
