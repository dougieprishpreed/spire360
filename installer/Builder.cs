using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net;
using System.Security.Cryptography;
using System.Text;
using System.Threading.Tasks;

namespace Spire360Installer
{
    /// <summary>
    /// The build, driven from Windows: a WSL instance of the installer's own
    /// (its disk in the installer's folder), the source checked out in it,
    /// and tools/install/build_all.sh run there. Its "##spire" lines become
    /// the window's stages.
    /// </summary>
    public sealed class Builder
    {
        public const string Distro = "Spire360";
        const string LinuxHome = "/opt/spire360";
        const string LinuxSource = LinuxHome + "/src/spire360";
        const string DistroManifest = "https://raw.githubusercontent.com/microsoft/WSL/master/distributions/DistributionInfo.json";

        public readonly string Here;
        public string SourceUrl = "https://github.com/dougieprishpreed/spire360.git";
        public string Branch = "main";

        public event Action<string> Line;
        /// <summary>n (1-based), total, id, title</summary>
        public event Action<int, int, string, string> Stage;
        /// <summary>id, title: the build's stages, before the first starts</summary>
        public event Action<string, string> Plan;

        public Builder(string here)
        {
            Here = here;
            Directory.CreateDirectory(StateDir);
            // source.txt beside the installer: another repository (and branch)
            string src = Path.Combine(here, "source.txt");
            if (File.Exists(src))
            {
                var lines = File.ReadAllLines(src).Select(l => l.Trim()).Where(l => l.Length > 0 && !l.StartsWith("#")).ToArray();
                if (lines.Length > 0) SourceUrl = lines[0];
                if (lines.Length > 1) Branch = lines[1];
            }
        }

        public string StateDir => Path.Combine(Here, "state");
        public string OutDir => Path.Combine(Here, "spire360");
        public string DistroDir => Path.Combine(Here, "wsl");

        void Log(string s) => Line?.Invoke(s);

        /// <summary>A Windows path as WSL mounts it (/mnt/d/...).</summary>
        public static string LinuxPath(string windowsPath)
        {
            string full = Path.GetFullPath(windowsPath);
            if (full.Length < 2 || full[1] != ':') throw new ArgumentException("Not a drive path: " + full);
            return "/mnt/" + char.ToLowerInvariant(full[0]) + full.Substring(2).Replace('\\', '/');
        }

        static string Quote(string s) => "'" + s.Replace("'", "'\\''") + "'";

        // ---- processes ----

        /// <summary>Runs a program, each line of its output to onLine and,
        /// unless quiet, to the log.</summary>
        Task<int> Exec(string file, string args, Action<string> onLine = null, bool quiet = false)
        {
            const bool utf8 = true;
            var tcs = new TaskCompletionSource<int>();
            var psi = new ProcessStartInfo(file, args)
            {
                UseShellExecute = false,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                CreateNoWindow = true,
                StandardOutputEncoding = utf8 ? Encoding.UTF8 : Encoding.Unicode,
                StandardErrorEncoding = utf8 ? Encoding.UTF8 : Encoding.Unicode,
            };
            // wsl.exe's own messages in UTF-8 rather than UTF-16
            psi.EnvironmentVariables["WSL_UTF8"] = "1";
            var p = new Process { StartInfo = psi, EnableRaisingEvents = true };
            DataReceivedEventHandler h = (s, e) =>
            {
                if (e.Data == null) return;
                string l = e.Data.Replace("\0", "");
                onLine?.Invoke(l);
                if (!quiet && !l.StartsWith("##spire ")) Log(l);
            };
            p.OutputDataReceived += h;
            p.ErrorDataReceived += h;
            p.Exited += (s, e) =>
            {
                p.WaitForExit();
                tcs.TrySetResult(p.ExitCode);
                p.Dispose();
            };
            p.Start();
            p.BeginOutputReadLine();
            p.BeginErrorReadLine();
            return tcs.Task;
        }

        /// <summary>A bash script run as root in the instance, from a file (no
        /// quoting through wsl.exe's command line).</summary>
        async Task<int> Bash(string script, Action<string> onLine = null, bool quiet = false)
        {
            string path = Path.Combine(StateDir, "job-" + Guid.NewGuid().ToString("N").Substring(0, 8) + ".sh");
            // git fails rather than waiting for a password nobody can type
            File.WriteAllText(path, "set -e\nexport GIT_TERMINAL_PROMPT=0\n" + script.Replace("\r\n", "\n") + "\n",
                              new UTF8Encoding(false));
            // double quotes: with -e, wsl.exe hands the arguments over as
            // Windows split them, where single quotes are characters
            try { return await Exec("wsl.exe", $"-d {Distro} -u root -e bash \"{LinuxPath(path)}\"", onLine, quiet); }
            finally { try { File.Delete(path); } catch { } }
        }

        /// <summary>A program's output, kept out of the log (WSL's list of the
        /// PC's other instances, its version).</summary>
        async Task<string> Capture(string file, string args)
        {
            var sb = new StringBuilder();
            await Exec(file, args, l => sb.AppendLine(l), quiet: true);
            return sb.ToString();
        }

        // ---- WSL and the instance ----

        /// <summary>WSL's version, or null when it isn't installed.</summary>
        public async Task<Version> WslVersion()
        {
            string wsl = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows), "System32", "wsl.exe");
            if (!File.Exists(wsl)) return null;
            string text;
            try { text = await Capture("wsl.exe", "--version"); }
            catch { return null; }
            foreach (var line in text.Split('\n'))
            {
                int colon = line.IndexOf(':');
                if (colon < 0 || !line.StartsWith("WSL")) continue;
                if (Version.TryParse(line.Substring(colon + 1).Trim(), out var v)) return v;
            }
            return null;
        }

        /// <summary>wsl --install --no-distribution, as administrator (Windows asks).</summary>
        public void InstallWsl()
        {
            Process.Start(new ProcessStartInfo("wsl.exe", "--install --no-distribution") { UseShellExecute = true, Verb = "runas" });
        }

        public async Task<bool> DistroExists()
        {
            string list = await Capture("wsl.exe", "--list --quiet");
            return list.Split('\n').Any(l => l.Trim() == Distro);
        }

        /// <summary>The instance, made from Microsoft's Debian image and kept in
        /// the installer's folder (wsl\ext4.vhdx).</summary>
        public async Task CreateDistro()
        {
            Directory.CreateDirectory(DistroDir);
            string image = Path.Combine(StateDir, "debian.wsl");
            if (!File.Exists(image))
            {
                var (url, sha) = await DebianImage();
                Log("Downloading " + url);
                string part = image + ".part";
                using (var wc = new WebClient())
                    await wc.DownloadFileTaskAsync(url, part);
                if (sha != null && !string.Equals(Sha256(part), sha, StringComparison.OrdinalIgnoreCase))
                {
                    File.Delete(part);
                    throw new Exception("The Debian image's checksum doesn't match: try again");
                }
                File.Move(part, image);
            }
            int rc = await Exec("wsl.exe", $"--import {Distro} \"{DistroDir}\" \"{image}\" --version 2");
            if (rc != 0) throw new Exception("wsl --import failed (" + rc + ")");
            File.Delete(image);
        }

        /// <summary>The instance's settings, each run until they're in place (a
        /// run stopped after the import comes back here): the build's tools are
        /// Linux's own, Windows' PATH (its dotnet, python) stays out.</summary>
        public async Task ConfigureDistro()
        {
            bool changed = false;
            // appended: Debian's image has a wsl.conf of its own ([boot])
            int rc = await Bash("grep -qs '^appendWindowsPath=false' /etc/wsl.conf && exit 0\n" +
                                "printf '\\n[interop]\\nappendWindowsPath=false\\n' >> /etc/wsl.conf\n" +
                                "echo @@restart",
                                l => { if (l.Trim() == "@@restart") changed = true; }, quiet: true);
            if (rc != 0) throw new Exception("Couldn't set up the build environment (see the log)");
            if (changed) await Exec("wsl.exe", $"--terminate {Distro}", quiet: true);
        }

        static async Task<(string, string)> DebianImage()
        {
            ServicePointManager.SecurityProtocol |= SecurityProtocolType.Tls12;
            string json;
            using (var wc = new WebClient())
                json = await wc.DownloadStringTaskAsync(DistroManifest);
            var root = (Dictionary<string, object>)MiniJson.Parse(json);
            var modern = (Dictionary<string, object>)root["ModernDistributions"];
            foreach (var family in modern)
            {
                if (!family.Key.StartsWith("Debian", StringComparison.OrdinalIgnoreCase)) continue;
                foreach (Dictionary<string, object> d in (List<object>)family.Value)
                {
                    if (!d.TryGetValue("Amd64Url", out var u) || !(u is Dictionary<string, object> au)) continue;
                    string sha = au.TryGetValue("Sha256", out var s) ? s as string : null;
                    if (sha != null && sha.StartsWith("0x")) sha = sha.Substring(2);
                    return ((string)au["Url"], sha);
                }
            }
            throw new Exception("Debian isn't in WSL's list of distributions");
        }

        static string Sha256(string path)
        {
            using (var f = File.OpenRead(path))
            using (var h = SHA256.Create())
                return BitConverter.ToString(h.ComputeHash(f)).Replace("-", "");
        }

        /// <summary>The removal of the instance (its disk goes with it).</summary>
        public Task<int> RemoveDistro() => Exec("wsl.exe", $"--unregister {Distro}");

        // ---- the source ----

        /// <summary>Clones or brings up to date the source; token: a GitHub
        /// token for a private repository (kept in the instance's git
        /// credentials).</summary>
        public async Task<int> FetchSource(string token = null)
        {
            var sb = new StringBuilder();
            sb.AppendLine("export DEBIAN_FRONTEND=noninteractive");
            sb.AppendLine("command -v git >/dev/null || { echo 'Installing git...'; apt-get update -qq && " +
                          "apt-get install -y -qq git ca-certificates > /dev/null; }");
            if (!string.IsNullOrEmpty(token))
            {
                sb.AppendLine("git config --global credential.helper store");
                sb.AppendLine("umask 077; printf 'https://x-access-token:%s@github.com\\n' " + Quote(token) + " > ~/.git-credentials");
            }
            sb.AppendLine($"mkdir -p {LinuxHome}/src");
            sb.AppendLine($"if [ -d {LinuxSource}/.git ]; then");
            sb.AppendLine($"  git -C {LinuxSource} fetch -q origin {Quote(Branch)}");
            sb.AppendLine("else");
            sb.AppendLine($"  git clone -q --branch {Quote(Branch)} {Quote(SourceUrl)} {LinuxSource}");
            sb.AppendLine("fi");
            sb.AppendLine($"git -C {LinuxSource} log -1 --format='source: %h %s'");
            return await Bash(sb.ToString());
        }

        public async Task<bool> HasSource()
        {
            var sb = new StringBuilder();
            int rc = await Bash($"test -d {LinuxSource}/.git && echo yes", l => sb.AppendLine(l), quiet: true);
            return rc == 0 && sb.ToString().Contains("yes");
        }

        /// <summary>The checked-out commit (short), or null.</summary>
        public async Task<string> SourceCommit()
        {
            string commit = null;
            await Bash($"git -C {LinuxSource} rev-parse --short HEAD", l => { if (l.Trim().Length >= 7) commit = l.Trim(); },
                       quiet: true);
            return commit;
        }

        /// <summary>The commits on the branch since the checkout ("hash  subject").</summary>
        public async Task<List<string>> PendingChanges()
        {
            var list = new List<string>();
            int rc = await Bash($"git -C {LinuxSource} fetch -q origin {Quote(Branch)}\n" +
                                $"git -C {LinuxSource} log --format='@@ %h  %s' HEAD..FETCH_HEAD",
                                l => { if (l.StartsWith("@@ ")) list.Add(l.Substring(3)); });
            if (rc != 0) throw new Exception("Couldn't reach the source repository");
            return list;
        }

        public Task<int> ApplyChanges() =>
            Bash($"git -C {LinuxSource} fetch -q origin {Quote(Branch)}\ngit -C {LinuxSource} reset -q --hard FETCH_HEAD\n" +
                 $"git -C {LinuxSource} log -1 --format='source: %h %s'");

        // ---- the build ----

        /// <summary>build_all.sh for the game's folder into spire360\; its stage
        /// lines to Stage. 0 when the folder is ready.</summary>
        public Task<int> Build(string gameDir, bool assetsOnly)
        {
            string args = $"--game {Quote(LinuxPath(gameDir))} --out {Quote(LinuxPath(OutDir))} --home {LinuxHome}";
            if (assetsOnly) args += " --assets-only";
            return Bash($"bash {LinuxSource}/tools/install/build_all.sh {args}", l =>
            {
                if (l.StartsWith("##spire plan "))
                {
                    var q = l.Substring(13).Split(new[] { ' ' }, 2);
                    if (q.Length == 2) Plan?.Invoke(q[0], q[1]);
                    return;
                }
                if (!l.StartsWith("##spire stage ")) return;
                var p = l.Substring(14).Split(new[] { ' ' }, 4);
                if (p.Length == 4 && int.TryParse(p[0], out int n) && int.TryParse(p[1], out int total))
                    Stage?.Invoke(n, total, p[2], p[3]);
            });
        }

        /// <summary>What the last build in spire360\ was made from (build.json).</summary>
        public Dictionary<string, object> LastBuild()
        {
            string path = Path.Combine(OutDir, "build.json");
            if (!File.Exists(path)) return null;
            try { return (Dictionary<string, object>)MiniJson.Parse(File.ReadAllText(path)); }
            catch { return null; }
        }

        /// <summary>The game's version, from its release_info.json.</summary>
        public static string GameVersion(string gameDir)
        {
            foreach (var p in new[] { Path.Combine(gameDir, "release_info.json") }
                         .Concat(Directory.Exists(gameDir) ? Directory.GetDirectories(gameDir, "data_*").Select(d => Path.Combine(d, "release_info.json")) : new string[0]))
            {
                if (!File.Exists(p)) continue;
                try
                {
                    var d = (Dictionary<string, object>)MiniJson.Parse(File.ReadAllText(p));
                    if (d.TryGetValue("version", out var v)) return v as string;
                }
                catch { }
            }
            return null;
        }
    }
}
