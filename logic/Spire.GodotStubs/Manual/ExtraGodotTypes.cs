// Forked from sts2-cli (https://github.com/Garglemesh/sts2-cli, MIT, (c) 2025 Hao Wu).
// See THIRD_PARTY.md. Local changes are marked "Spire360:".
namespace Godot;

// Missing Godot node types
public partial class Line2D : Node2D
{
    public Vector2[] Points { get; set; } = Array.Empty<Vector2>();
    public float Width { get; set; } = 1f;
    public Color DefaultColor { get; set; } = Color.White;
    public void AddPoint(Vector2 position, int? atPosition = null) { }
    public void ClearPoints() { }
}

public partial class CpuParticles2D : Node2D
{
    public bool Emitting { get; set; }
}

public partial class Marker2D : Node2D { }
public partial class PathFollow2D : Node2D
{
    public float Progress { get; set; }
    public float ProgressRatio { get; set; }
}
public partial class Path2D : Node2D { }

public partial class BackBufferCopy : Node2D { }
public partial class CanvasGroup : Node2D { }
public partial class CanvasItemMaterial : Material { }

public partial class NinePatchRect : Control
{
    public Texture2D? Texture { get; set; }
}

public partial class AspectRatioContainer : Container { }
public partial class VFlowContainer : FlowContainer { }

public partial class WorldEnvironment : Node { }
public partial class FastNoiseLite : global::Godot.Noise { }

public partial class Font : Resource
{
    public float GetStringSize(string text, int alignment = 0, float width = -1, int fontSize = 16) => text.Length * fontSize * 0.6f;
}

public partial class TextParagraph : global::Godot.RefCounted
{
    public void Clear() { }
    public void AddString(string text, Font font, int fontSize) { }
    public Vector2 GetSize() => Vector2.Zero;
    public float GetWidth() => 0;
}

public partial class StyleBoxEmpty : global::Godot.StyleBox { }

public partial class GradientTexture2D : Texture2D { }
public partial class Gradient : Resource { }

public partial class ParticleProcessMaterial : Material
{
    public Vector3 EmissionBoxExtents { get; set; }
}

public partial class RenderingServer
{
    public enum ViewportMsaa : long
    {
        Disabled = 0,
        Msaa2X = 1,
        Msaa4X = 2,
        Msaa8X = 3,
        Max = 4,
    }
    public static void GlobalShaderParameterSet(StringName name, Variant value) { }
}

// Input types
public enum Key : long
{
    None = 0,
    Special = 4194304,
    Escape = 4194305,
    Tab = 4194306,
    Backtab = 4194307,
    Backspace = 4194308,
    Enter = 4194309,
    KpEnter = 4194310,
    Insert = 4194311,
    Delete = 4194312,
    Pause = 4194313,
    Print = 4194314,
    Sysreq = 4194315,
    Clear = 4194316,
    Home = 4194317,
    End = 4194318,
    Left = 4194319,
    Up = 4194320,
    Right = 4194321,
    Down = 4194322,
    Pageup = 4194323,
    Pagedown = 4194324,
    Shift = 4194325,
    Ctrl = 4194326,
    Meta = 4194327,
    Alt = 4194328,
    Capslock = 4194329,
    Numlock = 4194330,
    Scrolllock = 4194331,
    F1 = 4194332,
    F2 = 4194333,
    F3 = 4194334,
    F4 = 4194335,
    F5 = 4194336,
    F6 = 4194337,
    F7 = 4194338,
    F8 = 4194339,
    F9 = 4194340,
    F10 = 4194341,
    F11 = 4194342,
    F12 = 4194343,
    F13 = 4194344,
    F14 = 4194345,
    F15 = 4194346,
    F16 = 4194347,
    F17 = 4194348,
    F18 = 4194349,
    F19 = 4194350,
    F20 = 4194351,
    F21 = 4194352,
    F22 = 4194353,
    F23 = 4194354,
    F24 = 4194355,
    F25 = 4194356,
    F26 = 4194357,
    F27 = 4194358,
    F28 = 4194359,
    F29 = 4194360,
    F30 = 4194361,
    F31 = 4194362,
    F32 = 4194363,
    F33 = 4194364,
    F34 = 4194365,
    F35 = 4194366,
    KpMultiply = 4194433,
    KpDivide = 4194434,
    KpSubtract = 4194435,
    KpPeriod = 4194436,
    KpAdd = 4194437,
    Kp0 = 4194438,
    Kp1 = 4194439,
    Kp2 = 4194440,
    Kp3 = 4194441,
    Kp4 = 4194442,
    Kp5 = 4194443,
    Kp6 = 4194444,
    Kp7 = 4194445,
    Kp8 = 4194446,
    Kp9 = 4194447,
    Menu = 4194370,
    Hyper = 4194371,
    Help = 4194373,
    Back = 4194376,
    Forward = 4194377,
    Stop = 4194378,
    Refresh = 4194379,
    Volumedown = 4194380,
    Volumemute = 4194381,
    Volumeup = 4194382,
    Mediaplay = 4194388,
    Mediastop = 4194389,
    Mediaprevious = 4194390,
    Medianext = 4194391,
    Mediarecord = 4194392,
    Homepage = 4194393,
    Favorites = 4194394,
    Search = 4194395,
    Standby = 4194396,
    Openurl = 4194397,
    Launchmail = 4194398,
    Launchmedia = 4194399,
    Launch0 = 4194400,
    Launch1 = 4194401,
    Launch2 = 4194402,
    Launch3 = 4194403,
    Launch4 = 4194404,
    Launch5 = 4194405,
    Launch6 = 4194406,
    Launch7 = 4194407,
    Launch8 = 4194408,
    Launch9 = 4194409,
    Launcha = 4194410,
    Launchb = 4194411,
    Launchc = 4194412,
    Launchd = 4194413,
    Launche = 4194414,
    Launchf = 4194415,
    Globe = 4194416,
    Keyboard = 4194417,
    JisEisu = 4194418,
    JisKana = 4194419,
    Unknown = 8388607,
    Space = 32,
    Exclam = 33,
    Quotedbl = 34,
    Numbersign = 35,
    Dollar = 36,
    Percent = 37,
    Ampersand = 38,
    Apostrophe = 39,
    Parenleft = 40,
    Parenright = 41,
    Asterisk = 42,
    Plus = 43,
    Comma = 44,
    Minus = 45,
    Period = 46,
    Slash = 47,
    Key0 = 48,
    Key1 = 49,
    Key2 = 50,
    Key3 = 51,
    Key4 = 52,
    Key5 = 53,
    Key6 = 54,
    Key7 = 55,
    Key8 = 56,
    Key9 = 57,
    Colon = 58,
    Semicolon = 59,
    Less = 60,
    Equal = 61,
    Greater = 62,
    Question = 63,
    At = 64,
    A = 65,
    B = 66,
    C = 67,
    D = 68,
    E = 69,
    F = 70,
    G = 71,
    H = 72,
    I = 73,
    J = 74,
    K = 75,
    L = 76,
    M = 77,
    N = 78,
    O = 79,
    P = 80,
    Q = 81,
    R = 82,
    S = 83,
    T = 84,
    U = 85,
    V = 86,
    W = 87,
    X = 88,
    Y = 89,
    Z = 90,
    Bracketleft = 91,
    Backslash = 92,
    Bracketright = 93,
    Asciicircum = 94,
    Underscore = 95,
    Quoteleft = 96,
    Braceleft = 123,
    Bar = 124,
    Braceright = 125,
    Asciitilde = 126,
    Yen = 165,
    Section = 167,
}
public enum MouseButton : long
{
    None = 0,
    Left = 1,
    Right = 2,
    Middle = 3,
    WheelUp = 4,
    WheelDown = 5,
    WheelLeft = 6,
    WheelRight = 7,
    Xbutton1 = 8,
    Xbutton2 = 9,
}
public partial class InputEventJoypadMotion : InputEvent
{
    public float AxisValue { get; set; }
}
public partial class InputEventAction : InputEvent
{
    public StringName Action { get; set; } = "";
}

// Error enum
public enum Error : long
{
    Ok = 0,
    Failed = 1,
    Unavailable = 2,
    Unconfigured = 3,
    Unauthorized = 4,
    ParameterRangeError = 5,
    OutOfMemory = 6,
    FileNotFound = 7,
    FileBadDrive = 8,
    FileBadPath = 9,
    FileNoPermission = 10,
    FileAlreadyInUse = 11,
    FileCantOpen = 12,
    FileCantWrite = 13,
    FileCantRead = 14,
    FileUnrecognized = 15,
    FileCorrupt = 16,
    FileMissingDependencies = 17,
    FileEof = 18,
    CantOpen = 19,
    CantCreate = 20,
    QueryFailed = 21,
    AlreadyInUse = 22,
    Locked = 23,
    Timeout = 24,
    CantConnect = 25,
    CantResolve = 26,
    ConnectionError = 27,
    CantAcquireResource = 28,
    CantFork = 29,
    InvalidData = 30,
    InvalidParameter = 31,
    AlreadyExists = 32,
    DoesNotExist = 33,
    DatabaseCantRead = 34,
    DatabaseCantWrite = 35,
    CompilationFailed = 36,
    MethodNotFound = 37,
    LinkFailed = 38,
    ScriptFailed = 39,
    CyclicLink = 40,
    InvalidDeclaration = 41,
    DuplicateSymbol = 42,
    ParseError = 43,
    Busy = 44,
    Skip = 45,
    Help = 46,
    Bug = 47,
    PrinterOnFire = 48,
}

// Tool attribute
[AttributeUsage(AttributeTargets.Class)]
public partial class ToolAttribute : Attribute { }

// ExportToolButton attribute
[AttributeUsage(AttributeTargets.Property | AttributeTargets.Field)]
public partial class ExportToolButtonAttribute : Attribute
{
    public ExportToolButtonAttribute(string text, string icon = "") { }
}

// AssemblyHasScripts attribute
[AttributeUsage(AttributeTargets.Assembly)]
public partial class AssemblyHasScriptsAttribute : Attribute
{
    public AssemblyHasScriptsAttribute() { }
    public AssemblyHasScriptsAttribute(string[] scripts) { }
}

// Signal class (not attribute) - note: this conflicts with Signal attribute,
// but decompiled code uses both patterns
// public class Signal { public Signal(GodotObject owner, StringName name) { } }

// Colors - static color constants
public static partial class Colors
{
    public static Color White { get; } = Color.White;
    public static Color Black { get; } = Color.Black;
    public static Color Red { get; } = new(1, 0, 0);
    public static Color Green { get; } = new(0, 1, 0);
    public static Color Blue { get; } = new(0, 0, 1);
    public static Color Yellow { get; } = new(1, 1, 0);
    public static Color Transparent { get; } = Color.Transparent;
    public static Color Orange { get; } = new(1, 0.65f, 0);
    public static Color Purple { get; } = new(0.63f, 0.13f, 0.94f);
    public static Color Cyan { get; } = new(0, 1, 1);
    public static Color Magenta { get; } = new(1, 0, 1);
    public static Color Gray { get; } = new(0.5f, 0.5f, 0.5f);
    public static Color DarkGray { get; } = new(0.25f, 0.25f, 0.25f);
    public static Color LightGray { get; } = new(0.75f, 0.75f, 0.75f);
}

// Range control
public partial class Range : Control
{
    public new partial class SignalName : Control.SignalName
    {
        public static readonly StringName ValueChanged = "ValueChanged";
    }
    public double Value { get; set; }
    public double MinValue { get; set; }
    public double MaxValue { get; set; } = 100;
    public double Step { get; set; } = 1;
    public double Page { get; set; }
    public float Ratio { get; set; }
}

// _ProcessCustomFX for RichTextEffect needs this signature
// Already defined in UI.cs - just ensure it's virtual
