/*
 * host_main.c - generic test host for a w2c2-translated WASI command module.
 *
 *   host <root_dir> <script_file> [args...]
 *
 * Starts the module's _start on the guest thread, writes the script to its
 * stdin, closes stdin, then copies every stdout line to this process's
 * stdout and finally prints "exit <code>".
 *
 * Build with -DSRT_MODULE=<w2c2 module prefix>, for example m5_guest for a
 * module translated from guest.wasm, and -DSRT_MODULE_HEADER="guest.h".
 */
#include "w2c2_base.h"
#include <stdio.h>
#include "srt.h"
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

int main(int argc, char** argv) {
    srt_config cfg;
    const char* guest_argv[8];
    const char* guest_env[2];
    FILE* script;
    char line[4096];
    int i;
    int code;
    int gargc = 1;

    if (argc < 3) {
        fprintf(stderr, "usage: %s <root_dir> <script> [guest args]\n", argv[0]);
        return 2;
    }
    guest_argv[0] = "guest";
    for (i = 3; i < argc && gargc < 8; i++) {
        guest_argv[gargc++] = argv[i];
    }
    guest_env[0] = "SPIRE_TEST=yes";

    memset(&cfg, 0, sizeof(cfg));
    cfg.root_dir = argv[1];
    cfg.argc = gargc;
    cfg.argv = guest_argv;
    cfg.envc = 1;
    cfg.envp = guest_env;
    cfg.memory_limit = (srt_u64)512 * 1024 * 1024;
    cfg.virtual_timers = 1;
    if (srt_init(&cfg) != 0) {
        fprintf(stderr, "srt_init failed\n");
        return 2;
    }

    MOD_INSTANTIATE(&instance, NULL);
    srt_set_memory(MOD_MEMORY(&instance));

    if (srt_guest_start(run_start, &instance, 0, -1) != 0) {
        fprintf(stderr, "guest thread failed\n");
        return 2;
    }

    script = fopen(argv[2], "rb");
    if (script == NULL) {
        fprintf(stderr, "cannot open script %s\n", argv[2]);
        return 2;
    }
    while (fgets(line, sizeof(line), script) != NULL) {
        srt_stdin_write(line, strlen(line));
    }
    fclose(script);
    srt_stdin_close();

    for (;;) {
        long n = srt_stdout_read_line(line, sizeof(line), 1);
        if (n == -2) {
            fprintf(stderr, "line too long\n");
            break;
        }
        if (n < 0) {
            break;
        }
        printf("%s\n", line);
    }
    code = srt_guest_join();
    printf("exit %d\n", code);
    return 0;
}
