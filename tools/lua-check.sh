#!/usr/bin/env bash
#
# tools/lua-check.sh — Lua validation and formatting checks for squared-pg.
#
#   tools/lua-check.sh [--fix] [--full]
#
#     --fix    format tracked Lua source with StyLua before checking
#     --full   also run the repository Lua test suite
#
# Runs from any directory: the repository is located from this script's own
# path, never from the working directory.
#
# Bash 3.2 compatible — macOS ships that, and CI runs macOS.

set -uo pipefail

usage() {
    echo "usage: tools/lua-check.sh [--fix] [--full]"
    echo
    echo "Run the squared-pg Lua tooling checks."
    echo
    echo "  --fix    Format tracked Lua source with StyLua before checking."
    echo "  --full   Also run the repository Lua test suite."
    echo "  --help   Show this help."
}

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd "$script_dir/.." && pwd)

fix=0
full=0

for argument in "$@"; do
    case "$argument" in
        --fix)  fix=1 ;;
        --full) full=1 ;;
        -h|--help) usage; exit 0 ;;
        *)
            echo "lua-check: unknown option: $argument" >&2
            usage >&2
            exit 2 ;;
    esac
done

failures=0

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "lua-check: required command not found: $1" >&2
        echo "Install it with: pkg install lua-language-server tree-sitter tree-sitter-lua stylua" >&2
        return 1
    fi
    return 0
}

for command_name in lua-language-server tree-sitter stylua; do
    require_command "$command_name" || exit 2
done

echo "== squared-pg Lua checks =="
echo "Repository: $repo_root"
echo

# Source, test and tool Lua only. Generated and vendored trees are excluded
# deliberately: third-party/ contains Penlight and ldoc, thousands of files
# this project does not own and must not reformat.
lua_roots=()
for candidate in lua tests tools; do
    [ -d "$repo_root/$candidate" ] && lua_roots+=("$repo_root/$candidate")
done

if [ "${#lua_roots[@]}" -eq 0 ]; then
    echo "lua-check: no Lua source directories found." >&2
    exit 2
fi

lua_files=$(find "${lua_roots[@]}" -type f -name '*.lua' 2>/dev/null | sort)
checked=$(printf '%s\n' "$lua_files" | grep -c . || true)

if [ "$checked" -eq 0 ]; then
    echo "No Lua files found."
    exit 0
fi

echo "Lua files: $checked"
echo

# ---------------------------------------------------------------------------
echo "-- StyLua --"

if [ "$fix" -eq 1 ]; then
    stylua "${lua_roots[@]}" || {
        echo "lua-check: StyLua formatting failed." >&2
        failures=$((failures + 1))
    }
else
    stylua --check "${lua_roots[@]}" || {
        echo "lua-check: StyLua check failed." >&2
        failures=$((failures + 1))
    }
fi

echo

# ---------------------------------------------------------------------------
echo "-- LuaLS --"

report_dir="$repo_root/build/lua-check"
mkdir -p "$report_dir"

# LuaLS's --check mode uses the workspace configuration when run against the
# repository root. Its generated report is kept out of the source tree.
lua-language-server \
    --check="$repo_root" \
    --checklevel=Information \
    --logpath="$report_dir" \
    --loglevel=warn
luals_status=$?

if [ "$luals_status" -ne 0 ]; then
    echo "lua-check: LuaLS reported diagnostics (exit status $luals_status)." >&2
    failures=$((failures + 1))
else
    echo "LuaLS: no errors reported."
fi

echo

# ---------------------------------------------------------------------------
echo "-- Tree-sitter --"

# Every file individually, so one malformed file does not hide the location of
# another.
parse_failures=0
while IFS= read -r file; do
    [ -n "$file" ] || continue
    if ! tree-sitter parse "$file" >/dev/null 2>&1; then
        echo "Tree-sitter parse failure: ${file#"$repo_root"/}" >&2
        parse_failures=$((parse_failures + 1))
    fi
done <<< "$lua_files"

if [ "$parse_failures" -ne 0 ]; then
    echo "Tree-sitter: $parse_failures file(s) failed to parse." >&2
    failures=$((failures + parse_failures))
else
    echo "Tree-sitter: all $checked Lua file(s) parsed successfully."
fi

# ---------------------------------------------------------------------------
if [ "$full" -eq 1 ]; then
    echo
    echo "-- Lua tests --"
    if [ -f "$repo_root/tests/run.lua" ]; then
        lua "$repo_root/tests/run.lua" || {
            echo "lua-check: Lua test suite failed." >&2
            failures=$((failures + 1))
        }
    else
        echo "lua-check: tests/run.lua not found; skipping." >&2
    fi
fi

echo
if [ "$failures" -eq 0 ]; then
    echo "Lua checks passed."
    exit 0
fi
echo "Lua checks failed: $failures check(s)." >&2
exit 1
