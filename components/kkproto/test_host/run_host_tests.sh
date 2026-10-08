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
CORE="$K/src/noise.cpp $K/src/frames.cpp $K/src/messages.cpp $K/src/pairing.cpp $K/src/selftest.cpp"
BASE="-std=c++17 -Wall -Wextra -Werror -I$K/include -I."
SAN="-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined"
FAST="-O2"

declare -a PORTS
have() { echo '#include <'"$1"'>' | $CXX -x c++ -E - >/dev/null 2>&1; }
if have sodium.h; then PORTS+=(sodium); fi
if have openssl/evp.h; then PORTS+=(openssl); fi
if [ -n "${MBEDTLS_DIR:-}" ]; then PORTS+=(psa); fi
[ ${#PORTS[@]} -gt 0 ] || { echo "no crypto library found (need libsodium-dev and/or libssl-dev)"; exit 1; }

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
    if [ "$p" = sodium ]; then
        $CXX $BASE $OPT test_codec.cpp $CORE $PS $PF -o "$OUT/codec_$p"
        "$OUT/codec_$p"
    fi
    $CXX $BASE $FAST transcript.cpp $CORE $PS $PF -o "$OUT/tr_$p"
    "$OUT/tr_$p" > "$OUT/tr_$p.txt"
done

echo "=========== transcripts of all ports must be byte-identical"
FIRST="${PORTS[0]}"
for p in "${PORTS[@]}"; do
    cmp -s "$OUT/tr_$FIRST.txt" "$OUT/tr_$p.txt" || { echo "MISMATCH between $FIRST and $p:"; diff "$OUT/tr_$FIRST.txt" "$OUT/tr_$p.txt"; exit 1; }
done
echo "OK: ${PORTS[*]} produce the same $(wc -l < "$OUT/tr_$FIRST.txt") transcript lines"

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
fi
echo "ALL OK"
