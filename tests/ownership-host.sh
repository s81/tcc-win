#!/bin/sh
# Ownership checks with the host compiler and the system's C headers
# (e.g. glibc on Linux): tcc gives malloc/free/realloc/strdup their
# ownership whatever header declares them.
#
#   tests/ownership-host.sh               use ./tcc
#   TCC="..." tests/ownership-host.sh
#
# ok_*.c must compile cleanly (-c only), err_*.c must be rejected with the
# diagnostic given in ownership/expect.txt.

top=$(cd "$(dirname "$0")/.." && pwd)
: "${TCC:=$top/tcc -B$top -I$top/include}"
dir=$top/tests/ownership
tmp=${TMPDIR:-/tmp}/ownership-host.$$
mkdir -p "$tmp"
fail=0

for f in "$dir"/ok_*.c; do
    name=$(basename "$f")
    if out=$(cd "$dir" && $TCC -c "$name" -o "$tmp/t.o" 2>&1) && [ -z "$out" ]; then
        echo "ok   $name"
    else
        echo "FAIL $name: $out"; fail=1
    fi
done
while read -r name msg; do
    out=$(cd "$dir" && $TCC -c "$name" -o "$tmp/t.o" 2>&1)
    case "$out" in
    *"error: $msg"*) echo "ok   $name (rejected)" ;;
    *) echo "FAIL $name: expected error '$msg', got: $out"; fail=1 ;;
    esac
done < "$dir/expect.txt"
for name in err_discard.c err_multi.c; do
    if out=$(cd "$dir" && $TCC -c "$name" -o "$tmp/t.o" 2>&1); then
        echo "FAIL $name: not rejected"; fail=1
    else
        echo "ok   $name (rejected)"
    fi
done

rm -rf "$tmp"
[ $fail = 0 ] && echo "all ownership tests passed (host)"
exit $fail
