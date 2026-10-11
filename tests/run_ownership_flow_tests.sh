#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

LOG_DIR="$HOME/clair_work/ownership_flow_regression"
mkdir -p "$LOG_DIR"

passed=0

reject_case() {
    local name="$1"
    local marker="$2"
    local binary="$LOG_DIR/$name.bin"

    if ./build/clair \
        "tests/ownership/flow/$name.cl" \
        -o "$binary" \
        >"$LOG_DIR/$name.stdout" \
        2>"$LOG_DIR/$name.stderr"; then
        echo "ERROR=$name:UNEXPECTED_ACCEPT"
        exit 1
    fi

    if ! grep -Fq "semantic error:" \
            "$LOG_DIR/$name.stderr" ||
       ! grep -Fq "$marker" \
            "$LOG_DIR/$name.stderr"; then
        echo "ERROR=$name:WRONG_REJECTION"
        sed -n '1,5p' "$LOG_DIR/$name.stderr"
        exit 1
    fi

    passed=$((passed + 1))
}

accept_case() {
    local name="$1"
    shift

    local binary="$LOG_DIR/$name.bin"

    if ! ./build/clair \
        "tests/ownership/flow/$name.cl" \
        -o "$binary" \
        >"$LOG_DIR/$name.stdout" \
        2>"$LOG_DIR/$name.stderr"; then
        echo "ERROR=$name:COMPILE_FAILED"
        sed -n '1,5p' "$LOG_DIR/$name.stderr"
        exit 1
    fi

    while [ "$#" -gt 0 ]; do
        local input="$1"
        local expected="$2"
        shift 2

        if ! printf '%b' "$input" |
             "$binary" \
             >"$LOG_DIR/$name.actual" \
             2>"$LOG_DIR/$name.runtime.stderr"; then
            echo "ERROR=$name:RUNTIME_FAILED"
            exit 1
        fi

        printf '%b' "$expected" \
            >"$LOG_DIR/$name.expected"

        if ! cmp -s \
            "$LOG_DIR/$name.expected" \
            "$LOG_DIR/$name.actual"; then
            echo "ERROR=$name:OUTPUT_MISMATCH"
            exit 1
        fi
    done

    passed=$((passed + 1))
}

accept_case direct_reinitialization '' '30\n'
reject_case direct_use_after_move 'use after move'
reject_case if_optional_reinitialization 'use after move'
accept_case if_else_full_reinitialization '1\n' '30\n' '0\n' '40\n'
reject_case while_optional_reinitialization 'use after move'
reject_case for_zero_reinitialization 'use after move'
accept_case if_reinitialization_inside '1\n' '30\n' '0\n' ''
reject_case if_direct_move 'direct function-level'
reject_case if_else_moved_path 'use after move'
accept_case conditional_recovery '0\n' '50\n' '1\n' '50\n'
reject_case nested_optional_reinit 'use after move'
accept_case nested_full_reinit '2\n' '20\n' '1\n' '30\n' '0\n' '40\n'
reject_case while_conditional_reinit 'use after move'
accept_case while_internal_read '1\n' '20\n' '0\n' ''
reject_case for_break_reinit 'use after move'
accept_case for_internal_read '' '20\n20\n'
reject_case conditional_alias 'unaliased source'
reject_case conditional_function_escape 'unaliased source'
reject_case owner_export_in_if 'cannot export owner'
accept_case shadow_non_escape '1\n' '10\n' '0\n' '10\n'
accept_case reinit_after_alias '1\n' '70\n' '0\n' '70\n'
reject_case sequential_if_unsafe 'use after move'

echo "OWNERSHIP_FLOW_TESTS=$passed/22"
[ "$passed" -eq 22 ]
