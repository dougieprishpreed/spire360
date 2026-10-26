/*
 * logic_host.c - runs the w2c2-translated game logic (out/logic/c) as a
 * native PC program that speaks the same line protocol as the .NET build:
 *
 *   logic_host [--root DIR | --data DIR] [--memory MB] [--real-timers]
 *
 * The guest runs on its own thread (as on the console); this thread copies
 * stdin lines to the guest and a second one copies guest stdout lines to
 * stdout, so tools/random_runs.py can drive it like `dotnet Spire.Logic.dll`.
 *
 * --root DIR (default ".") is mounted as the guest's "/"; the logic reads
 * its localization tables from /data (SPIRE_DATA=/data), i.e. DIR/data.
 * --data DIR mounts the data folder itself as "/" (SPIRE_DATA=/); without
 * either, the SPIRE_DATA of our environment is that folder when set (the
 * tools give each case its own copy that way). SPIRE_INVINCIBLE,
 * SPIRE_GODMODE, SPIRE_STEPS, SPIRE_UI_EVENTS and SPIRE_LOG_COMMANDS pass
 * through.
 * Build: tools/build_logic.py --stage native.
 */
#include "w2c2_base.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "srt.h"
#include "srt_plat.h"

#ifndef SRT_MODULE
#define SRT_MODULE m5_logic
#define SRT_MODULE_HEADER "logic.h"
#endif
#include SRT_MODULE_HEADER

#define SRT_CAT2(a, b) a##b
#define SRT_CAT(a, b) SRT_CAT2(a, b)
#define MOD_INSTANCE SRT_CAT(SRT_MODULE, Instance)
#define MOD_INSTANTIATE SRT_CAT(SRT_MODULE, Instantiate)
#define MOD_MEMORY SRT_CAT(SRT_MODULE, Export6_memory)
#define MOD_START SRT_CAT(SRT_MODULE, Export6_X5Fstart)

static MOD_INSTANCE instance;

static void run_start(void* p) {
    MOD_START((MOD_INSTANCE*)p);
}

/* Guest stdout -> our stdout, one line at a time, flushed (the driver waits
 * for each response line). The guest's end closes its stdout: the process
 * ends with it (as the .NET build exits after "quit"), whatever stdin does. */
static void pump_output(void* unused) {
    static char line[1 << 20];
    int code;
    (void)unused;
    for (;;) {
        long n = srt_stdout_read_line(line, sizeof(line), 1);
        if (n < 0) {
            break;
        }
        fwrite(line, 1, (size_t)n, stdout);
        fputc('\n', stdout);
        fflush(stdout);
    }
    code = srt_guest_join();
    /* Linear memory is committed as it grows and never given back: its size
     * now is the session's peak (the console's budget). */
    fprintf(stderr, "[logic_host] exit %d, linear memory %lu KB\n", code,
            (unsigned long)(MOD_MEMORY(&instance)->size / 1024));
    fflush(stderr);
    exit(code);
}

int main(int argc, char** argv) {
    srt_config cfg;
    static const char* const guest_argv[1] = { "spire-logic" };
    static const char* const passed[5] = { "SPIRE_INVINCIBLE", "SPIRE_GODMODE", "SPIRE_STEPS", "SPIRE_UI_EVENTS",
                                           "SPIRE_LOG_COMMANDS" };
    static char passed_env[5][128];
    static const char* guest_env[7] = { "SPIRE_DATA=/data", "DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1" };
    static char line[1 << 16];
    const char* root = NULL;
    const char* data = NULL;
    unsigned long memory_mb = 1024;
    int real_timers = 0;
    int envc = 2;
    srt_thread* out_thread;
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--root") == 0 && i + 1 < argc) {
            root = argv[++i];
        } else if (strcmp(argv[i], "--data") == 0 && i + 1 < argc) {
            data = argv[++i];
        } else if (strcmp(argv[i], "--memory") == 0 && i + 1 < argc) {
            memory_mb = strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--real-timers") == 0) {
            real_timers = 1;
        } else {
            fprintf(stderr, "usage: %s [--root DIR | --data DIR] [--memory MB] [--real-timers]\n", argv[0]);
            return 2;
        }
    }
    if (root == NULL && data == NULL) {
        data = getenv("SPIRE_DATA");
    }
    if (root == NULL && data != NULL && data[0] != '\0') {
        root = data;
        guest_env[0] = "SPIRE_DATA=/";
    }
    if (root == NULL) {
        root = ".";
    }
    for (i = 0; i < 5; i++) {
        const char* v = getenv(passed[i]);
        if (v != NULL && strlen(passed[i]) + strlen(v) + 2 <= sizeof(passed_env[i])) {
            sprintf(passed_env[i], "%s=%s", passed[i], v);
            guest_env[envc++] = passed_env[i];
        }
    }

    memset(&cfg, 0, sizeof(cfg));
    cfg.root_dir = root;
    cfg.argc = 1;
    cfg.argv = guest_argv;
    cfg.envc = envc;
    cfg.envp = guest_env;
    cfg.virtual_timers = !real_timers;
    cfg.memory_limit = (srt_u64)memory_mb * 1024 * 1024;
    if (srt_init(&cfg) != 0) {
        fprintf(stderr, "[logic_host] srt_init failed\n");
        return 2;
    }

    MOD_INSTANTIATE(&instance, NULL);
    srt_set_memory(MOD_MEMORY(&instance));
    /* Deep async chains: give the native stack room (the wasm shadow stack is
     * separate, inside linear memory). */
    if (srt_guest_start(run_start, &instance, (size_t)64 * 1024 * 1024, 2) != 0) {
        fprintf(stderr, "[logic_host] guest thread failed\n");
        return 2;
    }
    out_thread = srt_thread_start(pump_output, NULL, 0, 1);

    while (fgets(line, sizeof(line), stdin) != NULL) {
        srt_stdin_write(line, strlen(line));
    }
    srt_stdin_close();
    /* the output thread reports and exits once the guest has ended */
    srt_thread_join(out_thread);
    return 0;
}
