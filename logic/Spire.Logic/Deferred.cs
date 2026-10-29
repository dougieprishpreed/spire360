namespace Spire.Logic;

/// <summary>
/// The game's saves, made after the command's response instead of inside
/// it (the "defer" patches on ProgressSaveManager.SaveProgress and
/// RunSaveManager.SaveRun). The game makes them asynchronously; headless,
/// a fight's last blow serialized and wrote the run and the progress file
/// (236 KB on a played profile) before its response went out, and the
/// console's client waited on that card. RunAll makes them before the next
/// command is read, while the client plays the response; any read, delete
/// or rename of a save (FileStore) makes them first, so the files stay as
/// the game's order leaves them.
/// </summary>
public static class Deferred
{
    private static readonly List<KeyValuePair<string, Action>> Pending = new();

    /// <summary>True while RunAll makes the queued calls (the patched methods then run).</summary>
    public static bool Running { get; private set; }

    /// <summary>SPIRE_DEFER_SAVES=0: the saves inside the command again (timing them).</summary>
    public static readonly bool Enabled = Environment.GetEnvironmentVariable("SPIRE_DEFER_SAVES") != "0";

    /// <summary>
    /// Queues run under key (replacing one queued under it: the later
    /// snapshot); false while RunAll runs (or deferring is off), so the call
    /// goes through.
    /// </summary>
    public static bool Offer(string key, Action run)
    {
        if (Running || !Enabled) return false;
        Pending.RemoveAll(p => p.Key == key);
        Pending.Add(new KeyValuePair<string, Action>(key, run));
        return true;
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
                    p.Value();
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
