namespace Spire.HookBridge;

/// <summary>
/// The only thing the IL patcher makes the game assembly reference.
///
/// A patched method's body becomes "box the arguments, call Invoke(id, args),
/// unbox the result". The adapter (which references the game assembly) registers
/// the handler for each id at startup. Keeping this assembly free of any game
/// reference avoids a circular reference between the game and the adapter.
/// </summary>
public static class Hooks
{
    private static readonly Dictionary<string, Func<object?[], object?>> Handlers = new();
    private static readonly Dictionary<string, Action<object?[], object?>> AfterHandlers = new();
    private static readonly Dictionary<string, Action<object?[]>> BeforeHandlers = new();

    /// <summary>Handler for a "redirect" patch: receives (this?, args...) and returns the result.</summary>
    public static void Register(string id, Func<object?[], object?> handler) => Handlers[id] = handler;

    /// <summary>Handler for a "call_after" patch: receives (this?, args...) and the return value.</summary>
    public static void RegisterAfter(string id, Action<object?[], object?> handler) => AfterHandlers[id] = handler;

    /// <summary>Handler for a "call_before" patch: receives (this?, args...) as the method starts.</summary>
    public static void RegisterBefore(string id, Action<object?[]> handler) => BeforeHandlers[id] = handler;

    public static bool IsRegistered(string id) =>
        Handlers.ContainsKey(id) || AfterHandlers.ContainsKey(id) || BeforeHandlers.ContainsKey(id);

    /// <summary>
    /// The receiver a UI singleton getter returns where its ?. chain is a
    /// statement (redirect_calls_into "stand_in_singletons"): the chain then
    /// reaches the redirected calls instead of being skipped. Only those
    /// calls ever receive it.
    /// </summary>
    public static readonly object StandIn = new object();

    /// <summary>Called from redirected method bodies.</summary>
    public static object? Invoke(string id, object?[] args)
    {
        if (Handlers.TryGetValue(id, out var h))
            return h(args);
        // "prefix|detail" ids (redirect_calls_into) go to the prefix's handler,
        // which receives the full id as an extra first argument.
        var bar = id.IndexOf('|');
        if (bar > 0 && PrefixHandlers.TryGetValue(id[..bar], out var ph))
            return ph(id, args);
        throw new InvalidOperationException($"Spire hook '{id}' has no registered handler");
    }

    private static readonly Dictionary<string, Func<string, object?[], object?>> PrefixHandlers = new();

    /// <summary>Handler for every "prefix|..." id, e.g. all redirected UI calls.</summary>
    public static void RegisterPrefix(string prefix, Func<string, object?[], object?> handler) => PrefixHandlers[prefix] = handler;

    /// <summary>Called on entry to a method patched with "call_before".</summary>
    public static void Before(string id, object?[] args)
    {
        if (BeforeHandlers.TryGetValue(id, out var h))
            h(args);
    }

    /// <summary>Called at every return of a method patched with "call_after".</summary>
    public static void After(string id, object?[] args, object? result)
    {
        if (AfterHandlers.TryGetValue(id, out var h))
            h(args, result);
    }
}
