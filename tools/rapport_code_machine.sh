#!/usr/bin/env bash
# Compares programs compiled by Clair with the same programs written by hand
# in C: was the hot loop vectorized in each version, and how fast are they.
# Clair and C are run alternately 5 times and the best time of each is kept,
# so both are measured under the same conditions (a phone slows down when hot).
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BENCH="$ROOT/tools/bench"
WORK="${TMPDIR:-/tmp}/clair_rapport"
ROUNDS=5
mkdir -p "$WORK"

case "$(uname -m)" in
    aarch64|arm64) SIMD_NAME="NEON"; SIMD_RE='\bv[0-9]+\.(2d|4s|16b|8h)' ;;
    x86_64)        SIMD_NAME="SSE"; SIMD_RE='(mul|add|sub|fmadd[0-9]*|fmadd)pd|paddq|vpadd' ;;
    *)             SIMD_NAME="SIMD"; SIMD_RE='^$' ;;
esac

seconds() {
    local start end
    start=$EPOCHREALTIME
    "$@" > /dev/null 2>&1
    end=$EPOCHREALTIME
    awk -v s="$start" -v e="$end" 'BEGIN { printf "%.4f", e - s }'
}

min_of() { awk -v a="$1" -v b="$2" 'BEGIN { print (b == "" || a < b) ? a : b }'; }

# "oui" when the function's machine code contains SIMD instructions
vectorized() {
    local asm="$1" fn="$2" count
    count=$(awk -v f="$fn" '$0 ~ "^"f":" {p=1} p {print} p && /\.size|^\.Lfunc_end/ {exit}' "$asm" | grep -cE "$SIMD_RE")
    if [ "$count" -gt 0 ]; then printf 'oui'; else printf 'non'; fi
}

version=$(clang --version | head -n 1 | sed -E 's/.*clang version ([0-9.]+).*/clang \1/')
printf 'RAPPORT CODE MACHINE (%s, %s)\n' "$(uname -m)" "$version"
printf '%-9s %-11s %-7s %-7s %s\n' "programme" "$SIMD_NAME Cl/C" "Clair" "C" "ecart"

for name in collatz dot axpy while matmul matrice reseau; do
    # hot function: "// noyau: name" in the program, otherwise its first def
    kernel=$(grep -m 1 -oE '^// noyau: [A-Za-z_]+' "$BENCH/$name.cl" | cut -d' ' -f3)
    [ -n "$kernel" ] || kernel=$(grep -m 1 -oE '^def [a-z_]+' "$BENCH/$name.cl" | cut -d' ' -f2)
    ref_kernel=$(grep -m 1 -oE '^/\* noyau: [A-Za-z_]+' "$BENCH/${name}_ref.c" | cut -d' ' -f3)

    if ! "$ROOT/build/clair" "$BENCH/$name.cl" -o "$WORK/$name" > "$WORK/$name.log" 2>&1; then
        printf '%-9s ECHEC de compilation Clair\n' "$name"
        continue
    fi
    clang -std=c11 -O2 -fwrapv -fno-math-errno "$BENCH/${name}_ref.c" -o "$WORK/${name}_ref" -lm 2>> "$WORK/$name.log"

    # Keep the hot functions out of line so their machine code can be read alone.
    sed -E "s/^static ([a-z0-9_]+) clair_fn_${kernel}\(/static __attribute__((noinline)) \1 clair_fn_${kernel}(/" \
        "$WORK/$name.generated.c" > "$WORK/$name.noinline.c"
    clang -std=c11 -O2 -fwrapv -fno-math-errno -S "$WORK/$name.noinline.c" -o "$WORK/$name.s" 2>> "$WORK/$name.log"
    sed -E "s/^static ([a-z0-9_]+) ${ref_kernel}\(/static __attribute__((noinline)) \1 ${ref_kernel}(/" \
        "$BENCH/${name}_ref.c" > "$WORK/${name}_ref.noinline.c"
    clang -std=c11 -O2 -fwrapv -fno-math-errno -S "$WORK/${name}_ref.noinline.c" -o "$WORK/${name}_ref.s" 2>> "$WORK/$name.log"
    v_clair=$(vectorized "$WORK/$name.s" "clair_fn_${kernel}")
    v_ref=$(vectorized "$WORK/${name}_ref.s" "${ref_kernel}")

    out_clair=$("$WORK/$name"); out_ref=$("$WORK/${name}_ref")
    same=""
    [ "$out_clair" = "$out_ref" ] || same=" RESULTAT DIFFERENT"

    t_clair=""; t_ref=""
    for _ in $(seq "$ROUNDS"); do
        t_clair=$(min_of "$(seconds "$WORK/$name")" "$t_clair")
        t_ref=$(min_of "$(seconds "$WORK/${name}_ref")" "$t_ref")
    done
    gap=$(awk -v a="$t_clair" -v b="$t_ref" 'BEGIN { printf "%+.0f%%", (a / b - 1) * 100 }')
    printf '%-9s %-11s %-7s %-7s %s%s\n' "$name" "$v_clair/$v_ref" \
        "$(printf '%.3f' "$t_clair")" "$(printf '%.3f' "$t_ref")" "$gap" "$same"
done
