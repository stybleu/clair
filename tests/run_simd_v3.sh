#!/bin/sh
set -eu

cd "$(dirname "$0")/.." || exit 1

WORK="$HOME/clair_work/simd_v3_permanent"
mkdir -p "$WORK"

fail() {
    echo "SIMD_TEST_FAIL=$1" >&2
    exit 1
}

compile_case() {
    name="$1"

    if ! ./build/clair \
        "tests/$name.cl" \
        -o "build/test_$name" \
        >"$WORK/$name.compile.log" 2>&1; then

        tail -n 10 "$WORK/$name.compile.log"
        fail "$name"
    fi
}

compile_case simd_v3_int_sum
compile_case simd_v3_float_map

INT_BIN="./build/test_simd_v3_int_sum"
FLOAT_BIN="./build/test_simd_v3_float_map"

"$INT_BIN" >"$WORK/int.stdout" 2>"$WORK/int.stderr" \
    || fail INT_EXEC

printf '523776\n' | cmp -s - "$WORK/int.stdout" \
    || fail INT_RESULT

echo "INT_SUM=OK"

"$FLOAT_BIN" >"$WORK/float.stdout" \
    2>"$WORK/float.stderr" \
    || fail FLOAT_EXEC

awk '
NR == 1 && (NF != 1 || $1 != "1024") {exit 1}
NR == 2 && (NF != 1 || $1 + 0 != 1) {exit 1}
NR == 3 && (NF != 1 || $1 + 0 != 1024) {exit 1}
END {if (NR != 3) exit 1}
' "$WORK/float.stdout" || fail FLOAT_RESULT

echo "FLOAT_MAP_ALIAS_APPEND=OK"

INT_C="build/test_simd_v3_int_sum.generated.c"
FLOAT_C="build/test_simd_v3_float_map.generated.c"

grep -Fq \
    'clair_rt_list_at_i64_checked(values,' \
    "$INT_C" || fail INT_CODEGEN

grep -Fq \
    'clair_rt_list_at_f64_checked(values,' \
    "$FLOAT_C" || fail FLOAT_CODEGEN

echo "TYPED_CODEGEN=OK"

for name in \
    simd_v3_negative_read \
    simd_v3_negative_write \
    simd_v3_bounds_write \
    simd_v3_empty_read
do
    compile_case "$name"

    if "./build/test_$name" \
        >"$WORK/$name.stdout" \
        2>"$WORK/$name.stderr"; then
        fail "$name:UNEXPECTED_SUCCESS"
    fi

    grep -Fq 'Clair runtime error:' \
        "$WORK/$name.stderr" \
        || fail "$name:RUNTIME_MESSAGE"

    grep -Fq 'out of bounds' \
        "$WORK/$name.stderr" \
        || fail "$name:BOUNDS_MESSAGE"

    echo "$name=OK"
done

if [ "${CLAIR_REQUIRE_NEON:-0}" = "1" ]; then
    ARCH="$(uname -m)"

    [ "$ARCH" = "aarch64" ] \
        || fail "NEON_UNSUPPORTED_ARCH:$ARCH"

    clang -O3 -fno-inline -S "$FLOAT_C" \
        -o "$WORK/float_O3.s" \
        >"$WORK/float_asm.log" 2>&1 \
        || fail FLOAT_ASSEMBLY

    clang -O3 -fno-inline -S "$INT_C" \
        -o "$WORK/int_O3.s" \
        >"$WORK/int_asm.log" 2>&1 \
        || fail INT_ASSEMBLY

    awk '
    /^clair_fn_map_float:/ {inside=1}
    inside && /^[[:space:]]*\.size[[:space:]]+clair_fn_map_float/ {
        exit
    }
    inside {print}
    ' "$WORK/float_O3.s" >"$WORK/float_function.s"

    awk '
    /^clair_fn_sum_int:/ {inside=1}
    inside && /^[[:space:]]*\.size[[:space:]]+clair_fn_sum_int/ {
        exit
    }
    inside {print}
    ' "$WORK/int_O3.s" >"$WORK/int_function.s"

    grep -Eq \
        '^[[:space:]]*fadd[[:space:]]+v[0-9]+\.(2d|4s)' \
        "$WORK/float_function.s" \
        || fail FLOAT_NEON

    grep -Eq \
        '^[[:space:]]*add[[:space:]]+v[0-9]+\.2d' \
        "$WORK/int_function.s" \
        || fail INT_NEON

    echo "FLOAT_NEON=YES"
    echo "INT_NEON=YES"
fi

if [ "${CLAIR_REQUIRE_UBSAN:-0}" = "1" ]; then
    bash tests/run_simd_v3_ubsan.sh
fi

# CLAIR_SIMD_V3_MUTATION_TESTS
bash tests/run_simd_v3_mutations.sh

# CLAIR_SIMD_V3_ASAN
if [ "${CLAIR_REQUIRE_ASAN:-0}" = "1" ]; then
    bash tests/run_simd_v3_asan.sh
fi

echo "SIMD_V3_REGRESSION=OK"
