// hello.cpp - the first Spire360 XEX.
//
// Checks, in order, and reports each to the debug output (Xenia's log) and
// to game:\spire360_hello.txt:
//   1. compiler facts: _MSC_VER, type sizes, byte order, long long math
//   2. byte-reversed loads (_byteswap_ulong and the lwbrx intrinsic),
//      including an unaligned address (w2c2 can use either for wasm memory)
//   3. memory: reserve 256 MB of address space, commit 64 MB in steps
//      (what the wasm runtime does) and touch every page
//   4. D3D: create the 1280x720 device and cycle the clear colour
//   5. XInput: press START (or wait 10 s) to finish
#include <xtl.h>
#include <ppcintrinsics.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char g_report[8192];

static void Report(const char* fmt, ...)
{
    char line[512];
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf(line, sizeof(line) - 2, fmt, ap);
    va_end(ap);
    line[sizeof(line) - 2] = '\0';
    strcat(line, "\n");
    OutputDebugStringA(line);
    if (strlen(g_report) + strlen(line) < sizeof(g_report))
        strcat(g_report, line);
}

static void SaveReport()
{
    HANDLE h = CreateFileA("game:\\spire360_hello.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
    {
        OutputDebugStringA("[hello] could not write game:\\spire360_hello.txt\n");
        return;
    }
    DWORD written = 0;
    WriteFile(h, g_report, (DWORD)strlen(g_report), &written, NULL);
    CloseHandle(h);
}

static void CompilerFacts()
{
    Report("[hello] _MSC_VER=%d", _MSC_VER);
    Report("[hello] sizeof: short %d int %d long %d long long %d void* %d size_t %d wchar_t %d",
           (int)sizeof(short), (int)sizeof(int), (int)sizeof(long), (int)sizeof(long long),
           (int)sizeof(void*), (int)sizeof(size_t), (int)sizeof(wchar_t));
    unsigned int probe = 0x11223344;
    Report("[hello] first byte of 0x11223344: %02x (11 = big-endian)", *(unsigned char*)&probe);
    unsigned long long a = 0x0123456789ABCDEFull;
    long long n = -1234567890123ll;
    Report("[hello] 64-bit: %llx %llx %lld %d", a * 3, a >> 7, n / 7, (int)(-17 % 5));
    Report("[hello] expect:  369d0369d0369cd 2468acf13579b -176366841447 -2");
}

static void ByteSwaps()
{
    static unsigned char buf[16] = { 0x44, 0x33, 0x22, 0x11, 0x88, 0x77, 0x66, 0x55,
                                     0xAA, 0xBB, 0xCC, 0xDD, 0x01, 0x02, 0x03, 0x04 };
    unsigned int v;
    memcpy(&v, buf, 4);
    Report("[hello] _byteswap_ulong: %08x (expect 11223344)", _byteswap_ulong(v));
    Report("[hello] __loadwordbytereverse aligned: %08x (expect 11223344)", __loadwordbytereverse(0, buf));
    // Unaligned: wasm code does unaligned loads freely. If this line never
    // appears, lwbrx on unaligned addresses faults and must stay disabled.
    Report("[hello] __loadwordbytereverse offset 1: %08x (expect 88112233)", __loadwordbytereverse(1, buf));
    Report("[hello] __loadwordbytereverse offset 7: %08x (expect ccbbaa55)", __loadwordbytereverse(7, buf));
}

static void MemoryReserveCommit()
{
    const SIZE_T reserve = 256u * 1024 * 1024;
    const SIZE_T step = 4u * 1024 * 1024;
    char* base = (char*)VirtualAlloc(NULL, reserve, MEM_RESERVE, PAGE_READWRITE);
    if (!base)
    {
        Report("[hello] VirtualAlloc MEM_RESERVE 256 MB failed (%u)", GetLastError());
        return;
    }
    SIZE_T committed = 0;
    while (committed < 64u * 1024 * 1024)
    {
        if (!VirtualAlloc(base + committed, step, MEM_COMMIT, PAGE_READWRITE))
        {
            Report("[hello] MEM_COMMIT failed at %u MB (%u)", (unsigned)(committed >> 20), GetLastError());
            break;
        }
        for (SIZE_T i = 0; i < step; i += 4096)
            base[committed + i] = (char)i;
        committed += step;
    }
    MEMORYSTATUS ms;
    GlobalMemoryStatus(&ms);
    Report("[hello] reserve+commit ok: %u MB committed; free physical %u MB of %u MB",
           (unsigned)(committed >> 20), (unsigned)(ms.dwAvailPhys >> 20), (unsigned)(ms.dwTotalPhys >> 20));
    VirtualFree(base, 0, MEM_RELEASE);
}

void __cdecl main()
{
    g_report[0] = '\0';
    Report("[hello] Spire360 hello XEX");
    CompilerFacts();
    ByteSwaps();
    MemoryReserveCommit();
    SaveReport();

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    D3DPRESENT_PARAMETERS pp;
    ZeroMemory(&pp, sizeof(pp));
    pp.BackBufferWidth = 1280;
    pp.BackBufferHeight = 720;
    pp.BackBufferFormat = D3DFMT_A8R8G8B8;
    pp.BackBufferCount = 1;
    pp.MultiSampleType = D3DMULTISAMPLE_NONE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    IDirect3DDevice9* dev = NULL;
    if (!d3d || FAILED(d3d->CreateDevice(0, D3DDEVTYPE_HAL, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &dev)))
    {
        Report("[hello] D3D device creation failed");
        SaveReport();
        return;
    }
    Report("[hello] D3D device ok; press START to finish");
    SaveReport();

    DWORD start = GetTickCount();
    for (int frame = 0;; frame++)
    {
        float t = (float)frame / 60.0f;
        BYTE r = (BYTE)(127 + 127 * (frame % 120 < 60 ? 1 : -1) * ((frame % 60) / 60.0f));
        dev->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(r, 40, 90), 1.0f, 0);
        dev->Present(NULL, NULL, NULL, NULL);
        (void)t;

        XINPUT_STATE pad;
        if (XInputGetState(0, &pad) == ERROR_SUCCESS && (pad.Gamepad.wButtons & XINPUT_GAMEPAD_START))
            break;
        if (GetTickCount() - start > 10000)
            break;
    }
    Report("[hello] done after %u ms", GetTickCount() - start);
    SaveReport();
}
