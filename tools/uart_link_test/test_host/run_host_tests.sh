#!/usr/bin/env bash
# Host tests for the UART link test logic. Works on Linux, WSL and MSYS2 (UCRT64).
#   bash run_host_tests.sh
# Sanitizers are used when the compiler has them (MSYS2's g++ does not; the script then runs without).
#   SANITIZE=0 bash run_host_tests.sh   to force them off
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${TMPDIR:-/tmp}/uartlink_host_$$"
mkdir -p "$OUT"
trap 'rm -rf "$OUT"' EXIT
CXX="${CXX:-g++}"

SANFLAGS="-fsanitize=address,undefined -fno-sanitize-recover=undefined"
if [ "${SANITIZE:-auto}" = 0 ] || ! echo 'int main(){return 0;}' | $CXX -x c++ $SANFLAGS - -o "$OUT/san_probe" >/dev/null 2>&1; then
    SANFLAGS=""
    echo "(sanitizers not available or disabled: running without them)"
fi

WARN="-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror"
$CXX -std=c++17 -O1 -g $SANFLAGS $WARN "$HERE/test_link.cpp" -o "$OUT/test_link"
"$OUT/test_link"
