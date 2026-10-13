#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="$HOME/clair_work/int128_v4_3_regression"

mkdir -p "$WORK"
cd "$ROOT"

passed=0
valid=0
overflow=0
division=0

compile_case() {
    local name="$1"
    local binary="$WORK/$name.bin"

    rm -f "$binary"

    if ! ./build/clair \
        "tests/int128_v4_3/$name.cl" \
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
        "tests/int128_v4_3/$name.out" \
        "$WORK/$name.actual"; then

        echo "ERROR=$name:OUTPUT_MISMATCH"
        exit 1
    fi

    valid=$((valid + 1))
    passed=$((passed + 1))
}

reject_case() {
    local name="$1"
    local marker="$2"
    local category="$3"

    compile_case "$name"

    if "$WORK/$name.bin" \
        >"$WORK/$name.stdout" \
        2>"$WORK/$name.stderr"; then

        echo "ERROR=$name:UNEXPECTED_SUCCESS"
        exit 1
    fi

    if ! grep -Fq \
        "$marker" "$WORK/$name.stderr"; then

        echo "ERROR=$name:WRONG_DIAGNOSTIC"
        exit 1
    fi

    if [ "$category" = "overflow" ]; then
        overflow=$((overflow + 1))
    else
        division=$((division + 1))
    fi

    passed=$((passed + 1))
}

positive_case add_max
positive_case sub_min
positive_case mul_max
positive_case mul_min
positive_case unary_max
positive_case unary_direct_min
positive_case nested_arithmetic
positive_case nested_negation
positive_case div_safe
reject_case add_positive_overflow 'clair runtime error: int64 overflow' overflow
reject_case add_negative_overflow 'clair runtime error: int64 overflow' overflow
reject_case sub_negative_overflow 'clair runtime error: int64 overflow' overflow
reject_case sub_positive_overflow 'clair runtime error: int64 overflow' overflow
reject_case mul_positive_overflow 'clair runtime error: int64 overflow' overflow
reject_case mul_negative_overflow 'clair runtime error: int64 overflow' overflow
reject_case unary_overflow 'clair runtime error: int64 overflow' overflow
reject_case nested_overflow 'clair runtime error: int64 overflow' overflow
reject_case int64_division_by_zero 'Clair runtime error: division by zero' division
reject_case int64_division_min_overflow 'Clair runtime error: integer division overflow' division

echo "V4_3_SAFE_INT64=$valid/9"
echo "V4_3_OVERFLOW_REJECTIONS=$overflow/8"
echo "V4_3_DIVISION_REJECTIONS=$division/2"
echo "INT128_V4_3_TESTS=$passed/19"

[ "$valid" -eq 9 ]
[ "$overflow" -eq 8 ]
[ "$division" -eq 2 ]
[ "$passed" -eq 19 ]
