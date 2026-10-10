#!/bin/sh
set -eu

cd "$(dirname "$0")/.." || exit 1

WORK="$HOME/clair_work/simd_v3_asan"
mkdir -p "$WORK"

fail() {
    echo "ASAN_FAIL=$1" >&2
    exit 1
}

for OPT in 1 3; do
    COUNT=0

    for NAME in \
        simd_v3_int_sum \
        simd_v3_float_map \
        simd_v3_int_mutation \
        simd_v3_float_mutation \
        simd_v3_negative_read \
        simd_v3_negative_write \
        simd_v3_bounds_write \
        simd_v3_empty_read
    do
        SRC="build/test_${NAME}.generated.c"
        REF="build/test_${NAME}"
        BIN="$WORK/${NAME}_O${OPT}"

        [ -f "$SRC" ] || fail "$NAME:SOURCE"
        [ -x "$REF" ] || fail "$NAME:REFERENCE"

        clang -O"$OPT" \
            -fsanitize=address,undefined \
            -fno-sanitize-recover=all \
            -fno-omit-frame-pointer \
            "$SRC" -o "$BIN" \
            >"$WORK/${NAME}_build.log" 2>&1 \
            || fail "$NAME:BUILD"

        if "$REF" \
            >"$WORK/ref.stdout" \
            2>"$WORK/ref.stderr"; then
            REF_RC=0
        else
            REF_RC=$?
        fi

        if ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
            UBSAN_OPTIONS=halt_on_error=1 \
            "$BIN" \
            >"$WORK/asan.stdout" \
            2>"$WORK/asan.stderr"; then
            ASAN_RC=0
        else
            ASAN_RC=$?
        fi

        case "$NAME" in
            simd_v3_int_sum|simd_v3_float_map|\
            simd_v3_int_mutation|simd_v3_float_mutation)
                [ "$REF_RC" -eq 0 ] \
                    || fail "$NAME:REFERENCE_EXEC"
                [ "$ASAN_RC" -eq 0 ] \
                    || fail "$NAME:ASAN_EXEC"
                ;;
            *)
                [ "$REF_RC" -ne 0 ] \
                    || fail "$NAME:REFERENCE_EXPECTED_ERROR"
                [ "$ASAN_RC" -ne 0 ] \
                    || fail "$NAME:ASAN_EXPECTED_ERROR"

                grep -Fq 'Clair runtime error:' \
                    "$WORK/asan.stderr" \
                    || fail "$NAME:CLAIR_ERROR"

                grep -Fq 'out of bounds' \
                    "$WORK/asan.stderr" \
                    || fail "$NAME:BOUNDS_ERROR"
                ;;
        esac

        cmp -s "$WORK/ref.stdout" \
            "$WORK/asan.stdout" \
            || fail "$NAME:STDOUT"

        cmp -s "$WORK/ref.stderr" \
            "$WORK/asan.stderr" \
            || fail "$NAME:STDERR"

        COUNT=$((COUNT + 1))
    done

    echo "ASAN_UBSAN_O${OPT}=OK"
    echo "CASES_O${OPT}=$COUNT"
done

echo "ASAN_UBSAN_CASES=16"
echo "LEAK_DETECTION=DISABLED"
echo "SIMD_V3_ASAN=OK"
