using System.Text;
using MegaCrit.Sts2.Core.Map;

namespace Spire.Logic;

/// <summary>
/// MapPathPruning.FindMatchingSegments with the same answer, faster: the
/// act's map generation (StandardActMap, PruneAndRepair) keys every segment
/// of every path from the start to the boss. The game builds each key with a
/// StringBuilder (formatted coordinates, string.Join over the point types)
/// and keeps them in a SortedDictionary of strings: about 0.4 s under .NET's
/// JIT, 8 s on the console's translated code at each act change. Here a
/// segment's key is a short string of its coordinates and types (equal
/// exactly when the game's keys are), in a hash dictionary; the game's key
/// text is built once per distinct key, to put the lists in the order the
/// SortedDictionary gives them (ordinal). The segments, which ones each list
/// keeps (AnyOverlappingSegments) and their order are the game's, so the
/// shuffles that follow (PrunePaths) draw the same.
/// </summary>
internal static class MapPruning
{
    private const char RowZero = '￿'; /* a start in row 0: its column is not in the key */

    public static List<List<MapPoint[]>> FindMatchingSegments(MapPoint startingMapPoint)
    {
        var segments = new Dictionary<string, List<MapPoint[]>>(StringComparer.Ordinal);
        var key = new char[16];
        foreach (List<MapPoint> path in MapPathPruning.FindAllPaths(startingMapPoint))
        {
            int count = path.Count;
            for (int i = 0; i < count - 1; i++)
            {
                MapPoint start = path[i];
                // IsValidSegmentStartMapPoint
                if (start.Children.Count <= 1 && start.coord.row != 0) continue;
                for (int j = 2; j < count - i; j++)
                {
                    MapPoint end = path[i + j];
                    // IsValidSegmentEndMapPoint
                    if (end.parents.Count < 2) continue;
                    int length = j + 1;
                    if (key.Length < length + 4) key = new char[length + 4];
                    int k = 0;
                    if (start.coord.row == 0)
                    {
                        key[k++] = RowZero;
                        key[k++] = RowZero;
                    }
                    else
                    {
                        key[k++] = (char)start.coord.col;
                        key[k++] = (char)start.coord.row;
                    }
                    key[k++] = (char)end.coord.col;
                    key[k++] = (char)end.coord.row;
                    for (int t = 0; t < length; t++) key[k++] = (char)(int)path[i + t].PointType;
                    string text = new string(key, 0, k);
                    if (!segments.TryGetValue(text, out List<MapPoint[]>? list))
                        segments[text] = new List<MapPoint[]> { Slice(path, i, length) };
                    else if (!AnyOverlapping(list, path, i, length))
                        list.Add(Slice(path, i, length));
                }
            }
        }
        // GetDuplicateSegments over the SortedDictionary: the lists of more
        // than one, by their keys' ordinal order
        var duplicates = new List<KeyValuePair<string, List<MapPoint[]>>>();
        foreach (List<MapPoint[]> list in segments.Values)
            if (list.Count > 1) duplicates.Add(new KeyValuePair<string, List<MapPoint[]>>(GameKey(list[0]), list));
        duplicates.Sort((a, b) => string.CompareOrdinal(a.Key, b.Key));
        var result = new List<List<MapPoint[]>>(duplicates.Count);
        foreach (var d in duplicates) result.Add(d.Value);
        return result;
    }

    private static MapPoint[] Slice(List<MapPoint> path, int from, int length)
    {
        var a = new MapPoint[length];
        path.CopyTo(from, a, 0, length);
        return a;
    }

    /* AnyOverlappingSegments / OverlappingSegment: a kept segment shares an
     * inner point (same place) with this one. Same key, same length. */
    private static bool AnyOverlapping(List<MapPoint[]> kept, List<MapPoint> path, int from, int length)
    {
        foreach (MapPoint[] a in kept)
        {
            if (a.Length < 3 || length < 3) continue;
            for (int i = 1; i <= a.Length - 2; i++)
                if (Equals(a[i], path[from + i])) return true;
        }
        return false;
    }

    /* MapPathPruning.GenerateSegmentKey */
    private static string GameKey(MapPoint[] segment)
    {
        var sb = new StringBuilder();
        MapPoint first = segment[0], last = segment[segment.Length - 1];
        if (first.coord.row == 0)
            sb.Append($"{first.coord.row}-{last.coord.col},{last.coord.row}-");
        else
            sb.Append($"{first.coord.col},{first.coord.row}-{last.coord.col},{last.coord.row}-");
        sb.Append(string.Join(",", segment.Select(p => (int)p.PointType)));
        return sb.ToString();
    }
}
