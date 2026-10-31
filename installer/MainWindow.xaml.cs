using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Threading;

namespace Spire360Installer
{
    public enum StageState { Pending, Active, Done, Failed }

    public sealed class StageItem : INotifyPropertyChanged
    {
        static readonly Brush Gold = new SolidColorBrush(Color.FromRgb(0xD9, 0xB4, 0x5A));
        static readonly Brush Ember = new SolidColorBrush(Color.FromRgb(0xE0, 0x60, 0x3A));
        static readonly Brush Ink = new SolidColorBrush(Color.FromRgb(0xF2, 0xE6, 0xCF));
        static readonly Brush Dim = new SolidColorBrush(Color.FromRgb(0x8C, 0x80, 0x6B));

        public string Id { get; set; }
        public string Title { get; set; }
        StageState state;
        string time = "";
        public StageState State { get => state; set { state = value; Changed(nameof(Glyph), nameof(GlyphBrush), nameof(TitleBrush)); } }
        public string Time { get => time; set { time = value; Changed(nameof(Time)); } }
        public string Glyph => state == StageState.Done ? "✔" : state == StageState.Failed ? "✖" : state == StageState.Active ? "◆" : "◇";
        public Brush GlyphBrush => state == StageState.Failed ? Ember : state == StageState.Pending ? Dim : Gold;
        public Brush TitleBrush => state == StageState.Pending ? Dim : state == StageState.Failed ? Ember : Ink;

        public event PropertyChangedEventHandler PropertyChanged;
        void Changed(params string[] names)
        {
            foreach (var n in names) PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(n));
        }
    }

    public partial class MainWindow : Window
    {
        // a first build: the instance, the toolchain while it builds (7 GB,
        // deleted after), .NET's packages, the pack and the logic's objects
        const double NeedGb = 30;

        readonly Builder builder;
        readonly ObservableCollection<StageItem> stages = new ObservableCollection<StageItem>();
        readonly DispatcherTimer clock = new DispatcherTimer { Interval = TimeSpan.FromSeconds(1) };
        StreamWriter logFile;
        StageItem active;
        DateTime activeSince;
        string gameDir;
        bool busy, updatePending;

        public MainWindow()
        {
            InitializeComponent();
            builder = new Builder(AppDomain.CurrentDomain.BaseDirectory.TrimEnd('\\'));
            StageList.ItemsSource = stages;
            stages.CollectionChanged += (s, e) =>
                ProgressHint.Visibility = stages.Count == 0 ? Visibility.Visible : Visibility.Collapsed;
            builder.Line += l => Dispatcher.BeginInvoke(new Action(() => AddLog(l)));
            builder.Plan += (id, title) => Dispatcher.BeginInvoke(new Action(() => PlanStage(id, title)));
            builder.Stage += (n, total, id, title) => Dispatcher.BeginInvoke(new Action(() => StartStage(id, title, n, total)));
            clock.Tick += (s, e) => { if (active != null) active.Time = Elapsed(DateTime.Now - activeSince); };
            Loaded += async (s, e) => await Startup();
            Closing += OnClosing;
        }

        string SettingsPath => Path.Combine(builder.StateDir, "game.txt");

        async Task Startup()
        {
            try { logFile = new StreamWriter(Path.Combine(builder.StateDir, "setup.log"), true, new UTF8Encoding(false)) { AutoFlush = true }; }
            catch { }
            gameDir = File.Exists(SettingsPath) ? File.ReadAllText(SettingsPath).Trim() : null;
            ShowGame();
            ShowSpace();
            var last = builder.LastBuild();
            if (last != null && last.TryGetValue("source", out var src))
                StatusText.Text = $"The spire360 folder here was built from source {src}" +
                                  (last.TryGetValue("game_version", out var gv) && gv != null ? $" and game version {gv}." : ".");
            if (await builder.DistroExists() && await builder.HasSource())
                await CheckUpdates(false);
            else
                UpdateText.Text = "The first build fetches the latest source.";
        }

        // ---- the game's folder ----

        void ShowGame()
        {
            bool ok = gameDir != null && File.Exists(Path.Combine(gameDir, "SlayTheSpire2.pck"));
            GamePath.Text = ok ? gameDir + (Builder.GameVersion(gameDir) is string v ? $"   (version {v})" : "")
                               : "Choose the Slay the Spire 2 folder (the one holding SlayTheSpire2.pck)";
            GamePath.Foreground = (Brush)FindResource(ok ? "Ink" : "Dim");
            BuildButton.IsEnabled = ok && !busy;
        }

        void Browse_Click(object sender, RoutedEventArgs e)
        {
            using (var dlg = new System.Windows.Forms.FolderBrowserDialog
            {
                Description = "Choose the Slay the Spire 2 folder: the one holding SlayTheSpire2.pck",
                ShowNewFolderButton = false,
                SelectedPath = gameDir ?? "",
            })
            {
                if (dlg.ShowDialog() != System.Windows.Forms.DialogResult.OK) return;
                if (!File.Exists(Path.Combine(dlg.SelectedPath, "SlayTheSpire2.pck")))
                {
                    MessageBox.Show(this, "There's no SlayTheSpire2.pck in that folder. Choose the game's own folder " +
                                          "(in Steam: right-click the game, Manage, Browse local files).",
                                    "Spire360 Setup", MessageBoxButton.OK, MessageBoxImage.Warning);
                    return;
                }
                gameDir = dlg.SelectedPath;
                File.WriteAllText(SettingsPath, gameDir);
                ShowGame();
            }
        }

        void ShowSpace()
        {
            var drive = new DriveInfo(Path.GetPathRoot(builder.Here));
            double free = drive.AvailableFreeSpace / 1073741824.0;
            SpaceText.Text = $"Free on {drive.Name.TrimEnd('\\')} {free:0} GB. A first build needs about {NeedGb:0} GB here; " +
                             "the finished spire360 folder is about 1 GB.";
            SpaceText.Foreground = (Brush)FindResource(free < NeedGb ? "Ember" : "Dim");
        }

        // ---- the stages ----

        void ResetStages()
        {
            stages.Clear();
            active = null;
            Overall.Value = 0;
        }

        StageItem Item(string id, string title)
        {
            var s = stages.FirstOrDefault(x => x.Id == id);
            if (s == null)
            {
                s = new StageItem { Id = id, Title = title };
                stages.Add(s);
            }
            return s;
        }

        void PlanStage(string id, string title) => Item(id, title);

        void StartStage(string id, string title, int n = 0, int total = 0)
        {
            if (active != null && active.State == StageState.Active) active.State = StageState.Done;
            active = Item(id, title);
            active.Title = title;
            active.State = StageState.Active;
            activeSince = DateTime.Now;
            active.Time = "";
            StatusText.Text = title + "...";
            int done = stages.Count(x => x.State == StageState.Done);
            Overall.Value = stages.Count == 0 ? 0 : (double)done / stages.Count;
            clock.Start();
        }

        void FinishStages(bool ok)
        {
            clock.Stop();
            if (active != null) active.State = ok ? StageState.Done : StageState.Failed;
            if (ok) Overall.Value = 1;
        }

        static string Elapsed(TimeSpan t) => t.TotalHours >= 1 ? t.ToString(@"h\:mm\:ss") : t.ToString(@"m\:ss");

        /// <summary>A line to the log file and the log panel. The panel follows
        /// the newest line unless it was scrolled up to read (setting its whole
        /// text each line sent it back to the top); past 4000 lines its oldest
        /// half goes (the file keeps them all).</summary>
        void AddLog(string line)
        {
            logFile?.WriteLine(line);
            var sv = LogScroll();
            bool atEnd = sv == null || sv.VerticalOffset >= sv.ScrollableHeight - 2;
            LogBox.AppendText(line + "\n");
            if (LogBox.LineCount > 4000)
            {
                int cut = LogBox.GetCharacterIndexFromLineIndex(2000);
                if (cut > 0) LogBox.Text = LogBox.Text.Substring(cut);
            }
            if (atEnd) LogBox.ScrollToEnd();
        }

        ScrollViewer logScroll;
        ScrollViewer LogScroll()
        {
            if (logScroll != null) return logScroll;
            DependencyObject d = LogBox;
            var queue = new Queue<DependencyObject>();
            queue.Enqueue(d);
            while (queue.Count > 0)
            {
                d = queue.Dequeue();
                if (d is ScrollViewer s) return logScroll = s;
                for (int i = 0; i < VisualTreeHelper.GetChildrenCount(d); i++) queue.Enqueue(VisualTreeHelper.GetChild(d, i));
            }
            return null;
        }

        void SetBusy(bool on)
        {
            busy = on;
            BrowseButton.IsEnabled = !on;
            UpdateButton.IsEnabled = !on;
            ResetButton.IsEnabled = !on;
            ShowGame();
        }

        // ---- the build ----

        async void Build_Click(object sender, RoutedEventArgs e)
        {
            if (busy || gameDir == null) return;
            SetBusy(true);
            ResetStages();
            bool ok = false;
            try { ok = await RunBuild(); }
            catch (Exception ex)
            {
                AddLog("error: " + ex.Message);
                StatusText.Text = ex.Message;
            }
            FinishStages(ok);
            SetBusy(false);
            ShowSpace();
            if (ok)
            {
                StatusText.Text = "Done. Copy the spire360 folder to your console's drive (as Games\\spire360) and start " +
                                  "default.xex from the dashboard.";
                updatePending = false;
                BuildButton.Content = "Build again";
            }
        }

        async Task<bool> RunBuild()
        {
            StartStage("wsl", "Checking WSL");
            var v = await builder.WslVersion();
            if (v == null)
            {
                FinishStages(false);
                var r = MessageBox.Show(this, "Spire360 builds in WSL 2 (Windows Subsystem for Linux), which isn't installed. " +
                                              "Install it now? Windows asks for permission, and you'll need to restart the PC; " +
                                              "then open this again.", "Spire360 Setup", MessageBoxButton.YesNo, MessageBoxImage.Question);
                if (r == MessageBoxResult.Yes) builder.InstallWsl();
                StatusText.Text = "Install WSL, restart the PC, then open this again.";
                return false;
            }
            AddLog("WSL " + v);

            StartStage("instance", "Preparing the build environment");
            if (!await builder.DistroExists())
                await builder.CreateDistro();
            await builder.ConfigureDistro();

            StartStage("source", updatePending ? "Fetching the latest source" : "Fetching the source");
            if (await builder.FetchSource() != 0)
            {
                // a private repository: a token (asked again, with what to
                // check, when GitHub refuses one)
                string refused = null;
                bool fetched = false;
                for (int attempt = 0; attempt < 3 && !fetched; attempt++)
                {
                    string token = TokenDialog.Ask(this, refused);
                    if (token == null) break;
                    fetched = await builder.FetchSource(token) == 0;
                    refused = "GitHub refused that token for this repository. On the token's page check: Repository " +
                              "access is \"Only select repositories\" with this repository chosen, and Repository " +
                              "permissions, Contents, is \"Read-only\". Then paste it again (or a new one).";
                }
                if (!fetched) throw new Exception("Couldn't fetch the source (see the log)");
            }
            if (updatePending && await builder.ApplyChanges() != 0)
                throw new Exception("Couldn't update the source (see the log)");
            string commit = await builder.SourceCommit();

            // the default.xex here already from this source and this game:
            // the game's files converted again, nothing else built
            var last = builder.LastBuild();
            string gameVersion = Builder.GameVersion(gameDir);
            bool assetsOnly = last != null && File.Exists(Path.Combine(builder.OutDir, "default.xex")) &&
                              last.TryGetValue("source", out var s) && (s as string) == commit &&
                              last.TryGetValue("game_version", out var g) && (g as string) == gameVersion && gameVersion != null;
            if (assetsOnly) AddLog("default.xex is current (source " + commit + "): converting the game's files only");

            int rc = await builder.Build(gameDir, assetsOnly);
            if (rc != 0)
            {
                StatusText.Text = "The build stopped: the log says where. Its full logs are in the build environment's " +
                                  "/opt/spire360/logs.";
                return false;
            }
            return true;
        }

        // ---- updates ----

        async void Update_Click(object sender, RoutedEventArgs e) => await CheckUpdates(true);

        async Task CheckUpdates(bool asked)
        {
            if (busy) return;
            if (!await builder.DistroExists() || !await builder.HasSource())
            {
                UpdateText.Text = "No build yet: the first build fetches the latest source.";
                return;
            }
            UpdateText.Text = "Checking...";
            SetBusy(true);
            try
            {
                var changes = await builder.PendingChanges();
                ChangeList.ItemsSource = changes;
                updatePending = changes.Count > 0;
                UpdateText.Text = changes.Count == 0 ? "Your source is the latest."
                                : $"{changes.Count} change{(changes.Count == 1 ? "" : "s")} since your build:";
                BuildButton.Content = updatePending ? "Update and rebuild" : "Build";
            }
            catch (Exception ex)
            {
                UpdateText.Text = asked ? ex.Message : "Couldn't check for updates.";
            }
            SetBusy(false);
        }

        // ---- the rest ----

        async void Reset_Click(object sender, RoutedEventArgs e)
        {
            if (busy) return;
            var r = MessageBox.Show(this,
                "This removes everything Setup made in this folder, to start again from nothing:\n\n" +
                "  • the build environment (the WSL instance \"" + Builder.Distro + "\" and its disk, in wsl\\)\n" +
                "  • the spire360 folder (copy it to your console first if you want to keep it)\n" +
                "  • Setup's own state and logs (state\\)\n\n" +
                "Spire360Setup.exe and anything else in the folder stay. Your saves on the console aren't touched.\n\n" +
                "Remove them?", "Spire360 Setup", MessageBoxButton.YesNo, MessageBoxImage.Warning, MessageBoxResult.No);
            if (r != MessageBoxResult.Yes) return;
            SetBusy(true);
            ResetStages();
            StatusText.Text = "Removing...";
            logFile?.Dispose();
            logFile = null;
            string problem = null;
            try { problem = await builder.Reset(); }
            catch (Exception ex) { problem = ex.Message; }
            Directory.CreateDirectory(builder.StateDir);
            try { logFile = new StreamWriter(Path.Combine(builder.StateDir, "setup.log"), true, new UTF8Encoding(false)) { AutoFlush = true }; }
            catch { }
            if (problem != null) AddLog(problem);
            if (!File.Exists(SettingsPath) && gameDir != null) File.WriteAllText(SettingsPath, gameDir);
            StatusText.Text = problem ?? "Everything Setup made here is removed. Build starts again from nothing.";
            UpdateText.Text = "The first build fetches the latest source.";
            ChangeList.ItemsSource = null;
            updatePending = false;
            BuildButton.Content = "Build";
            SetBusy(false);
            ShowSpace();
        }

        void Open_Click(object sender, RoutedEventArgs e)
        {
            Directory.CreateDirectory(builder.OutDir);
            Process.Start("explorer.exe", "\"" + builder.OutDir + "\"");
        }

        void OnClosing(object sender, CancelEventArgs e)
        {
            if (!busy) return;
            var r = MessageBox.Show(this, "A build is running. Close anyway? It stops where it is; the next build " +
                                          "goes on from what's done.", "Spire360 Setup", MessageBoxButton.YesNo, MessageBoxImage.Warning);
            if (r != MessageBoxResult.Yes) e.Cancel = true;
            else System.Diagnostics.Process.Start(new ProcessStartInfo("wsl.exe", "--terminate " + Builder.Distro)
            { CreateNoWindow = true, UseShellExecute = false });
        }
    }

    /// <summary>A GitHub token for a private repository; refused: why the last
    /// one didn't do.</summary>
    static class TokenDialog
    {
        public static string Ask(Window owner, string refused = null)
        {
            var box = new PasswordBox { Margin = new Thickness(0, 10, 0, 14), Padding = new Thickness(4) };
            var ok = new Button { Content = "Use this token", Style = (Style)owner.FindResource("GoldButton"), IsDefault = true };
            var w = new Window
            {
                Title = "Spire360 Setup", Owner = owner, Width = 460, SizeToContent = SizeToContent.Height,
                WindowStartupLocation = WindowStartupLocation.CenterOwner, ResizeMode = ResizeMode.NoResize,
                Background = (Brush)owner.FindResource("Panel"), Foreground = (Brush)owner.FindResource("Ink"),
            };
            ok.Click += (s, e) => w.DialogResult = true;
            var panel = new StackPanel { Margin = new Thickness(18) };
            if (refused != null)
                panel.Children.Add(new TextBlock
                {
                    TextWrapping = TextWrapping.Wrap, Margin = new Thickness(0, 0, 0, 10),
                    Foreground = (Brush)owner.FindResource("Ember"), Text = refused,
                });
            panel.Children.Add(new TextBlock
            {
                TextWrapping = TextWrapping.Wrap,
                Text = "The source repository asks for access. Paste a GitHub token that can read it (it is kept in " +
                       "the build environment only):",
            });
            panel.Children.Add(box);
            panel.Children.Add(ok);
            w.Content = panel;
            return w.ShowDialog() == true && box.Password.Trim().Length > 0 ? box.Password.Trim() : null;
        }
    }
}
