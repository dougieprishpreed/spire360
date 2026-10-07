using System.Runtime.CompilerServices;

namespace Spire.Logic;

/// <summary>
/// Single-threaded execution of the game's async code.
///
/// The real game runs its async logic on Godot's main thread through a
/// synchronisation context that queues continuations. Here every call into
/// the game runs with <see cref="Context"/> installed, so continuations queue
/// in one FIFO that <see cref="Pump"/> drains on the calling thread. Nothing
/// ever blocks on a task: when the game awaits a player decision (a
/// TaskCompletionSource the adapter owns), the queue simply runs dry and the
/// adapter reports a decision point.
///
/// This is the same model on every build. Under NativeAOT-LLVM on WASI there
/// are no threads at all; CoreLib's own single-threaded thread pool and timer
/// queue run when the adapter yields to the event loop (<see cref="Settle"/>).
/// </summary>
public static class Sched
{
    public static readonly QueueContext Context = new();

    /// <summary>Called after each queued callback runs (used to record animation steps).</summary>
    public static Action? AfterCallback;

    /// <summary>Total callbacks executed, for diagnostics.</summary>
    public static long CallbacksRun;

    /// <summary>
    /// Runs every queued continuation, including ones queued while draining.
    /// Returns the number executed. maxCallbacks guards against livelock.
    /// </summary>
    public static int Pump(int maxCallbacks = 1_000_000)
    {
        int n = 0;
        var previous = SynchronizationContext.Current;
        SynchronizationContext.SetSynchronizationContext(Context);
        try
        {
            while (n < maxCallbacks && Context.TryDequeue(out var work))
            {
                try
                {
                    work.Callback(work.State);
                }
                catch (Exception ex)
                {
                    // Matches Godot's behaviour: an exception escaping a posted
                    // continuation is logged, the loop keeps going.
                    Log.Error($"unhandled exception in game continuation: {ex}");
                }
                n++;
                CallbacksRun++;
                AfterCallback?.Invoke();
            }
        }
        finally
        {
            SynchronizationContext.SetSynchronizationContext(previous);
        }
        if (n >= maxCallbacks)
            Log.Error($"Sched.Pump: stopped after {maxCallbacks} callbacks (livelock?)");
        return n;
    }

    /// <summary>
    /// Calls into the game with the queue context installed, then pumps.
    /// Use for every game call that may start async work.
    /// </summary>
    public static void Call(Action action)
    {
        var previous = SynchronizationContext.Current;
        SynchronizationContext.SetSynchronizationContext(Context);
        try
        {
            action();
        }
        finally
        {
            SynchronizationContext.SetSynchronizationContext(previous);
        }
        Pump();
    }

    /// <summary>
    /// Starts a game task and pumps until it finishes or stalls (waits on
    /// something only a later command can provide). Returns true if it
    /// completed; rethrows its exception if it faulted.
    /// Replaces sts2-cli's <c>task.GetAwaiter().GetResult()</c>, which would
    /// block forever on a single thread.
    /// </summary>
    public static bool Run(Func<Task> start)
    {
        Task? task = null;
        Call(() => task = start());
        return Finish(task!);
    }

    public static bool Run<T>(Func<Task<T>> start, out T? result)
    {
        Task<T>? task = null;
        Call(() => task = start());
        if (Finish(task!))
        {
            result = task!.Result;
            return true;
        }
        result = default;
        return false;
    }

    /// <summary>Pumps until the task completes or nothing is left to run.</summary>
    public static bool Finish(Task task)
    {
        while (!task.IsCompleted)
        {
            if (Pump() == 0)
                break;
        }
        if (task.IsFaulted)
            ExceptionDispatchInfoThrow(task.Exception!);
        return task.IsCompleted;
    }

    private static void ExceptionDispatchInfoThrow(AggregateException ex)
    {
        var inner = ex.InnerExceptions.Count == 1 ? ex.InnerExceptions[0] : ex;
        System.Runtime.ExceptionServices.ExceptionDispatchInfo.Capture(inner).Throw();
    }

    /// <summary>
    /// Lets work that left the queue context (thread-pool items, timers,
    /// ConfigureAwait(false) continuations) run, then drains the queue again,
    /// until both are idle. Awaited only by the top-level command loop.
    /// </summary>
    public static async Task Settle(int maxRounds = 10_000)
    {
        int idleRounds = 0;
        for (int round = 0; round < maxRounds; round++)
        {
            int ran = Pump();
            await ThreadPoolYield.Instance;
            bool poolIdle = ThreadPool.PendingWorkItemCount == 0;
            if (ran == 0 && Context.IsEmpty && poolIdle)
            {
                if (++idleRounds >= 2)
                    return;
            }
            else
            {
                idleRounds = 0;
            }
        }
        Log.Error("Sched.Settle: did not go idle");
    }

    /// <summary>
    /// The queue. Post enqueues; Send runs inline (the game never needs a
    /// cross-thread Send, there is only one thread).
    /// </summary>
    public sealed class QueueContext : SynchronizationContext
    {
        private readonly Queue<(SendOrPostCallback Callback, object? State)> _queue = new();
        private readonly object _lock = new();

        public bool IsEmpty
        {
            get { lock (_lock) return _queue.Count == 0; }
        }

        public int Count
        {
            get { lock (_lock) return _queue.Count; }
        }

        public override void Post(SendOrPostCallback d, object? state)
        {
            // A lock because JIT builds may post from thread-pool threads.
            lock (_lock) _queue.Enqueue((d, state));
        }

        public override void Send(SendOrPostCallback d, object? state) => d(state);

        public override SynchronizationContext CreateCopy() => this;

        internal bool TryDequeue(out (SendOrPostCallback Callback, object? State) work)
        {
            lock (_lock) return _queue.TryDequeue(out work);
        }
    }

    /// <summary>
    /// Awaitable that resumes on the thread pool regardless of the current
    /// context (Task.Yield would post back to the queue context).
    /// </summary>
    private sealed class ThreadPoolYield : INotifyCompletion
    {
        public static readonly ThreadPoolYield Instance = new();
        public ThreadPoolYield GetAwaiter() => this;
        public bool IsCompleted => false;
        public void OnCompleted(Action continuation) =>
            ThreadPool.UnsafeQueueUserWorkItem(static c => ((Action)c!)(), continuation);
        public void GetResult() { }
    }
}
