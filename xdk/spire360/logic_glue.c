/* Starts the translated game logic (out/logic/c) in this process: the wasm
 * runtime, the module instance, and its _start on a guest thread. The game
 * then talks to it through engine/game/logic_link (SrtLink). C, because the
 * translated module's header is C. */
#include "w2c2_base.h"
#include <string.h>
#include "srt.h"
#include "srt_plat.h"
#include "logic.h"

static m5_logicInstance instance;

static void run_start(void* p) {
    m5_logicExport6_X5Fstart((m5_logicInstance*)p);
}

/* root: the folder mounted as the logic's "/" (it reads /data). Returns 0
 * once the guest thread runs. */
int spire_logic_start(const char* root) {
    static const char* const argv[1] = { "spire-logic" };
    static const char* const env[2] = { "SPIRE_DATA=/data", "DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1" };
    srt_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.root_dir = root;
    cfg.argc = 1;
    cfg.argv = argv;
    cfg.envc = 2;
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
