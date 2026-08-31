#!/usr/bin/env bash
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -fno-sanitize=leak,alignment}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${STANDALONE_FUZZ_MAIN:=/opt/mayhem/StandaloneFuzzTargetMain.c}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
export CC CXX

# wolfSSL is pre-baked in the image by the Dockerfile (offline-safe; no network needed here)
WOLFSSL_DIR=/opt/toolchains/wolfssl

# Defines and includes matching the test Makefile (default non-CLIENT_ONLY build)
DEF="-D_POSIX_C_SOURCE=200809L -DWOLFSSL_USER_SETTINGS -DWOLFHSM_CFG"
DEF="$DEF -DWOLFHSM_CFG_TEST_POSIX -DWOLFHSM_CFG_ENABLE_CLIENT -DWOLFHSM_CFG_ENABLE_SERVER"
INC="-I$SRC/test -I$SRC/test/config -I$WOLFSSL_DIR -I$SRC -I$SRC/port/posix"

# ---- 1. Test suite with NORMAL flags (for test.sh oracle) ----
make -C "$SRC/test" build_app -j"$MAYHEM_JOBS" \
    CC="$CC" \
    WOLFSSL_DIR="$WOLFSSL_DIR" \
    WOLFHSM_DIR="$SRC" \
    CFLAGS="${COVERAGE_FLAGS}" \
    LDFLAGS="${COVERAGE_FLAGS}" \
    NO_LSAN=1
cp "$SRC/test/Build/wh_test.elf" /mayhem/wh_test

# ---- 2. Clean and rebuild with sanitizers + SanitizerCoverage to get instrumented objects ----
# -fsanitize=fuzzer-no-link adds edge-tracing hooks to the library code so libFuzzer
# can actually track coverage inside wolfHSM/wolfSSL (without it only the 8-line harness
# gets instrumented → 3 edges → Mayhem reports 0).
make -C "$SRC/test" clean
make -C "$SRC/test" build_app -j"$MAYHEM_JOBS" \
    CC="$CC" \
    WOLFSSL_DIR="$WOLFSSL_DIR" \
    WOLFHSM_DIR="$SRC" \
    CFLAGS="$SANITIZER_FLAGS -fsanitize=fuzzer-no-link $DEBUG_FLAGS" \
    LDFLAGS="$SANITIZER_FLAGS" \
    NO_LSAN=1

# Collect instrumented objects, excluding wh_test.o (has main)
OBJS=$(find "$SRC/test/Build" -name '*.o' ! -name 'wh_test.o' | sort | tr '\n' ' ')

# ---- 3. Compile standalone driver (C object to keep C linkage for LLVMFuzzerTestOneInput) ----
"$CC" $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o /tmp/standalone_main.o

# LeakSanitizer off at build time (docs/rules/c.md): the object defines __lsan_is_turned_off
"$CXX" $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o /tmp/lsan_off.o

# ---- 4. Build each libFuzzer harness + standalone reproducer ----
for t in fuzz_wh_clientserver fuzz_wh_crypto fuzz_wh_flash_ramsim fuzz_wh_nvmflash; do
    target="${t#fuzz_}"   # wh_clientserver, wh_crypto, etc. (matches Mayhemfile target: field)
    "$CC" $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE \
        $DEF $INC "$SRC/mayhem/harness/${t}.c" $OBJS /tmp/lsan_off.o \
        -o "/mayhem/${target}"
    "$CC" $SANITIZER_FLAGS $DEBUG_FLAGS \
        $DEF $INC "$SRC/mayhem/harness/${t}.c" /tmp/standalone_main.o $OBJS /tmp/lsan_off.o \
        -o "/mayhem/${target}-standalone"
done

# ---- 5. Build example binaries (network targets) with sanitizer flags ----
for ex in wh_posix_server wh_posix_client; do
    make -C "$SRC/examples/posix/${ex}" build_app -j"$MAYHEM_JOBS" \
        CC="$CC" \
        WOLFSSL_DIR="$WOLFSSL_DIR" \
        WOLFHSM_DIR="$SRC" \
        CFLAGS="$SANITIZER_FLAGS $DEBUG_FLAGS" \
        LDFLAGS="$SANITIZER_FLAGS /tmp/lsan_off.o" \
        NO_LSAN=1
    cp "$SRC/examples/posix/${ex}/Build/${ex}.elf" "/mayhem/${ex}"
done
