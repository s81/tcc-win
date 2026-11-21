#!/bin/sh
# Unit tests, on the build host, of the Windows runtime library sources
#   lib/c99fenv.c lib/c23strto.c lib/c23scanf.c lib/c23printf.c lib/c23uchar.c
# and the C99/C11 part of lib/c23rt.c
# (they build against the host C library without _WIN32; the public names
# are renamed with -D so that they do not clash with it).
#
#   tests/c99rt-host.sh [path/to/tcc]       default: ./tcc in the source tree

top=$(cd "$(dirname "$0")/.." && pwd)
tcc=${1:-$top/tcc}
tmp=${TMPDIR:-/tmp}/c99rt-host.$$
mkdir -p "$tmp"
fail=0

R=""
for f in feclearexcept fetestexcept feraiseexcept fegetexceptflag fesetexceptflag \
         fetestexceptflag fegetround fesetround fegetenv fesetenv feholdexcept \
         feupdateenv fegetmode fesetmode \
         strtoimax strtoumax wcstoimax wcstoumax wcstoll wcstoull wcstof wcstold \
         vsscanf vfscanf vscanf vswscanf vfwscanf vwscanf \
         mbrtoc16 c16rtomb mbrtoc32 c32rtomb mbrtoc8 c8rtomb \
         isblank iswblank lldiv imaxdiv quick_exit at_quick_exit call_once; do
    R="$R -D$f=x_$f"
done
CC="$tcc -B$top -w -fgoto"
# fenv.h of the Windows headers (fenv_t layout), the rest from the host
WIN="-nostdinc -I$top/win32/include -I$top/include -I/usr/include -I/usr/include/$(uname -m)-linux-gnu -D__TCC_MSVCRT_PRINTF"

run() { # name lib-source [flags]
    n=$1; lib=$2; shift 2
    if $CC $R "$@" -c "$top/lib/$lib" -o "$tmp/lib.o" \
       && $CC $R "$@" -c "$top/tests/c99rt-host/$n.c" -o "$tmp/t.o" \
       && $CC "$tmp/t.o" "$tmp/lib.o" -o "$tmp/t" && "$tmp/t"; then
        :
    else
        echo "FAIL $n"; fail=1
    fi
}

# quick_exit ends the program with status 3 after running the handlers
run_misc() {
    if $CC $R -c "$top/lib/c23rt.c" -o "$tmp/lib.o" \
       && $CC $R -c "$top/tests/c99rt-host/misc.c" -o "$tmp/t.o" \
       && $CC "$tmp/t.o" "$tmp/lib.o" -o "$tmp/t"; then
        out=$("$tmp/t"); rc=$?
        echo "$out" | head -1
        [ $rc = 3 ] && [ "$(echo "$out" | tail -2 | tr '\n' ' ')" = "handler B handler A " ] \
            || { echo "FAIL misc (rc=$rc): $out"; fail=1; }
    else
        echo "FAIL misc"; fail=1
    fi
}

run fenv c99fenv.c $WIN
run strto c23strto.c
run scanf c23scanf.c
run wprintf c23printf.c
run uchar c23uchar.c
run_misc
rm -rf "$tmp"
[ $fail = 0 ] && echo "all c99rt host tests passed"
exit $fail
