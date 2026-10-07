// Runs against the patched mock sts2.dll and checks every patch action.
using MegaCrit.Sts2.Core.Commands;
using MegaCrit.Sts2.Core.GameActions;
using MegaCrit.Sts2.Core.Localization;
using Spire.HookBridge;

int failures = 0;
void Check(string name, bool ok)
{
    Console.WriteLine($"{(ok ? "PASS" : "FAIL")} {name}");
    if (!ok) failures++;
}

var afterLog = new List<string>();
Hooks.Register("loc.raw", a => ((string)a[1]!) + "!");
Hooks.Register("coord.sum", a => ((Coord)a[0]!).X * 100 + ((Coord)a[0]!).Y);
Hooks.Register("echo", a => a[0] is int i ? i + 1 : a[0]);
Hooks.Register("delay", a => Task.CompletedTask);
Hooks.RegisterAfter("after-exec", (a, r) => afterLog.Add($"{((GameAction)a[1]!).Name}={r}"));

var t = Cmd.Wait(30f);
Check("return_completed_task", t.IsCompleted && Cmd.WaitCalls == 0);
Check("return_default (ref type)", TalkCmd.Play("hi", new object()) == null);

var table = new LocTable("cards", new Dictionary<string, string>());
Check("redirect instance method", table.GetRawText("STRIKE.title") == "STRIKE.title!");
Check("return_const bool", table.HasEntry("anything"));

var exec = new ActionExecutor();
exec.WaitUntilQueueIsEmptyOrWaitingOnNonPlayerDrivenAction();
Check("wildcard type + void return_default", true);

int r1 = exec.Execute(new GameAction { Name = "a" });
int r2 = exec.Execute(new GameAction { Name = "early" });
Check("call_after keeps result", r1 == 10 && r2 == -1);
Check("call_after sees every return", string.Join(",", afterLog) == "a=10,early=-1");

Check("redirect on value type", new Coord(3, 4).Sum() == 304);
Check("redirect generic method (int)", ActionExecutor.Echo(41) == 42);
Check("redirect generic method (string)", ActionExecutor.Echo("x") == "x");

var anim = ActionExecutor.Animate(60000);
Check("replace_calls Task.Delay", anim.IsCompleted && anim.Result == 60000);

Console.WriteLine(failures == 0 ? "ALL PASS" : $"{failures} FAILED");
return failures == 0 ? 0 : 1;
