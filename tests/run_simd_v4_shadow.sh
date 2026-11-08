#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK="$HOME/clair_work/simd_v4_shadow_regression"

mkdir -p "$WORK"

if ! "$ROOT/build/clair" \
    "$ROOT/tests/simd_v4_shadow.cl" \
    -o "$WORK/shadow" \
    >"$WORK/compile.log" 2>&1; then
    tail -n 15 "$WORK/compile.log"
    exit 1
fi

if ! "$WORK/shadow" >"$WORK/actual" 2>"$WORK/runtime.log"; then
    tail -n 15 "$WORK/runtime.log"
    exit 1
fi

printf '%s\n' \
    -1 15 1 15 \
    16 16 2 17 \
    17 17 2 18 \
    256 256 2 257 \
    > "$WORK/expected"

if cmp -s "$WORK/expected" "$WORK/actual"; then
    echo "SIMD_V4_SHADOW=OK"
else
    echo "SIMD_V4_SHADOW=FAILED"
    diff -u "$WORK/expected" "$WORK/actual" || true
    exit 1
fi
