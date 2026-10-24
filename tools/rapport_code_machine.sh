#!/usr/bin/env bash
# Compares programs compiled by Clair with the same programs written by hand
# in C, and checks whether Clang vectorized each hot loop.  Prints one line
# per program.
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BENCH="$ROOT/tools/bench"
WORK="${TMPDIR:-/tmp}/clair_rapport"
mkdir -p "$WORK"

case "$(uname -m)" in
    aarch64|arm64) SIMD_NAME="NEON"; SIMD_RE='\bv[0-9]+\.(2d|4s|16b|8h)' ;;
    x86_64)        SIMD_NAME="SSE/AVX"; SIMD_RE='(mul|add|sub|fmadd[0-9]*|fmadd)pd|paddq|vpadd' ;;
    *)             SIMD_NAME="SIMD"; SIMD_RE='^$' ;;
esac

seconds() {
    local start end
    start=$EPOCHREALTIME
    "$@" > /dev/null 2>&1
    end=$EPOCHREALTIME
    awk -v s="$start" -v e="$end" 'BEGIN { printf "%.3f", e - s }'
}

best_of_3() {
    local best="" t
    for _ in 1 2 3; do
        t=$(seconds "$@")
        if [ -z "$best" ] || awk -v a="$t" -v b="$best" 'BEGIN { exit !(a < b) }'; then best=$t; fi
    done
    printf '%s' "$best"
}

printf 'RAPPORT CODE MACHINE (%s, %s)\n' "$(uname -m)" "$(clang --version | head -n 1 | sed 's/ (.*//')"

for name in collatz dot axpy matmul; do
    kernel=$(grep -m 1 -oE '^def [a-z_]+' "$BENCH/$name.cl" | cut -d' ' -f2)

    if ! "$ROOT/build/clair" "$BENCH/$name.cl" -o "$WORK/$name" > "$WORK/$name.log" 2>&1; then
        printf '%-8s : ECHEC de compilation Clair\n' "$name"
        continue
    fi
    clang -std=c11 -O2 -fwrapv "$BENCH/${name}_ref.c" -o "$WORK/${name}_ref" 2>> "$WORK/$name.log"

    # Keep the hot function out of line so its machine code can be read alone.
    sed -E "s/^static ([a-z0-9_]+) clair_fn_${kernel}\(/static __attribute__((noinline)) \1 clair_fn_${kernel}(/" \
        "$WORK/$name.generated.c" > "$WORK/$name.noinline.c"
    clang -std=c11 -O2 -fwrapv -S "$WORK/$name.noinline.c" -o "$WORK/$name.s" 2>> "$WORK/$name.log"
    vec=$(awk -v f="clair_fn_${kernel}" '$0 ~ "^"f":" {p=1} p {print} p && /\.size|^\.Lfunc_end/ {exit}' "$WORK/$name.s" |
          grep -cE "$SIMD_RE")
    if [ "$vec" -gt 0 ]; then vtxt="oui"; else vtxt="non"; fi

    out_clair=$("$WORK/$name"); out_ref=$("$WORK/${name}_ref")
    same="meme resultat"
    [ "$out_clair" = "$out_ref" ] || same="RESULTAT DIFFERENT"

    t_clair=$(best_of_3 "$WORK/$name")
    t_ref=$(best_of_3 "$WORK/${name}_ref")
    printf '%-8s : %s %s | Clair %ss  C main %ss | %s\n' \
        "$name" "$SIMD_NAME" "$vtxt" "$t_clair" "$t_ref" "$same"
done
