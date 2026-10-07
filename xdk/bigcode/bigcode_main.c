/* bigcode_main.c - runs the generated call chain and reports PASS/FAIL. */
#ifdef _XBOX
#include <xtl.h>
#endif
#include <stdio.h>
#include <string.h>
#include "gen/bigcode.h"

extern const int big_starts[];
extern const int big_chain;
extern const int big_depth;
extern const u32 big_expected;
extern const int big_functions;

static void report(const char* text) {
#ifdef _XBOX
    HANDLE h;
    DWORD written;
    OutputDebugStringA(text);
    h = CreateFileA("game:\\spire360_bigcode.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        WriteFile(h, text, (DWORD)strlen(text), &written, NULL);
        CloseHandle(h);
    }
#else
    fputs(text, stdout);
#endif
}

static int run(void) {
    char msg[256];
    u32 x = 0x12345678u;
    int i;
    for (i = 0; i < big_chain; i++)
        x = big_table[big_starts[i]](x, big_depth);
    sprintf(msg, "[bigcode] %d functions, checksum %08x, expected %08x: %s\n",
            big_functions, x, big_expected, x == big_expected ? "PASS" : "FAIL");
    report(msg);
    return x == big_expected ? 0 : 1;
}

#ifdef _XBOX
void __cdecl main(void) { run(); }
#else
int main(void) { return run(); }
#endif
