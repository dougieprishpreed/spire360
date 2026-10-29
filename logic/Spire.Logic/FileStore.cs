using MegaCrit.Sts2.Core.Saves;

namespace Spire.Logic;

/// <summary>
/// The game's save storage (ISaveStore) on plain System.IO, rooted at
/// &lt;data&gt;/user. Replaces GodotFileIo, which needs Godot's FileAccess.
/// On the console System.IO goes through the runtime's WASI layer to the
/// mounted game folder, so saves land in a normal directory there.
///
/// Writes go to a temporary file first and are then moved into place, so a
/// power cut mid-save leaves the previous save intact.
///
/// The game's saves come after the command's response (Deferred): every
/// other operation makes the queued ones first, so it sees the files as
/// they would be.
/// </summary>
public sealed class FileStore : ISaveStore
{
    private readonly string _root;

    public FileStore(string root)
    {
        _root = Path.GetFullPath(root);
        Directory.CreateDirectory(_root);
    }

    public string Root => _root;

    public string GetFullPath(string filename)
    {
        if (filename.StartsWith("user://", StringComparison.Ordinal)) filename = filename[7..];
        if (Path.IsPathRooted(filename) && filename.StartsWith(_root, StringComparison.Ordinal)) return filename;
        return Path.Combine(_root, filename.TrimStart('/'));
    }

    public void CreateDirectory(string directoryPath)
    {
        Deferred.RunAll();
        Directory.CreateDirectory(GetFullPath(directoryPath));
    }

    public void DeleteDirectory(string directoryPath)
    {
        Deferred.RunAll();
        var p = GetFullPath(directoryPath);
        if (Directory.Exists(p)) Directory.Delete(p, recursive: false);
    }

    public void DeleteFile(string path)
    {
        Deferred.RunAll();
        var p = GetFullPath(path);
        if (File.Exists(p)) File.Delete(p);
    }

    public void DeleteTemporaryFiles(string directoryPath)
    {
        Deferred.RunAll();
        var p = GetFullPath(directoryPath);
        if (!Directory.Exists(p)) return;
        foreach (var f in Directory.GetFiles(p, "*.tmp")) File.Delete(f);
    }

    public bool DirectoryExists(string path)
    {
        Deferred.RunAll();
        return Directory.Exists(GetFullPath(path));
    }

    public bool FileExists(string path)
    {
        Deferred.RunAll();
        return File.Exists(GetFullPath(path));
    }

    public string[] GetDirectoriesInDirectory(string directoryPath)
    {
        Deferred.RunAll();
        var p = GetFullPath(directoryPath);
        return Directory.Exists(p) ? Directory.GetDirectories(p).Select(Path.GetFileName).ToArray()! : Array.Empty<string>();
    }

    public string[] GetFilesInDirectory(string directoryPath)
    {
        Deferred.RunAll();
        var p = GetFullPath(directoryPath);
        return Directory.Exists(p) ? Directory.GetFiles(p).Select(Path.GetFileName).ToArray()! : Array.Empty<string>();
    }

    public int GetFileSize(string path)
    {
        Deferred.RunAll();
        return (int)new FileInfo(GetFullPath(path)).Length;
    }

    public DateTimeOffset GetLastModifiedTime(string path)
    {
        Deferred.RunAll();
        return File.GetLastWriteTimeUtc(GetFullPath(path));
    }

    public void SetLastModifiedTime(string path, DateTimeOffset time)
    {
        Deferred.RunAll();
        try { File.SetLastWriteTimeUtc(GetFullPath(path), time.UtcDateTime); }
        catch (Exception ex) { Log.Debug($"SetLastModifiedTime {path}: {ex.Message}"); }
    }

    public string ReadFile(string path)
    {
        Deferred.RunAll();
        var p = GetFullPath(path);
        return File.Exists(p) ? File.ReadAllText(p) : null!;
    }

    public Task<string> ReadFileAsync(string path) => Task.FromResult(ReadFile(path));

    public void RenameFile(string sourcePath, string destinationPath)
    {
        Deferred.RunAll();
        File.Move(GetFullPath(sourcePath), GetFullPath(destinationPath), overwrite: true);
    }

    public void WriteFile(string path, string content) => WriteAtomic(path, System.Text.Encoding.UTF8.GetBytes(content));

    public void WriteFile(string path, byte[] content) => WriteAtomic(path, content);

    public Task WriteFileAsync(string path, string content)
    {
        WriteFile(path, content);
        return Task.CompletedTask;
    }

    public Task WriteFileAsync(string path, byte[] content)
    {
        WriteFile(path, content);
        return Task.CompletedTask;
    }

    private void WriteAtomic(string path, byte[] content)
    {
        var p = GetFullPath(path);
        Directory.CreateDirectory(Path.GetDirectoryName(p)!);
        var tmp = p + ".tmp";
        File.WriteAllBytes(tmp, content);
        File.Move(tmp, p, overwrite: true);
    }
}
