# Local patches to w2c2

Upstream: https://github.com/turbolent/w2c2 at the commit in `UPSTREAM_COMMIT`
(MIT, see `LICENSE`). Only the `w2c2/` translator directory is vendored; the
upstream `examples/`, `tests/` and `wasi/` trees are not (Spire360 has its own
WASI layer in `runtime/`).

1. `w2c2/CMakeLists.txt`: the embed example and embedding tests are only added
   when upstream's `examples/` and `tests/` trees exist.
2. `w2c2/w2c2_base.h`, `W2C2_HOST_MEMORY`: when defined, non-shared linear
   memory is a single block reserved by the host (`w2c2HostMemoryReserve` /
   `w2c2HostMemoryGrow` / `w2c2HostMemoryRelease`) and `memory.grow` asks the
   host to commit and zero more of it instead of calling `realloc`.
   This avoids `realloc` of a block hundreds of megabytes large on the console.
3. `w2c2/w2c2_base.h`: on big-endian MSVC (the XDK compiler) byte swaps use
   `_byteswap_ushort/_ulong/_uint64` instead of mask-and-shift.
4. `w2c2/w2c2_base.h`, `W2C2_XBOX_BYTEREVERSE` (opt-in, off by default): memory
   loads and stores use the Xenon byte-reversed load/store intrinsics.
5. `w2c2/w2c2_base.h`: endianness detection knows MSVC's PowerPC macros
   (`_M_PPC`, `_M_PPCBE`) and `_XBOX`; the XDK compiler defines none of the
   GCC-style ones.

Keep this list in sync when re-vendoring.
