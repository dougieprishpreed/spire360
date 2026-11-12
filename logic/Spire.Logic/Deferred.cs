namespace Spire.Logic;

/// <summary>
/// The game's saves, made after the command's response instead of inside
/// it (the "defer" patches on ProgressSaveManager.SaveProgress and
/// RunSaveManager.SaveRun). The game makes them asynchronously; headless,
/// a fight's last blow serialized and wrote the run and the progress file
/// (236 KB on a played profile) before its response went out, and the
/// console's client waited on that card. RunAll makes them before the next
/// command is read, while the client plays the response; any read, delete
/// or rename of a queued save's file, or a look into a folder (FileStore),
/// makes them first, so the files stay as the game's order leaves them.
/// Another file's delete leaves them queued: the run's end deleted
/// current_run.save, which wrote the progress file (248 KB) inside the
/// fight's last response, 1.3 s on the console's USB (rp111).
/// </summary>
public static class Deferred
{
    private static readonly List<(string Key, Action Run, string? File)> Pending = new();

    /// <summary>True while RunAll makes the queued calls (the patched methods then run).</summary>
    public static bool Running { get; private set; }

    /// <summary>SPIRE_DEFER_SAVES=0: the saves inside the command again (timing them).</summary>
    public static readonly bool Enabled = Environment.GetEnvironmentVariable("SPIRE_DEFER_SAVES") != "0";

    /// <summary>
    /// Queues run under key (replacing one queued under it: the later
    /// snapshot); false while RunAll runs (or deferring is off), so the call
    /// goes through. file: the save's file name (its .tmp and .backup go
    /// with it); null: any operation makes it.
    /// </summary>
    public static bool Offer(string key, Action run, string? file = null)
    {
        if (Running || !Enabled) return false;
        Pending.RemoveAll(p => p.Key == key);
        Pending.Add((key, run, file));
        return true;
    }

    /// <summary>
    /// Before an operation on one file: the queued saves (all of them, in
    /// their order) when one of them writes that file.
    /// </summary>
    public static void RunFor(string path)
    {
        if (Running || Pending.Count == 0) return;
        var name = Path.GetFileName(path);
        foreach (var p in Pending)
        {
            if (p.File == null || name.StartsWith(p.File, StringComparison.Ordinal))
            {
                RunAll();
                return;
            }
        }
    }

    public static void RunAll()
    {
        if (Running || Pending.Count == 0) return;
        Running = true;
        try
        {
            while (Pending.Count > 0)
            {
                var p = Pending[0];
                Pending.RemoveAt(0);
                try
                {
                    p.Run();
                }
                catch (Exception ex)
                {
                    Log.Error($"deferred {p.Key}: {ex}");
                }
            }
        }
        finally
        {
            Running = false;
        }
    }

    /// <summary>A deferred async save: it completes within the call (a MemoryStream, FileStore's writes).</summary>
    public static void Finish(Task task, string what)
    {
        if (!task.IsCompleted) Log.Warn($"deferred {what} did not complete at once");
        else if (task.IsFaulted) Log.Error($"deferred {what}: {task.Exception}");
    }
}
