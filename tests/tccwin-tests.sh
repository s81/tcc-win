#!/bin/sh
# tcc-win tests: C23 features and [[owner]] ownership checking.
#
#   tests/tccwin-tests.sh                 use ./x86_64-win32-tcc (cross, compile+link only)
#   TCC="win32/tcc.exe" RUN=1 tests/tccwin-tests.sh     on Windows: also run the programs
#
# Files err_*.c must be rejected with the diagnostic given in the expect.txt
# of their directory; all other files must compile cleanly.

top=$(cd "$(dirname "$0")/.." && pwd)
: "${TCC:=$top/x86_64-win32-tcc -B$top/win32 -I$top/include -L$top}"
tmp=${TMPDIR:-/tmp}/tccwin-tests.$$
mkdir -p "$tmp"
fail=0

check_ok() { # dir file [flags]
    if out=$(cd "$1" && $TCC $3 "$2" -o "$tmp/t.exe" 2>&1) && [ -z "$out" ]; then
        if [ -n "$RUN" ] && ! "$tmp/t.exe" >"$tmp/out.txt" 2>&1; then
            echo "FAIL (run) $2"; cat "$tmp/out.txt"; fail=1
        else
            echo "ok   $2"
        fi
    else
        echo "FAIL (compile) $2: $out"; fail=1
    fi
}

check_ok "$top/tests/c23" c23_features.c
check_ok "$top/tests/c23" c23_more.c
check_ok "$top/tests/c23" c23_bitint.c
check_ok "$top/tests/c23" c23_constexpr.c
check_ok "$top/tests/c23" c23_audit_fixes.c -Wall
check_ok "$top/tests/c23" c23_runtime.c
check_ok "$top/tests/c23" c23_snan.c
check_ok "$top/tests/c23" c23_printf_b.c -Wall
check_ok "$top/tests/c23" c23_math2.c -Wall
for f in "$top"/tests/ownership/ok_*.c; do
    check_ok "$top/tests/ownership" "$(basename "$f")"
done
for dir in c23 ownership; do
    while read -r name msg; do
        out=$(cd "$top/tests/$dir" && $TCC -c "$name" -o "$tmp/t.o" 2>&1)
        case "$out" in
        *"error: $msg"*) echo "ok   $name (rejected)" ;;
        *) echo "FAIL $name: expected error '$msg', got: $out"; fail=1 ;;
        esac
    done < "$top/tests/$dir/expect.txt"
done

# all ownership errors of a file are reported in one run, one per variable
out=$(cd "$top/tests/ownership" && $TCC -c err_multi.c -o "$tmp/t.o" 2>&1)
n=$(printf '%s\n' "$out" | grep -c "error:")
if [ "$n" = 4 ]; then
    echo "ok   err_multi.c (4 errors in one run)"
else
    echo "FAIL err_multi.c: expected 4 errors, got $n: $out"; fail=1
fi

rm -rf "$tmp"
[ $fail = 0 ] && echo "all tcc-win tests passed"
exit $fail
