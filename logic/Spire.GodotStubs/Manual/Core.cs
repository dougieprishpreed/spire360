// Forked from sts2-cli (https://github.com/Garglemesh/sts2-cli, MIT, (c) 2025 Hao Wu).
// See THIRD_PARTY.md. Local changes are marked "Spire360:".
using System.Runtime.CompilerServices;

namespace Godot;

public partial class GodotObject : IDisposable
{
    // Spire360: real GodotObject is IDisposable (the game uses `using` on nodes).
    public void Dispose() { Dispose(true); GC.SuppressFinalize(this); }
    protected virtual void Dispose(bool disposing) { }

    public partial class SignalName { }

    public static bool IsInstanceValid(GodotObject? obj) => obj != null;
    public virtual bool IsQueuedForDeletion() => false;

    // ToSignal - must be on GodotObject (not Node) to match real Godot
    public SignalAwaiter ToSignal(GodotObject source, StringName signal)
    {
        return new SignalAwaiter();
    }

    // Bridge methods overridden in generated code
    protected virtual void SaveGodotObjectData(Bridge.GodotSerializationInfo info) { }
    protected virtual void RestoreGodotObjectData(Bridge.GodotSerializationInfo info) { }
    protected virtual bool InvokeGodotClassMethod(in NativeInterop.godot_string_name method, NativeInterop.NativeVariantPtrArgs args, out NativeInterop.godot_variant ret) { ret = default; return false; }
    protected virtual bool HasGodotClassMethod(in NativeInterop.godot_string_name method) => false;
    protected virtual bool SetGodotClassPropertyValue(in NativeInterop.godot_string_name name, in NativeInterop.godot_variant value) => false;
    protected virtual bool GetGodotClassPropertyValue(in NativeInterop.godot_string_name name, out NativeInterop.godot_variant value) { value = default; return false; }
    protected virtual void RaiseGodotClassSignalCallbacks(in NativeInterop.godot_string_name signal, NativeInterop.NativeVariantPtrArgs args) { }
    protected virtual bool HasGodotClassSignal(in NativeInterop.godot_string_name signal) => false;
}

public partial class Node : GodotObject
{
    public enum InternalMode : long
    {
        Disabled = 0,
        Front = 1,
        Back = 2,
    }

    private Node? _parent;
    private readonly List<Node> _children = new();

    public partial class MethodName : global::Godot.GodotObject.MethodName
    {
        public static readonly StringName AddChild = "AddChild";
        public static readonly StringName RemoveChild = "RemoveChild";
        public static readonly StringName QueueFree = "QueueFree";
        public static readonly StringName _Ready = "_Ready";
    }

    public partial class PropertyName : global::Godot.GodotObject.PropertyName { }
    public new partial class SignalName : GodotObject.SignalName
    {
        public static readonly StringName ProcessFrame = "ProcessFrame";
    }

    public virtual StringName Name { get; set; } = "";

    public Node? GetParent() => _parent;

    public Godot.Collections.Array<Node> GetChildren(bool includeInternal = false)
    {
        return new Godot.Collections.Array<Node>(_children);
    }

    public T? GetNodeOrNull<T>(string path) where T : class => null;
    public T? GetNodeOrNull<T>(NodePath path) where T : class => null;
    public T GetNode<T>(string path) where T : class => default!;
    public T GetNode<T>(NodePath path) where T : class => default!;

    public virtual void AddChild(Node child, bool forceReadableName = false, InternalMode mode = InternalMode.Disabled)
    {
        child._parent = this;
        _children.Add(child);
    }

    public virtual void RemoveChild(Node child)
    {
        child._parent = null;
        _children.Remove(child);
    }

    public void Reparent(Node newParent)
    {
        _parent?.RemoveChild(this);
        newParent.AddChild(this);
    }

    public virtual void QueueFree() { }

    public SceneTree GetTree() => Engine.GetMainLoop() as SceneTree ?? new SceneTree();

    public Tween CreateTween() => new Tween();
    public Viewport GetViewport() => new Viewport();
    public double GetProcessDeltaTime() => 0.016;
    public bool IsAncestorOf(Node node) => false;
    public bool IsInsideTree() => false;
    public int GetChildCount(bool includeInternal = false) => _children.Count;

    public void CallDeferred(StringName method, params Variant[] args) { }

    public virtual void _Ready() { }
    public virtual void _EnterTree() { }
    public virtual void _ExitTree() { }
    public virtual void _Process(double delta) { }
    public virtual void _Notification(int what) { }
    public virtual void _Input(InputEvent @event) { }
    public virtual void _UnhandledInput(InputEvent @event) { }
    public virtual void _UnhandledKeyInput(InputEvent @event) { }
}

public partial class SceneTree : MainLoop
{
    public new partial class SignalName : global::Godot.MainLoop.SignalName
    {
        public static new readonly StringName ProcessFrame = "process_frame";
    }

    public SceneTreeTimer CreateTimer(double timeSec, bool processAlways = true, bool processInPhysics = false, bool ignoreTimeScale = false)
    {
        var timer = new SceneTreeTimer();
        // Immediately fire the timeout in headless mode
        timer.FireTimeout();
        return timer;
    }

    public Window Root { get; } = new Window();
}

public partial class SceneTreeTimer : global::Godot.RefCounted
{
    public event Action? Timeout;

    internal void FireTimeout()
    {
        Timeout?.Invoke();
    }
}

public partial class MainLoop : GodotObject { }

public static partial class Engine
{
    private static readonly SceneTree _mainLoop = new();
    public static MainLoop GetMainLoop() => _mainLoop;
    public static bool IsEditorHint() => false;
}

public static partial class GD
{
    public static void Print(params object[] args) => Console.Error.WriteLine(string.Join("", args));
    public static void Print(string msg) => Console.Error.WriteLine(msg);
    public static void PrintErr(params object[] args) => Console.Error.WriteLine("[ERROR] " + string.Join("", args));
    public static void PrintErr(string msg) => Console.Error.WriteLine("[ERROR] " + msg);
    public static void PushError(params object[] args) => Console.Error.WriteLine("[ERROR] " + string.Join("", args));
    public static void PushError(string msg) => Console.Error.WriteLine("[ERROR] " + msg);
    public static void PushWarning(params object[] args) { }
    public static void PushWarning(string msg) { }
    public static void PrintRich(params object[] args) { }
    public static void PrintRich(string msg) { }
    public static Variant Str(params Variant[] args) => string.Join("", args.Select(a => a.ToString()));
}

public static partial class OS
{
    public static Error ShellOpen(string uri) => Error.Ok;
    public static string GetLocale() => "en";
    public static string GetName() => "headless";
    public static string GetVersion() => "0.0";
    public static string GetExecutablePath() => "";
    public static bool HasFeature(string feature) => false;
    public static bool IsDebugBuild() => false;
    public static string GetDataDir() => ".";
    public static string GetUserDataDir() => ".";
    public static string[] GetCmdlineArgs() => Array.Empty<string>();
}

public static partial class ProjectSettings
{
    public static string GlobalizePath(string path) => path;
    public static Variant GetSetting(string name, Variant @default = default) => @default;
    public static bool LoadResourcePack(string path) => false;
}

public static partial class ResourceLoader
{
    public enum CacheMode : long
    {
        Ignore = 0,
        Reuse = 1,
        Replace = 2,
        IgnoreDeep = 3,
        ReplaceDeep = 4,
    }
    /// <summary>Nothing loads headless; a texture comes back as a stand-in
    /// that keeps its path, so the presentation cues that pass one (a
    /// potion's image thrown, ...) say which.</summary>
    public static T? Load<T>(string path, string? typeHint = null, CacheMode cacheMode = CacheMode.Reuse) where T : class
    {
        if (typeof(T) == typeof(Texture2D) || typeof(T) == typeof(Texture))
            return new Texture2D { ResourcePath = path } as T;
        return null;
    }
    public static bool Exists(string path) => false;
    public static bool Exists(string path, string typeHint) => false;
}

public static partial class Time
{
    public static ulong GetTicksMsec() => (ulong)Environment.TickCount64;
}

public partial class Window : global::Godot.Viewport
{
    public new partial class SignalName : global::Godot.Viewport.SignalName
    {
        public static readonly StringName SizeChanged = "SizeChanged";
    }
}

public partial class Viewport : Node
{
    public new partial class SignalName : Node.SignalName
    {
        public static readonly StringName GuiFocusChanged = "GuiFocusChanged";
    }

    public Vector2 GetMousePosition() => Vector2.Zero;
    public Rect2 GetVisibleRect() => new Rect2(0, 0, 1920, 1080);
}
