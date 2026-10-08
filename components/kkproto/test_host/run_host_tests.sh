#!/usr/bin/env bash
# Host tests for components/kkproto. No ESP-IDF needed: g++ (C++17) plus the libraries below.
#   ./run_host_tests.sh                       libsodium (+ OpenSSL if its dev package is installed)
#   MBEDTLS_DIR=/path/to/mbedtls-3.6.x ./run_host_tests.sh   also the PSA port (what ESP-IDF uses);
#       build mbedtls first:  cmake -S . -B build -DENABLE_PROGRAMS=OFF -DENABLE_TESTING=OFF \
#                             -DUSE_SHARED_MBEDTLS_LIBRARY=OFF && cmake --build build --target mbedcrypto
#   RUN_MUTATIONS=1 ./run_host_tests.sh       additionally plants bugs and checks the tests go red
# Windows: WSL or MSYS2.
set -euo pipefail
cd "$(dirname "$0")"
K=..
CXX="${CXX:-g++}"
OUT=$(mktemp -d)
CORE="$K/src/noise.cpp $K/src/frames.cpp $K/src/messages.cpp $K/src/pairing.cpp $K/src/selftest.cpp $K/src/link.cpp"
BASE="-std=c++17 -Wall -Wextra -Werror -I$K/include -I."
# Sanitizers (memory / undefined-behaviour checks) are used when the compiler has them. The g++ that
# ships with MSYS2/MinGW on Windows does not, so the script quietly runs without them there.
# Force it off with:  SANITIZE=0 ./run_host_tests.sh
SANFLAGS="-fsanitize=address,undefined -fno-sanitize-recover=undefined"
if [ "${SANITIZE:-auto}" = 0 ] || ! echo 'int main(){return 0;}' | $CXX -x c++ $SANFLAGS - -o "$OUT/san_probe" >/dev/null 2>&1; then
    SANFLAGS=""
    echo "(sanitizers not available or disabled: running without them)"
fi
SAN="-O1 -g $SANFLAGS"
FAST="-O2"

declare -a PORTS
have() { echo '#include <'"$1"'>' | $CXX -x c++ -E - >/dev/null 2>&1; }
if have sodium.h; then PORTS+=(sodium); fi
if have openssl/evp.h; then PORTS+=(openssl); fi
if [ -n "${MBEDTLS_DIR:-}" ]; then PORTS+=(psa); fi
[ ${#PORTS[@]} -gt 0 ] || { echo "no crypto library found (need libsodium-dev and/or libssl-dev)"; exit 1; }

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

port_src() { echo "$K/platform/$1/crypto_port_$1.cpp"; }
port_flags() {
    case "$1" in
        sodium)  echo "-lsodium" ;;
        openssl) echo "-lcrypto" ;;
        psa)     echo "-I$MBEDTLS_DIR/include $MBEDTLS_DIR/build/library/libmbedcrypto.a" ;;
    esac
}

for p in "${PORTS[@]}"; do
    echo "=========== crypto_port: $p"
    # Sanitizers on the libsodium build (fast); the others just prove the port is right.
    OPT="$FAST"; [ "$p" = sodium ] && OPT="$SAN"
    PF=$(port_flags "$p"); PS=$(port_src "$p")
    $CXX $BASE $OPT test_noise_vectors.cpp $CORE $PS $PF -o "$OUT/vec_$p"
    "$OUT/vec_$p"
    $CXX $BASE $OPT test_protocol.cpp $CORE $PS $PF -o "$OUT/proto_$p"
    "$OUT/proto_$p"
    $CXX $BASE $OPT test_link.cpp $CORE $PS $PF -o "$OUT/link_$p"
    "$OUT/link_$p"
    if [ "$p" = sodium ]; then
        $CXX $BASE $OPT test_codec.cpp $CORE $PS $PF -o "$OUT/codec_$p"
        "$OUT/codec_$p"
    fi
    $CXX $BASE $FAST transcript.cpp $CORE $PS $PF -o "$OUT/tr_$p"
    "$OUT/tr_$p" > "$OUT/tr_$p.txt"
done

echo "=========== transcripts of all ports must be byte-identical"
FIRST="${PORTS[0]}"
build_filecmp
LINES=""
for p in "${PORTS[@]}"; do
    set +e
    RESULT=$("$OUT/filecmp" "$OUT/tr_$FIRST.txt" "$OUT/tr_$p.txt"); RC=$?
    set -e
    if [ "$RC" = 1 ]; then echo "MISMATCH between $FIRST and $p:"; echo "$RESULT"; exit 1; fi
    if [ "$RC" != 0 ]; then echo "COULD NOT COMPARE $FIRST and $p: $RESULT"; exit 2; fi
    LINES="$RESULT"
done
echo "OK: ${PORTS[*]} produce the same $LINES transcript lines"

if have sodium.h; then
    echo "=========== defences against deliberately broken ports"
    $CXX $BASE $SAN -DLEAKY test_core_checks.cpp $K/src/noise.cpp $K/src/pairing.cpp $K/src/selftest.cpp broken_ports/leaky_dh_port.cpp -lsodium -o "$OUT/leaky"
    "$OUT/leaky"
    $CXX $BASE $SAN -DNOAUTH test_core_checks.cpp $K/src/noise.cpp $K/src/pairing.cpp $K/src/selftest.cpp broken_ports/no_auth_port.cpp -lsodium -o "$OUT/noauth"
    "$OUT/noauth"
fi

if [ "${RUN_MUTATIONS:-0}" = 1 ]; then
    echo "=========== mutation check"
    python3 mutation_check.py
    python3 mutation_check_link.py
fi
echo "ALL OK"
