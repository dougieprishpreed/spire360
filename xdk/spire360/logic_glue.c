/* Starts the translated game logic (out/logic/c) in this process: the wasm
 * runtime, the module instance, and its _start on a guest thread. The game
 * then talks to it through engine/game/logic_link (SrtLink). C, because the
 * translated module's header is C. */
#include "w2c2_base.h"
#include <stdio.h>
#include <string.h>
#include "srt.h"
#include "srt_plat.h"
#include "logic.h"

static m5_logicInstance instance;

static void run_start(void* p) {
    m5_logicExport6_X5Fstart((m5_logicInstance*)p);
}

/* root: the folder mounted as the logic's "/" (it reads /data); extra:
 * more of its environment ("NAME=value", the debug options of
 * spire360.ini), extra_count of them (at most 6). Returns 0 once the guest
 * thread runs. */
int spire_logic_start(const char* root, const char* const* extra, int extra_count) {
    static const char* const argv[1] = { "spire-logic" };
    static const char* env[8] = { "SPIRE_DATA=/data", "DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1" };
    int envc = 2;
    srt_config cfg;
    FILE* stderr_log;
    while (extra_count-- > 0 && envc < 8) env[envc++] = *extra++;
    /* The logic's stderr (its warnings, errors and exceptions; 10-25 KB a
     * run) in game:\spire360_logic.txt, new each start: on a console
     * without a debugger the debug output goes nowhere. */
    stderr_log = fopen("game:\\spire360_logic.txt", "wb");
    if (stderr_log != NULL) fclose(stderr_log);
    srt_plat_set_log_file("game:\\spire360_logic.txt");
    memset(&cfg, 0, sizeof(cfg));
    cfg.root_dir = root;
    cfg.argc = 1;
    cfg.argv = argv;
    cfg.envc = envc;
    cfg.envp = env;
    /* The logic's animation waits (Task.Delay) cost nothing: the engine
     * animates on its own. */
    cfg.virtual_timers = 1;
    cfg.memory_limit = (srt_u64)384 * 1024 * 1024; /* reserved; committed as it grows */
    if (srt_init(&cfg) != 0) return 1;
    m5_logicInstantiate(&instance, NULL);
    srt_set_memory(m5_logicExport6_memory(&instance));
    /* Hardware thread 2 (core 1): the frame loop keeps core 0. */
    return srt_guest_start(run_start, &instance, (size_t)16 * 1024 * 1024, 2) != 0 ? 2 : 0;
}

/* The logic's linear memory (committed as it grows, never given back), in
 * KB. */
unsigned spire_logic_memory_kb(void) {
    wasmMemory* m = m5_logicExport6_memory(&instance);
    return m ? (unsigned)(m->size / 1024) : 0;
}
