#!/usr/bin/env bash
# Host tests for the pairing flow (real kkproto crypto, simulated wires). No ESP-IDF needed.
#   bash run_host_tests.sh
# Needs libsodium (preferred) or OpenSSL development files: in MSYS2 UCRT64
#   pacman -S mingw-w64-ucrt-x86_64-libsodium mingw-w64-ucrt-x86_64-openssl
# Sanitizers are used when the compiler has them (MSYS2's g++ does not; the script then runs without).
#   SANITIZE=0 bash run_host_tests.sh   to force them off
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
K="$HERE/../../../components/kkproto"
OUT="${TMPDIR:-/tmp}/pairtest_host_$$"
mkdir -p "$OUT"
trap 'rm -rf "$OUT"' EXIT
CXX="${CXX:-g++}"

SANFLAGS="-fsanitize=address,undefined -fno-sanitize-recover=undefined"
if [ "${SANITIZE:-auto}" = 0 ] || ! echo 'int main(){return 0;}' | $CXX -x c++ $SANFLAGS - -o "$OUT/san_probe" >/dev/null 2>&1; then
    SANFLAGS=""
    echo "(sanitizers not available or disabled: running without them)"
fi

have() { echo '#include <'"$1"'>' | $CXX -x c++ -E - >/dev/null 2>&1; }
if have sodium.h; then
    PORT="$K/platform/sodium/crypto_port_sodium.cpp"; LIBS="-lsodium"; NAME=sodium
elif have openssl/evp.h; then
    PORT="$K/platform/openssl/crypto_port_openssl.cpp"; LIBS="-lcrypto"; NAME=openssl
else
    echo "no crypto library found (need libsodium or OpenSSL development files)"; exit 1
fi
echo "crypto port: $NAME"

CORE="$K/src/noise.cpp $K/src/frames.cpp $K/src/messages.cpp $K/src/pairing.cpp $K/src/selftest.cpp"
$CXX -std=c++17 -O1 -g $SANFLAGS -Wall -Wextra -Werror -I"$K/include" \
    "$HERE/test_flow.cpp" $CORE "$PORT" $LIBS -o "$OUT/test_flow"
"$OUT/test_flow"
