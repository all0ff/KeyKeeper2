#!/usr/bin/env bash
# Host tests for components/usb (no ESP-IDF needed). Needs g++ (C++17).
#   ./run_host_tests.sh [cases]        (Windows: WSL or MSYS2)
# 1. unit tests of plan_events() and run_plan();
# 2. differential test: the ORIGINAL type_engine.cpp (oracle/) and the new TypeEngine
#    must produce byte-identical call traces for the same random inputs.
set -euo pipefail
cd "$(dirname "$0")"
CASES="${1:-20000}"
USB=..
OUT=$(mktemp -d)
CXX="${CXX:-g++}"
# Sanitizers (memory / undefined-behaviour checks) are used when the compiler has them. The g++ that
# ships with MSYS2/MinGW on Windows does not, so the script quietly runs without them there.
# Force it off with:  SANITIZE=0 ./run_host_tests.sh
SANFLAGS="-fsanitize=address,undefined -fno-sanitize-recover=undefined"
if [ "${SANITIZE:-auto}" = 0 ] || ! echo 'int main(){return 0;}' | $CXX -x c++ $SANFLAGS - -o "$OUT/san_probe" >/dev/null 2>&1; then
    SANFLAGS=""
    echo "(sanitizers not available or disabled: running without them)"
fi
FLAGS="-std=c++17 -O1 -g -Wall -Wextra $SANFLAGS"
SHIM="-Ishim -I$USB/include"
REAL="$USB/src/keycode_map.cpp $USB/src/cyrillic_layout.cpp fake_hid.cpp"

# A tiny file comparer, compiled here, so the script needs neither cmp, diff nor wc
# (none of them is installed in a minimal MSYS2). Exit 0 same / 1 different / 2 could not compare.
build_filecmp() {
    cat > "$OUT/filecmp.cpp" << 'FILECMP_EOF'
// Tiny file comparer so the test script does not depend on cmp/diff/wc (a minimal MSYS2 has none of them).
// exit 0: identical (prints the line count)   1: different (prints the first difference)   2: cannot compare
#include <cstdio>
#include <fstream>
#include <string>

int main(int argc, char** argv)
{
    if (argc != 3) {
        std::printf("usage: filecmp <a> <b>\n");
        return 2;
    }
    std::ifstream a(argv[1], std::ios::binary), b(argv[2], std::ios::binary);
    if (!a || !b) {
        std::printf("cannot open %s or %s\n", argv[1], argv[2]);
        return 2;
    }
    std::string x, y;
    long line = 0;
    for (;;) {
        const bool ga = static_cast<bool>(std::getline(a, x));
        const bool gb = static_cast<bool>(std::getline(b, y));
        if (!ga && !gb) {
            std::printf("%ld\n", line);
            return 0;
        }
        ++line;
        if (ga != gb || x != y) {
            std::printf("first difference at line %ld\n  first : %s\n  second: %s\n", line,
                        ga ? x.substr(0, 300).c_str() : "(end of file)", gb ? y.substr(0, 300).c_str() : "(end of file)");
            return 1;
        }
    }
}
FILECMP_EOF
    $CXX -O1 "$OUT/filecmp.cpp" -o "$OUT/filecmp"
}

echo "== unit tests"
$CXX $FLAGS -Werror $SHIM -I. test_plan.cpp $USB/src/typing_plan.cpp $USB/src/typing_runner.cpp $REAL -o "$OUT/unit"
"$OUT/unit"

echo "== wire path (radio dongle): plan -> wire -> executor must behave like the local executor, failures included"
KK=$USB/../kkproto
$CXX $FLAGS -Werror $SHIM -I$KK/include -I. test_wire.cpp $USB/src/typing_plan.cpp $USB/src/typing_runner.cpp $USB/src/plan_wire.cpp \
     $USB/src/wire_executor.cpp $USB/src/dongle_sink.cpp $USB/src/keycode_map.cpp $USB/src/cyrillic_layout.cpp $KK/src/messages.cpp -o "$OUT/wire"
"$OUT/wire" "$CASES"
if command -v python3 >/dev/null 2>&1 && [ "${MUTATION:-0}" = 1 ]; then python3 mutation_check_wire.py; fi

echo "== Russian-layout punctuation: plan played through an independent host model"
$CXX $FLAGS -Werror $SHIM -I. test_ru_punct.cpp $USB/src/typing_plan.cpp $USB/src/keycode_map.cpp $USB/src/cyrillic_layout.cpp -o "$OUT/ru_punct"
"$OUT/ru_punct"

echo "== API compatibility with usb_service.cpp (compile only)"
$CXX -std=c++17 -Wall -Wextra -Werror $SHIM -c api_compat.cpp -o /dev/null

echo "== differential: oracle (original type_engine.cpp) vs new"
# the original header relies on <cstdint> arriving transitively (true with ESP-IDF); keep the oracle verbatim and force it here
$CXX $FLAGS -Ioracle $SHIM -I. -include cstdint trace_main.cpp oracle/type_engine_old.cpp $REAL -o "$OUT/old"
$CXX $FLAGS -Werror $SHIM -I. trace_main.cpp $USB/src/type_engine.cpp $USB/src/typing_plan.cpp \
     $USB/src/typing_runner.cpp $USB/src/cable_sink.cpp $REAL -o "$OUT/new"
"$OUT/old" "$CASES" > "$OUT/old.txt"
"$OUT/new" "$CASES" > "$OUT/new.txt"
build_filecmp
set +e
RESULT=$("$OUT/filecmp" "$OUT/old.txt" "$OUT/new.txt"); RC=$?
set -e
if [ "$RC" = 0 ]; then
    echo "OK: $RESULT identical trace lines ($CASES cases x {type_string, type_char})"
elif [ "$RC" = 1 ]; then
    echo "MISMATCH (the new code behaves differently from the original):"; echo "$RESULT"; exit 1
else
    echo "COULD NOT COMPARE the two traces: $RESULT"; exit 2
fi
