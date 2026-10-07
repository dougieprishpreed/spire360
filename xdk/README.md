# Xbox 360 spike (milestone M0)

Four small XEXs answer the questions the plan depends on before any engine
work: what the XDK compiler accepts, whether the translated-code runtime works
on a real Xenon, how fast translated code runs there, and whether a very large
code image links.

Everything here builds with the XDK and Visual Studio 2010 (platform
"Xbox 360"). The translated C in `selftest/gen` and `bench/gen` is committed
(regenerate with `tools/gen_xdk_tests.py`); it passed on x86-64 and on
big-endian PowerPC under qemu before being committed.

## Build

From the **Xbox 360 Command Prompt** (or any prompt where `msbuild` and the
XDK are set up), in the repository root:

```
msbuild xdk\hello\hello.vcxproj       /p:Configuration=Release /p:Platform="Xbox 360"
msbuild xdk\selftest\selftest.vcxproj /p:Configuration=Release /p:Platform="Xbox 360"
msbuild xdk\bench\bench.vcxproj       /p:Configuration=Release /p:Platform="Xbox 360"

python xdk\bigcode\gen_bigcode.py --mb 64
msbuild xdk\bigcode\bigcode.vcxproj   /p:Configuration=Release /p:Platform="Xbox 360"

xdk\probe\probe.cmd
```

Each XEX lands in `xdk\<project>\Release\`. If a build fails, the error
output is exactly what I need: library names in `xdk/common/spire360.props`
(`xinput2.lib`, `xaudio2.lib`, ...) and the project keyword are best guesses
from the XDK's conventions and may need adjusting for your XDK version.

## Run and collect

Run each XEX in Xenia Canary (and on hardware when convenient). Each writes
its report next to itself (`game:\` is the XEX's folder in Xenia) and to the
debug output:

| XEX | Report | Pass when |
|---|---|---|
| hello | `game:\spire360_hello.txt` | all lines present; values match the "expect" lines; the screen cycles colour; START exits |
| selftest | `game:\spire360_selftest\result.txt` | last line ends in `PASS` |
| bench | `game:\spire360_bench\result.txt` | `check` values equal the PC ones below; send the `us` numbers |
| bigcode | `game:\spire360_bigcode.txt` | `PASS` (also note link time, XEX size, linker messages) |
| probe | `xdk\probe\probe_results.txt` | (informational: which language features compile) |

Reference bench output from this sandbox (x86-64 PC, same translated code):

```
strings     42173 us  check cb385040
sort       505092 us  check 7f645ccb
objects     23271 us  check 075862f1
math        54511 us  check 992414c544b3a77a
total      625047 us
```

The `check` values must match exactly on the 360; the time ratio gives the
Xenon/PC factor for translated code.

## What each one tells us

- **hello**: compiler version and type sizes; whether `lwbrx` byte-reversed
  loads work on unaligned addresses (decides `W2C2_XBOX_BYTEREVERSE`);
  reserve-then-commit memory (what the runtime does for the .NET heap).
- **selftest**: the whole runtime (WASI files/dirs/clocks, pipes, guest
  thread on HW thread 2, memory growth, preview2 timers) compiled by the XDK
  compiler and run big-endian on the real CPU.
- **bench**: speed of w2c2 output on Xenon vs PC.
- **bigcode**: whether the XDK linker handles code beyond the +-32 MB branch
  range (the game logic will be tens of MB). Try `--mb 32`, `64`, `96`.
- **probe**: C and C++ features of the XDK compiler, for CODING_RULES.md.
