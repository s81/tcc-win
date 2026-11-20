#!/bin/sh
# Run win32/tcc.exe; retry only when the output file is still locked
# ("could not write"), which happens right after the previous test ran it.
# usage: TCC="sh $PWD/tests/jitbench/tcc-retry.sh" RUN=1 sh tests/tccwin-tests.sh
# (absolute path: tccwin-tests.sh changes directory before calling $TCC)
tcc=$(cd "$(dirname "$0")/../.." && pwd)/win32/tcc.exe
n=0
while :; do
    out=$("$tcc" "$@" 2>&1); rc=$?
    case "$out" in
    *"could not write"*) n=$((n+1)); [ $n -lt 50 ] && { echo retry >> "${RETRYLOG:-/dev/null}"; sleep 0.2; continue; } ;;
    esac
    [ -n "$out" ] && printf '%s\n' "$out" >&2
    exit $rc
done
