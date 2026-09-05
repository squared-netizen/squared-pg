#!/usr/bin/env bash
#
# tools/clang-check.sh — clang-format and clang-tidy over project-owned sources.
#
#   tools/clang-check.sh [--fix] [--full]
#
#     --fix    rewrite files in place rather than reporting
#     --full   also run the Clang Static Analyzer (needs scan-build and cmake)
#
# Environment:
#   CLANG_COMPILE_DB           path to compile_commands.json
#   CLANG_FULL_BUILD_COMMAND   build command for --full on non-CMake layouts
#
# Bash 3.2 compatible: macOS still ships that, and CI runs macOS. No
# associative arrays, no `mapfile`, no `${var,,}`.

set -uo pipefail

fix=0
full=0

for argument in "$@"; do
    case "$argument" in
        --fix)  fix=1 ;;
        --full) full=1 ;;
        -h|--help)
            echo "usage: tools/clang-check.sh [--fix] [--full]"
            exit 0 ;;
        *)
            echo "error: unknown argument: $argument" >&2
            exit 2 ;;
    esac
done

# --- project root ----------------------------------------------------------

root=$(pwd)
while [ ! -f "$root/.clang-tidy" ]; do
    parent=$(dirname "$root")
    if [ "$parent" = "$root" ]; then
        echo "error: could not find project root containing .clang-tidy" >&2
        exit 2
    fi
    root="$parent"
done
cd "$root" || exit 2

# --- prerequisites ---------------------------------------------------------

required="clang-format clang-tidy python3"
# fd is a convenience, not a requirement: find does the same job and is always
# present. Preferring fd when available keeps the fast path on the machines
# that have it without excluding the ones that do not.
if [ "$full" -eq 1 ]; then
    required="$required scan-build cmake"
fi
for command_name in $required; do
    if ! command -v "$command_name" >/dev/null 2>&1; then
        echo "error: required command is unavailable: $command_name" >&2
        exit 2
    fi
done

# --- compilation database --------------------------------------------------

compile_database="${CLANG_COMPILE_DB:-}"
if [ -z "$compile_database" ]; then
    for candidate in compile_commands.json \
                     build/compile_commands.json \
                     build-cmake/compile_commands.json \
                     native-build/compile_commands.json; do
        if [ -f "$candidate" ]; then
            compile_database="$candidate"
            break
        fi
    done
fi

if [ -z "$compile_database" ] || [ ! -f "$compile_database" ]; then
    echo "error: compile_commands.json is required" >&2
    echo "configure CMake with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON" >&2
    echo "or set CLANG_COMPILE_DB to its path" >&2
    exit 2
fi

# realpath is not on macOS by default; python3 is already required above.
compile_database=$(python3 -c 'import os,sys; print(os.path.realpath(sys.argv[1]))' "$compile_database")
compile_directory=$(dirname "$compile_database")
overall=0

# --- files to format -------------------------------------------------------

EXCLUDES="build build-cmake native-build external third_party third-party vendor generated"

collect_format_files() {
    if command -v fd >/dev/null 2>&1; then
        local args=()
        local directory
        for directory in $EXCLUDES; do
            args+=(--exclude "$directory")
        done
        fd --type f \
           --extension c --extension cc --extension cpp --extension cxx \
           --extension h --extension hh --extension hpp --extension hxx \
           "${args[@]}" .
    else
        local prune=""
        local directory
        for directory in $EXCLUDES; do
            prune="$prune -path ./$directory -prune -o"
        done
        # shellcheck disable=SC2086
        find . $prune \
            \( -name '*.c' -o -name '*.cc' -o -name '*.cpp' -o -name '*.cxx' \
               -o -name '*.h' -o -name '*.hh' -o -name '*.hpp' -o -name '*.hxx' \) \
            -type f -print
    fi
}

format_files=$(collect_format_files)
if [ -z "$format_files" ]; then
    echo "error: no project-owned C/C++ files were found" >&2
    exit 2
fi
format_count=$(printf '%s\n' "$format_files" | grep -c .)

echo "project root: $root"
echo "compile database: $compile_database"
echo "project-owned format files: $format_count"
echo
echo "== clang-format =="

# xargs rather than word-splitting: a path with a space would otherwise be two
# arguments, and the failure would be a confusing "no such file".
if [ "$fix" -eq 1 ]; then
    printf '%s\n' "$format_files" | xargs -d '\n' clang-format -i 2>/dev/null \
        || printf '%s\n' "$format_files" | tr '\n' '\0' | xargs -0 clang-format -i \
        || overall=1
else
    printf '%s\n' "$format_files" | tr '\n' '\0' | xargs -0 clang-format --dry-run --Werror \
        || overall=1
fi

# --- clang-tidy ------------------------------------------------------------

echo
echo "== clang-tidy =="

translation_units=$(python3 -c '
import json
import os
import sys

database, root = sys.argv[1], os.path.realpath(sys.argv[2])
excluded = ("build", "build-cmake", "native-build", "external",
            "third_party", "third-party", "vendor", "generated")
seen = set()
with open(database, encoding="utf-8") as stream:
    commands = json.load(stream)
for command in commands:
    filename = command.get("file")
    directory = command.get("directory", root)
    if not filename:
        continue
    absolute = os.path.realpath(
        filename if os.path.isabs(filename) else os.path.join(directory, filename))
    try:
        relative = os.path.relpath(absolute, root)
    except ValueError:
        continue
    if relative.startswith(".." + os.sep) or relative == "..":
        continue
    if any(part in excluded for part in relative.split(os.sep)):
        continue
    if os.path.splitext(relative)[1].lower() not in {".c", ".cc", ".cpp", ".cxx"}:
        continue
    if absolute not in seen:
        seen.add(absolute)
        print(absolute)
' "$compile_database" "$root")

if [ -z "$translation_units" ]; then
    echo "error: compilation database contains no project-owned translation units" >&2
    exit 2
fi

unit_count=$(printf '%s\n' "$translation_units" | grep -c .)
echo "project-owned translation units: $unit_count"

index=0
while IFS= read -r translation_unit; do
    [ -n "$translation_unit" ] || continue
    index=$((index + 1))
    echo "[clang-tidy $index/$unit_count] $translation_unit"
    if [ "$fix" -eq 1 ]; then
        clang-tidy -p "$compile_directory" --fix --format-style=file "$translation_unit" || overall=1
    else
        clang-tidy -p "$compile_directory" "$translation_unit" || overall=1
    fi
done <<< "$translation_units"

# --- static analyzer -------------------------------------------------------

if [ "$full" -eq 1 ]; then
    echo
    echo "== Clang Static Analyzer =="
    if [ -n "${CLANG_FULL_BUILD_COMMAND:-}" ]; then
        bash -c "$CLANG_FULL_BUILD_COMMAND" || overall=1
    elif [ -f CMakeLists.txt ]; then
        analyzer_build=build/clang-analyzer
        scan-build --status-bugs cmake -S . -B "$analyzer_build" \
            -DCMAKE_BUILD_TYPE=Debug \
            -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        && scan-build --status-bugs cmake --build "$analyzer_build" --clean-first \
        || overall=1
    else
        echo "error: --full requires CLANG_FULL_BUILD_COMMAND for this project layout" >&2
        overall=1
    fi
fi

echo
if [ "$overall" -eq 0 ]; then
    echo "ALL CLANG CHECKS PASSED"
else
    echo "CLANG CHECKS FAILED" >&2
fi
exit "$overall"
