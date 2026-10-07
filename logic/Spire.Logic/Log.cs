namespace Spire.Logic;

/// <summary>Diagnostics go to stderr; stdout carries only protocol lines.</summary>
public static class Log
{
    public static bool Verbose = Environment.GetEnvironmentVariable("SPIRE_VERBOSE") == "1";

    public static void Info(string message) => Console.Error.WriteLine("[SIM] " + message);

    public static void Debug(string message)
    {
        if (Verbose) Console.Error.WriteLine("[DBG] " + message);
    }

    public static void Warn(string message) => Console.Error.WriteLine("[WARN] " + message);

    public static void Error(string message) => Console.Error.WriteLine("[ERROR] " + message);
}
