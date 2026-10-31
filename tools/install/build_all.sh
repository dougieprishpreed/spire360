#!/usr/bin/env bash
# The whole build in one go, from your copy of Slay the Spire 2 to a folder
# ready for the console: default.xex, spire360.ini, spire/pack.s3p and
# spire/data. It runs on Linux (Debian or Ubuntu), or inside the Windows
# installer's own WSL instance, which calls it.
#
#   tools/install/build_all.sh --game "<the game's folder>" --out <folder> [options]
#
#   --game DIR      the folder holding SlayTheSpire2.pck (and the game's DLLs)
#   --out DIR       where the console folder goes (made or replaced)
#   --home DIR      where the toolchain, the tools and the work go
#                   (default $SPIRE_HOME, else ~/spire360-build)
#   --assets-only   convert the game's files and place them beside the
#                   default.xex already in --out (the XEX of the same source)
#   --jobs N        compilers at once (default: by cores and memory, 1 GB each)
#   --no-packages   don't install the system packages (they are there)
#
# Every stage can run again: what is built stays built (the toolchain, the
# tools), and a run after a source update rebuilds what changed. Each stage's
# output goes to <home>/logs/<stage>.log; the terminal gets lines the
# installer reads:
#
#   ##spire plan <id> <title>          (each stage, before the first)
#   ##spire stage <n> <total> <id> <title>
#   ##spire done <out>
#   ##spire fail <id> <log>
#
# Nothing it makes from the game leaves this machine: keep the output to
# yourself.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
GAME=""
OUT=""
HOME_DIR="${SPIRE_HOME:-$HOME/spire360-build}"
ASSETS_ONLY=0
PACKAGES=1
JOBS=""

while [ $# -gt 0 ]; do
  case "$1" in
    --game) GAME="$2"; shift 2 ;;
    --out) OUT="$2"; shift 2 ;;
    --home) HOME_DIR="$2"; shift 2 ;;
    --assets-only) ASSETS_ONLY=1; shift ;;
    --jobs) JOBS="$2"; shift 2 ;;
    --no-packages) PACKAGES=0; shift ;;
    -h|--help) sed -n '2,28p' "$0"; exit 0 ;;
    *) echo "unknown option: $1" >&2; exit 2 ;;
  esac
done

[ -n "$GAME" ] || { echo "--game is needed: the folder holding SlayTheSpire2.pck" >&2; exit 2; }
[ -n "$OUT" ] || { echo "--out is needed: where the console folder goes" >&2; exit 2; }
[ -f "$GAME/SlayTheSpire2.pck" ] || { echo "No SlayTheSpire2.pck in $GAME" >&2; exit 2; }

if [ -z "$JOBS" ]; then
  # GCC takes about 1 GB on each of the logic's large files
  mem_gb=$(awk '/MemAvailable/ { print int($2 / 1048576) }' /proc/meminfo)
  JOBS=$(nproc)
  [ "$JOBS" -gt "$mem_gb" ] && JOBS=$mem_gb
  [ "$JOBS" -lt 1 ] && JOBS=1
fi

SRC="$HOME_DIR/src"           # xsdk, vulkan4xenon, glslang-xenon
TC="$HOME_DIR/toolchain"      # xsdk's GCC (and glslang beside it)
TOOLS="$HOME_DIR/tools"       # .NET, wasi-sdk, Python's packages
WORK="$HOME_DIR/work"         # the pack, the logic's data
LOGS="$HOME_DIR/logs"
mkdir -p "$SRC" "$TC" "$TOOLS" "$WORK" "$LOGS"

export DOTNET_ROOT="$TOOLS/dotnet"
export DOTNET_CLI_TELEMETRY_OPTOUT=1 DOTNET_NOLOGO=1 DOTNET_SKIP_FIRST_TIME_EXPERIENCE=1
export NUGET_PACKAGES="$TOOLS/nuget"
export WASI_SDK_PATH="$TOOLS/wasi-sdk-29.0-x86_64-linux"
export PATH="$DOTNET_ROOT:$TOOLS/venv/bin:$PATH"

if [ "$ASSETS_ONLY" = 1 ]; then
  STAGES="python pack stage"
else
  STAGES="packages sources toolchain glslang python dotnet wasi pack logic xex stage"
  [ "$PACKAGES" = 1 ] || STAGES="${STAGES#packages }"
fi
TOTAL=$(echo $STAGES | wc -w)
N=0

title() {
  case "$1" in
    packages) echo "Installing the system packages" ;;
    sources) echo "Fetching xsdk, vulkan4xenon and glslang" ;;
    toolchain) echo "Building the Xbox 360 compiler (the longest step the first time)" ;;
    glslang) echo "Building the shader compiler" ;;
    python) echo "Setting up Python" ;;
    dotnet) echo "Setting up .NET" ;;
    wasi) echo "Setting up wasi-sdk" ;;
    pack) echo "Converting the game's art, sound and text" ;;
    logic) echo "Building the game's logic" ;;
    xex) echo "Building default.xex" ;;
    stage) echo "Putting the console folder together" ;;
  esac
}

# A stage: its output to the terminal and its log; on failure, the log's
# end again, and the fail line. The stage runs in a subshell of its own with
# -e in force (under an if or a ||, bash would ignore -e in the stage and
# go on past a failed command).
run_stage() {
  local id="$1" rc
  N=$((N + 1))
  echo "##spire stage $N $TOTAL $id $(title "$id")"
  set +e
  (set -e; "stage_$id") 2>&1 | tee "$LOGS/$id.log"
  rc=${PIPESTATUS[0]}
  set -e
  if [ "$rc" != 0 ]; then
    echo "---- the end of $LOGS/$id.log:"
    tail -n 40 "$LOGS/$id.log"
    echo "##spire fail $id $LOGS/$id.log"
    exit 1
  fi
}

# A checkout of url at branch, cloned or brought to its origin's latest
fetch() {
  local url="$1" dir="$2" branch="${3:-}"
  if [ -d "$dir/.git" ]; then
    git -C "$dir" fetch --depth 1 origin ${branch:+"$branch"}
    git -C "$dir" reset --hard FETCH_HEAD
  else
    git clone --depth 1 ${branch:+--branch "$branch"} "$url" "$dir"
  fi
}

stage_packages() {
  local sudo=""
  [ "$(id -u)" = 0 ] || sudo=sudo
  # (through env: sudo resets the environment, and apt would stop to ask)
  $sudo env DEBIAN_FRONTEND=noninteractive apt-get update
  $sudo env DEBIAN_FRONTEND=noninteractive apt-get install -y build-essential bison flex texinfo libgmp-dev \
    libmpfr-dev libmpc-dev curl xz-utils patch python3 python3-venv python3-dev cmake ninja-build git ca-certificates \
    unzip libicu-dev zlib1g-dev
}

stage_sources() {
  fetch https://github.com/xenosworks/xsdk.git "$SRC/xsdk"
  fetch https://github.com/xenosworks/vulkan4xenon.git "$SRC/vulkan4xenon"
  fetch https://github.com/xenosworks/glslang-xenon.git "$SRC/glslang-xenon" xenon
}

stage_toolchain() {
  # skips what it has built; its sources are checked by SHA-256
  (cd "$SRC/xsdk" && PREFIX="$TC" WORK="$HOME_DIR/tmp/toolchain" JOBS="$JOBS" tools/toolchain/build.sh)
  rm -rf "$HOME_DIR/tmp/toolchain"
  "$TC/bin/powerpc-xsdk-elf-gcc" --version
}

stage_glslang() {
  local g="$SRC/glslang-xenon"
  (cd "$g" && python3 update_glslang_sources.py)
  cmake -S "$g" -B "$g/build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_OPT=ON -DSPIRV_SKIP_EXECUTABLES=ON \
    -DGLSLANG_TESTS=OFF
  cmake --build "$g/build" -j "$JOBS"
  cp "$g/build/StandAlone/glslang" "$TC/bin/"
  "$TC/bin/glslang" --version
}

stage_python() {
  [ -x "$TOOLS/venv/bin/python" ] || python3 -m venv "$TOOLS/venv"
  "$TOOLS/venv/bin/pip" install -q -r "$ROOT/converter/requirements.txt"
}

stage_dotnet() {
  if ! "$DOTNET_ROOT/dotnet" --list-sdks 2>/dev/null | grep -q '^10\.'; then
    curl -fsSL https://dot.net/v1/dotnet-install.sh -o "$TOOLS/dotnet-install.sh"
    bash "$TOOLS/dotnet-install.sh" --channel 10.0 --install-dir "$DOTNET_ROOT"
  fi
  dotnet --version
}

stage_wasi() {
  if [ ! -x "$WASI_SDK_PATH/bin/clang" ]; then
    curl -fsSL https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-29/wasi-sdk-29.0-x86_64-linux.tar.gz \
      -o "$TOOLS/wasi-sdk.tar.gz"
    tar -xzf "$TOOLS/wasi-sdk.tar.gz" -C "$TOOLS"
    rm "$TOOLS/wasi-sdk.tar.gz"
  fi
  "$WASI_SDK_PATH/bin/clang" --version
}

stage_pack() {
  # Godot 4.5.1 renders the game's fonts; downloaded once
  ls "$ROOT"/converter/.cache/godot/Godot_v* > /dev/null 2>&1 || python "$ROOT/converter/convert.py" fetch-godot
  python "$ROOT/converter/convert.py" build "$GAME/SlayTheSpire2.pck" "$WORK/pack"
}

stage_logic() {
  # the logic reads the game's text (<data>/localization) in its smoke test
  rm -rf "$WORK/data/localization"
  python "$ROOT/converter/pck_tool.py" extract "$GAME/SlayTheSpire2.pck" "$WORK/data" "localization/*"
  SPIRE_DATA="$WORK/data" python "$ROOT/tools/build_logic.py" --game-dir "$GAME" \
    --stage copy,patch,jit,smoke,wasm,c
}

stage_xex() {
  XSDK="$SRC/xsdk" VULKAN4XENON="$SRC/vulkan4xenon" GLSLANG="$TC/bin/glslang" XSDK_TOOLCHAIN="$TC" \
    SPIRE_XSDK_BUILD="$HOME_DIR/xex" JOBS="$JOBS" bash "$ROOT/tools/xsdk/build.sh"
}

stage_stage() {
  local args=(--pack "$WORK/pack" --out "$OUT")
  [ "$ASSETS_ONLY" = 1 ] && args+=(--keep-xex) || args+=(--xex "$HOME_DIR/xex/default.xex")
  python "$ROOT/tools/install/stage.py" "${args[@]}"
}

for s in $STAGES; do
  echo "##spire plan $s $(title "$s")"
done
for s in $STAGES; do
  run_stage "$s"
done
echo "##spire done $OUT"
