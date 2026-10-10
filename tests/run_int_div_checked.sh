#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK="$HOME/clair_work/int_div_checked_permanent"

mkdir -p "$WORK"
cd "$ROOT"

fail() {
    echo "INT_DIV_CHECKED=FAIL"
    echo "DETAIL=$1"
    exit 1
}

compile_cl() {
    local name="$1"

    "$ROOT/build/clair" \
        "$WORK/$name.cl" \
        -o "$WORK/$name" \
        > "$WORK/$name.compile.log" 2>&1 \
        || fail "COMPILE_$name"

    test -f "$WORK/$name.generated.c" \
        || fail "GENERATED_$name"
}

cat > "$WORK/safe.cl" <<'CL'
def div_int(a: int, b: int) -> int {
    return a / b
}

def main() -> int {
    print(div_int(7, 2))
    print(div_int(-7, 2))
    print(div_int(7, -2))

    let min: int = -9223372036854775807 - 1
    print(div_int(min, 1))

    return 0
}
CL

cat > "$WORK/zero.cl" <<'CL'
def div_int(a: int, b: int) -> int {
    return a / b
}

def main() -> int {
    let a: int = 7
    let b: int = 0
    print(div_int(a, b))
    return 0
}
CL

cat > "$WORK/overflow.cl" <<'CL'
def div_int(a: int, b: int) -> int {
    return a / b
}

def main() -> int {
    let a: int = -9223372036854775807 - 1
    let b: int = -1
    print(div_int(a, b))
    return 0
}
CL

cat > "$WORK/floating.cl" <<'CL'
def div_float(a: float, b: float) -> float {
    return a / b
}

def main() -> int {
    if div_float(7.5, 2.0) == 3.75 {
        print(1)
    } else {
        print(0)
    }

    return 0
}
CL

for name in safe zero overflow floating; do
    compile_cl "$name"
done

grep -Fq 'clair_rt_int_div_checked(a, b)' \
    "$WORK/safe.generated.c" \
    || fail "INTEGER_HELPER_NOT_USED"

cat > "$WORK/safe.expected" <<'OUT'
3
-3
-3
-9223372036854775808
OUT

"$WORK/safe" > "$WORK/safe.out" \
    2> "$WORK/safe.err" \
    || fail "SAFE_EXEC"

cmp -s "$WORK/safe.expected" "$WORK/safe.out" \
    || fail "SAFE_RESULT"

test ! -s "$WORK/safe.err" \
    || fail "SAFE_STDERR"

echo "SAFE_DIVISION=OK"

"$WORK/floating" > "$WORK/floating.out" \
    2> "$WORK/floating.err" \
    || fail "FLOAT_EXEC"

printf '1\n' > "$WORK/floating.expected"

cmp -s "$WORK/floating.expected" "$WORK/floating.out" \
    || fail "FLOAT_RESULT"

test ! -s "$WORK/floating.err" \
    || fail "FLOAT_STDERR"

echo "FLOAT_DIVISION=OK"

check_error() {
    local exe="$1"
    local expected="$2"
    local label="$3"

    if "$exe" > "$WORK/$label.stdout" \
              2> "$WORK/$label.stderr"; then
        fail "${label}_UNEXPECTED_SUCCESS"
    fi

    test ! -s "$WORK/$label.stdout" \
        || fail "${label}_STDOUT"

    printf '%s\n' "$expected" \
        > "$WORK/$label.expected"

    cmp -s "$WORK/$label.expected" \
           "$WORK/$label.stderr" \
        || fail "${label}_ERROR_MISMATCH"
}

for name in zero overflow; do

    if [ "$name" = zero ]; then
        message="Clair runtime error: division by zero"
    else
        message="Clair runtime error: integer division overflow"
    fi

    check_error \
        "$WORK/$name" \
        "$message" \
        "${name}_native"

    echo "${name^^}_NATIVE=OK"

    for opt in 0 2; do

        clang "-O$opt" \
            -std=c11 \
            -fwrapv \
            -fsanitize=address,undefined \
            -fno-sanitize-recover=all \
            -fno-omit-frame-pointer \
            "$WORK/$name.generated.c" \
            -o "$WORK/${name}_san_O${opt}" \
            > "$WORK/${name}_build_O${opt}.log" 2>&1 \
            || fail "SAN_BUILD_${name}_O${opt}"

        if ASAN_OPTIONS="detect_leaks=0:halt_on_error=1" \
           UBSAN_OPTIONS="halt_on_error=1" \
           "$WORK/${name}_san_O${opt}" \
           > "$WORK/${name}_O${opt}.stdout" \
           2> "$WORK/${name}_O${opt}.stderr"; then
            fail "SAN_${name}_O${opt}_UNEXPECTED_SUCCESS"
        fi

        test ! -s "$WORK/${name}_O${opt}.stdout" \
            || fail "SAN_${name}_O${opt}_STDOUT"

        cmp -s "$WORK/${name}_native.expected" \
               "$WORK/${name}_O${opt}.stderr" \
            || fail "SAN_${name}_O${opt}_ERROR"

        echo "${name^^}_ASAN_UBSAN_O${opt}=OK"
    done
done

echo "INT_DIV_CHECKED=OK"
