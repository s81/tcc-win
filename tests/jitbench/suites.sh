#!/bin/sh
# make-free replay of tests/tests2/Makefile, tests/pp/Makefile and the parts of
# tests/Makefile that need no reference compiler (x86_64-win32 settings).
# usage (Git Bash, after building win32/tcc.exe): sh tests/jitbench/suites.sh
# Exit status is 0 only if nothing failed. Work files go to $TMPDIR/jitbench-suites.
TOP=$(cd "$(dirname "$0")/../.." && { pwd -W 2>/dev/null || pwd; })  # D:/... form, as tcc prints it
W=${TMPDIR:-/tmp}/jitbench-suites
rm -rf "$W"; mkdir -p "$W"; cd "$W" || exit 1
F="-B$TOP/win32 -I$TOP/include -I$TOP -L$TOP/win32 -fgoto"
TCC="$TOP/win32/tcc.exe $F"
PATH="$(cd "$TOP/win32" && pwd):$PATH"   # libtcc.dll for libtcc_test
pass=0; fail=0; skip=0; failed=""

# ---------------- tests2 ----------------
SRC=$TOP/tests/tests2
for c in "$SRC"/??_*.c "$SRC"/???_*.c; do
    n=$(basename "$c" .c); FLAGS=""; ARGS=""; NORUN=""; XF=""
    case $n in
    34_*|98_*|99_*|106_*|114_*|117_*|148_*|138_*|139_*|140_*|141_*|145_*) skip=$((skip+1)); continue ;;
    esac
    case $n in
    31_args) ARGS="arg1 arg2 arg3 arg4 arg5" ;;
    76_*) FLAGS="-fdollars-in-identifiers" ;;
    60_*) FLAGS="-std=c11 -dt" ;;
    95_*|102_*|81_*|129_*|133_*) FLAGS="-std=c11" ;;
    96_*|125_*|128_*) FLAGS="-dt" ;;
    104_inline) FLAGS="$SRC/104+_inline.c" ;;
    120_alias) FLAGS="$SRC/120+_alias.c"; NORUN=1 ;;
    42_*|108_*) NORUN=1 ;;
    112_*) FLAGS="-std=c11 -dt -b"; XF=1 ;;
    113_*) XF=1 ;;
    115_*|116_*|121_*|122_*|132_*) FLAGS="-b" ;;
    126_*) FLAGS="-b"; NORUN=1; XF=1 ;;
    124_*) FLAGS="-pthread" ;;
    144_*) FLAGS="-pthread"; NORUN=1 ;;
    esac
    {
    case $n in
    46_grep) $TCC -std=c11 -run "$c" '[^* ]*[:a:d: ]+\:\*-/: $' "$SRC/46_grep.c" ;;
    113_btdll)
        $TCC -bt "$c" -shared -D DLL=1 -o a1.dll && $TCC -bt "$c" -shared -D DLL=2 -o a2.dll &&
        $TCC -bt "$c" a1.dll a2.dll -Wl,-rpath=. -o $n.exe && ./$n.exe ;;
    156_tls_dll)
        $TCC "$c" -shared -D DLL=1 -o 156a1.dll && $TCC "$c" -shared -D DLL=1 -o 156a2.dll &&
        $TCC "$c" 156a1.dll -o $n.exe && ./$n.exe ;;
    146_tls_extern)
        $TCC -c "$c" -o $n-main.o && $TCC -c "$c" -DDEFS -o $n-defs.o &&
        $TCC -no-pie -Wl,-z,noexecstack $n-main.o $n-defs.o -o $n.exe && ./$n.exe ;;
    *)  if [ -n "$NORUN" ]; then $TCC $FLAGS "$c" -o $n.exe && ./$n.exe $ARGS
        else $TCC $FLAGS -run "$c" $ARGS; fi ;;
    esac
    } 2>&1 | sed -e "s,$SRC/,,g" |
    if [ -n "$XF" ]; then sed -e 's;[0-9A-Fa-fx]\{5,\};........;g' -e 's;0x[0-9A-Fa-f]\{1,\};0x?;g'; else cat; fi > $n.output
    if diff -Nbu "$SRC/$n.expect" $n.output > $n.diff 2>&1; then pass=$((pass+1)); rm -f $n.diff
    else fail=$((fail+1)); failed="$failed tests2/$n"; fi
done
echo "tests2: pass=$pass fail=$fail skip=$skip"
pass=0; fail=0

# ---------------- pp ----------------
SRC=$TOP/tests/pp
for c in "$SRC"/*.c "$SRC"/*.S; do
    n=$(basename "$c"); n=${n%.*}; D="-Nu -b"
    [ $n = 02 ] && D="$D -w"; [ $n = 16 ] && D="$D -B"
    $TCC -E -P "$c" 2>&1 | sed "s,$SRC/,,g" > pp-$n.output
    if diff $D "$SRC/$n.expect" pp-$n.output > pp-$n.diff 2>&1; then pass=$((pass+1)); rm -f pp-$n.diff
    else fail=$((fail+1)); failed="$failed pp/$n"; fi
done
echo "pp: pass=$pass fail=$fail"

# ---------------- tests/Makefile targets without a reference compiler ----------------
t() { # name command...
    name=$1; shift
    if "$@" > "m-$name.log" 2>&1; then echo "ok   $name"; else echo "FAIL $name (see $W/m-$name.log)"; failed="$failed $name"; fi
}
T=$TOP/tests
RUN_TCC="-run $TOP/tcc.c $F"
CT="-DCC_NAME=CC_gcc -DGCC_MAJOR=15 -w -std=c17"   # what test-win32.bat puts in config.h
t hello-exe   sh -c "$TCC $TOP/examples/ex1.c -o hello.exe && ./hello.exe"
t hello-run   sh -c "$TCC -run $TOP/examples/ex1.c"
# upstream builds these two with the reference CC; here: built by tcc itself
t libtest     sh -c "$TCC $T/libtcc_test.c $TOP/win32/libtcc.dll -o libtcc_test.exe && ./libtcc_test.exe $TOP/tcc.c $F"
t libtest_mt  sh -c "$TCC $T/libtcc_test_mt.c $TOP/win32/libtcc.dll -o libtcc_test_mt.exe && ./libtcc_test_mt.exe $TOP/tcc.c $F"
t abitest-tcc sh -c "$TCC -o abitest-tcc.exe $T/abitest.c $TOP/libtcc.c && ./abitest-tcc.exe $F"
t vla_test    sh -c "$TCC -o vla_test.exe $T/vla_test.c && ./vla_test.exe"
t asm-c-connect sh -c "$TCC -o acc.exe $T/asm-c-connect-1.c $T/asm-c-connect-2.c && $TCC -c -o acc1.o $T/asm-c-connect-1.c && $TCC -c -o acc2.o $T/asm-c-connect-2.c && $TCC -o acc-sep.exe acc1.o acc2.o && ./acc.exe > acc.out1 && ./acc-sep.exe > acc.out2 && diff -u acc.out1 acc.out2"
t dlltest     sh -c "$TCC -DLIBTCC_AS_DLL $TOP/libtcc.c -shared -o libtcc2.dll && $TCC -DONE_SOURCE=0 $TOP/tcc.c libtcc2.dll -Wl,-rpath=. -o tcc2.exe && ./tcc2.exe $F $RUN_TCC -run $TOP/examples/ex1.c"
# test3 without test.ref (needs gcc): tcctest run directly vs through 3 nested tcc -run levels
t tcctest-1x-vs-3x sh -c "$TCC $CT -run $T/tcctest.c > test.out0 && $TCC $RUN_TCC $RUN_TCC $RUN_TCC $CT -run $T/tcctest.c > test.out3 && diff -u test.out0 test.out3"
for X in I386 X86_64 ARM64; do
    t cross-test-$X sh -c "$TCC -DTCC_TARGET_PE -DTCC_TARGET_$X -DTCC_CROSS_TEST -run $TOP/tcc.c -c $T/tcctest.c $CT $F -o cross-$X.o"
done
bt() {
    for i in 1 4 8 10 14 16; do $TCC -b -run $T/boundtest.c $i >/dev/null 2>&1 || { echo "Failed positive test $i"; return 1; }; done
    for i in 2 5 6 7 9 11 12 13 15 17 18; do $TCC -b -bt1 -run $T/boundtest.c $i >/dev/null 2>&1 && { echo "Failed negative test $i"; return 1; }; done
    return 0
}
t btest bt
echo "FAILED:${failed:- none}"
[ -z "$failed" ]
