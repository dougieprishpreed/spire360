# Coding rules for console code

From the XDK dialect probe (`xdk/probe`), run on the player's XDK:
`Microsoft (R) 32-bit C/C++ Optimizing Compiler Version 16.00.11886.00 for
PowerPC` (the Visual Studio 2010 compiler).

## C (runtime, translated code, anything shared with the PC host)

Strict C89, as MSVC 2010 accepts it:

| Feature | XDK | Rule |
|---|---|---|
| declarations after statements | no | declare at the top of a block |
| `inline` | no | `__inline` (or `W2C2_INLINE` / a project macro) |
| `<stdbool.h>` | no | `int` flags |
| designated initializers | no | positional initializers |
| `//` comments | yes | allowed, `/* */` preferred |
| `long long`, `<stdint.h>` | yes | allowed |
| variadic macros | yes | allowed |
| `_byteswap_*`, PPC intrinsics | yes | through `w2c2_base.h` / `srt_plat.h` only |

Projects compile `.c` as C (`CompileAs=Default` in `xdk/common/spire360.props`;
the platform default `/TP` would compile it as C++).

Local check (approximates the XDK's C mode):

```
gcc -std=c89 -pedantic -Werror=declaration-after-statement -Wno-long-long -fsyntax-only <file.c>
```

## C++ (engine, presentation)

VS2010 C++: `auto`, lambdas, `nullptr`, rvalue references, `static_assert`,
`decltype`, `override`, `<type_traits>`, `<unordered_map>` and placement new
work.

Not available: range-for, `enum class`, `final`, `constexpr`, variadic
templates, template aliases (`using X = ...`), `std::initializer_list`,
`char16_t` / `u""` literals.

Built with `/EHs-c-` and `/GR-`: no exceptions, no RTTI.
