#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK="$HOME/clair_work/simd_v4_safety_regression"
mkdir -p "$WORK"

for n in boundary_16 boundary_17 negative_index dynamic_append; do
    if ! "$ROOT/build/clair" \
        "$ROOT/tests/simd_v4_safety_${n}.cl" \
        -o "$WORK/$n" >"$WORK/$n.compile.log" 2>&1; then
        tail -n 12 "$WORK/$n.compile.log"
        exit 1
    fi
done

check_output() {
    local n="$1"
    shift

    printf '%s\n' "$@" > "$WORK/$n.expected"

    if ! "$WORK/$n" >"$WORK/$n.out" 2>"$WORK/$n.err"; then
        cat "$WORK/$n.err"
        return 1
    fi

    if ! cmp -s "$WORK/$n.expected" "$WORK/$n.out"; then
        diff -u "$WORK/$n.expected" "$WORK/$n.out" || true
        return 1
    fi
}

check_output boundary_16 \
    16 -9223372036854775808 -9223372036854775807 16

check_output boundary_17 \
    17 9223372036854775807 -9223372036854775807 17

check_output dynamic_append 17 17 78

if "$WORK/negative_index" \
    >"$WORK/negative_index.out" \
    2>"$WORK/negative_index.err"; then
    echo "NEGATIVE_INDEX=UNEXPECTED_SUCCESS"
    exit 1
fi

if [ ! -s "$WORK/negative_index.err" ]; then
    echo "NEGATIVE_INDEX=MISSING_RUNTIME_ERROR"
    exit 1
fi

echo "SIMD_V4_SAFETY=PASS"
echo "SAFETY_CASES=4"
