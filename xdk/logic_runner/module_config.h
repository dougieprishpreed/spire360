/* logic_runner: the translated game logic (out/logic/c) on the console.
 * Runs a self-play session and writes the timing report the M2 milestone
 * gates on. Copy data\localization next to the XEX:
 *   <xex folder>\spire360_logic\data\localization\eng\*.json
 * An optional <xex folder>\spire360_logic\script.txt replaces the default
 * commands (one JSON command per line). */
#define SRT_MODULE m5_logic
#define SRT_MODULE_HEADER "logic.h"
#define SPIRE_ROOT "game:/spire360_logic"
#define SPIRE_SCRIPT_FILE SPIRE_ROOT "/script.txt"
#define SPIRE_ENV "SPIRE_DATA=/data"
#define SPIRE_MEMORY_MB 384
#define SPIRE_STACK_KB 16384
#define SPIRE_SCRIPT \
    "{\"cmd\":\"ping\"}\n" \
    "{\"cmd\":\"selfplay\",\"character\":\"Ironclad\",\"seed\":\"x360\",\"steps\":400}\n" \
    "{\"cmd\":\"ping\"}\n" \
    "{\"cmd\":\"quit\"}\n"
