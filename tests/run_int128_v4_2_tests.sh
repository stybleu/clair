#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="$HOME/clair_work/int128_v4_2_regression"

mkdir -p "$WORK"
cd "$ROOT"

passed=0
valid=0
runtime_rejected=0
semantic_rejected=0

compile_positive() {
    local name="$1"
    local binary="$WORK/$name.bin"

    rm -f "$binary"

    if ! ./build/clair \
        "tests/int128_v4_2/$name.cl" \
        -o "$binary" \
        >"$WORK/$name.compile.log" 2>&1; then

        echo "ERROR=$name:COMPILE_FAILED"
        tail -n 8 "$WORK/$name.compile.log"
        exit 1
    fi
}

positive_case() {
    local name="$1"

    compile_positive "$name"

    if ! "$WORK/$name.bin" \
        >"$WORK/$name.actual" \
        2>"$WORK/$name.stderr"; then

        echo "ERROR=$name:RUNTIME_FAILED"
        exit 1
    fi

    if ! cmp -s \
        "tests/int128_v4_2/$name.out" \
        "$WORK/$name.actual"; then

        echo "ERROR=$name:OUTPUT_MISMATCH"
        exit 1
    fi

    valid=$((valid + 1))
    passed=$((passed + 1))
}

runtime_reject_case() {
    local name="$1"
    local expected="$2"

    compile_positive "$name"

    if "$WORK/$name.bin" \
        >"$WORK/$name.stdout" \
        2>"$WORK/$name.stderr"; then

        echo "ERROR=$name:UNEXPECTED_SUCCESS"
        exit 1
    fi

    if ! grep -Fq \
        "$expected" "$WORK/$name.stderr"; then

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
        "tests/int128_v4_2/$name.cl" \
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

positive_case cast_int128_large_literal
positive_case cast_uint128_max_literal
positive_case cast_int128_from_int
positive_case cast_int_from_int128
positive_case cast_int_from_uint128
positive_case cast_int128_from_uint128
positive_case cast_uint128_from_int128
positive_case cast_uint128_from_int
positive_case cast_float128_from_float
positive_case cast_float128_from_int
positive_case cast_nested
positive_case cast_int128_min
positive_case cast_float128_precision
positive_case cast_identity
runtime_reject_case cast_int128_to_int_overflow 'numeric cast out of range'
runtime_reject_case cast_uint128_to_int_overflow 'numeric cast out of range'
runtime_reject_case cast_uint128_to_int128_overflow 'numeric cast out of range'
runtime_reject_case cast_negative_int128_to_uint128 'numeric cast out of range'
runtime_reject_case cast_negative_int_to_uint128 'numeric cast out of range'
semantic_reject_case cast_float128_to_int
semantic_reject_case cast_int128_to_float128
semantic_reject_case cast_float128_to_int128
semantic_reject_case cast_uint128_to_float128
semantic_reject_case cast_wrong_arity
semantic_reject_case cast_empty
semantic_reject_case cast_literal_out_of_range

echo "V4_2_SAFE_CASTS=$valid/14"
echo "V4_2_RANGE_REJECTIONS=$runtime_rejected/5"
echo "V4_2_SEMANTIC_REJECTIONS=$semantic_rejected/7"
echo "INT128_V4_2_TESTS=$passed/26"

[ "$valid" -eq 14 ]
[ "$runtime_rejected" -eq 5 ]
[ "$semantic_rejected" -eq 7 ]
[ "$passed" -eq 26 ]
