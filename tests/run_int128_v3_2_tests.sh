#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="$HOME/clair_work/int128_v3_2_regression"

mkdir -p "$WORK"
cd "$ROOT"

passed=0
valid=0
runtime_rejected=0
semantic_rejected=0

compile_case() {
    local name="$1"
    local binary="$WORK/$name.bin"

    rm -f "$binary"

    if ! ./build/clair \
        "tests/int128_v3_2/$name.cl" \
        -o "$binary" \
        >"$WORK/$name.compile.log" 2>&1; then
        echo "ERROR=$name:COMPILE_FAILED"
        tail -n 8 "$WORK/$name.compile.log"
        exit 1
    fi
}

positive_case() {
    local name="$1"

    compile_case "$name"

    if ! "$WORK/$name.bin" \
        >"$WORK/$name.actual" \
        2>"$WORK/$name.stderr"; then
        echo "ERROR=$name:RUNTIME_FAILED"
        exit 1
    fi

    if ! cmp -s \
        "tests/int128_v3_2/$name.out" \
        "$WORK/$name.actual"; then
        echo "ERROR=$name:OUTPUT_MISMATCH"
        exit 1
    fi

    valid=$((valid + 1))
    passed=$((passed + 1))
}

runtime_reject_case() {
    local name="$1"
    local marker="$2"

    compile_case "$name"

    if "$WORK/$name.bin" \
        >"$WORK/$name.stdout" \
        2>"$WORK/$name.stderr"; then
        echo "ERROR=$name:UNEXPECTED_SUCCESS"
        exit 1
    fi

    if ! grep -Fq \
        "$marker" "$WORK/$name.stderr"; then
        echo "ERROR=$name:WRONG_RUNTIME_DIAGNOSTIC"
        exit 1
    fi

    runtime_rejected=$((runtime_rejected + 1))
    passed=$((passed + 1))
}

semantic_reject_case() {
    local name="$1"
    local binary="$WORK/$name.bin"

    rm -f "$binary"

    if ./build/clair \
        "tests/int128_v3_2/$name.cl" \
        -o "$binary" \
        >"$WORK/$name.compile.log" 2>&1; then
        echo "ERROR=$name:UNEXPECTED_ACCEPT"
        exit 1
    fi

    if ! grep -Fq \
        "semantic error:" \
        "$WORK/$name.compile.log"; then
        echo "ERROR=$name:WRONG_REJECTION"
        tail -n 8 "$WORK/$name.compile.log"
        exit 1
    fi

    semantic_rejected=$((semantic_rejected + 1))
    passed=$((passed + 1))
}

positive_case widen_int_to_int128
positive_case widen_negative_int
positive_case widen_float_to_float128
positive_case signed_128_plus_literal
positive_case signed_literal_left
positive_case signed_negative_literal
positive_case unsigned_128_plus_literal
positive_case float128_plus_literal
positive_case signed_unary_variable
positive_case float128_unary_variable
positive_case signed_nested_arithmetic
positive_case signed_unary_max
runtime_reject_case signed_min_unary_variable '128-bit overflow'
semantic_reject_case unsigned_unary_minus
semantic_reject_case narrow_int128
semantic_reject_case narrow_float128
semantic_reject_case mixed_signed_unsigned
semantic_reject_case negative_to_uint128
semantic_reject_case signed_literal_overflow

echo "V3_2_SAFE_OPERATIONS=$valid/12"
echo "V3_2_UNARY_OVERFLOW=$runtime_rejected/1"
echo "V3_2_UNSAFE_REJECTIONS=$semantic_rejected/6"
echo "INT128_V3_2_TESTS=$passed/19"

[ "$valid" -eq 12 ]
[ "$runtime_rejected" -eq 1 ]
[ "$semantic_rejected" -eq 6 ]
[ "$passed" -eq 19 ]
