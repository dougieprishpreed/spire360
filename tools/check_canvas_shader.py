#!/usr/bin/env python3
"""Compile the PC canvas shader and the effect shaders (effects.hlsl's
ps_eN) with Windows D3DCompiler, without the XDK."""
import ctypes
from pathlib import Path
import re
import sys


def blob_bytes(blob):
    table = ctypes.cast(blob, ctypes.POINTER(ctypes.POINTER(ctypes.c_void_p))).contents
    pointer = ctypes.WINFUNCTYPE(ctypes.c_void_p, ctypes.c_void_p)(table[3])(blob)
    size = ctypes.WINFUNCTYPE(ctypes.c_size_t, ctypes.c_void_p)(table[4])(blob)
    return ctypes.string_at(pointer, size)


def release(blob):
    if blob:
        table = ctypes.cast(blob, ctypes.POINTER(ctypes.POINTER(ctypes.c_void_p))).contents
        ctypes.WINFUNCTYPE(ctypes.c_ulong, ctypes.c_void_p)(table[2])(blob)


def main():
    if sys.platform != "win32":
        print("Windows D3DCompiler is required", file=sys.stderr)
        return 2
    compiler = ctypes.WinDLL("d3dcompiler_47.dll").D3DCompile
    compiler.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_char_p, ctypes.c_void_p, ctypes.c_void_p,
                         ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint, ctypes.c_uint,
                         ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(ctypes.c_void_p)]
    compiler.restype = ctypes.c_long
    shaders = Path(__file__).resolve().parents[1] / "engine/gfx/shaders"
    effects = (shaders / "effects.hlsl").read_bytes()
    jobs = [(shaders / "canvas.hlsl", b"vs_main", b"vs_3_0"), (shaders / "canvas.hlsl", b"ps_main", b"ps_3_0")]
    jobs += [(shaders / "effects.hlsl", e, b"ps_3_0") for e in re.findall(rb"^float4 (ps_e\d+)\(", effects, re.M)]
    for path, entry, target in jobs:
        source = path.read_bytes()
        code, errors = ctypes.c_void_p(), ctypes.c_void_p()
        status = compiler(source, len(source), str(path).encode(), None, None, entry, target,
                          0x1000, 0, ctypes.byref(code), ctypes.byref(errors))
        try:
            if errors:
                print(blob_bytes(errors).decode("utf-8", errors="replace").rstrip("\0"), file=sys.stderr)
            if status < 0:
                return 1
            print(f"{path.name} {entry.decode()} {target.decode()}: {len(blob_bytes(code))} bytes, compiled")
        finally:
            release(code)
            release(errors)
    return 0


if __name__ == "__main__":
    sys.exit(main())
