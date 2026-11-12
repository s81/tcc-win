# JIT design (Phase 1)

Design only: no compiler source is changed by this phase. Baseline numbers are
in [JIT-BASELINE.md](JIT-BASELINE.md). New measurements made for this document
are in section 2 and section 3.5, with their commands. Anything not measured is
marked **estimate** or **unverified**.

## 0. Summary

1. **Option B can plausibly meet goal 1a, but not as "a peephole".** Measured
   with hand-written assembly stand-ins on five kernels: a peephole alone gives
   1.0 - 1.6x. Keeping locals in registers, using them directly as operands and
   putting the loop test at the bottom gives 1.6 - 3.4x (median 1.75x) and
   closes 67 - 100 % of the gap to the best scalar code I could write (median 84 %).
   These are proxies written by hand, not compiler output and not GCC.
2. **Register-homed locals need a look at the function body before code is
   generated**, which a strict single pass cannot do. Measured cost of saving
   and replaying every function body: +11 - 15 % compile time. That breaks goal
   1b if applied to everything, so the design applies it **per function, only
   to functions that contain a loop** (found by a cheap text scan), and leaves
   all other code on today's path, byte for byte.
3. **No IR, no copy-and-patch tier is proposed now.** The papers show
   copy-and-patch beating *baseline* compilers on bytecode; none of them shows
   it beating a register-allocating compiler on C source. Option A stays the
   fallback if the GATE fails.
4. **Per-context state by thread-local globals**, not by rewriting every
   function to take a context pointer: smallest diff, mergeable with upstream.
   Its compile-time cost is unmeasured and has its own stop rule.
5. **Decimal: clean-room BID library first (Stage L), compiler support second
   (Stage T).** `_Decimal128` fits the code generator as a memory value (like a
   16-byte struct); nothing needs 128-bit registers. One real obstacle: Intel
   holds an active US patent (US 7,707,233, adjusted expiry 2029-01-16) whose
   claims, as summarized publicly, cover converting between bases by
   multiplying with stored reciprocal powers, the core trick of the published
   fast BID algorithms. The design avoids that trick; parity with GCC is
   therefore at risk. Your decision.
6. Eight decisions are listed in section 9.

## 1. What the baseline says must change

| Finding (JIT-BASELINE.md) | Consequence for the design |
|---|---|
| Every local lives in the stack frame; a loop iteration of `bytesum` is 15 instructions, 6 loads, 2 stores, 3 taken jumps | Goal 1a is about memory traffic and jumps, not about clever optimizations |
| `VirtualFree(ptr, size, MEM_RELEASE)` never frees | Fix first; it blocks goal 4 |
| Compile is serialized by one lock; relocate is not | Goal 3a needs the front end and code generator to be re-entrant |
| Image size is `int`; image > 2 GiB fails silently | Goal 3b needs a diagnostic at least |
| `_ftelli64`, `ftello64`, `fseeko64`, `GetFileSizeEx` do not link | Goal 2 is mostly import definitions |
| 4 FP defect classes, same in AOT and JIT | Goal 5a needs fixes in the shared front end |
| No decimal, no 128-bit type | Goal 5b starts from nothing |

Decoded from `tcc -c -DK_bytesum kernels.c` (116 bytes), the loop today:

```
cond: mov rax,[rbp-16]      ; i
      mov rcx,[rbp+32]      ; n
      cmp rax,rcx
      jae end
      jmp body
inc:  mov rax,[rbp-16]
      mov rcx,rax
      add rax,1
      mov [rbp-16],rax
      jmp cond
body: mov rax,[rbp-16]
      mov rcx,[rbp+16]      ; src
      add rcx,rax
      movzx ecx,byte [rcx]
      mov rax,[rbp-8]       ; a
      add rax,rcx
      mov [rbp-8],rax
      jmp inc
```

The accumulator and the counter each make a round trip through memory per
iteration (store, then load), which costs more than the arithmetic itself.

## 2. Goal 1a: what each option can reach (measured)

No reference compiler is installed, so I wrote the candidate code shapes by
hand in assembly (`tests/jitbench/asmproxy.c`) and ran them through the same
harness as the baseline. Every proxy returns the same result and output digest
as the tcc-compiled kernel, so they compute the same thing.

```
$ jitbench kern tests/jitbench/kernels.c  <name>           64 7 <b|d> 1
$ jitbench kern tests/jitbench/asmproxy.c <name>_<variant> 64 7 <b|d> 1
```

Variants:

- **peep**: locals stay in the frame; loop test at the bottom, no jump-to-jump,
  no reload of a value still in a register, load-op-store folded. What a
  peephole can do without knowing anything about the function.
- **regmov**: tcc's exact instruction sequence and jump layout, with each stack
  slot replaced by a callee-saved register. What you get if only `load()` and
  `store()` learn about register homes.
- **B target** (`_b`): register homes, home registers used directly as source
  operands, loop test at the bottom. No indexed addressing, no in-place
  update, no hoisting. This is what section 3 proposes to build.
- **best scalar** (`_reg`): what a register-allocating compiler emits without
  loop transformations (my stand-in for `-O1`).
- **needs IR** (`_simd`, `_cmov`): SSE2 vectors or if-conversion (my stand-in
  for `-O2` and above). Not reachable without a real optimizer.

ns per byte, best of 7 runs over 64 MiB, three passes, spread under 2 %:

| Kernel | tcc today | peep | regmov | **B target** | best scalar | needs IR |
|---|---:|---:|---:|---:|---:|---:|
| bytesum | 2.04 | 1.26 | 1.02 | **0.61** | 0.33 | 0.054 (SSE2) |
| xor | 1.33 | 1.23 | 1.13 | **0.66** | 0.33 | 0.12 (SSE2) |
| fnv1a | 1.67 | 1.67 | 1.68 | **0.95** | 0.95 | n/a (serial chain) |
| upper | 7.57 | not written | 4.80 | **4.74** | 4.34 | 1.43 (cmov) |
| dsum | 0.282 | 0.278 | 0.195 | **0.168** | 0.130 | n/a (order must be kept) |

Speedup over today and share of the run-time gap closed:

| Kernel | peep | regmov | B target | B target: gap to best scalar closed | B target: gap to "needs IR" closed |
|---|---:|---:|---:|---:|---:|
| bytesum | 1.6x | 2.0x | 3.4x | 84 % | 72 % |
| xor | 1.1x | 1.2x | 2.0x | 67 % | 55 % |
| fnv1a | 1.0x | 1.0x | 1.75x | 100 % | 100 % |
| upper | n/a | 1.6x | 1.6x | 88 % | 46 % |
| dsum | 1.0x | 1.45x | 1.7x | 75 % | 75 % |
| **median** | 1.05x | 1.45x | **1.75x** | **84 %** | **72 %** |

What this shows:

- A peephole alone does not meet goal 1a: 0 - 46 % of the gap to best scalar,
  and nothing at all on three of the four kernels it was written for.
- Register homes alone (regmov) are not enough either: `fnv1a` does not move,
  because it is limited by instruction count, not by memory.
- Homes + direct operands + loop rotation together are what pays.
- `dsum` and `daxpy` cannot be vectorized or reordered under goal 5a, so for
  floating-point reductions "best scalar" is the ceiling for any tier.
- An optimizer with an IR would add another 3 - 6x on byte kernels
  (vectorization) and 3x on `upper` (if-conversion) beyond best scalar. That
  is the part B gives up.

Limits of this evidence: five kernels of twelve, hand-written code, one CPU
(i7-8700). "Best scalar" and "needs IR" are my stand-ins for `-O1` and `-O2`;
real GCC or MSVC output was not measured. The B target column assumes the
implementation reaches exactly that code shape.

## 3. Option B plan

Items are ordered as they would be committed in Phase 2. Sizes are estimates.

### 3.1 Code ownership and unload (goal 4)

Additive API in `libtcc.h`:

```c
typedef struct TCCCode TCCCode;
/* after tcc_relocate(): take ownership of the run image away from the state.
   The state can then be deleted; the code stays valid until tcc_code_free(). */
LIBTCCAPI TCCCode *tcc_detach(TCCState *s);
LIBTCCAPI void *tcc_code_symbol(TCCCode *code, const char *name);
LIBTCCAPI void tcc_code_free(TCCCode *code);
```

- `TCCCode` holds: image base and size, the unwind table registration, the
  loaded-DLL handles, and a compact exported-symbol table (name to address,
  global symbols only). Nothing else of the compiler survives.
- Fix `tcc_run_free`: `VirtualFree(ptr, 0, MEM_RELEASE)`, check the result.
- Pages: reserve once, commit code as RW, switch code to `PAGE_EXECUTE_READ`
  after relocation (never RWX); `.rdata` to `PAGE_READONLY`. On arm64 keep
  `FlushInstructionCache` after the protection change (**unverified** there).
- Default (goal 1d): `tcc_relocate` releases compile-only memory immediately
  (it already drops sections; add the preprocessor tables and token allocators).
  Opt-out: `-fkeep-state`.
- Tests first: `jitbench cycles` in all three modes must exit 0 (it exits 1
  today), plus a detach/free cycle test and "code runs after `tcc_delete`".

Size: small (about 150 lines). Risk: low.

### 3.2 Per-context state, no global compile lock (goal 3a)

Three ways to remove `tcc_compile_sem`:

| Way | Diff | Upstream merge | Compile-time cost | Risk |
|---|---|---|---|---|
| (a) Pass a context pointer everywhere | about 90 file-scope variables, thousands of uses | very hard | none | high (mechanical but huge) |
| (b) Make the globals thread-local | one macro on `ST_DATA` and the file-scope statics; `tcc_state` itself thread-local | easy (macro is empty upstream) | **unmeasured**: every access to `tok`, `vtop`, `ind` goes through the TLS block | medium |
| (c) One process per compile | none | none | process start 14 ms per compile | fails "many contexts in one process" |

Proposal: **(b)**. One compile runs on one thread from `tcc_enter_state` to
`tcc_exit_state`, which is already how the code is structured, so thread-local
globals are sufficient. The lock is removed; `tcc_enter_state` only sets the
thread's `tcc_state`.

Open points to settle with tests before anything else in this item:

1. `thread_local` inside `libtcc.dll` built by tcc-win itself. The PE linker
   writes a TLS directory (`tccpe.c:776`, `1361`) and `144_tls` passes for an
   EXE; a DLL loaded with `LoadLibrary` was not tested. **unverified.**
2. Compile-time cost with tcc-win's own TLS code generation. Stop rule: if
   `jitbench scale` on the 10 KLOC unit slows by more than the 3 % noise, stop
   and report; the alternative is (a) restricted to the hottest dozen variables.
3. The relocate phase is already unlocked today; audit its use of
   `tcc_state`-based macros (`symtab_section` in `bt_link`) while doing this.
4. Backtrace state (`g_s1`, `g_rc`, `rt_sem`) stays process-global and locked:
   it is a registry of running images, not compile state.

Opt-out (goal 1d): `CONFIG_TCC_SEMLOCK=1` build keeps the lock and plain globals.
Target from Phase 0: at least 4x at 6 threads on the 10 KLOC unit.

Size: medium. Risk: medium, concentrated in points 1 and 2.

### 3.3 Arena memory (goals 1c, 4)

Today every allocation is `realloc` on the process heap, which is one lock
shared by all threads, and freeing a state walks every object.

Proposal: one private Win32 heap per `TCCState` (`HeapCreate` with
`HEAP_NO_SERIALIZE`), used by `tcc_malloc`/`tcc_realloc`/`tcc_free` while that
state is current; `tcc_delete` calls `HeapDestroy`. Everything a compile
allocated is released in one call and cannot leak by construction; threads
stop contending on the process heap. A custom `tcc_set_realloc` allocator
still takes precedence (that is the opt-out). `TCCCode` (3.1) is allocated
outside the arena because it outlives the state.

"Bounded optimizer memory" (goal 4) follows from section 3.5: the only new
compile-time data is one function body's tokens and a per-function table of
locals, both freed at the end of that function.

Size: small (about 40 lines). Risk: low. Gain: unmeasured.

### 3.4 64-bit streams and large images (goals 2, 3b)

- Add to `win32/lib/msvcrt.def`: `_ftelli64`. Provide `ftello64` and `fseeko64`
  as inline wrappers over `_ftelli64`/`_fseeki64` in `stdio.h` (I do not expect
  msvcrt to export them: check first). Add `GetFileSizeEx` to `kernel32.def`.
- Generated code needs no change: Phase 0 found no truncation.
- Images above 2 GiB: widen `run_size` and the size variables in `tccrun.c`
  to `size_t`, and report a clear error when the image would exceed the reach
  of 32-bit PC-relative references (decision 3: reject or support).
- The front end's "invalid array size" (2 GiB up to 4 GiB) and "constant
  exceeds 32 bit" (4 GiB and more) stay as they are unless decision 3 says otherwise.
- Tests: the three `stream.c` loops over a file larger than 4 GiB (scratch
  file, deleted after); link probes for the four symbols; `reloc bigbss`
  expecting the new diagnostic.

Size: small. Risk: low.

### 3.5 Code quality inside the single pass (goal 1a)

Three changes, which only pay together (section 2). All are in the shared
front end and the x86-64 and arm64 back ends, so AOT and JIT stay identical.

**(1) Register homes for locals.** A scalar local (integer, pointer, `float`,
`double`) whose address is never taken lives in a callee-saved register for
the whole function instead of a stack slot. Win64 x86-64 has `rbx, rsi, rdi,
r12-r15` and `xmm6-xmm15`, which the generator does not allocate today, so the
existing temporary allocator (`rax, rcx, rdx, r8-r11`) is untouched. arm64 has
`x19-x28`, `d8-d15` (**unverified**: not run here). Parameters are copied into
their home in the prologue. Homes are saved and restored in prologue and
epilogue and described in the unwind data. Locals beyond the available
registers stay in memory (those used inside loops first).

A value-stack entry for a homed local is a register rvalue that the generator
may read but must not overwrite: the first operand of a binary operation is
copied to a temporary, the second is used in place.

**(2) Loop test at the bottom.** For `for (e1; e2; e3) body` and `while`: emit
`e1; if (!e2) goto end; top: body; e3; if (e2) goto top; end:`. This needs
`e2` and `e3` compiled after the body, which the front end can do by saving
and replaying their tokens (`skip_or_save_block` and `begin_macro` already do
this for inline functions, VLA sizes and initializers).

**(3) Small peephole at emission.** No reload of a value just stored, no
jump to a jump, no `mov r,r` to itself. Kept deliberately tiny.

**(0) The prerequisite: a pre-scan.** (1) needs to know, before the first
instruction of a function is emitted, which locals have their address taken
and whether the function uses anything that forbids homes (`setjmp`, inline
assembly, `goto` under `-fgoto`). The front end has no such knowledge in one
pass. The function body is therefore read twice: tokens are saved, scanned (on
preprocessed tokens, so macros cannot hide an `&`), then replayed into the
normal parser.

Measured cost of save-and-replay, using the existing inline-function path on
10,000 functions (the synthetic unit, once as plain functions and once with
every function `static inline`, all referenced through a table; `jitbench scale`):

```
plain  compile=148.5 ms  peak_private=19,920 KB
inline compile=164.7 ms  peak_private=26,768 KB
plain  compile=150.3 ms
inline compile=171.0 ms
plain  compile=148.8 ms
inline compile=166.8 ms
```

+11 to +15 % compile time. (The memory figure overstates the pre-scan: the
inline path keeps all bodies to the end; the pre-scan frees each body after
its function.)

**Tiering, to keep goal 1b.** Paying 12 % on every function violates 1b. So:

- **Tier 0** = today's path, unchanged, no token saving. Used for every
  function without a loop, and for everything under `-O0`.
- **Tier 1** = pre-scan + (1) (2) (3). Used for functions that contain a loop.
- The choice is made before the body is parsed, by scanning the raw source
  text from `{` to the matching `}` for the words `for`, `while`, `do`. This
  scan may be wrong in either direction (a macro can hide or fake a loop);
  that only affects speed, never correctness, because the address-taken
  analysis runs on real tokens in the pre-scan. If the body is not wholly in
  the current input buffer, the function gets Tier 0.

Why not run-time tiering (count calls, recompile hot functions)? It needs the
compiler state and source kept alive next to the running code, which is the
opposite of goal 4, and it adds a counter to cold code.

Rule R2 with these numbers: Tier 1 costs about 1.8 microseconds more per
10-line function (12 % of 14.5). On `bytesum` it saves 1.43 ns per byte at the
B target, so it pays for itself after about 1,300 loop iterations; on `dsum`
after about 2,000 elements. For the kernel benchmarks the added compile time
is 2 - 3 microseconds on a 2.4 ms unit, far inside the 3 % noise, so R1 holds
at every input size (**estimate** from the two measurements, not yet run).

**What is deliberately not in B:** common-subexpression elimination,
loop-invariant motion, strength reduction, if-conversion, inlining,
vectorization, indexed addressing modes. Each needs an IR or a second pass
over generated code.

**Dead code and constant folding.** The front end already folds integer and
floating constants and already suppresses code after `return`/`break`
(`nocode_wanted`). Nothing new is proposed except the FP folding fix in 3.6.

**Floating point under Tier 1 (goal 5a).** Homes change where a value lives,
not how it is computed: the same SSE2 instruction per C operation, in source
order. No contraction, no reassociation, no wider intermediates. `fpka` and
the stream-reduction equality test run with Tier 1 on and off.

Size: large (the biggest item; **estimate** 600 - 900 lines across `tccgen.c`,
`x86_64-gen.c`, `arm64-gen.c`). Risk: high: this is where a miscompilation
can come from. Safety nets in section 6.

### 3.6 Floating-point fixes (goal 5a)

| Defect | Fix |
|---|---|
| uint64 to double, values >= 2^63 | halve with the low bit OR-ed in (sticky), convert, double; same in the folder |
| uint64 to float | convert directly with one rounding (same sticky trick), not through double |
| double literal near half the smallest subnormal | correct decimal-to-binary conversion in the lexer (exact big-integer path for hard cases) |
| float literal rounded twice | parse decimal directly to binary32 |
| Folding uses the host's `long double` (`gen_opif`, `tccgen.c:2612`) | fold in the operand type: `float` ops in `float`, `double` ops in `double`, through volatile temporaries so the host compiler cannot widen |
| `FLT_EVAL_METHOD`, `__STDC_IEC_559__` undefined | define `FLT_EVAL_METHOD 0`; define `__STDC_IEC_559__` only after rounding modes and flags are tested |

The folding item matters for goal 5a across builds: `build-tcc.bat` uses
`gcc` by default, and with MinGW GCC `long double` is the 80-bit x87 type, so
a libtcc built by GCC would fold `double` expressions with double rounding.
This could not be shown here (no GCC): **unverified**, but the fix removes the
dependency either way.

These change AOT output for the affected inputs (decision 4).

### 3.7 Defaults and opt-outs (goal 1d)

| Feature | Default in libtcc / `-run` | Opt-out |
|---|---|---|
| Tier 1 for functions with loops | on | `-O0` (everything Tier 0); `-fno-reg-locals`, `-fno-loop-rotate` |
| Arena (private heap per state) | on | `tcc_set_realloc()`, or `-fno-arena` |
| Compile memory released at relocate | on | `-fkeep-state` |
| Code detach / unload | available; `tcc_delete` frees code unless detached | n/a |
| Per-context state (no lock) | on | build with `CONFIG_TCC_SEMLOCK=1` |
| 64-bit stream imports | on | n/a |

For `tcc.exe` producing files (AOT), Tier 1 stays **off** until the separate
flip step of the DEFAULTS POLICY; `-O1` turns it on explicitly.

## 4. The alternatives to B (goal 1a/1b comparison)

| | B (this plan) | A-i: IR + optimizing pass for hot code | A-ii: copy-and-patch baseline tier |
|---|---|---|---|
| Run-time gain | 1.6 - 3.4x on 5 proxied kernels (measured on proxies) | up to the "needs IR" column: 5 - 38x on byte kernels, 1.75 - 2.2x on the serial and FP kernels (measured on proxies) | no direct evidence for C; **estimate**: between regmov and B target |
| Compile-time cost | +12 % on functions with loops (measured upper bound) | **estimate** 5 - 20x on optimized functions (IR build, SSA, register allocation); tiering mandatory | papers report less than a classic baseline; tcc is already a baseline compiler at 690k lines/s |
| Memory | one function's tokens | IR for one function at a time; bounded, **estimate** tens of KB per function | stencil library in the binary, **estimate** 100s of KB per target |
| Miscompilation risk | medium-high in 3.5, low elsewhere | high: a new optimizer is the classic source of wrong code; FP rules of 5a must be enforced pass by pass | medium: stencils are produced by another compiler, correctness depends on the patching |
| Effort | 1 large item + small ones | very large: an IR, a lowering from tcc's one-pass front end (which has no AST), passes, a second back end path per target | large, plus a build-time dependency on clang to produce stencils (needs approval; not installed) |
| Mergeable with upstream | yes | hard | hard |

What the papers do **not** prove for C source input:

- **Xu & Kjolstad, OOPSLA 2021.** Results are for a C-like metaprogramming
  language built as an AST, a SQL query compiler, and WebAssembly. Generated
  code is 14 % faster than LLVM `-O0` and 39 - 63 % faster than Liftoff (a
  baseline compiler); compile speed is orders of magnitude better than LLVM.
  It is not compared with a register-allocating C compiler at `-O1`/`-O2`, and
  its input is an AST or bytecode, which tcc does not have.
- **Xu & Kjolstad, Deegen (2024).** A generator of interpreters and baseline
  JITs for dynamic languages (Lua, SOM). The baseline JIT is on average 33 %
  slower than LuaJIT's optimizing JIT. The gains come from inline caches and
  type speculation, which C does not need. The authors call the baseline JIT a
  complement to an optimizing JIT, not a substitute.
- **Kocourek, Křikava, Vitek, VMIL 2025** (copy-and-patch JIT for R). 1.15 -
  1.91x over the GNU R bytecode interpreter, at 980 bytecode instructions per
  millisecond. tcc compiles about 690 lines of C per millisecond already and
  starts from machine code, not from an interpreter. (Abstract only was read.)

Common thread: copy-and-patch replaces an interpreter or a slow baseline. tcc
*is* a fast baseline; its gap is register allocation and loop shape, which is
what B addresses directly. I therefore do not propose A-ii. A-i is the
fallback if the GATE fails.

## 5. The GATE

Measured with `jitbench kern` on the 12 kernels of `kernels.c`, 64 MiB, on
this machine, against the Phase 0 numbers.

B passes if all of these hold:

1. **Speed (goal 1a).**
   - If a reference compiler is installed by then (decision 1): your rule, at
     least 50 % of the run-time gap to GCC `-O2` closed on the median kernel.
   - If not: median speedup of at least **1.5x** over Phase 0 on the 12
     kernels, **and** at least 50 % of the gap to the `_reg` proxies closed on
     the median of the 5 proxied kernels. (Proxies predict 1.75x and 84 %.)
2. **R1.** No kernel slower than Phase 0 beyond noise (3 % compile, 6 % run) at
   1 MiB, 64 MiB and 1024 MiB; peak memory within 5 %.
3. **Goal 1b.** Loop-free code: `jitbench scale` on a loop-free 10 KLOC unit
   within 3 % of Phase 0. Functions with loops: at most +15 % compile time.
4. **Correctness.** Bootstrap stage 2 == stage 3 with Tier 1 on and with it
   off; all suites pass both ways; `fpka` 0 mismatches both ways; fuzzing
   (section 6) clean.

If 1 fails with 2 - 4 holding, option A-i is justified and I come back with a
proposal and data. If 3 or 4 fails, B is not done; no gate decision is made.

## 6. Safety nets

- **`-O0`**: selects Tier 0 for every function; that path is today's code.
- **Self-differential testing.** Tier 0 is the trusted reference. Every
  program in `tests/tests2` (131), the C23/ownership suite and `tcctest.c`
  runs under both tiers and the outputs must be identical.
- **Bootstrap under Tier 1.** tcc compiling itself (52 KLOC) with Tier 1 on,
  stage 2 == stage 3, is the strongest single test available here.
- **Random programs.** Csmith is the usual tool, but it must be built with a
  C++ compiler (none installed), its programs use `goto` unless told not to,
  and without GCC or clang there is no third opinion. Proposal: a small
  generator in Python (already available) that emits loop-shaped programs
  over unsigned integers and doubles with a random mix of address-taken and
  plain locals, compared Tier 0 vs Tier 1, kept in `tests/`. If you install a
  toolchain, Csmith with a reference compiler is added on top.
- **Determinism.** No hashing by address, no ordering by pointer value in the
  new code; the byte-identical bootstrap checks it.

## 7. Decimal (goal 5b)

### 7.1 Patent status (flag, not legal advice)

I am not a lawyer; this is what a search found, for you to judge.

- **US 7,707,233 B2**, "Converting a number from a first base to a second
  base", inventor M. Cornea-Hasegan, assignee Intel, filed 2005-05-19, status
  shown as active, adjusted expiration **2029-01-16** (Google Patents). As
  summarized there, the independent claims cover converting a number between
  bases with operations that multiply the number by previously calculated
  approximations of negative powers of the second base, then derive digits
  from the results (one claim includes a table lookup of digits). I read the
  summary, not the full claims.
- The ARITH-18 paper (Cornea et al.) rounds a binary coefficient to p decimal
  digits by multiplying with stored approximations of 10^-x instead of
  dividing. That is the same family of idea.
- Two Intel publications from the same period are listed next to it
  (US 2007/0266072 and 2007/0266073). I did not read them: **unverified**.
- I did not do a full search; other patents may exist.

Design consequence: Stage L does **not** use stored reciprocal powers of ten.
It divides exactly (hardware division and schoolbook long division, which are
textbook methods). That is slower. The general compiler technique of replacing
division by a constant with a multiply (Granlund and Montgomery, 1994) is
older than the patent, but whether using it here is safe is exactly the
question I cannot answer. Decision 5.

### 7.2 Stage L: library, no compiler change

Files (clean-room, written from IEEE 754-2019 and the published papers; no
Intel code read, no GCC code read):

- `lib/decbid.c`, `include/tcc_decimal.h`: built into `libtcc1.a`, so AOT
  programs and JIT code link the same object.

API (C, usable from JIT code today):

```c
typedef struct { uint32_t w; }      tcc_dec32;
typedef struct { uint64_t w; }      tcc_dec64;
typedef struct { uint64_t w[2]; }   tcc_dec128;   /* little-endian halves */

typedef struct { int rounding; unsigned flags; } tcc_dec_ctx;
tcc_dec_ctx *tcc_dec_thread_ctx(void);            /* per-thread default */

tcc_dec64 tcc_d64_add (tcc_dec64, tcc_dec64, tcc_dec_ctx *);
/* sub mul div sqrt fma quantize compare; d32, d128 likewise;
   conversions to and from every integer type, float, double, and
   between the three decimal formats; strings: */
tcc_dec64 tcc_d64_from_string(const char *, char **end, tcc_dec_ctx *);
int       tcc_d64_to_string (char *buf, size_t n, tcc_dec64, const char *fmt);
```

- **Representation.** BID as IEEE 754-2019 specifies (matches GCC on x86).
  Unpacked form: sign, exponent as `int`, coefficient as an integer: 64 bits
  for decimal64 (below 10^16), two 64-bit words for decimal128 (below 10^34),
  32 bits for decimal32. Non-canonical encodings are read as zero, as the
  standard requires.
- **Add/subtract.** Align by multiplying the operand with the larger exponent
  by a power of ten (table of 10^k as exact integers, not reciprocals), add as
  integers, round.
- **Multiply.** Exact integer product: 64x64 to 128 bits for decimal64,
  113x113 to 226 bits for decimal128 (four-limb schoolbook), then round.
- **Rounding step.** Count decimal digits (bit length, then one compare
  against the power-of-ten table), divide by 10^x with exact remainder,
  decide from the remainder and the mode. Exact by construction.
- **Divide.** Scale the dividend by a power of ten so the integer quotient has
  p + 1 digits or more, integer long division, sticky from the remainder.
- **Square root.** Integer square root of the scaled coefficient (Newton
  iteration on integers, final correction step), sticky from the remainder.
- **Conversions.** Decimal to/from binary floating point through exact
  multi-word integers (**estimate**: up to about 1,500 bits for the extreme
  exponents); slow but exact. Decimal to/from strings is direct because the
  coefficient is a decimal integer.
- **Correct rounding** in all five IEEE modes for + - * / sqrt, fma and
  conversions; all five exception flags.
- **Per-thread mode and flags, no process-global state.** Every function takes
  a context pointer. The per-thread default context is found through a
  Win32 TLS index (`TlsAlloc`/`TlsGetValue`), not through a `thread_local`
  variable, because `-run` rejects TLS sections (`tccrun.c:389`) and JIT code
  must be able to use the library.
- **128-bit multiply/divide primitives.** tcc-win has no `__int128`. Portable
  C with 32-bit limbs first; `mul`/`div` (x86-64) and `umulh` (arm64) in
  assembly as the first speed-tuning step.
- **printf / scanf / strto\*.** `strtod32`, `strtod64`, `strtod128` are thin
  names for the `from_string` functions. `lib/c23printf.c` already wraps the
  printf family in C23 mode; it gains the C23 length modifiers `H`, `D`, `DD`.
  msvcrt's scanf cannot be extended and tcc-win has no scanf wrapper today:
  Stage L supplies `strto*`; a full `scanf` wrapper is decision 6.

### 7.3 Stage T: compiler support (after Stage L passes)

- Keywords `_Decimal32`, `_Decimal64`, `_Decimal128`; literal suffixes `DF`,
  `DD`, `DL`; `<float.h>` macros (`DEC64_MAX` and so on); opt-in with
  `-fdecimal` (off by default, so programs that do not use it see no change).
- **Constant folding** calls the same `decbid.c` functions compiled into the
  compiler. No host floating point is involved.
- **Operators** compile to calls of the library functions with the thread
  context. Conversions likewise. Usual arithmetic conversions per C23 (mixing
  decimal with binary floating types is a constraint violation).
- **Does 128 bits fit the value model?** Yes, without 128-bit registers:
  - `CValue` is already 16 bytes (the `str` member), so a decimal128 constant
    fits with one added union member; `sizeof(CValue)` does not change.
  - A `_Decimal128` value is handled like a 16-byte struct: always in memory,
    referenced by address on the value stack. The generator's "one or two
    registers" limit that blocks `_BitInt(N > 64)` does not apply because no
    arithmetic is done inline.
  - `_Decimal32`/`_Decimal64` travel as 32/64-bit integers with a type flag.
  - What must change: `CType.t` needs room to mark the three types. The basic
    type field is 4 bits and nearly full (`VT_QLONG` 13, `VT_QFLOAT` 14), so a
    flag bit in the style of `VT_BITINT` is needed. Whether a free bit exists
    is **unverified**; if not, the type word must be widened, which touches
    every target.
- **ABI.** `_Decimal128`: passed by reference to a caller-made copy and
  returned through a hidden pointer, the Windows x64 rule for 16-byte
  aggregates (the choice you named). `_Decimal32`/`_Decimal64`: open. I
  believe GCC on x86-64 treats them as floating types and uses XMM registers;
  whether MinGW-w64 GCC does so under the Windows ABI, and how it returns
  `_Decimal128`, must be probed with GCC as a black box. **unverified**, no
  GCC here. Interoperability with GCC-built objects is only possible if we
  match what GCC does (decision 7). arm64: same by-reference rule proposed,
  **unverified**.

### 7.4 Tests and oracles

| Oracle / tool | License | Use | Kept out of shipped code |
|---|---|---|---|
| IBM General Decimal Arithmetic test cases (`dectest.zip`, 81,300+ cases) | ICU License, ICU 1.8.1 and later (read on speleotrove.com): use, copy, modify, distribute freely; keep the copyright and permission notice; no use of the holder's name in advertising | main correctness oracle for all operations, modes and flags | fetched by a test script, not vendored |
| IBM decNumber | same ICU License | second oracle for random operands, built as a separate test tool; results compared as decimal strings | not linked into anything shipped |
| GCC-built programs as black boxes | GPL-3.0 with the runtime exception (from memory: **verify**); only outputs are used | BID bit patterns and ABI probing | n/a |
| Python `decimal` module (already installed) | CPython is PSF-licensed; its C core is libmpdec, BSD-2-Clause (from memory: **verify**) | independent arbitrary-precision oracle that needs no install: exact result, rounded to p digits by the test script | test-only |
| MPFR | LGPL-3.0-or-later (from memory: **verify**) | exact sqrt and conversion checks, if installed | test-only |
| Berkeley TestFloat (binary32/64) | BSD-3-Clause (from memory: **verify**) | goal 5a | test-only; must be built, tcc may be able to |
| Paranoia | no explicit license that I know of: **verify, may be unusable** | goal 5a | test-only |
| Csmith | BSD-2-Clause (from memory: **verify**) | section 6, only with a toolchain | test-only |

Two things you should know before approving the oracles:

- **GCC's decimal runtime on x86 is, as far as I know, Intel's library**
  (`libbid` inside `libgcc`). Using GCC-built programs as a black-box oracle
  or speed reference then means comparing against the Intel library
  indirectly. You allowed GCC as a black box and excluded Intel as an oracle;
  these overlap. **unverified** here (no GCC), please confirm how you want it.
- The Python `decimal` oracle was not on your list. It needs nothing
  installed and is independent of Intel's code. Say if it is acceptable.

Plan: tests first. A script converts the IBM `.decTest` files into calls of
the Stage L API (operands, rounding mode, expected result and flags). Random
testing against Python `decimal` for + - * / sqrt, fma and conversions, with
operands concentrated on rounding boundaries (halfway cases, subnormals,
overflow threshold, cohort selection).

### 7.5 Speed risk versus GCC

Honest expectation: **Stage L as designed will be slower than GCC**, by a
factor I cannot measure today (no GCC); my **estimate** is 2 - 5x for multiply
and divide. Reasons: GCC's runtime is tuned and uses reciprocal tables; the
design here divides exactly; tcc-win's own code generator compiles the library
(until section 3.5 lands, and even then without inlining). Tuning steps, in
order: assembly `mul`/`div` primitives; fast paths for results that need no
rounding (most additions in practice); decision 5 on reciprocal
multiplication. If parity is not reached, I stop and report the measured gap.

Rule (1), no slowdown for programs that do not use decimal: the library is
only linked when referenced, and the compiler changes of Stage T sit behind
`-fdecimal`. Acceptance test: compile time, code size and kernel run time
unchanged within noise with decimal support built in.

## 8. Phase 2 order

1. Leak fix + detach API + cycle tests (3.1)
2. 64-bit stream imports + tests; image-size diagnostic (3.4)
3. FP fixes + `fpka` at 0 mismatches (3.6)
4. Thread-local state: the two open points first, then lock removal (3.2)
5. Arena (3.3)
6. Tier 1: pre-scan, register homes, loop rotation, peephole, x86-64 first,
   then arm64 (3.5), with the safety nets of section 6
7. Defaults and opt-outs (3.7)
8. Stage L decimal library: tests, correctness, then tuning (7.2)

One commit per item, tests first. Items 1 - 3 and 8 do not depend on 4 - 7.

## 9. Decisions needed

1. **Reference compiler.** Still none. The GATE has a fallback (section 5) that
   uses only Phase 0 numbers and the proxies. Approve the fallback, or install
   a compiler (w64devkit, portable, also gives `make`).
2. **Goal 1b reading.** Approve that functions containing a loop may compile up
   to 15 % slower (they are the optimized tier), while loop-free code stays
   within noise. If not, register homes are off the table and B fails goal 1a.
3. **Images above 2 GiB.** Reject with a clear error (proposed), or support.
4. **FP fixes change AOT output** for the affected conversions and literals.
   Approve doing them in Phase 2.
5. **Patent (7.1).** (a) Avoid reciprocal-table methods and accept lower speed
   (proposed), (b) get legal advice first, or (c) allow them.
6. **scanf.** `strto*` only (proposed), or a full `scanf`-family wrapper.
7. **Decimal ABI for `_Decimal32/64`.** Match MinGW-w64 GCC (needs GCC to
   probe), or choose integer-register passing now and document it.
8. **Oracles.** Approve: IBM test cases and decNumber (ICU License), Python
   `decimal`, GCC black box when available. TestFloat, MPFR, Paranoia and
   Csmith need their licenses confirmed and a toolchain to build.

## 10. Not verified in this phase

- Anything about GCC, MSVC or clang output, including the MinGW decimal ABI,
  whether MinGW-w64 enables BID decimal at all, and what its runtime is.
- `thread_local` in a tcc-built DLL, and the compile-time cost of TLS globals.
- Everything arm64: register homes, unwind data for saved registers, cache flush.
- That the implementation reaches the "B target" code shape; the 12 % pre-scan
  cost on real code (measured through the inline-function path only).
- A free type-flag bit for the decimal types.
- The licenses marked "from memory", the full text of the patent claims, and
  the two related patent publications.
- The VMIL 2025 paper beyond its abstract.
- Effort and line counts: estimates.
