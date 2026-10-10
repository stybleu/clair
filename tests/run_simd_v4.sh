#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK="$HOME/clair_work/simd_v4_permanent"
mkdir -p "$WORK"

cd "$ROOT"

fail() {
    echo "SIMD_V4=FAIL"
    echo "DETAIL=$1"
    exit 1
}

cat > "$WORK/permanent.cl" <<'CL'
def map_v4(values: list[int]) -> int {
    let i: int = 0
    while i < len(values) {
        values[i] = values[i] + 1
        i = i + 1
    }
    return i
}

def map_dynamic(values: list[int]) -> int {
    let i: int = 0
    while i < len(values) {
        if i == 0 {
            values.append(99)
        }
        values[i] = values[i] + 1
        i = i + 1
    }
    return i
}

def map_partial(values: list[int]) -> int {
    let i: int = 1
    while i < len(values) {
        values[i] = values[i] + 1
        i = i + 1
    }
    return i
}

def main() -> int {
    let empty: list[int] = []
    print(map_v4(empty))
    print(len(empty))

    let small: list[int] = []
    for j in range(15) {
        small.append(j)
    }
    print(map_v4(small))
    print(small[0])
    print(small[14])

    let medium: list[int] = []
    for j in range(16) {
        medium.append(j)
    }
    let alias: list[int] = medium
    print(map_v4(medium))
    print(alias[0])
    print(alias[15])

    let large: list[int] = []
    for j in range(1024) {
        large.append(j)
    }
    print(map_v4(large))
    print(large[1023])

    let dynamic: list[int] = [1, 2]
    print(map_dynamic(dynamic))
    print(len(dynamic))
    print(dynamic[2])

    let partial: list[int] = [10, 20, 30]
    print(map_partial(partial))
    print(partial[0])
    print(partial[1])
    print(partial[2])

    let negative: list[int] = [-5, 0, 5]
    print(map_v4(negative))
    print(negative[0])
    print(negative[1])
    print(negative[2])

    return 0
}
CL

cat > "$WORK/expected.txt" <<'OUT'
0
0
15
1
15
16
1
16
1024
1024
3
3
100
3
10
21
31
3
-4
1
6
OUT

"$ROOT/build/clair" \
    "$WORK/permanent.cl" \
    -o "$WORK/permanent" \
    > "$WORK/compile.log" 2>&1 \
    || fail "CLAIR_COMPILE"

"$WORK/permanent" \
    > "$WORK/actual.txt" \
    2> "$WORK/runtime.log" \
    || fail "RUNTIME"

cmp -s "$WORK/expected.txt" "$WORK/actual.txt" \
    || fail "OUTPUT_MISMATCH"

echo "OUTPUT=OK"
echo "BOUNDARY_15_16=OK"
echo "ALIAS_AND_DYNAMIC=OK"

GEN="$WORK/permanent.generated.c"
test -f "$GEN" || fail "GENERATED_C_MISSING"

grep -Fq 'values->len >= 16' "$GEN" \
    || fail "THRESHOLD_MISSING"

grep -Fq '__clair_v4_data_' "$GEN" \
    || fail "V4_FAST_PATH_MISSING"

python - "$GEN" "$WORK/dynamic_function.c" <<'PY_V4_EXTRACT'
from pathlib import Path
import re
import sys

source = Path(sys.argv[1]).read_text()

pattern = re.compile(
    r"(?ms)^int64_t[ \t]+map_dynamic"
    r"\(ClairList values\)[ \t]*\{"
    r".*?^\}[ \t]*$"
)

matches = list(pattern.finditer(source))

if len(matches) != 1:
    raise SystemExit(
        f"DYNAMIC_EXTRACTION_COUNT={len(matches)}"
    )

body = matches[0].group(0)

if body.count("map_dynamic(") != 1:
    raise SystemExit("DYNAMIC_EXTRACTION_INVALID")

Path(sys.argv[2]).write_text(body + "\n")
PY_V4_EXTRACT

grep -Fq 'return i;' "$WORK/dynamic_function.c" \
    || fail "DYNAMIC_EXTRACT_TRUNCATED"

FUNC_COUNT=$(grep -Ec '^int64_t [A-Za-z_][A-Za-z0-9_]*\(' \
    "$WORK/dynamic_function.c")

[ "$FUNC_COUNT" -eq 1 ] \
    || fail "DYNAMIC_EXTRACT_MULTIPLE_FUNCTIONS"

if grep -Fq '__clair_v4_data_' \
    "$WORK/dynamic_function.c"; then
    fail "DYNAMIC_WRONGLY_OPTIMIZED"
fi

echo "THRESHOLD_16=OK"
echo "DYNAMIC_FALLBACK=OK"

clang -O2 -S "$GEN" \
    -o "$WORK/permanent_O2.s" \
    > "$WORK/assembly.log" 2>&1 \
    || fail "ASSEMBLY_BUILD"

awk '
    /^map_v4:/ { inside = 1 }
    inside {
        print
        if ($0 ~ /^[[:space:]]*\.size[[:space:]]+map_v4/) exit
    }
' "$WORK/permanent_O2.s" \
    > "$WORK/map_v4.s"

grep -Eq \
    '^[[:space:]]*add[[:space:]]+v[0-9]+\.2d' \
    "$WORK/map_v4.s" \
    || fail "NEON_NOT_FOUND"

echo "NEON_O2=YES"

for OPT in 1 2; do
    clang "-O$OPT" \
        -fsanitize=address,undefined \
        -fno-sanitize-recover=all \
        -fno-omit-frame-pointer \
        "$GEN" \
        -o "$WORK/sanitized_O$OPT" \
        > "$WORK/sanitizer_build_O$OPT.log" 2>&1 \
        || fail "SANITIZER_BUILD_O$OPT"

    ASAN_OPTIONS="detect_leaks=0:halt_on_error=1" \
    UBSAN_OPTIONS="halt_on_error=1" \
        "$WORK/sanitized_O$OPT" \
        > "$WORK/sanitizer_output_O$OPT.txt" \
        2> "$WORK/sanitizer_run_O$OPT.log" \
        || fail "SANITIZER_RUN_O$OPT"

    cmp -s \
        "$WORK/expected.txt" \
        "$WORK/sanitizer_output_O$OPT.txt" \
        || fail "SANITIZER_OUTPUT_O$OPT"

    echo "ASAN_UBSAN_O$OPT=OK"
done

echo "SIMD_V4_PERMANENT=OK"
