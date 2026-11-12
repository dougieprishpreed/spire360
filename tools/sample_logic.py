"""sample_logic.py: samples a PC logic_host while a test driver uses it, for the console's -O2 / -Os split.

    python tools/sample_logic.py --exe-map out/logic/native-prof/logic_host.map --out-dir work/prof
        -- F:/.../out/logic/native-prof/logic_host.exe [its arguments]

A wrapper the drivers start in place of logic_host (random_runs.py, presentation_scenarios.py,
content_coverage.py --cmd "python tools/sample_logic.py ... -- <logic_host>"): logic_host gets this
process's stdin, stdout and stderr, so the driver talks to it as before, and no window opens. While it
runs, each of its threads is looked at --hz times a second; a thread running the program's own code (not
waiting in the system) has its stack walked (dbghelp StackWalk64). When logic_host ends, the stacks go to
<out-dir>/stacks_<pid>.txt, one sample a line, leaf first, functions as the C names them (fN: wasm
function N). tools/hot_functions.py turns the stacks into the hot list.

Build the sampled logic_host without inlining, so each function the C has shows in the stacks under its own
name: tools/build_logic.py --stage native --native-tag prof --native-flags /Ob0 --native-map. Windows only."""
import argparse
import bisect
import ctypes
import ctypes.wintypes as wt
import os
import subprocess
import sys
import time

ap = argparse.ArgumentParser()
ap.add_argument("--exe-map", required=True, help="logic_host's linker map (/MAP)")
ap.add_argument("--out-dir", required=True)
ap.add_argument("--hz", type=float, default=400)
ap.add_argument("cmd", nargs=argparse.REMAINDER)
a = ap.parse_args()
cmd = a.cmd[1:] if a.cmd and a.cmd[0] == "--" else a.cmd
if not cmd:
    ap.error("the program to run comes after --")

# the exe's code symbols (section 0001), as RVAs from the preferred base 0x140000000
addrs, syms = [], []
for line in open(a.exe_map, encoding="utf-8", errors="replace"):
    p = line.split()
    if len(p) >= 3 and p[0].startswith("0001:"):
        try:
            rva = int(p[2], 16) - 0x140000000
        except ValueError:
            continue
        addrs.append(rva)
        syms.append(p[1])
order = sorted(range(len(addrs)), key=lambda i: addrs[i])
addrs = [addrs[i] for i in order]
syms = [syms[i] for i in order]

k32 = ctypes.WinDLL("kernel32", use_last_error=True)
dbg = ctypes.WinDLL("dbghelp", use_last_error=True)
psapi = ctypes.WinDLL("psapi", use_last_error=True)
k32.OpenThread.restype = wt.HANDLE
k32.OpenProcess.restype = wt.HANDLE
k32.CreateToolhelp32Snapshot.restype = wt.HANDLE
dbg.StackWalk64.argtypes = [wt.DWORD, wt.HANDLE, wt.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p,
                            ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]
dbg.SymInitialize.argtypes = [wt.HANDLE, ctypes.c_char_p, wt.BOOL]
k32.GetThreadContext.argtypes = [wt.HANDLE, ctypes.c_void_p]
k32.SuspendThread.argtypes = [wt.HANDLE]
k32.ResumeThread.argtypes = [wt.HANDLE]
k32.CloseHandle.argtypes = [wt.HANDLE]
psapi.EnumProcessModules.argtypes = [wt.HANDLE, ctypes.c_void_p, wt.DWORD, ctypes.c_void_p]
psapi.GetModuleInformation.argtypes = [wt.HANDLE, ctypes.c_void_p, ctypes.c_void_p, wt.DWORD]


class THREADENTRY32(ctypes.Structure):
    _fields_ = [("dwSize", wt.DWORD), ("cntUsage", wt.DWORD), ("th32ThreadID", wt.DWORD),
                ("th32OwnerProcessID", wt.DWORD), ("tpBasePri", wt.LONG), ("tpDeltaPri", wt.LONG), ("dwFlags", wt.DWORD)]


class MODULEINFO(ctypes.Structure):
    _fields_ = [("lpBaseOfDll", ctypes.c_void_p), ("SizeOfImage", wt.DWORD), ("EntryPoint", ctypes.c_void_p)]


def threads(pid):
    snap = k32.CreateToolhelp32Snapshot(4, 0)
    te = THREADENTRY32()
    te.dwSize = ctypes.sizeof(te)
    out = []
    ok = k32.Thread32First(snap, ctypes.byref(te))
    while ok:
        if te.th32OwnerProcessID == pid:
            out.append(te.th32ThreadID)
        ok = k32.Thread32Next(snap, ctypes.byref(te))
    k32.CloseHandle(snap)
    return out


# this process's stdin, stdout and stderr passed on by name: on Windows a child gets pipe handles only when
# they are given (Popen without them left logic_host with none, the driver waiting on it for ever)
proc = subprocess.Popen(cmd, stdin=sys.stdin, stdout=sys.stdout, stderr=sys.stderr, creationflags=0x08000000)
hp = k32.OpenProcess(0x1F0FFF, False, proc.pid)
dbg.SymSetOptions(0x2 | 0x10)
dbg.SymInitialize(hp, None, True)
base = size = 0
for _ in range(200):   # the exe's module, once the loader has it
    mods = (wt.HMODULE * 1)()
    need = wt.DWORD()
    if psapi.EnumProcessModules(hp, mods, ctypes.sizeof(mods), ctypes.byref(need)) and mods[0]:
        mi = MODULEINFO()
        psapi.GetModuleInformation(hp, mods[0], ctypes.byref(mi), ctypes.sizeof(mi))
        base, size = mi.lpBaseOfDll or 0, mi.SizeOfImage
        break
    if proc.poll() is not None:
        break
    time.sleep(0.01)

ctx_buf = ctypes.create_string_buffer(1232 + 16)
ctx_addr = (ctypes.addressof(ctx_buf) + 15) & ~15
frame = ctypes.create_string_buffer(512)
fta = ctypes.cast(dbg.SymFunctionTableAccess64, ctypes.c_void_p).value
gmb = ctypes.cast(dbg.SymGetModuleBase64, ctypes.c_void_p).value
u64 = ctypes.c_uint64
handles = {}
samples = []
period = 1.0 / a.hz
listed = 0.0
while base and proc.poll() is None:
    now = time.time()
    if now - listed > 0.5:
        for tid in threads(proc.pid):
            if tid not in handles:
                h = k32.OpenThread(0x1FFFFF, False, tid)
                if h:
                    handles[tid] = h
        listed = now
    for tid, ht in list(handles.items()):
        if k32.SuspendThread(ht) == 0xFFFFFFFF:
            k32.CloseHandle(ht)
            del handles[tid]
            continue
        pcs = []
        try:
            ctypes.memset(ctx_addr, 0, 1232)
            ctypes.c_uint32.from_address(ctx_addr + 0x30).value = 0x10000B
            if not k32.GetThreadContext(ht, ctx_addr):
                continue
            rip = u64.from_address(ctx_addr + 0xF8).value
            if not (base <= rip < base + size):
                continue   # waiting in the system, or in a DLL
            rsp = u64.from_address(ctx_addr + 0x98).value
            rbp = u64.from_address(ctx_addr + 0xA0).value
            ctypes.memset(frame, 0, 512)
            for off, val in ((0, rip), (32, rbp), (48, rsp)):
                u64.from_buffer(frame, off).value = val
                ctypes.c_uint32.from_buffer(frame, off + 12).value = 3
            while len(pcs) < 200 and dbg.StackWalk64(0x8664, hp, ht, ctypes.addressof(frame), ctx_addr, None, fta, gmb,
                                                     None):
                pc = u64.from_buffer(frame, 0).value
                if pc == 0:
                    break
                pcs.append(pc)
        finally:
            k32.ResumeThread(ht)
        if pcs:
            samples.append(pcs)
    time.sleep(period)
rc = proc.wait()


def name_at(pc):
    rva = pc - base
    i = bisect.bisect_right(addrs, rva) - 1
    if i < 0 or rva < 0 or rva >= size:
        return "?"
    return syms[i]


os.makedirs(a.out_dir, exist_ok=True)
names = {}
with open(os.path.join(a.out_dir, "stacks_%d.txt" % proc.pid), "w", encoding="utf-8") as f:
    for pcs in samples:
        st = []
        for pc in pcs:
            n = names.get(pc)
            if n is None:
                n = names[pc] = name_at(pc)
            st.append(n)
        f.write(";".join(st) + "\n")
sys.exit(rc)
