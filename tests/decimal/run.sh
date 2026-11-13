#!/bin/sh
# Builds tests/decimal/dectest.c with the tcc-win in win32/ and replays the
# committed vectors (libtcc1.a supplies the library).  Extra vectors:
#   python -I tests/decimal/gen_vectors.py --seed 7 --count 2000 --out big.txt
#   sh tests/decimal/run.sh big.txt
# usage: [TMPDIR=...] sh tests/decimal/run.sh [vectorfile] [maxfail]
TOP=$(cd "$(dirname "$0")/../.." && { pwd -W 2>/dev/null || pwd; })
W=${TMPDIR:-/tmp}/dectest-run; mkdir -p "$W"
V=${1:-$TOP/tests/decimal/vectors.txt}
"$TOP/win32/tcc.exe" -B"$TOP/win32" -I"$TOP/include" -I"$TOP" -L"$TOP/win32" -fgoto \
    "$TOP/tests/decimal/dectest.c" -o "$W/dectest.exe" || exit 1
"$W/dectest.exe" "$V" ${2:-20}
