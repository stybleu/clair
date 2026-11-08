#!/bin/sh
set -eu

cd "$(dirname "$0")/.." || exit 1

WORK="$HOME/clair_work/simd_v3_ubsan"
mkdir -p "$WORK"

fail() {
    echo "UBSAN_FAIL=$1" >&2
    exit 1
}

for OPT in 0 3; do

    for NAME in \
        simd_v3_int_sum \
        simd_v3_float_map \
        simd_v3_negative_read \
        simd_v3_negative_write \
        simd_v3_bounds_write \
        simd_v3_empty_read
    do

        SRC="build/test_${NAME}.generated.c"
        BASE="build/test_${NAME}"
        BIN="$WORK/${NAME}_O${OPT}"

        [ -f "$SRC" ] || fail "$NAME:SOURCE"
        [ -f "$BASE" ] || fail "$NAME:BASELINE"

        if ! clang -O"$OPT" \
            -fsanitize=undefined \
            -fno-sanitize-recover=all \
            "$SRC" -o "$BIN" \
            >"$WORK/${NAME}_build.log" 2>&1; then

            tail -n 8 "$WORK/${NAME}_build.log"
            fail "$NAME:BUILD_O${OPT}"
        fi

        if "$BASE" \
            >"$WORK/reference.stdout" \
            2>"$WORK/reference.stderr"; then
            BASE_RC=0
        else
            BASE_RC=$?
        fi

        if UBSAN_OPTIONS=halt_on_error=1 \
            "$BIN" \
            >"$WORK/sanitized.stdout" \
            2>"$WORK/sanitized.stderr"; then
            UBSAN_RC=0
        else
            UBSAN_RC=$?
        fi

        case "$NAME" in
            simd_v3_int_sum|simd_v3_float_map)
                [ "$BASE_RC" -eq 0 ] \
                    || fail "$NAME:BASE_EXEC"

                [ "$UBSAN_RC" -eq 0 ] \
                    || fail "$NAME:UBSAN_EXEC"
                ;;
            *)
                [ "$BASE_RC" -ne 0 ] \
                    || fail "$NAME:BASE_NOT_REJECTED"

                [ "$UBSAN_RC" -ne 0 ] \
                    || fail "$NAME:UBSAN_NOT_REJECTED"

                grep -Fq 'Clair runtime error:' \
                    "$WORK/sanitized.stderr" \
                    || fail "$NAME:ERROR_MISSING"

                grep -Fq 'out of bounds' \
                    "$WORK/sanitized.stderr" \
                    || fail "$NAME:BOUNDS_MISSING"
                ;;
        esac

        cmp -s \
            "$WORK/reference.stdout" \
            "$WORK/sanitized.stdout" \
            || fail "$NAME:OUTPUT_DIFFERENT"

        cmp -s \
            "$WORK/reference.stderr" \
            "$WORK/sanitized.stderr" \
            || fail "$NAME:ERROR_DIFFERENT"

    done

    echo "UBSAN_O${OPT}=OK"
done

echo "UBSAN_CASES=12"
echo "SIMD_V3_UBSAN=OK"
