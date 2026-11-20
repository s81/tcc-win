# JIT baseline (Phase 0 audit)

Read-only audit of the in-memory execution path (`tcc -run`, `tcc_relocate()`,
`tcc_run()`) of tcc-win at commit `7e2de65` (v1.1.0), branch `jit-work`.
Measured 2026-11-13. No compiler source was changed. Everything below was
produced on the machine described in section 1 unless marked **unverified**.

Harness and inputs: `tests/jitbench/` (`jitbench.c`, `kernels.c`, `stream.c`,
`gen_src.py`, `gen_fpka.py`).

## 0. Summary

| Area | Baseline | Status |
|---|---|---|
| Suites | C23/ownership 104/104, tests2 131/131 (12 skipped by Makefile rules), pp 24/24, bootstrap stage 2 == stage 3 | pass |
| Compile latency | 1.45 ms/KLOC, linear to 1M lines; ~1.4 ms fixed cost per unit | measured |
| Leak | **+4.14 KB per successful compile/unload cycle** (+414 MB over 100,000); 0 after compile errors | defect |
| Code lifetime | image is supposed to die in `tcc_delete`; in fact it is never unmapped (same defect) and stays RWX | defect |
| Threads | compile phase fully serialized (1.13x at 12 threads); relocate phase scales 4.4x | measured |
| Stream > 4 GiB | correct, flat memory (10.5 MB peak), 0.45 GiB/s; `_ftelli64`, `ftello64`, `fseeko64`, `GetFileSizeEx` do not link | gaps |
| Large process | 100k functions OK; image > 2 GiB fails **with no message**; one object >= 2 GiB rejected | gaps |
| Relocation range | external code and data reachable at 126 TiB distance (through the import table) | ok |
| binary32/64 | + - * / sqrt, a*b+c, most conversions bit-exact; AOT == `-run` == libtcc; **4 defect classes** (69 of 22,065 checks) | defects |
| Reference compilers | none installed (owner chose not to install) | every comparison **unverified** |
| Decimal reference speed | no GCC | **unverified** |
| arm64 | read from source only | **unverified** |

## 1. Machine and toolchain

```
> Get-CimInstance Win32_OperatingSystem | select Caption,Version,BuildNumber,OSArchitecture
Caption        : Microsoft Windows 11 Pro
Version        : 10.0.26300
BuildNumber    : 26300
OSArchitecture : 64-bit
RAM_GB         : 31.9

> Get-CimInstance Win32_Processor | select Name,NumberOfCores,NumberOfLogicalProcessors
Name                      : Intel(R) Core(TM) i7-8700 CPU @ 3.20GHz
NumberOfCores             : 6
NumberOfLogicalProcessors : 12

Free disk: C: 815.1 GB   D: 417.7 GB   E: 397.7 GB

cl             NOT ON PATH      (vswhere.exe not present: no Visual Studio / Build Tools)
gcc            NOT ON PATH      (no C:\msys64, C:\mingw64, C:\MinGW, C:\w64devkit)
clang          NOT ON PATH      (no C:\Program Files\LLVM)
make           NOT ON PATH      (also no mingw32-make, nmake, cmake)
tcc            C:\tools\tcc\tcc.exe   tcc version 1.1.0 2026-11-11_v1.1.0@7e2de65 (x86_64 Windows)
git            C:\Program Files\Git\cmd\git.exe   2.56.0.windows.4   (provides sh, diff, sed)
python         C:\Users\s81\.local\bin\python.exe   Python 3.15.2
```

The machine is x86-64. Nothing arm64 was run.

Consequences of having no reference compiler and no `make`:

- The upstream `tests/test-win32.bat` cannot run as written (needs `make`, `gcc`, `sh`).
  The suites were replayed by a shell script that applies the same per-test
  flags and skips as the Makefiles (section 2).
- `test.ref` (tcctest.c compiled by gcc), `abitest-cc` and `memtest` built by a
  reference compiler: **unverified**.
- Goal 1a "fraction of the gap to GCC -O1/-O2 and MSVC /O2": **unverified**, no gap can be computed.
- Goal 5b decimal reference throughput: **unverified**.

Environment notes that affect measurements:

- `NoDefaultCurrentDirectoryInExePath=1` is set, so `cmd` does not run
  `build-tcc.bat` without `.\`.
- A freshly executed `.exe` stays locked for a moment (real-time scanning is the
  likely cause, not confirmed). `tests/tccwin-tests.sh` reuses one output name
  and fails 9 to 23 tests per run with `could not write ... t.exe: Invalid
  argument`; with a wrapper that retries only on that message it passes 104/104
  (7 retries). Timings of freshly written executables are therefore not used
  anywhere below; all run-time numbers are taken in-process.

## 2. Build and existing suites

Build (stage 1 by the installed tcc 1.1.0, stage 2 by stage 1, stage 3 by stage 2):

```
> cd win32
> .\build-tcc.bat -c C:\tools\tcc\tcc.exe -t x86_64        (copy to stage1)
> .\build-tcc.bat -clean & .\build-tcc.bat -c <stage1>\tcc.exe -t x86_64   (copy to stage2)
> .\build-tcc.bat -clean & .\build-tcc.bat -c <stage2>\tcc.exe -t x86_64
> fc /b tcc.exe <stage2>\tcc.exe  (and libtcc.dll, lib\libtcc1.a)

stage1 exit=0
stage2 exit=0
stage3 exit=0
tcc.exe: stage 2 == stage 3, 35328 bytes
libtcc.dll: stage 2 == stage 3, 474112 bytes
lib\libtcc1.a: stage 2 == stage 3, 181040 bytes
tcc version 1.1.0 2026-11-11_jit-work@7e2de65 (x86_64 Windows)
total 5.69s            (all three stages, including the runtime library)
```

Only warnings: `tcc.h:66: warning: snprintf redefined`, `tcc.h:67: warning: vsnprintf redefined`.

| Suite | Command | Result |
|---|---|---|
| C23 + ownership | `TCC="sh $PWD/tests/jitbench/tcc-retry.sh" RUN=1 sh tests/tccwin-tests.sh` | `ok=104 fail=0`, "all tcc-win tests passed" |
| tests2 | replay of `tests/tests2/Makefile` (x86_64-win32 flags) | `pass=131 fail=0 skip=12` |
| pp | replay of `tests/pp/Makefile` | `pass=24 fail=0` |
| hello-exe, hello-run, vla_test, asm-c-connect, dlltest, btest | replay of `tests/Makefile` | ok |
| libtest, libtest_mt | `libtcc_test.c`, `libtcc_test_mt.c` built **by tcc** instead of gcc | ok |
| abitest-tcc | `abitest.c` + `libtcc.c` built by tcc | ok |
| tcctest self-consistency | `tcc -run tcctest.c` vs three nested `tcc -run tcc.c` levels | identical (1041 lines) |
| cross-test | `tcc -DTCC_TARGET_{I386,X86_64,ARM64} -run tcc.c -c tcctest.c` | 3/3 produce an object |
| MEM_DEBUG=2 build of tcc | compile `tcc.c`; `-run tcctest.c` | no leak report, exit 0 |
| tcctest vs gcc (`test3`), abitest-cc | need gcc | **unverified** |

tests2 skips are the Makefile's own: `34 98 99 106 114 117 148 138 139 140 141 145`.
The replay scripts are in the repository:

```
> tests\jitbench\bootstrap.cmd C:\tools\tcc\tcc.exe          (3-stage build, exit 1 if stage 2 != stage 3)
$ TCC="sh $PWD/tests/jitbench/tcc-retry.sh" RUN=1 sh tests/tccwin-tests.sh
$ sh tests/jitbench/suites.sh                                (tests2, pp, Makefile targets; exit 1 on any failure)
```

## 3. Compile latency and memory (goal 1b, 1c, 3b)

`jitbench scale <file> bench_entry call`: one `tcc_new` / `tcc_compile_string`
/ `tcc_relocate` / call / `tcc_delete` in a fresh process. Input from
`python tests/jitbench/gen_src.py N` (N functions of 10 lines). Three runs
each; the first run of the smallest file was a cold cache (18.6 ms) and is omitted.

| Functions | Lines | Source bytes | compile ms | relocate ms | ms/KLOC | Image bytes | Exec bytes | Peak WS KB | Peak private KB |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 100 | 1,004 | 30,639 | 2.50 - 2.54 | 1.40 - 1.60 | 2.49 - 2.53 | 28,672 | 24,576 | 5,156 | 1,648 |
| 1,000 | 10,004 | 307,435 | 15.18 - 15.60 | 1.73 - 1.82 | 1.52 - 1.56 | 253,952 | 233,472 | 6,792 | 3,176 |
| 10,000 | 100,004 | 3,093,161 | 144.3 - 145.5 | 5.23 - 6.19 | 1.44 - 1.46 | 2,502,656 | 2,301,952 | 21,684 | 19,816 |
| 100,000 | 1,000,004 | 31,128,531 | 1447.8 - 1452.2 | 39.5 - 40.2 | 1.448 - 1.452 | 25,014,272 | 23,011,328 | 169,812 | 168,480 |

- `tcc_new` 0.05 ms, `tcc_delete` 0.3 - 1.6 ms.
- Scaling is linear: **1.45 ms/KLOC** (about 690,000 lines/s). 100k functions compile in 1.45 s.
- Code size: 230 bytes of code per 10-line function, 250 bytes of image.
- Peak memory at 1M lines includes the 31 MB source string held by the harness: about 137 KB per KLOC for the compiler.
- Fixed cost per unit (no includes): 1.44 ms for a successful `new/compile/relocate/delete`, 0.30 ms when the compile fails (section 4). The relocate step is the larger part.
- Realistic input: `tcc -c tcc.c` (47,411 preprocessed lines, one source): min 94.8 ms, median 95.3 ms, of which 13.9 ms is process start (`tcc -v`).

Run-to-run spread of these numbers: under 1 % for 10 KLOC and above, about 3 % below.

## 4. Repeated cycles (goal 4)

`jitbench cycles 100000 <mode>`; modes: `ok` = compile + `tcc_relocate` + call,
`run` = compile + `tcc_run`, `err` = compile a unit with two errors.

```
=== cycles 100000 ok
cycle       0  ws_kb=4228 private_kb=828 handles=81 t=0.0s
cycle   10000  ws_kb=45956 private_kb=43892 handles=81 t=14.7s
cycle   50000  ws_kb=205936 private_kb=208848 handles=81 t=72.3s
cycle  100000  ws_kb=405944 private_kb=415116 handles=81 t=144.0s
mode=ok cycles=100000 unexpected=0 errors_reported=0  1440.4 us/cycle
=== cycles 100000 err
cycle       0  ws_kb=4228 private_kb=832 handles=81 t=0.0s
cycle   10000  ws_kb=5364 private_kb=1944 handles=81 t=2.9s
cycle   50000  ws_kb=5428 private_kb=2012 handles=81 t=14.4s
cycle  100000  ws_kb=5404 private_kb=1964 handles=81 t=30.0s
mode=err cycles=100000 unexpected=0 errors_reported=100000  300.4 us/cycle
=== cycles 100000 run
cycle       0  ws_kb=4244 private_kb=860 handles=81 t=0.0s
cycle   10000  ws_kb=46012 private_kb=44160 handles=81 t=16.7s
cycle   50000  ws_kb=205976 private_kb=209088 handles=81 t=84.7s
cycle  100000  ws_kb=405984 private_kb=415356 handles=81 t=165.2s
mode=run cycles=100000 unexpected=0 errors_reported=0  1651.8 us/cycle
```

| Mode | Private bytes growth over 100,000 cycles | Per cycle | Handles |
|---|---:|---:|---|
| ok (relocate) | +414,288 KB | 4.14 KB | constant (81) |
| run (`tcc_run`) | +414,496 KB | 4.14 KB | constant (81) |
| err (compile error) | +1,132 KB, flat after the first 10,000 | ~0 | constant (81) |

`jitbench cycles` exits 1 and prints `LEAK` when private bytes grow by more
than 1 MiB between the first checkpoint (10 % of the cycles) and the end, so
today `ok` and `run` fail and `err` passes.

Cause, confirmed by `jitbench outlive`:

```
before tcc_delete: 0000000000A50000 state=0x1000 protect=0x40 (0x40 = PAGE_EXECUTE_READWRITE)
after  tcc_delete: 0000000000A50000 state=0x1000 (0x10000 = MEM_FREE) -> code is still mapped
```

`tcc_run_free()` (`tccrun.c:204`) calls `VirtualFree(ptr, size, MEM_RELEASE)`.
Windows requires the size to be 0 with `MEM_RELEASE`; with a non-zero size the
call fails, the return value is not checked, and the whole run image leaks.
The line is unchanged since the initial import (`1b71121`). The compiler heap
itself does not leak (MEM_DEBUG=2 run reports nothing).

## 5. Concurrency (goal 3a)

`jitbench threads T K file`: T threads, each K times `new/compile/relocate/delete`.

```
10 KLOC unit (syn1000.c), K=40
threads=1  compiles=40  wall_s=0.665 cpu_s=0.672 compiles_per_s=60.2 cpu/wall=1.01
threads=2  compiles=80  wall_s=1.201 cpu_s=1.406 compiles_per_s=66.6 cpu/wall=1.17
threads=4  compiles=160 wall_s=2.387 cpu_s=2.656 compiles_per_s=67.0 cpu/wall=1.11
threads=6  compiles=240 wall_s=3.550 cpu_s=3.953 compiles_per_s=67.6 cpu/wall=1.11
threads=12 compiles=480 wall_s=7.062 cpu_s=7.953 compiles_per_s=68.0 cpu/wall=1.13

one-line unit (relocate-dominated), K=500
threads=1  compiles=500  wall_s=0.797 cpu_s=0.813 compiles_per_s=627.2  cpu/wall=1.02
threads=2  compiles=1000 wall_s=0.803 cpu_s=1.609 compiles_per_s=1245.1 cpu/wall=2.00
threads=4  compiles=2000 wall_s=0.830 cpu_s=3.266 compiles_per_s=2410.8 cpu/wall=3.94
threads=6  compiles=3000 wall_s=1.183 cpu_s=4.766 compiles_per_s=2535.7 cpu/wall=4.03
threads=12 compiles=6000 wall_s=2.164 cpu_s=9.297 compiles_per_s=2772.9 cpu/wall=4.30
```

- Parsing and code generation are serialized by `tcc_compile_sem`
  (`libtcc.c:74`, taken in `tcc_enter_state`, `libtcc.c:138`; the only
  call sites are `libtcc.c:810`/`842` around the compile and `635` on error).
  Throughput is flat: 1.13x at 12 threads.
- `tcc_relocate()` (linking into memory) is outside the lock and scales to
  about 4x on 6 cores.
- Global state behind the lock (file-scope variables, counted by `grep`):
  `tccgen.c` 40, `tccpp.c` 18, `arm64-gen.c` 10, `libtcc.c` 8, `x86_64-gen.c` 7,
  `tccelf.c` 2, `tccasm.c` 1, `tccrun.c` 1 (plus `g_s1`, `g_rc`, `rt_sem` for
  backtraces). Whether every path of the unlocked relocate phase is free of
  `tcc_state` use was not proven: **unverified** (no failure in 12,000 parallel relocations).

## 6. Generated code: stream kernels (goal 1a, R1, R2)

`jitbench kern tests/jitbench/kernels.c <name> 64 7 <b|d> 200`: each kernel is
compiled as its own unit 200 times (mean and min of `new+compile+relocate`),
then run over a 64 MiB buffer 7 times (best). Three passes; the range is shown.
Code size is the `.text` size of `tcc -c -DK_<name> kernels.c`.

| Kernel | What | ns/byte (3 passes) | GiB/s | `.text` bytes | compile ms (mean) | Bytes that cost one compile |
|---|---|---:|---:|---:|---:|---:|
| bytesum | sum of bytes | 1.918 - 2.034 | 0.46 | 116 | 2.37 - 2.41 | 1.2 MB |
| xor | byte transform | 1.250 - 1.329 | 0.70 | 116 | 2.35 - 2.44 | 1.8 MB |
| upper | branchy byte transform | 7.123 - 7.128 | 0.13 | 176 | 2.37 - 2.44 | 0.33 MB |
| wc | word/line state machine | 2.611 - 2.616 | 0.36 | 252 | 2.37 - 2.39 | 0.91 MB |
| fnv1a | serial hash | 1.565 - 1.585 | 0.60 | 140 | 2.35 - 2.39 | 1.5 MB |
| crc32 | table lookup | 3.025 - 3.027 | 0.31 | 324 | 2.38 | 0.79 MB |
| sum32 | sum of 32-bit words | 0.429 - 0.437 | 2.16 | 132 | 2.35 - 2.37 | 5.5 MB |
| hist | byte histogram | 1.498 - 1.502 | 0.62 | 312 | 2.40 - 2.41 | 1.6 MB |
| delta32 | 32-bit delta encode | 0.498 - 0.500 | 1.87 | 164 | 2.36 | 4.7 MB |
| copy64 | 64-bit word copy | 0.224 - 0.225 | 4.16 | 144 | 2.36 - 2.39 | 10.6 MB |
| dsum | binary64 sum | 0.271 - 0.281 | 3.44 | 172 | 2.38 - 2.43 | 8.7 MB |
| daxpy | binary64 a*x+y | 0.268 - 0.274 | 3.48 | 232 | 2.38 - 2.41 | 8.8 MB |

Median kernel: 1.41 ns/byte (between `xor` and `hist`). Noise: run time up to
6 % between passes on `bytesum`/`xor`, under 2 % on the others; compile time 3 %.

Reference compilers (GCC -O1/-O2, MSVC /O2, clang): **not measured, none installed.**
The gap that goal 1a and the GATE refer to therefore has no value yet.

### Rule R1 (no regression) baseline

Time-to-result for input size n bytes is `C + n * t`, with `C` = compile ms and
`t` = ns/byte from the table. These are the values no optimized configuration may exceed
(beyond noise: 3 % on C, 6 % on t). Memory: 1.6 MB peak private for a 1 KLOC unit, section 3.

### Rule R2 (break-even)

An optimization that adds `dC` ms of compile time and speeds a kernel up by a
factor `s` pays off above

    n* = dC * 1e6 / (t * (1 - 1/s))   bytes

Phase 0 can supply `t` but not `s` or `dC`: no optimizer exists and there is no
reference compiler to bound `s`. Worked values for the formula only
(**hypothetical `s`, not measurements**), per 1 ms of added compile time:

| Kernel | t ns/byte | n* at s=1.5 | n* at s=2 | n* at s=4 |
|---|---:|---:|---:|---:|
| upper | 7.13 | 0.42 MB | 0.28 MB | 0.19 MB |
| bytesum | 2.03 | 1.5 MB | 0.98 MB | 0.66 MB |
| median | 1.41 | 2.1 MB | 1.4 MB | 0.95 MB |
| copy64 | 0.224 | 13.4 MB | 8.9 MB | 6.0 MB |

The last column of the kernel table gives the scale: today a unit must process
0.3 - 10 MB before run time exceeds its own compile time, so below roughly
1 MB of input any added compile cost is a net loss.

`-O` exists as an option but only defines `__OPTIMIZE__` (`libtcc.c:2152`, `tccpp.c:3948`); it changes no code.

## 7. Stream larger than 4 GiB (goal 2)

Scratch file: 4,608 MiB (4,831,838,208 bytes) written by `jitbench mkfile`
in 15.5 s to the scratch directory on C: (815 GB free), deleted after the runs.
Byte at offset `o` is `(o ^ o>>13 ^ (o>>32)*7) & 0xff`, so a truncated offset reads wrong data.
`jitbench stream tests/jitbench/stream.c <fn> big.bin` JIT-compiles the loop and runs it.

```
wrote 4831838208 bytes sum=616059371520 in 15.5s
stream_stdio rc=0 seconds=10.14 out0=4831838208 out1=616059371520 out2=0 out3=18446744073709551615 out4=4831838208 out5=1 out6=0 out7=848
  GiB_per_s=0.444 ws_before_kb=5640 peak_ws_kb=10512 peak_private_kb=7100
stream_win32 rc=0 seconds=10.04 out0=4831838208 out1=616059371520 out2=0 out3=4831838208 out4=0 out5=1 out6=0 out7=0
  GiB_per_s=0.448 ws_before_kb=5624 peak_ws_kb=10496 peak_private_kb=7068
stream_mmap rc=0 seconds=11.41 out0=4831838208 out1=616059371520 out2=0 out3=0 out4=0 out5=1 out6=0 out7=0
  GiB_per_s=0.394 ws_before_kb=5624 peak_ws_kb=4724240 peak_private_kb=10904
stream_stdio rc=0 seconds=9.95  ... GiB_per_s=0.452 ws_before_kb=5624 peak_ws_kb=10496 peak_private_kb=7072
stream_win32 rc=0 seconds=9.98  ... GiB_per_s=0.451 ws_before_kb=5624 peak_ws_kb=10496 peak_private_kb=7068
```

| Loop | Bytes, sum, pattern | Throughput | Peak working set | Notes |
|---|---|---:|---:|---|
| `fread`, 1 MiB chunks | correct | 0.44 - 0.45 GiB/s | 10.5 MB (flat) | `ftell` returns -1 at EOF (`out3`); `fgetpos` correct (`out4`); `_fseeki64` correct (`out5`); `fseek(long)` to 4 GiB+12345 does not reach it (`out6=0`) |
| `ReadFile`, 1 MiB chunks | correct | 0.45 GiB/s | 10.5 MB (flat) | `SetFilePointerEx` and `GetFileSize` (two halves) correct |
| one `MapViewOfFile` of the file | correct | 0.39 GiB/s | 4.7 GB | 64-bit indexing `p[i]` past 4 GiB is correct; not flat |

- `sizeof(size_t)=8`, `sizeof(long)=4`, `sizeof(fpos_t)=8` (`out7=848`). No truncation in generated code: 64-bit counters, offsets and indexing are correct.
- Throughput is CPU-bound by the byte loop (matches `bytesum`, 0.46 GiB/s).
- Link gaps found with `tcc -run` on a one-line reference to each symbol:

```
_fseeki64: resolved            _ftelli64: unresolved reference
fseeko64:  unresolved          ftello64:  unresolved
_lseeki64, _telli64, _filelengthi64, _fstat64, _stat64, fgetpos, fsetpos: resolved
SetFilePointerEx, GetFileSize, ReadFile, MapViewOfFile: resolved
GetFileSizeEx: unresolved      VirtualAlloc2, MapViewOfFile3: unresolved
```

`_ftelli64`, `ftello64` and `fseeko64` are declared in `win32/include/stdio.h`
(lines 190-194) but missing from `win32/lib/msvcrt.def`; `GetFileSizeEx` is
missing from `win32/lib/kernel32.def`. `psapi.h` is not shipped.

## 8. Code lifetime and relocation range (goals 3b, 4)

**Lifetime.** By design the run image belongs to the `TCCState`: `tcc_delete`
calls `tcc_run_free`, which unregisters the unwind table
(`RtlDeleteFunctionTable`), frees loaded DLLs and releases the image. There is
no API to detach code from the state. After `tcc_relocate` the state already
drops all sections except the symbol table (`cleanup_sections`, `tccrun.c:295`),
but the `TCCState`, symbol table and preprocessor/define tables stay allocated
until `tcc_delete`. Because of the `VirtualFree` defect (section 4) the code in
fact stays mapped and executable after `tcc_delete`, with its unwind table
removed: a leak, not a supported feature.

**Page protection.** One `VirtualAlloc(MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE)`
for code and data together, then `.text` is switched to
`PAGE_EXECUTE_READWRITE` (`CONFIG_RUNMEM_RO` is 0 on Windows, `tccrun.c:320`,
`418-421`). Measured: `protect=0x40`. Code is never W^X. Data pages stay RW (`.rdata` is writable).

**Range.** `jitbench reloc <test>`:

```
neardata: code=0000000000A50000 var=0000000000A978D0 distance=0 MiB
neardata: get()=77 *v=77 -> OK
fardata:  code=0000000000A50000 var=00007E0000000000 distance=132120565 MiB
fardata:  get()=77 *v=77 -> OK
farfunc:  code=0000000000470000 host_fn=00007E0000000000 (132120571 MiB) msvcrt sprintf=0000000000472491 (0 MiB)
farfunc:  call(1)=1001 -> OK
bss1g:    image_bytes=1073745920
bss1g:    probe()=1073741827 (expect 1073741827)
bigbss:   compile/relocate FAILED (errors=0)
bigarray: compile/relocate FAILED (errors=1)
tcc: <string>:1: error: invalid array size
```

- External functions and data are reached through import-table slots inside
  the image (the address of `sprintf` seen by JIT code is a thunk in the
  image), so distance does not matter: 126 TiB works for both.
- A host variable added with `tcc_add_symbol` must be declared
  `__declspec(dllimport)`; otherwise: `error: symbol 'host_var' is missing __declspec(dllimport)`.
- Image of 1 GiB: works. Image of 3,000 MiB (two 1.5 GiB arrays): `tcc_relocate`
  returns -1 **without any diagnostic**. Sizes are `int`/`unsigned` in
  `tcc_relocate`, `rt_mem`, `tcc_relocate_ex` (`tccrun.c:116`, `156`, `333`)
  and `run_size` (`tcc.h:995`); intra-image references are 32-bit
  PC-relative (`R_X86_64_PC32`/`PLT32`, `x86_64-link.c:236-263`).
- A single object of 2 GiB or more is rejected by the front end (`tccgen.c:7339`, "invalid array size").
- Code beyond 2 GiB was not attempted: at 230 bytes per synthetic function it
  needs about 9.3 million functions, by extrapolation roughly 135 s and 13 GB
  of compiler memory. The same `int` sizes apply. **unverified**.
- TLS: `thread_local` in `-run` fails with "thread-local storage not supported with -run" (`tccrun.c:389`).

## 9. arm64 (read from source, nothing run: unverified)

- Instruction cache: `protect_pages()` calls `FlushInstructionCache(GetCurrentProcess(), ptr, length)`
  for the executable range when the target is ARM64 (`tccrun.c:494-499`), after the code was copied. Present.
- W^X: same as x86-64, `.text` becomes `PAGE_EXECUTE_READWRITE`; no separate RX mapping.
- Branch range: `R_AARCH64_CALL26` (+-128 MiB) and `R_AARCH64_ADR_PREL_PG_HI21` (+-4 GiB) inside the image
  (`arm64-link.c:248-340`); an image above 128 MiB of code would need veneers. Whether they exist was not checked.
- Unwind: `.pdata`/`.xdata` are generated for arm64 (`tccpe.c:2010-2060`) and registered with `RtlAddFunctionTable` under `_WIN64`.
- The CI workflow builds, self-hosts and tests arm64 on `windows-11-arm`; none of that was reproduced here.

## 10. Floating point (goal 5a)

`python tests/jitbench/gen_fpka.py > fpka.c` generates 22,065 checks. Expected
values come from CPython 3.15 binary64 arithmetic on this CPU and, for
binary32 conversions and decimal literals, exact rational rounding. This is
the only reference available; no second compiler was compared (**unverified**).
The same file was run three ways:

```
> win32\tcc.exe fpka.c -o fpka.exe & fpka.exe         (AOT)
> win32\tcc.exe -run fpka.c                           (-run)
> jitbench scale fpka.c bench_entry call              (libtcc)
AOT == -run
AOT == libtcc JIT (modulo CRLF)
```

Output (identical in all three):

```
rt binary64 add        checked= 1684 mismatches=0
rt binary64 sub        checked= 1684 mismatches=0
rt binary64 mul        checked= 1684 mismatches=0
rt binary64 div        checked= 1684 mismatches=0
rt binary64 sqrt (crt) checked=  604 mismatches=0
rt binary64 a*b+c      checked=  600 mismatches=0
rt binary32 add        checked= 1489 mismatches=0
rt binary32 sub        checked= 1489 mismatches=0
rt binary32 mul        checked= 1489 mismatches=0
rt binary32 div        checked= 1489 mismatches=0
rt binary32 sqrt (crt) checked=  603 mismatches=0
rt binary32 a*b+c      checked=  600 mismatches=0
rt int64->binary64     checked=  413 mismatches=0
rt int64->binary32     checked=  413 mismatches=0
  MISMATCH u64->d[5] got 43e0000000000000 want 43e0000000000001
  MISMATCH u64->d[19] got 43e21e21368b9e00 want 43e21e21368b9dff
  MISMATCH u64->d[23] got 43ec830e30524780 want 43ec830e30524781
rt uint64->binary64    checked=  408 mismatches=44
  MISMATCH u64->f[7] got 5f800000 want 5f7fffff
rt uint64->binary32    checked=  408 mismatches=1
rt binary64->int64     checked=  325 mismatches=0
rt binary64->binary32  checked=  325 mismatches=0
rt binary64->uint64    checked=  210 mismatches=0
fold binary64 add      checked=  306 mismatches=0
fold binary64 sub      checked=  306 mismatches=0
fold binary64 mul      checked=  306 mismatches=0
fold binary64 div      checked=  306 mismatches=0
fold binary64 a*b+c    checked=  300 mismatches=0
fold binary32 add      checked=  305 mismatches=0
fold binary32 sub      checked=  305 mismatches=0
fold binary32 mul      checked=  305 mismatches=0
fold binary32 div      checked=  305 mismatches=0
fold binary32 a*b+c    checked=  300 mismatches=0
  MISMATCH K u64->d[5] got 43e0000000000000 want 43e0000000000001
  MISMATCH K u64->d[19] got 43e21e21368b9e00 want 43e21e21368b9dff
  MISMATCH K u64->d[23] got 43ec830e30524780 want 43ec830e30524781
fold uint64->binary64  checked=  200 mismatches=21
  MISMATCH K u64->f[7] got 5f800000 want 5f7fffff
fold uint64->binary32  checked=  200 mismatches=1
fold binary64->binary32 checked=  200 mismatches=0
  MISMATCH lit d[409] got 0000000000000000 want 0000000000000001
literal binary64       checked=  412 mismatches=1
  MISMATCH lit f[406] got 3f800002 want 3f800001
literal binary32       checked=  408 mismatches=1
TOTAL mismatches=69
```

What holds:

- + - * / on binary32 and binary64 are bit-exact at run time and when folded,
  including subnormals, overflow, signed zero, infinities and NaN-ness.
- No contraction: 67 of the 600 binary64 `a*b+c` triples would change under FMA; none did.
- No excess precision: 65 of the 600 binary32 triples would change if `a*b`
  were kept wider than binary32; none did, at run time or folded.
- Folded results equal run-time results in every category (the defects below are the same in both).
- AOT, `-run` and libtcc give identical output: one code generator, no tiers yet.

Defects (same in all three paths), each checked by hand against the exact value:

1. **uint64 -> binary64, values >= 2^63, run time and folded** (44/408 and 21/200).
   Example: 2^63 + 1025 gives `43e0000000000000` (2^63); the nearest double is
   2^63 + 2048 (`43e0000000000001`). A sticky bit is lost.
2. **uint64 -> binary32, run time and folded** (1/408, 1/200). 2^64 - 2^39 - 1
   gives 2^64 (`5f800000`) instead of `5f7fffff`: rounded twice (to binary64, then binary32).
3. **binary64 decimal literal** (1/412): `2.4703282292062328e-324` (just above
   half of the smallest subnormal) is read as 0 instead of 5e-324. The 400 random
   literals of 2 to 21 significant digits were all correct.
4. **binary32 decimal literal** (1/408): `1.00000017881393432617187499f` gives
   `3f800002`, correct is `3f800001`: parsed to binary64 first, then rounded again.

Also: `FLT_EVAL_METHOD` and `__STDC_IEC_559__` are not defined by the headers.
`sqrt` is the msvcrt function, not generated code. Rounding modes other than
round-to-nearest, exception flags, and math functions were not tested.

## 11. Decimal (goal 5b)

Current state, from `tcc -run`:

```
_Decimal64          => error: '_Decimal64' undeclared
1.5DD               => error: invalid number
_BitInt(128)        => error: _BitInt width must be between 1 and 64 (BITINT_MAXWIDTH)
__int128            => error: '__int128' undeclared
```

There is no 128-bit value type of any kind in the front end. Reference
throughput of GCC (MinGW-w64, -O2) decimal operations: **not measured, GCC is not installed.**
The speed rule (2) of goal 5b has no number to compare against.

## 12. Numbers for the goal-1 rules, and proposed targets

Values below are baselines measured here; "proposed" entries need approval.

| Goal | Baseline | Proposed target |
|---|---|---|
| R1 time | `C + n*t` per kernel, section 6 | no kernel above baseline + noise (3 % on C, 6 % on t) at n = 1 MiB, 64 MiB and 1024 MiB (the sizes `jitbench kern` can run; it takes whole MiB and two buffers), plus n = 0, which is the compile time C alone |
| R1 memory | 1.6 MB peak private (1 KLOC); 137 KB/KLOC | not above baseline + 5 % |
| R2 | formula in section 6; `s` and `dC` unknown | tier threshold = n* computed per kernel once an optimizer exists |
| 1a | gap to reference: **unknown** | cannot be set without a reference compiler (decision 1 below) |
| 1b | 1.45 ms/KLOC; 1.44 ms per tiny unit; 2.35 - 2.44 ms per kernel unit | cold code within 3 % of these |
| 1c / 4 | +4.14 KB per cycle; image never released; RWX | 0 KB growth from cycle 10,000 to 100,000 in all three modes; image released on unload; code pages RX |
| 2 | correct at 4.5 GiB, 10.5 MB peak; 4 symbols unresolved | same three loops pass; peak working set <= 12 MB for the chunked loops; `_ftelli64`, `ftello64`, `fseeko64`, `GetFileSizeEx` link |
| 3a | 1.13x at 12 threads (10 KLOC unit) | >= 4x at 6 threads, >= 5x at 12 threads on the same test |
| 3b | 100k functions: 1.45 s, 25 MB image; > 2 GiB image fails silently | 100k functions not slower; image > 2 GiB either works or fails with a clear error (which one is decision 3) |
| 5a | 69 mismatches in 4 classes; AOT == `-run` == libtcc | 0 mismatches; identical output on every tier and with the optimizer on and off |
| 5b | no decimal support; no reference numbers | set after a reference is chosen (decision 2) |

## 13. Open decisions

1. **Reference compiler for goal 1a and the GATE.** None is installed, so the
   run-time gap is undefined and the GATE ("close half of the gap on the median
   kernel") cannot be evaluated. Either install one (w64devkit is a portable
   ~80 MB zip that also provides `make` for the upstream suite), or redefine the
   gate without a reference.
2. **Reference for the decimal speed rule.** The rule names GCC (MinGW-w64, -O2); it is not installed.
3. **Image larger than 2 GiB.** Support it (64-bit sizes plus a way around
   32-bit PC-relative references) or reject it with a diagnostic.
4. **The four floating-point defects and the `VirtualFree` leak** are present in
   the AOT compiler too (the leak in `tcc -run`). Fixing the FP defects changes
   AOT output for the affected inputs; confirm they belong in Phase 2.
5. **Benchmark files.** `tests/jitbench/` was added on the branch so the numbers
   can be reproduced; say if it should live elsewhere.

## 14. Not verified

- Any comparison with GCC, MSVC or clang (run time, code size, FP results, decimal speed).
- `tests/test-win32.bat` as written, `test3` against a gcc `test.ref`, `abitest-cc`.
- Everything arm64 (section 9) and i386.
- Code size beyond 2 GiB; thread safety of the unlocked relocate phase beyond 12,000 parallel runs.
- FP rounding modes, exception flags, math library functions, `printf`/`strtod` round trips.
- The cause of the transient `.exe` lock after execution.
