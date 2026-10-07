/*
 * guest_selftest.c - a WASI program that exercises everything the runtime
 * provides. Built with wasi-sdk, translated by w2c2 and driven by
 * host_main.c over the stdin/stdout pipes. Each command prints one line.
 */
#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

/* WASI preview2 imports that .NET's CoreLib uses for its event loop. */
#define P2(mod, fn) __attribute__((import_module(mod), import_name(fn)))
P2("wasi:clocks/monotonic-clock@0.2.0", "now") extern uint64_t p2_now(void);
P2("wasi:clocks/monotonic-clock@0.2.0", "subscribe-duration") extern int32_t p2_sub_dur(uint64_t ns);
P2("wasi:io/poll@0.2.0", "poll") extern void p2_poll(int32_t* list, int32_t n, int32_t* ret);
P2("wasi:io/poll@0.2.0", "[method]pollable.ready") extern int32_t p2_ready(int32_t h);
P2("wasi:io/poll@0.2.0", "[resource-drop]pollable") extern void p2_drop(int32_t h);
/* Not provided by the runtime: exercises the generated trapping stubs. */
__attribute__((import_module("spire_test"), import_name("missing"))) extern int32_t missing_import(int32_t x);

static void cmd_p2(void) {
    int32_t list[3];
    int32_t ret[2];
    uint64_t t0 = p2_now();
    int32_t a = p2_sub_dur(50000000ull);  /* 50 ms */
    int32_t b = p2_sub_dur(10000000ull);  /* 10 ms */
    int32_t c = p2_sub_dur(0);
    int ready_c = p2_ready(c);
    int ready_a = p2_ready(a);
    list[0] = a; list[1] = b;
    p2_poll(list, 2, ret);
    printf("p2 %d %d n=%d first=%d waited=%d\n", ready_c, ready_a, ret[1],
           ret[1] > 0 ? ((int32_t*)(uintptr_t)ret[0])[0] : -1,
           (int)(p2_now() - t0 >= 10000000ull));
    p2_drop(a); p2_drop(b); p2_drop(c);
}

static void cmd_math(void) {
    volatile uint64_t a = 0x0123456789ABCDEFull;
    volatile double d = 1.0 / 3.0;
    union { uint32_t u; uint8_t b[4]; } un;
    int64_t neg = -1234567890123ll;
    un.u = 0x11223344u;
    /* Wasm is little-endian whatever the host is: b[0] must be 0x44. */
    printf("math %llx %llx %.17g %02x%02x%02x%02x %lld %d %.6f\n",
           (unsigned long long)(a * 3), (unsigned long long)(a >> 7), d,
           un.b[0], un.b[1], un.b[2], un.b[3], (long long)(neg / 7),
           (int)(-17 % 5), sqrt(2.0));
}

static void cmd_file(void) {
    FILE* f;
    char buf[64];
    struct stat st;
    size_t n;
    mkdir("/scratch", 0755);
    f = fopen("/scratch/a.txt", "wb");
    if (!f) { printf("file open-write-failed\n"); return; }
    fputs("hello spire", f);
    fclose(f);
    f = fopen("/scratch/a.txt", "ab");
    fputs("!", f);
    fclose(f);
    f = fopen("/scratch/a.txt", "rb");
    n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = 0;
    fseek(f, 6, SEEK_SET);
    n = fread(buf + 32, 1, 5, f);
    buf[32 + n] = 0;
    fclose(f);
    stat("/scratch/a.txt", &st);
    printf("file %s|%s|%lld|%d\n", buf, buf + 32, (long long)st.st_size, S_ISREG(st.st_mode) ? 1 : 0);
    rename("/scratch/a.txt", "/scratch/b.txt");
    printf("rename %d %d\n", access("/scratch/a.txt", F_OK) == 0, access("/scratch/b.txt", F_OK) == 0);
    unlink("/scratch/b.txt");
    rmdir("/scratch");
    printf("cleanup %d\n", access("/scratch", F_OK) == 0);
}

static void cmd_dir(void) {
    DIR* d = opendir("/data");
    struct dirent* e;
    int count = 0;
    long total = 0;
    if (!d) { printf("dir none\n"); return; }
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        count++;
        total += (long)strlen(e->d_name);
    }
    closedir(d);
    printf("dir %d %ld\n", count, total);
}

static void cmd_escape(void) {
    FILE* f = fopen("/../../etc/passwd", "rb");
    printf("escape %s\n", f ? "OPEN" : "blocked");
    if (f) fclose(f);
}

static void cmd_grow(long mb) {
    /* Allocate and touch mb megabytes to force memory.grow. */
    long i;
    unsigned long sum = 0;
    char** blocks = (char**)malloc(sizeof(char*) * (size_t)mb);
    for (i = 0; i < mb; i++) {
        blocks[i] = (char*)malloc(1 << 20);
        if (!blocks[i]) { printf("grow fail at %ld\n", i); return; }
        memset(blocks[i], (int)(i & 0xFF), 1 << 20);
    }
    for (i = 0; i < mb; i++) { sum += (unsigned char)blocks[i][12345]; free(blocks[i]); }
    free(blocks);
    printf("grow %ld %lu\n", mb, sum);
}

static void cmd_time(void) {
    struct timespec a, b;
    clock_gettime(CLOCK_MONOTONIC, &a);
    usleep(2000);
    clock_gettime(CLOCK_MONOTONIC, &b);
    printf("time %d %d\n", (int)(time(NULL) > 1600000000), (int)(b.tv_sec > a.tv_sec || b.tv_nsec > a.tv_nsec));
}

static void cmd_env(int argc, char** argv) {
    const char* v = getenv("SPIRE_TEST");
    printf("env %d %s %s\n", argc, argc > 1 ? argv[1] : "-", v ? v : "-");
}

int main(int argc, char** argv) {
    char line[256];
    setvbuf(stdout, NULL, _IOLBF, 0);
    fprintf(stderr, "guest: started\n");
    printf("ready\n");
    while (fgets(line, sizeof(line), stdin)) {
        size_t n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
        if (strncmp(line, "echo ", 5) == 0) printf("echo %s\n", line + 5);
        else if (strcmp(line, "math") == 0) cmd_math();
        else if (strcmp(line, "file") == 0) cmd_file();
        else if (strcmp(line, "dir") == 0) cmd_dir();
        else if (strcmp(line, "escape") == 0) cmd_escape();
        else if (strncmp(line, "grow ", 5) == 0) cmd_grow(atol(line + 5));
        else if (strcmp(line, "time") == 0) cmd_time();
        else if (strcmp(line, "env") == 0) cmd_env(argc, argv);
        else if (strcmp(line, "p2") == 0) cmd_p2();
        else if (strcmp(line, "missing") == 0) { fflush(stdout); printf("%d\n", missing_import(1)); }
        else if (strcmp(line, "exit3") == 0) { fflush(stdout); exit(3); }
        else if (strcmp(line, "trap") == 0) { fflush(stdout); __builtin_trap(); }
        else printf("unknown %s\n", line);
    }
    printf("eof\n");
    return 0;
}
