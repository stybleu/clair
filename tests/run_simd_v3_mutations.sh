#!/bin/sh
set -eu

cd "$(dirname "$0")/.." || exit 1

WORK="$HOME/clair_work/simd_v3_mutation_tests"
mkdir -p "$WORK"

fail() {
    echo "MUTATION_FAIL=$1" >&2
    exit 1
}

for NAME in \
    simd_v3_float_mutation \
    simd_v3_int_mutation
do
    ./build/clair \
        "tests/$NAME.cl" \
        -o "build/test_$NAME" \
        >"$WORK/$NAME.compile.log" 2>&1 \
        || {
            tail -n 10 "$WORK/$NAME.compile.log"
            fail "$NAME:COMPILE"
        }

    "./build/test_$NAME" \
        >"$WORK/$NAME.stdout" \
        2>"$WORK/$NAME.stderr" \
        || fail "$NAME:EXEC"
done

awk '
BEGIN {
    split("4096 4096 0.5 1024.5 4095.5 4096.5 4097",
          expected, " ")
}
{
    if (NF != 1 || ($1 + 0) != (expected[NR] + 0))
        exit 1
}
END {
    if (NR != 7) exit 1
}
' "$WORK/simd_v3_float_mutation.stdout" \
    || fail FLOAT_RESULT

echo "FLOAT_ALIAS_REALLOC=OK"

awk '
BEGIN {
    split("4096 8386560 10000 8392465 7 4097",
          expected, " ")
}
{
    if (NF != 1 || ($1 + 0) != (expected[NR] + 0))
        exit 1
}
END {
    if (NR != 6) exit 1
}
' "$WORK/simd_v3_int_mutation.stdout" \
    || fail INT_RESULT

echo "INT_ALIAS_REALLOC=OK"

FLOAT_C="build/test_simd_v3_float_mutation.generated.c"
INT_C="build/test_simd_v3_int_mutation.generated.c"

grep -Fq \
    'clair_rt_list_at_f64_checked(' \
    "$FLOAT_C" || fail FLOAT_TYPED

grep -Fq \
    'clair_rt_list_at_i64_checked(' \
    "$INT_C" || fail INT_TYPED

echo "TYPED_ACCESS=OK"

if [ "${CLAIR_REQUIRE_UBSAN:-0}" = "1" ]; then

    COUNT=0

    for OPT in 0 3; do
        for NAME in \
            simd_v3_float_mutation \
            simd_v3_int_mutation
        do
            SRC="build/test_$NAME.generated.c"
            BIN="$WORK/${NAME}_ubsan_O${OPT}"

            clang -O"$OPT" \
                -fsanitize=undefined \
                -fno-sanitize-recover=all \
                "$SRC" -o "$BIN" \
                >"$WORK/${NAME}_O${OPT}.build.log" 2>&1 \
                || fail "$NAME:UBSAN_BUILD"

            UBSAN_OPTIONS=halt_on_error=1 \
                "$BIN" \
                >"$WORK/${NAME}_O${OPT}.stdout" \
                2>"$WORK/${NAME}_O${OPT}.stderr" \
                || fail "$NAME:UBSAN_EXEC"

            cmp -s \
                "$WORK/$NAME.stdout" \
                "$WORK/${NAME}_O${OPT}.stdout" \
                || fail "$NAME:UBSAN_OUTPUT"

            if [ -s "$WORK/${NAME}_O${OPT}.stderr" ]; then
                fail "$NAME:UBSAN_STDERR"
            fi

            COUNT=$((COUNT + 1))
        done

        echo "MUTATION_UBSAN_O${OPT}=OK"
    done

    echo "UBSAN_MUTATION_CASES=$COUNT"
fi

echo "SIMD_V3_MUTATIONS=OK"
