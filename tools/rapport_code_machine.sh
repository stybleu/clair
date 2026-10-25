#!/usr/bin/env bash
# Compares programs compiled by Clair with the same programs written by hand
# in C: was the hot loop vectorized in each version, and how fast are they.
# Clair and C are run alternately 5 times and the median time of each is kept,
# so both are measured under the same conditions (a phone slows down when hot)
# and one lucky run (memory placed favourably by chance) does not decide.
# Every run is pinned to the same core, the fastest one (tools/pin_core.c):
# otherwise the phone may give the fast core to one program and not the other.
# A last table runs the matrix programs on every core and gives the gain.
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

# Same core for every run, when the system lets us choose it.
PIN=()
CORE_INFO="coeur non choisi (epinglage refuse)"
if clang -O2 "$ROOT/tools/pin_core.c" -o "$WORK/pin_core" 2> "$WORK/pin_core.log" &&
   best=$("$WORK/pin_core" best 2>> "$WORK/pin_core.log") &&
   "$WORK/pin_core" "${best%% *}" true 2>> "$WORK/pin_core.log"; then
    PIN=("$WORK/pin_core" "${best%% *}")
    CORE_INFO=$(awk -v c="${best%% *}" -v k="${best##* }" \
        'BEGIN { printf "coeur %d", c; if (k > 0) printf " (%.2f GHz)", k / 1e6; printf " pour Clair et C" }')
fi

median() { printf '%s\n' "$@" | sort -n | awk '{ v[NR] = $1 } END { print (NR % 2) ? v[(NR + 1) / 2] : (v[NR / 2] + v[NR / 2 + 1]) / 2 }'; }

# "oui" when the machine code of these functions contains SIMD instructions
vectorized() {
    local asm="$1" fn count=0
    shift
    for fn in "$@"; do
        count=$((count + $(awk -v f="$fn" '$0 ~ "^"f":" {p=1} p {print} p && /\.size|^\.Lfunc_end/ {exit}' "$asm" | grep -cE "$SIMD_RE")))
    done
    if [ "$count" -gt 0 ]; then printf 'oui'; else printf 'non'; fi
}

version=$(clang --version | head -n 1 | sed -E 's/.*clang version ([0-9.]+).*/clang \1/')
printf 'RAPPORT CODE MACHINE (%s, %s)\n' "$(uname -m)" "$version"
printf '%s\n' "$CORE_INFO"
printf '%-9s %-11s %-7s %-7s %s\n' "programme" "$SIMD_NAME Cl/C" "Clair" "C" "ecart"

for name in collatz dot axpy while forin matmul matrice couche reseau; do
    # hot function: "// noyau: name" in the program, otherwise its first def
    kernel=$(grep -m 1 -oE '^// noyau: [A-Za-z_]+' "$BENCH/$name.cl" | cut -d' ' -f3)
    [ -n "$kernel" ] || kernel=$(grep -m 1 -oE '^def [a-z_]+' "$BENCH/$name.cl" | cut -d' ' -f2)
    ref_kernel=$(grep -m 1 -oE '^/\* noyau: [A-Za-z_]+' "$BENCH/${name}_ref.c" | cut -d' ' -f3)

    if ! "$ROOT/build/clair" "$BENCH/$name.cl" -o "$WORK/$name" > "$WORK/$name.log" 2>&1; then
        printf '%-9s ECHEC de compilation Clair\n' "$name"
        continue
    fi
    clang -std=c11 -O2 -fwrapv -fno-math-errno -ffp-contract=off -falign-loops=64 "$BENCH/${name}_ref.c" -o "$WORK/${name}_ref" -lm 2>> "$WORK/$name.log"

    # Keep the hot functions out of line so their machine code can be read alone.
    sed -E "s/^static ([a-z0-9_]+) clair_fn_${kernel}\(/static __attribute__((noinline)) \1 clair_fn_${kernel}(/" \
        "$WORK/$name.generated.c" > "$WORK/$name.noinline.c"
    clang -std=c11 -O2 -fwrapv -fno-math-errno -ffp-contract=off -falign-loops=64 -S "$WORK/$name.noinline.c" -o "$WORK/$name.s" 2>> "$WORK/$name.log"
    sed -E "s/^static ([a-z0-9_]+) ${ref_kernel}\(/static __attribute__((noinline)) \1 ${ref_kernel}(/" \
        "$BENCH/${name}_ref.c" > "$WORK/${name}_ref.noinline.c"
    clang -std=c11 -O2 -fwrapv -fno-math-errno -ffp-contract=off -falign-loops=64 -S "$WORK/${name}_ref.noinline.c" -o "$WORK/${name}_ref.s" 2>> "$WORK/$name.log"
    # the function and the jobs of its dense layers, run by each core (clair_layer_<function>_N)
    layers=$(grep -oE "^clair_layer_${kernel}_[0-9]+:" "$WORK/$name.s" | tr -d ':')
    v_clair=$(vectorized "$WORK/$name.s" "clair_fn_${kernel}" $layers)
    v_ref=$(vectorized "$WORK/${name}_ref.s" "${ref_kernel}")

    out_clair=$("$WORK/$name"); out_ref=$("$WORK/${name}_ref")
    same=""
    [ "$out_clair" = "$out_ref" ] || same=" RESULTAT DIFFERENT"

    times_clair=(); times_ref=()
    for _ in $(seq "$ROUNDS"); do
        times_clair+=("$(seconds ${PIN[@]+"${PIN[@]}"} "$WORK/$name")")
        times_ref+=("$(seconds ${PIN[@]+"${PIN[@]}"} "$WORK/${name}_ref")")
    done
    t_clair=$(median "${times_clair[@]}")
    t_ref=$(median "${times_ref[@]}")
    gap=$(awk -v a="$t_clair" -v b="$t_ref" 'BEGIN { printf "%+.0f%%", (a / b - 1) * 100 }')
    printf '%-9s %-11s %-7s %-7s %s%s\n' "$name" "$v_clair/$v_ref" \
        "$(printf '%.3f' "$t_clair")" "$(printf '%.3f' "$t_ref")" "$gap" "$same"
done

# Several cores: the big matrix products and dense layers share their rows
# between all the cores. Same programs, one core (pinned) against all of them.
cores=$(nproc 2>/dev/null || echo 1)
printf '\nPLUSIEURS COEURS (%s coeurs)\n' "$cores"
printf '%-9s %-7s %-7s %s\n' "programme" "1 coeur" "tous" "gain"
for name in matmul matrice couche; do
    [ -x "$WORK/$name" ] || continue
    out_one=$(CLAIR_THREADS=1 "$WORK/$name"); out_all=$("$WORK/$name")
    same=""
    [ "$out_one" = "$out_all" ] || same=" RESULTAT DIFFERENT"
    times_one=(); times_all=()
    for _ in $(seq "$ROUNDS"); do
        times_one+=("$(seconds env CLAIR_THREADS=1 ${PIN[@]+"${PIN[@]}"} "$WORK/$name")")
        times_all+=("$(seconds "$WORK/$name")")
    done
    t_one=$(median "${times_one[@]}")
    t_all=$(median "${times_all[@]}")
    gain=$(awk -v a="$t_one" -v b="$t_all" 'BEGIN { printf "x%.1f", a / b }')
    printf '%-9s %-7s %-7s %s%s\n' "$name" "$(printf '%.3f' "$t_one")" "$(printf '%.3f' "$t_all")" "$gain" "$same"
done
