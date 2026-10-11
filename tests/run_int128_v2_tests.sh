#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="$HOME/clair_work/int128_v2_regression"

mkdir -p "$WORK"
cd "$ROOT"

passed=0

compile_case() {
    local name="$1"

    if ! ./build/clair \
        "tests/int128_v2/$name.cl" \
        -o "$WORK/$name.bin" \
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
        "tests/int128_v2/$name.out" \
        "$WORK/$name.actual"; then
        echo "ERROR=$name:OUTPUT_MISMATCH"
        exit 1
    fi

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

    if ! grep -Fq "$marker" "$WORK/$name.stderr"; then
        echo "ERROR=$name:WRONG_RUNTIME_ERROR"
        exit 1
    fi

    passed=$((passed + 1))
}

semantic_reject_case() {
    local name="$1"
    local marker="$2"

    if ./build/clair \
        "tests/int128_v2/$name.cl" \
        -o "$WORK/$name.bin" \
        >"$WORK/$name.compile.log" 2>&1; then
        echo "ERROR=$name:UNEXPECTED_ACCEPT"
        exit 1
    fi

    if ! grep -Fq "semantic error:" \
            "$WORK/$name.compile.log" ||
       ! grep -Fq "$marker" \
            "$WORK/$name.compile.log"; then
        echo "ERROR=$name:WRONG_DIAGNOSTIC"
        exit 1
    fi

    passed=$((passed + 1))
}

float_precision_case() {
    local name="$1"

    compile_case "$name"

    if ! "$WORK/$name.bin" \
        >"$WORK/$name.actual" \
        2>"$WORK/$name.stderr"; then
        echo "ERROR=$name:RUNTIME_FAILED"
        exit 1
    fi

    if ! awk '
        NR == 1 && $0 == "1" { good1 = 1 }
        NR == 2 && $0 == "0" { good2 = 1 }
        NR == 3 {
            value = $0 + 0
            if (value > 0 && value < 2e-32)
                good3 = 1
        }
        END {
            if (NR != 3 || !good1 || !good2 || !good3)
                exit 1
        }
    ' "$WORK/$name.actual"; then
        echo "ERROR=$name:PRECISION_LOST"
        exit 1
    fi

    passed=$((passed + 1))
}

positive_case signed_arithmetic
positive_case unsigned_arithmetic
positive_case unsigned_mul_valid
positive_case signed_comparisons

float_precision_case float_arithmetic

runtime_reject_case signed_add_overflow "128-bit overflow"
runtime_reject_case signed_mul_overflow "128-bit overflow"
runtime_reject_case signed_div_overflow "128-bit division"
runtime_reject_case signed_sub_underflow "128-bit overflow"
runtime_reject_case signed_div_zero "128-bit division"

runtime_reject_case unsigned_add_overflow "128-bit overflow"
runtime_reject_case unsigned_sub_underflow "128-bit overflow"
runtime_reject_case unsigned_mul_overflow "128-bit overflow"
runtime_reject_case unsigned_div_zero "128-bit division"

semantic_reject_case mixed_types \
    "128-bit operands must have identical types"

echo "INT128_V2_TESTS=$passed/15"

[ "$passed" -eq 15 ]
