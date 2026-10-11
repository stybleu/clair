#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="$HOME/clair_work/int128_v3_1_regression"

mkdir -p "$WORK"
cd "$ROOT"

passed=0
valid=0
rejected=0

positive_case() {
    local name="$1"
    local binary="$WORK/$name.bin"

    rm -f "$binary"

    if ! ./build/clair \
        "tests/int128_v3_1/$name.cl" \
        -o "$binary" \
        >"$WORK/$name.compile.log" 2>&1; then
        echo "ERROR=$name:COMPILE_FAILED"
        tail -n 8 "$WORK/$name.compile.log"
        exit 1
    fi

    if ! "$binary" \
        >"$WORK/$name.actual" \
        2>"$WORK/$name.stderr"; then
        echo "ERROR=$name:RUNTIME_FAILED"
        exit 1
    fi

    if ! cmp -s \
        "tests/int128_v3_1/$name.out" \
        "$WORK/$name.actual"; then
        echo "ERROR=$name:OUTPUT_MISMATCH"
        exit 1
    fi

    valid=$((valid + 1))
    passed=$((passed + 1))
}

negative_case() {
    local name="$1"
    local binary="$WORK/$name.bin"

    rm -f "$binary"

    if ./build/clair \
        "tests/int128_v3_1/$name.cl" \
        -o "$binary" \
        >"$WORK/$name.compile.log" 2>&1; then
        echo "ERROR=$name:UNEXPECTED_ACCEPT"
        exit 1
    fi

    if ! grep -Fq \
        "semantic error:" \
        "$WORK/$name.compile.log"; then
        echo "ERROR=$name:NOT_SEMANTIC_REJECTION"
        exit 1
    fi

    if ! grep -Fq \
        "64-bit signed integer literal out of range" \
        "$WORK/$name.compile.log"; then
        echo "ERROR=$name:WRONG_DIAGNOSTIC"
        exit 1
    fi

    rejected=$((rejected + 1))
    passed=$((passed + 1))
}

positive_case int64_max
positive_case int64_min
positive_case int64_min_zeros
positive_case int128_min_zeros
positive_case int128_max_zeros
positive_case uint128_max_zeros
positive_case decimal_leading_zeros
positive_case untyped_int64_min
negative_case int64_positive_overflow
negative_case int64_negative_underflow
negative_case untyped_huge_print
negative_case int64_overflow_zeros
negative_case int64_underflow_zeros
negative_case untyped_int64_overflow

echo "V3_1_VALID_LITERALS=$valid/8"
echo "V3_1_OVERFLOW_REJECTIONS=$rejected/6"
echo "INT128_V3_1_TESTS=$passed/14"

[ "$valid" -eq 8 ]
[ "$rejected" -eq 6 ]
[ "$passed" -eq 14 ]
