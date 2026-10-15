// Forked from sts2-cli (https://github.com/Garglemesh/sts2-cli, MIT, (c) 2025 Hao Wu).
// See THIRD_PARTY.md. Local changes are marked "Spire360:".
namespace Godot;

// CanvasItem
public partial class CanvasItem : Node
{
    public Color Modulate { get; set; } = Color.White;
    public Color SelfModulate { get; set; } = Color.White;
    public bool Visible { get; set; } = true;
    public virtual void Show() => Visible = true;
    public virtual void Hide() => Visible = false;
    public bool IsVisibleInTree() => Visible;
    public Tween CreateTween() => new Tween();
    public Rect2 GetViewportRect() => new Rect2(Vector2.Zero, new Vector2(1920, 1080));
}

// Control
public partial class Control : CanvasItem
{
    public enum FocusModeEnum : long
    {
        None = 0,
        Click = 1,
        All = 2,
        Accessibility = 3,
    }
    public enum MouseFilterEnum : long
    {
        Stop = 0,
        Pass = 1,
        Ignore = 2,
    }
    public enum LayoutPreset : long
    {
        TopLeft = 0,
        TopRight = 1,
        BottomLeft = 2,
        BottomRight = 3,
        CenterLeft = 4,
        CenterTop = 5,
        CenterRight = 6,
        CenterBottom = 7,
        Center = 8,
        LeftWide = 9,
        TopWide = 10,
        RightWide = 11,
        BottomWide = 12,
        VcenterWide = 13,
        HcenterWide = 14,
        FullRect = 15,
    }

    public new partial class MethodName : global::Godot.CanvasItem.MethodName { }
    public new partial class PropertyName : global::Godot.CanvasItem.PropertyName { }
    public new partial class SignalName : global::Godot.CanvasItem.SignalName
    {
        public static readonly StringName FocusEntered = "FocusEntered";
        public static readonly StringName FocusExited = "FocusExited";
        public static readonly StringName MouseEntered = "MouseEntered";
        public static readonly StringName MouseExited = "MouseExited";
        public static readonly StringName Resized = "Resized";
    }

    public Vector2 Position { get; set; }
    public Vector2 GlobalPosition { get; set; }
    public Vector2 Size { get; set; }
    public Vector2 CustomMinimumSize { get; set; }
    public float Rotation { get; set; }
    public Vector2 Scale { get; set; } = Vector2.One;
    public Vector2 PivotOffset { get; set; }
    public FocusModeEnum FocusMode { get; set; }
    public MouseFilterEnum MouseFilter { get; set; }
    public string TooltipText { get; set; } = "";

    public Rect2 GetViewportRect() => new Rect2(0, 0, 1920, 1080);
    public void GrabFocus() { }
    public void ReleaseFocus() { }
    public bool HasFocus() => false;
    public Viewport? GetViewport() => null;

    public virtual void _GuiInput(InputEvent @event) { }

    public void Connect(StringName signal, Callable callable) { }
    public void Disconnect(StringName signal, Callable callable) { }
    public void EmitSignal(StringName signal, params Variant[] args) { }
}

// Node2D
public partial class Node2D : CanvasItem
{
    public new partial class MethodName : global::Godot.CanvasItem.MethodName { }
    public new partial class PropertyName : global::Godot.CanvasItem.PropertyName { }
    public new partial class SignalName : global::Godot.CanvasItem.SignalName { }

    public Vector2 Position { get; set; }
    public Vector2 GlobalPosition { get; set; }
    public float Rotation { get; set; }
    public float RotationDegrees { get; set; }
    public Vector2 Scale { get; set; } = Vector2.One;
    public Transform2D GlobalTransform { get; set; }
    public Transform2D Transform { get; set; }
}

// Resource
public partial class Resource : global::Godot.RefCounted
{
    public string ResourcePath { get; set; } = "";
    public partial class MethodName : global::Godot.RefCounted.MethodName { }
    public partial class PropertyName : global::Godot.RefCounted.PropertyName { }
    public partial class SignalName : global::Godot.RefCounted.SignalName { }
}

// PackedScene
public partial class PackedScene : Resource
{
    public enum GenEditState : long
    {
        Disabled = 0,
        Instance = 1,
        Main = 2,
        MainInherited = 3,
    }
    // Spire360: real constraint is `class` (scenes are not loaded headless).
    public T Instantiate<T>(GenEditState editState = GenEditState.Disabled) where T : class =>
        typeof(T).GetConstructor(Type.EmptyTypes)?.Invoke(null) as T ?? default!;
    public Node Instantiate(GenEditState editState = GenEditState.Disabled) => new Node();
}

// Texture types
public partial class Texture2D : global::Godot.Texture { }
public partial class CompressedTexture2D : Texture2D { }
public partial class AtlasTexture : Texture2D
{
    public Rect2 Region { get; set; }
    public Texture2D? Atlas { get; set; }
}
public partial class ImageTexture : Texture2D { }

// Material types
public partial class Material : Resource { }
public partial class ShaderMaterial : Material
{
    public void SetShaderParameter(StringName param, Variant value) { }
    public Variant GetShaderParameter(StringName param) => default;
}
public partial class Shader : Resource { }

// Curve
public partial class Curve : Resource
{
    public float Sample(float offset) => 0f;
}

// Tween
public partial class Tween : global::Godot.RefCounted
{
    public enum EaseType : long
    {
        In = 0,
        Out = 1,
        InOut = 2,
        OutIn = 3,
    }
    public enum TransitionType : long
    {
        Linear = 0,
        Sine = 1,
        Quint = 2,
        Quart = 3,
        Quad = 4,
        Expo = 5,
        Elastic = 6,
        Cubic = 7,
        Circ = 8,
        Bounce = 9,
        Back = 10,
        Spring = 11,
    }

    public new partial class SignalName : global::Godot.RefCounted.SignalName
    {
        public static readonly StringName Finished = "finished";
    }

    public event Action? Finished;

    public PropertyTweener TweenProperty(GodotObject obj, NodePath property, Variant finalVal, double duration) => new();
    public CallbackTweener TweenCallback(Callable callback) => new();
    public MethodTweener TweenMethod(Callable method, Variant from, Variant to, double duration) => new();
    public IntervalTweener TweenInterval(double time) => new();
    public Tween Parallel() => this;
    public Tween SetParallel(bool parallel = true) => this;
    public Tween SetLoops(int loops = 0) => this;
    public Tween SetEase(EaseType ease) => this;
    public Tween SetTrans(TransitionType trans) => this;
    public Tween Chain() => this;
    public bool CustomStep(double delta) { Finished?.Invoke(); return true; }
    public void Play() { Finished?.Invoke(); }
    public void Stop() { }
    public void Pause() { }
    public void Kill() { }
    public bool IsRunning() => false;
    public bool IsValid() => true;
    public Tween BindNode(Node node) => this;
    public Tween SetSpeedScale(float scale) => this;
    public Tween SetProcessMode(ProcessModeEnum mode) => this;
    public enum ProcessModeEnum { Physics, Idle, Always }
}

public partial class PropertyTweener : global::Godot.Tweener
{
    public PropertyTweener From(Variant value) => this;
    public PropertyTweener SetEase(Tween.EaseType ease) => this;
    public PropertyTweener SetTrans(Tween.TransitionType trans) => this;
    public PropertyTweener SetDelay(double delay) => this;
    public PropertyTweener AsRelative() => this;
}

public partial class CallbackTweener : global::Godot.Tweener
{
    public CallbackTweener SetDelay(double delay) => this;
}

public partial class MethodTweener : global::Godot.Tweener
{
    public MethodTweener SetEase(Tween.EaseType ease) => this;
    public MethodTweener SetTrans(Tween.TransitionType trans) => this;
    public MethodTweener SetDelay(double delay) => this;
}

public partial class IntervalTweener : global::Godot.Tweener { }

// UI Controls
public partial class TextureRect : Control
{
    public new partial class MethodName : Control.MethodName { }
    public new partial class PropertyName : Control.PropertyName { }
    public new partial class SignalName : Control.SignalName { }
    public Texture2D? Texture { get; set; }
}

public partial class ColorRect : Control
{
    public Color Color { get; set; }
}

public partial class Panel : Control { }
public partial class PanelContainer : global::Godot.Container { }

public partial class Container : Control { }
public partial class BoxContainer : Container { }
public partial class VBoxContainer : BoxContainer { }
public partial class HBoxContainer : BoxContainer { }
public partial class FlowContainer : Container { }
public partial class HFlowContainer : FlowContainer { }
public partial class GridContainer : Container
{
    public int Columns { get; set; }
}
public partial class MarginContainer : Container { }
public partial class CenterContainer : Container { }
public partial class ScrollContainer : Container { }
public partial class SubViewportContainer : Container { }
public partial class SubViewport : Viewport { }

public partial class Label : Control
{
    public string Text { get; set; } = "";
}

public partial class RichTextLabel : Control
{
    public string Text { get; set; } = "";
    public void Clear() { Text = ""; }
    public void AppendText(string text) { Text += text; }
    public void AddText(string text) { Text += text; }
}

public partial class Button : global::Godot.BaseButton
{
    public new partial class SignalName : global::Godot.BaseButton.SignalName
    {
        public static readonly StringName Pressed = "Pressed";
    }
    public string Text { get; set; } = "";
    public event Action? Pressed;
}

public partial class BaseButton : Control
{
    public new partial class SignalName : Control.SignalName
    {
        public static readonly StringName Pressed = "Pressed";
    }
}

public partial class CheckBox : Button { }
public partial class CheckButton : Button { }

public partial class OptionButton : Button
{
    public int Selected { get; set; }
    public void AddItem(string label, int id = -1) { }
    public void Select(int idx) { Selected = idx; }
}

public partial class LineEdit : Control
{
    public string Text { get; set; } = "";
    public string PlaceholderText { get; set; } = "";
    public new partial class SignalName : Control.SignalName
    {
        public static readonly StringName TextChanged = "TextChanged";
        public static readonly StringName TextSubmitted = "TextSubmitted";
    }
}

public partial class TextEdit : Control
{
    public string Text { get; set; } = "";
}

public partial class SpinBox : global::Godot.Range
{
    public double Value { get; set; }
}

public partial class Slider : global::Godot.Range
{
    public double Value { get; set; }
}
public partial class HSlider : Slider { }
public partial class VSlider : Slider { }

public partial class ScrollBar : global::Godot.Range
{
    public double Value { get; set; }
}
public partial class HScrollBar : ScrollBar { }
public partial class VScrollBar : ScrollBar { }

public partial class Separator : Control { }
public partial class HSeparator : Separator { }
public partial class VSeparator : Separator { }

public partial class TabContainer : Container { }

// Timer
public partial class Timer : Node
{
    public double WaitTime { get; set; } = 1.0;
    public bool OneShot { get; set; }
    public bool Autostart { get; set; }
    public event Action? Timeout;
    public void Start(double timeSec = -1) { Timeout?.Invoke(); }
    public void Stop() { }
}

// Audio
public partial class AudioStream : Resource { }
public partial class AudioStreamPlayer : Node
{
    public new partial class SignalName : Node.SignalName
    {
        public static readonly StringName Finished = "Finished";
    }
    public AudioStream? Stream { get; set; }
    public float VolumeDb { get; set; }
    public void Play(float fromPosition = 0) { }
    public void Stop() { }
}

// Input
public partial class InputEvent : Resource
{
    public virtual bool IsActionPressed(StringName action, bool allowEcho = false) => false;
    public virtual bool IsActionReleased(StringName action) => false;
    public bool IsPressed() => false;
    public bool IsReleased() => true;
}
public partial class InputEventKey : global::Godot.InputEventWithModifiers { }
public partial class InputEventMouseButton : global::Godot.InputEventMouse
{
    public Vector2 Position { get; set; }
    public Vector2 GlobalPosition { get; set; }
}
public partial class InputEventMouseMotion : global::Godot.InputEventMouse
{
    public Vector2 Position { get; set; }
    public Vector2 Relative { get; set; }
}

// FileAccess
public partial class FileAccess : global::Godot.RefCounted
{
    public enum ModeFlags : long
    {
        Read = 1,
        Write = 2,
        ReadWrite = 3,
        WriteRead = 7,
    }
    public static FileAccess? Open(string path, ModeFlags flags) => null;
    public static bool FileExists(string path) => File.Exists(path);
    public string GetAsText(bool skipCr = false) => "";
    public bool StoreString(string str) => true;
    public void Close() { }
}

// DirAccess
public partial class DirAccess : global::Godot.RefCounted
{
    public static bool DirExistsAbsolute(string path) => Directory.Exists(path);
    public static Error MakeDirAbsolute(string path) { try { Directory.CreateDirectory(path); return Error.Ok; } catch { return Error.Failed; } }
    public static Error MakeDirRecursiveAbsolute(string path) { try { Directory.CreateDirectory(path); return Error.Ok; } catch { return Error.Failed; } }
    public Error MakeDirRecursive(string path) { try { Directory.CreateDirectory(Path.Combine(_path, path)); return Error.Ok; } catch { return Error.Failed; } }
    // Spire360: res:// folders are inside the game's .pck, which the logic
    // does not have. The adapter sets ResFiles to list them from the
    // converter's data/res_dirs.json (the .pck's own file order, which is
    // the order Godot lists them in): the combat background picker reads one.
    public static Func<string, string[]?>? ResFiles;
    public static DirAccess? Open(string path)
    {
        if (path.StartsWith("res://"))
        {
            var files = ResFiles?.Invoke(path.Substring(6).TrimEnd('/'));
            return files != null ? new DirAccess(path, files) : null;
        }
        return Directory.Exists(path) ? new DirAccess(path) : null;
    }
    private readonly string _path;
    private readonly string[]? _resFiles;
    private int _next = -1;
    private DirAccess(string path) { _path = path; }
    private DirAccess(string path, string[] files) { _path = path; _resFiles = files; }
    public DirAccess() { _path = ""; }
    public string[] GetFiles()
    {
        if (_resFiles != null) return (string[])_resFiles.Clone();
        try { return Directory.GetFiles(_path).Select(Path.GetFileName).ToArray()!; } catch { return Array.Empty<string>(); }
    }
    public string[] GetDirectories()
    {
        if (_resFiles != null) return Array.Empty<string>();
        try { return Directory.GetDirectories(_path).Select(Path.GetFileName).ToArray()!; } catch { return Array.Empty<string>(); }
    }
    public Error ListDirBegin() { _next = 0; return Error.Ok; }
    public string GetNext()
    {
        var files = _resFiles ?? GetFiles();
        if (_next < 0 || _next >= files.Length) return "";
        return files[_next++];
    }
    public bool CurrentIsDir() => false;
    public void ListDirEnd() { _next = -1; }
}

// Animation
public partial class AnimationPlayer : global::Godot.AnimationMixer
{
    public new partial class SignalName : global::Godot.AnimationMixer.SignalName
    {
        public static readonly StringName AnimationFinished = "AnimationFinished";
    }
    public void Play(StringName name = default, double customBlend = -1, float customSpeed = 1f, bool fromEnd = false) { }
    public void Stop(bool keepState = false) { }
}

// Particles
public partial class GpuParticles2D : Node2D
{
    public bool Emitting { get; set; }
    public Material? ProcessMaterial { get; set; }
    // build 23372702 sets Amount on enemy/VFX particles (e.g. BygoneEffigy); without
    // this no-op stub the JIT throws "Method not found: set_Amount" mid enemy turn,
    // derailing the async turn continuation and forcing the EndTurn nuclear fallback.
    public int Amount { get; set; } = 1;
    public double Lifetime { get; set; } = 1.0;
    public bool OneShot { get; set; }
    public float Explosiveness { get; set; }
    public Texture2D? Texture { get; set; }
}

// Sprite
public partial class Sprite2D : Node2D
{
    public Texture2D? Texture { get; set; }
}

// CharFXTransform for RichTextEffects
public partial class CharFXTransform : global::Godot.RefCounted
{
    public Color Color { get; set; } = Color.White;
    public Vector2 Offset { get; set; }
    public Transform2D Transform { get; set; }
    public bool Visible { get; set; } = true;
    public double ElapsedTime { get; set; }
}

// RichTextEffect
public partial class RichTextEffect : Resource
{
    public new partial class SignalName : global::Godot.Resource.SignalName { }
    public virtual bool _ProcessCustomFX(CharFXTransform charFx) => false;
}

// ResourceFormatLoader
public partial class ResourceFormatLoader : global::Godot.RefCounted
{
    public partial class MethodName : global::Godot.RefCounted.MethodName { }
    public partial class PropertyName : global::Godot.RefCounted.PropertyName { }
    public partial class SignalName : global::Godot.RefCounted.SignalName { }

    public virtual Variant _Load(string path, string originalPath, bool useSubThreads, int cacheMode) => default;
    public virtual string[] _GetRecognizedExtensions() => Array.Empty<string>();
    public virtual bool _HandlesType(StringName type) => false;
    public virtual string _GetResourceType(string path) => "";
    public virtual bool _RecognizePath(string path, StringName type) => false;
    public virtual string[] _GetDependencies(string path, bool addTypes) => Array.Empty<string>();
    public virtual bool _Exists(string path) => false;
}

// Image
public partial class Image : Resource
{
    public enum Format : long
    {
        L8 = 0,
        La8 = 1,
        R8 = 2,
        Rg8 = 3,
        Rgb8 = 4,
        Rgba8 = 5,
        Rgba4444 = 6,
        Rgb565 = 7,
        Rf = 8,
        Rgf = 9,
        Rgbf = 10,
        Rgbaf = 11,
        Rh = 12,
        Rgh = 13,
        Rgbh = 14,
        Rgbah = 15,
        Rgbe9995 = 16,
        Dxt1 = 17,
        Dxt3 = 18,
        Dxt5 = 19,
        RgtcR = 20,
        RgtcRg = 21,
        BptcRgba = 22,
        BptcRgbf = 23,
        BptcRgbfu = 24,
        Etc = 25,
        Etc2R11 = 26,
        Etc2R11S = 27,
        Etc2Rg11 = 28,
        Etc2Rg11S = 29,
        Etc2Rgb8 = 30,
        Etc2Rgba8 = 31,
        Etc2Rgb8A1 = 32,
        Etc2RaAsRg = 33,
        Dxt5RaAsRg = 34,
        Astc4X4 = 35,
        Astc4X4Hdr = 36,
        Astc8X8 = 37,
        Astc8X8Hdr = 38,
        Max = 39,
    }
    public static Image CreateEmpty(int width, int height, bool useMipmaps, Format format) => new();
    public void SetPixel(int x, int y, Color color) { }
}
