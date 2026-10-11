#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="$HOME/clair_work/int128_v1_regression"

mkdir -p "$WORK"
cd "$ROOT"

passed=0

compile_positive() {
    local name="$1"

    if ! ./build/clair \
        "tests/int128_v1/$name.cl" \
        -o "$WORK/$name.bin" \
        >"$WORK/$name.compile.log" 2>&1; then
        echo "ERROR=$name:COMPILE_FAILED"
        tail -n 8 "$WORK/$name.compile.log"
        exit 1
    fi

    if ! "$WORK/$name.bin" \
        >"$WORK/$name.actual" \
        2>"$WORK/$name.runtime.log"; then
        echo "ERROR=$name:RUNTIME_FAILED"
        exit 1
    fi
}

compile_positive limits

cat >"$WORK/limits.expected" <<'EXPECTED'
170141183460469231731687303715884105727
-170141183460469231731687303715884105728
340282366920938463463374607431768211455
0.5
18446744073709551616
EXPECTED

if ! cmp -s \
    "$WORK/limits.expected" "$WORK/limits.actual"; then
    echo "ERROR=LIMITS_OUTPUT"
    exit 1
fi

passed=$((passed + 1))

compile_positive precision

if ! grep -Eq \
    '^1\.0{20,}[1-9][0-9]*$' \
    "$WORK/precision.actual"; then
    echo "ERROR=FLOAT128_PRECISION"
    cat "$WORK/precision.actual"
    exit 1
fi

passed=$((passed + 1))

expect_rejection() {
    local name="$1"
    local marker="$2"

    if ./build/clair \
        "tests/int128_v1/$name.cl" \
        -o "$WORK/$name.bin" \
        >"$WORK/$name.compile.log" 2>&1; then
        echo "ERROR=$name:UNEXPECTED_ACCEPT"
        exit 1
    fi

    if ! grep -Fq "semantic error:" \
            "$WORK/$name.compile.log" ||
       ! grep -Fq "$marker" \
            "$WORK/$name.compile.log"; then
        echo "ERROR=$name:WRONG_REJECTION"
        tail -n 8 "$WORK/$name.compile.log"
        exit 1
    fi

    passed=$((passed + 1))
}

expect_rejection \
    signed_overflow \
    "128-bit integer literal out of range"

expect_rejection \
    signed_underflow \
    "128-bit integer literal out of range"

expect_rejection \
    unsigned_overflow \
    "128-bit integer literal out of range"

expect_rejection \
    unsigned_negative \
    "cannot initialize"

echo "INT128_V1_TESTS=$passed/6"
[ "$passed" -eq 6 ]
