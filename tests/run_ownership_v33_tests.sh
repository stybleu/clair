#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

LOG="$HOME/clair_work/ownership_v33_regression"
mkdir -p "$LOG"

passed=0
precision_conservative=0
precision_accepted=0

reject_case() {
    local name="$1"
    local marker="$2"
    local binary="$LOG/$name.bin"

    if ./build/clair \
        "tests/ownership/flow_v33/$name.cl" \
        -o "$binary" \
        >"$LOG/$name.stdout" \
        2>"$LOG/$name.stderr"; then
        echo "ERROR=$name:UNEXPECTED_ACCEPT"
        exit 1
    fi

    if ! grep -Fq "semantic error:" "$LOG/$name.stderr" ||
       ! grep -Fq "$marker" "$LOG/$name.stderr"; then
        echo "ERROR=$name:WRONG_REJECTION"
        sed -n '1,5p' "$LOG/$name.stderr"
        exit 1
    fi

    passed=$((passed + 1))
}

run_samples() {
    local name="$1"
    shift

    local binary="$LOG/$name.bin"

    while [ "$#" -gt 0 ]; do
        local input="$1"
        local expected="$2"
        shift 2

        if ! printf '%b' "$input" |
             "$binary" \
             >"$LOG/$name.actual" \
             2>"$LOG/$name.runtime.stderr"; then
            echo "ERROR=$name:RUNTIME_FAILED"
            exit 1
        fi

        printf '%b' "$expected" >"$LOG/$name.expected"

        if ! cmp -s \
            "$LOG/$name.expected" \
            "$LOG/$name.actual"; then
            echo "ERROR=$name:OUTPUT_MISMATCH"
            exit 1
        fi
    done
}

accept_case() {
    local name="$1"
    shift

    if ! ./build/clair \
        "tests/ownership/flow_v33/$name.cl" \
        -o "$LOG/$name.bin" \
        >"$LOG/$name.stdout" \
        2>"$LOG/$name.stderr"; then
        echo "ERROR=$name:COMPILE_FAILED"
        sed -n '1,5p' "$LOG/$name.stderr"
        exit 1
    fi

    run_samples "$name" "$@"
    passed=$((passed + 1))
}

precision_case() {
    local name="$1"
    local marker="$2"
    shift 2

    if ./build/clair \
        "tests/ownership/flow_v33/$name.cl" \
        -o "$LOG/$name.bin" \
        >"$LOG/$name.stdout" \
        2>"$LOG/$name.stderr"; then

        run_samples "$name" "$@"
        precision_accepted=$((precision_accepted + 1))

    elif grep -Fq "semantic error:" "$LOG/$name.stderr" &&
         grep -Fq "$marker" "$LOG/$name.stderr"; then

        precision_conservative=$((precision_conservative + 1))

    else
        echo "ERROR=$name:UNEXPECTED_DIAGNOSTIC"
        sed -n '1,5p' "$LOG/$name.stderr"
        exit 1
    fi

    passed=$((passed + 1))
}

accept_case break_control '' '2
'
accept_case continue_control '' '4
'
reject_case break_skips_reinit 'use after move'
reject_case continue_skips_reinit 'use after move'
reject_case for_zero_break 'use after move'
reject_case nested_break_moved 'use after move'
reject_case break_alias_escape 'unaliased source'
reject_case continue_alias_escape 'unaliased source'
accept_case break_no_alias '0
' '10
' '1
' '10
'
accept_case continue_no_alias '0
' '10
' '1
' '10
'
accept_case shadow_break '' '10
'
accept_case shadow_continue '' '10
'
precision_case break_branch_precision 'use after move' '0
' '' '1
' '' '2
' '20
'
precision_case continue_branch_precision 'use after move' '0
' '' '1
' '' '2
' '20
'

echo "OWNERSHIP_V33_TESTS=$passed/14"
echo "V33_PRECISION_CONSERVATIVE=$precision_conservative"
echo "V33_PRECISION_ACCEPTED=$precision_accepted"

[ "$passed" -eq 14 ]
