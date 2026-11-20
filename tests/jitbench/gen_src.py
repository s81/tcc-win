"""Generate a synthetic C translation unit with N functions (10 lines each).

usage: python gen_src.py N > out.c
Deterministic: same N gives the same bytes. Function fK calls f(K-1) so the
unit is one connected program; bench_entry() calls the last one.
"""
import sys

n = int(sys.argv[1])
out = ["#include <stdint.h>", "static uint32_t g_state = 1;", ""]
for k in range(n):
    prev = f"f{k - 1}(x + 1)" if k and k % 50 else "x"
    out += [
        f"int f{k}(int x)",
        "{",
        f"    uint32_t a = (uint32_t){prev}, b = {k * 2654435761 % 1000003}u;",
        f"    double d = (double)a * {1 + k % 7}.5;",
        f"    for (int i = 0; i < {k % 5 + 1}; i++) {{",
        f"        a = a * 1664525u + b + (uint32_t)i;",
        f"        if (a & {1 << (k % 13)}u) b ^= a >> {k % 17 + 1}; else d += (double)b;",
        "    }",
        "    g_state += a ^ b; return (int)(a + (uint32_t)d);",
        "}",
    ]
out += [f"int bench_entry(void) {{ return f{n - 1}(1) != 0x7fffffff; }}", ""]
sys.stdout.write("\n".join(out))
