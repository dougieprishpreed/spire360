using System.Collections;
using System.Diagnostics;

namespace Spire.Logic;

/// <summary>
/// The seeded random agent of tools/random_runs.py, inside the logic, so the
/// console can measure itself without a PC driving it:
///
///   {"cmd":"selfplay","character":"Ironclad","seed":"s","steps":2000,"tank":false}
///
/// Plays from a fresh run until game over or the step limit and returns
/// timing per action (the numbers milestone M2 gates on) and memory use.
/// </summary>
internal static class SelfPlay
{
    public static Dictionary<string, object?> Run(RunDriver driver, string character, string seed, int maxSteps, bool tank)
    {
        var rng = new Random(StableHash(seed));
        var total = Stopwatch.StartNew();
        var sw = Stopwatch.StartNew();
        var state = driver.StartRun(character, 0, seed, "en");
        Sched.Settle();
        double startMs = sw.Elapsed.TotalMilliseconds;
        if (tank) driver.SetPlayerHp(999, 999);

        var times = new List<double>();
        var perDecision = new Dictionary<string, (int n, double ms, double max)>();
        int errors = 0;
        string status = "step_limit";
        for (int step = 0; step < maxSteps; step++)
        {
            if (Str(state, "type") == "error")
            {
                if (++errors > 25) { status = "errors"; break; }
                state = driver.CurrentDecision();
                if (Str(state, "type") == "error")
                    state = driver.ExecuteAction("proceed", null);
                continue;
            }
            var decision = Str(state, "decision") ?? "";
            if (decision == "game_over")
            {
                status = Equals(Get(state, "victory"), true) ? "victory" : "defeat";
                break;
            }
            if (tank && decision == "map_select") driver.SetPlayerHp(999, null);

            var (action, args) = Choose(rng, state);
            sw.Restart();
            state = driver.ExecuteAction(action, args);
            long before = Sched.CallbacksRun;
            Sched.Settle();
            if (Sched.CallbacksRun != before && Str(state, "type") == "decision")
                state = driver.CurrentDecision();
            Steps.Drain();
            double ms = sw.Elapsed.TotalMilliseconds;
            times.Add(ms);
            var key = decision + ":" + action;
            perDecision.TryGetValue(key, out var acc);
            perDecision[key] = (acc.n + 1, acc.ms + ms, Math.Max(acc.max, ms));
        }

        times.Sort();
        double Pct(double p) => times.Count == 0 ? 0 : times[Math.Min(times.Count - 1, (int)(p * times.Count))];
        var ctx = Get(state, "context") as IDictionary<string, object?>;
        return new Dictionary<string, object?>
        {
            ["type"] = "selfplay_result",
            ["status"] = status,
            ["character"] = character,
            ["seed"] = seed,
            ["steps"] = times.Count,
            ["errors"] = errors,
            ["act"] = ctx != null ? Get(ctx, "act") : null,
            ["floor"] = ctx != null ? Get(ctx, "floor") : null,
            ["start_run_ms"] = Math.Round(startMs, 1),
            ["total_ms"] = Math.Round(total.Elapsed.TotalMilliseconds, 1),
            ["action_ms_avg"] = times.Count == 0 ? 0 : Math.Round(times.Average(), 2),
            ["action_ms_p50"] = Math.Round(Pct(0.5), 2),
            ["action_ms_p95"] = Math.Round(Pct(0.95), 2),
            ["action_ms_max"] = Math.Round(times.Count == 0 ? 0 : times[^1], 2),
            ["by_action"] = perDecision.OrderByDescending(kv => kv.Value.ms).Take(12).ToDictionary(
                kv => kv.Key,
                kv => (object?)new Dictionary<string, object?>
                {
                    ["n"] = kv.Value.n,
                    ["avg_ms"] = Math.Round(kv.Value.ms / kv.Value.n, 2),
                    ["max_ms"] = Math.Round(kv.Value.max, 2),
                }),
            ["gc_heap_bytes"] = GC.GetTotalMemory(false),
            ["gc_collections"] = GC.CollectionCount(0),
            ["pool_work_seen"] = Sched.PoolWorkSeen,
        };
    }

    private static object? Get(IDictionary<string, object?> d, string k) => d.TryGetValue(k, out var v) ? v : null;

    private static int StableHash(string s)
    {
        unchecked
        {
            int h = 17;
            foreach (var c in s) h = h * 31 + c;
            return h;
        }
    }

    private static string? Str(IDictionary<string, object?> d, string k) => Get(d, k) as string;

    private static int Int(IDictionary<string, object?> d, string k, int fallback = 0) =>
        Get(d, k) is { } v && v is IConvertible ? Convert.ToInt32(v) : fallback;

    private static List<IDictionary<string, object?>> List(IDictionary<string, object?> d, string k)
    {
        var r = new List<IDictionary<string, object?>>();
        if (Get(d, k) is IEnumerable e and not string)
            foreach (var x in e)
                if (x is IDictionary<string, object?> item) r.Add(item);
        return r;
    }

    private static (string, Dictionary<string, object?>?) Choose(Random rng, IDictionary<string, object?> s)
    {
        Dictionary<string, object?> A(params (string, object?)[] kv) => kv.ToDictionary(p => p.Item1, p => p.Item2);
        switch (Str(s, "decision"))
        {
            case "map_select":
            {
                var c = List(s, "choices");
                if (c.Count == 0) return ("proceed", null);
                var pick = c[rng.Next(c.Count)];
                return ("select_map_node", A(("col", Int(pick, "col")), ("row", Int(pick, "row"))));
            }
            case "combat_play":
            {
                int energy = Int(s, "energy");
                var hand = List(s, "hand").Where(c => Equals(Get(c, "can_play"), true) && Int(c, "cost") <= energy).ToList();
                int enemies = List(s, "enemies").Count;
                if (hand.Count > 0 && rng.NextDouble() < 0.9)
                {
                    var card = hand[rng.Next(hand.Count)];
                    var args = A(("card_index", Int(card, "index")));
                    if (Str(card, "target_type") == "AnyEnemy" && enemies > 0) args["target_index"] = rng.Next(enemies);
                    return ("play_card", args);
                }
                return ("end_turn", null);
            }
            case "event_choice":
            case "rest_site":
            {
                var opts = List(s, "options").Where(o => !Equals(Get(o, "is_locked"), true)
                                                        && !Equals(Get(o, "is_enabled"), false)).ToList();
                return opts.Count > 0 ? ("choose_option", A(("option_index", Int(opts[rng.Next(opts.Count)], "index")))) : ("leave_room", null);
            }
            case "treasure":
                // Open, take the first relic, proceed: the old automatic pick.
                return Str(s, "stage") switch
                {
                    "closed" => ("open_chest", null),
                    "open" => ("pick_relic", A(("relic_index", 0))),
                    _ => ("proceed", null),
                };
            case "rewards":
            {
                var left = List(s, "rewards").Where(r => !Equals(Get(r, "skipped"), true)).ToList();
                return left.Count > 0 ? ("take_reward", A(("reward_index", Int(left[0], "index")))) : ("proceed", null);
            }
            case "card_reward":
            {
                var cards = List(s, "cards");
                return cards.Count > 0 && rng.NextDouble() < 0.8
                    ? ("select_card_reward", A(("card_index", rng.Next(cards.Count))))
                    : ("skip_card_reward", null);
            }
            case "bundle_select":
                return ("select_bundle", A(("bundle_index", rng.Next(Math.Max(1, List(s, "bundles").Count)))));
            case "card_select":
            {
                var cards = List(s, "cards");
                int lo = Math.Max(0, Int(s, "min_select", 1));
                int hi = Math.Min(cards.Count, Math.Max(lo, Int(s, "max_select", 1)));
                if (cards.Count == 0 || hi == 0) return ("skip_select", null);
                int n = rng.Next(Math.Max(lo, 1), hi + 1);
                var picks = Enumerable.Range(0, cards.Count).OrderBy(_ => rng.Next()).Take(n).OrderBy(i => i);
                return ("select_cards", A(("indices", string.Join(",", picks))));
            }
            case "crystal_sphere":
            {
                if (rng.NextDouble() < 0.2)
                    return ("crystal_sphere_tool", A(("tool", rng.Next(2) == 0 ? "Small" : "Big")));
                var cells = List(s, "cells");
                var hidden = cells.Where(c => Equals(Get(c, "hidden"), true)).ToList();
                var pick = (hidden.Count > 0 ? hidden : cells)[rng.Next(Math.Max(1, (hidden.Count > 0 ? hidden : cells).Count))];
                return ("crystal_sphere_click", A(("x", Int(pick, "x")), ("y", Int(pick, "y"))));
            }
            case "shop":
                return ("leave_room", null);
            default:
                return ("proceed", null);
        }
    }
}
