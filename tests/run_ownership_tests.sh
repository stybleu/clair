#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

LOG_DIR="$HOME/clair_work/ownership_regression"
mkdir -p "$LOG_DIR"

passed=0

reject_case() {
    local name="$1"
    local marker="$2"
    local binary="$LOG_DIR/$name.bin"
    local stdout="$LOG_DIR/$name.stdout"
    local stderr="$LOG_DIR/$name.stderr"

    if ./build/clair \
        "tests/ownership/$name.cl" \
        -o "$binary" >"$stdout" 2>"$stderr"; then
        echo "ERROR=$name:UNEXPECTED_ACCEPT"
        exit 1
    fi

    if ! grep -Fq "semantic error:" "$stderr" ||
       ! grep -Fq "$marker" "$stderr"; then
        echo "ERROR=$name:WRONG_REJECTION"
        sed -n '1,6p' "$stderr"
        exit 1
    fi

    passed=$((passed + 1))
}

accept_case() {
    local name="$1"
    local expected="$2"
    local binary="$LOG_DIR/$name.bin"

    if ! ./build/clair \
        "tests/ownership/$name.cl" \
        -o "$binary" \
        >"$LOG_DIR/$name.stdout" \
        2>"$LOG_DIR/$name.stderr"; then
        echo "ERROR=$name:COMPILE_FAILED"
        sed -n '1,6p' "$LOG_DIR/$name.stderr"
        exit 1
    fi

    if ! "$binary" \
        >"$LOG_DIR/$name.actual" \
        2>"$LOG_DIR/$name.runtime.stderr"; then
        echo "ERROR=$name:RUNTIME_FAILED"
        exit 1
    fi

    printf '%b' "$expected" >"$LOG_DIR/$name.expected"

    if ! cmp -s \
        "$LOG_DIR/$name.expected" \
        "$LOG_DIR/$name.actual"; then
        echo "ERROR=$name:OUTPUT_MISMATCH"
        exit 1
    fi

    passed=$((passed + 1))
}

reject_case "fn_return_alias" "unaliased source"
reject_case "fn_assign_alias" "unaliased source"
reject_case "fn_owner_export" "cannot export owner"
reject_case "class_constructor_alias" "unaliased source"
reject_case "class_member_alias" "unaliased source"
reject_case "class_owner_export" "cannot export owner"
accept_case "function_control" "10\n"
accept_case "class_control" "10\n"
reject_case "alias_before_move" "unaliased source"
reject_case "alias_mutation" "unaliased source"
reject_case "move_from_alias" "unaliased source"
reject_case "alias_after_move" "cannot copy exclusive"
accept_case "simple_move" "10\n"
accept_case "rebind_before_move" "10\n30\n"
reject_case "append_after_move" "use after move"
reject_case "index_write_after_move" "use after move"

echo "OWNERSHIP_TESTS=$passed/16"

if [ "$passed" -ne 16 ]; then
    exit 1
fi
