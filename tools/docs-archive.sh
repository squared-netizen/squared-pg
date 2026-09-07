#!/usr/bin/env bash
#
# tools/docs-archive.sh — zip the documentation tree.
#
#   tools/docs-archive.sh [destination]
#
# The archive is named squared-pg-docs-<version>-<date>.zip and moved to
# `destination`, or to $HOME when none is given.
#
#   tools/docs-archive.sh                    -> ~/squared-pg-docs-0.1.0-alpha.1-2026-09-05.zip
#   tools/docs-archive.sh ~/storage/shared   -> onto shared storage, for sharing off-device
#   tools/docs-archive.sh --list             -> what would go in, without writing anything
#
# Runs from anywhere: the repository is located from this script's own path,
# never from the working directory.
#
# Bash 3.2 compatible — macOS ships that, and CI runs macOS.

set -uo pipefail

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
root=$(cd "$script_dir/.." && pwd)

destination=""
list_only=0

for argument in "$@"; do
    case "$argument" in
        --list) list_only=1 ;;
        -h|--help)
            sed -n '3,17p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
            exit 0 ;;
        -*)
            echo "docs-archive: unknown option $argument" >&2
            exit 2 ;;
        *)
            if [ -n "$destination" ]; then
                echo "docs-archive: only one destination may be given" >&2
                exit 2
            fi
            destination="$argument" ;;
    esac
done

[ -n "$destination" ] || destination="$HOME"

# --- checks ----------------------------------------------------------------

[ -d "$root/docs" ] || { echo "docs-archive: no docs/ under $root" >&2; exit 1; }

command -v zip >/dev/null 2>&1 \
    || { echo "docs-archive: zip is not installed (pkg install zip)" >&2; exit 1; }

# The version is read from VERSION, the single source both build systems use.
# Falling back to "unversioned" rather than guessing: an archive labelled with
# a version it was not built from is worse than one labelled as unknown.
if [ -f "$root/VERSION" ]; then
    version=$(tr -d ' \t\n\r' < "$root/VERSION")
else
    version="unversioned"
fi

date_stamp=$(date +%Y-%m-%d)
name="squared-pg-docs-$version-$date_stamp.zip"

# --- what goes in ----------------------------------------------------------
#
# The whole tree, minus the things that are not documentation. Obsidian's
# workspace state and editor scratch files are per-machine and would make two
# archives of the same docs differ.

EXCLUDES=(
    '.obsidian/*'
    '.trash/*'
    '*/.DS_Store'
    '.DS_Store'
    '*.tmp'
    '*.swp'
    '*~'
)

if [ "$list_only" -eq 1 ]; then
    echo "would archive as: $name"
    echo "             to: $destination"
    echo
    ( cd "$root" && find docs -type f \
        ! -path 'docs/.obsidian/*' ! -path 'docs/.trash/*' \
        ! -name '.DS_Store' ! -name '*.swp' ! -name '*~' | sort )
    exit 0
fi

# --- build it --------------------------------------------------------------
#
# Staged in $TMPDIR and moved into place, so an interrupted run never leaves a
# half-written archive at the destination. $TMPDIR, never /tmp: Termux has none.

scratch="${TMPDIR:-/tmp}/squared-pg-docs.$$"
mkdir -p "$scratch" || { echo "docs-archive: could not create $scratch" >&2; exit 1; }
trap 'rm -rf "$scratch"' EXIT INT TERM

# -r recurse, -q quiet, -X drop platform-specific extra attributes so the same
# docs produce the same archive on any host.
( cd "$root" && zip -r -q -X "$scratch/$name" docs -x "${EXCLUDES[@]/#/docs/}" ) \
    || { echo "docs-archive: zip failed" >&2; exit 1; }

# The destination may not exist yet, and creating it is friendlier than
# refusing — but only if it is creatable.
if [ ! -d "$destination" ]; then
    mkdir -p "$destination" \
        || { echo "docs-archive: cannot create $destination" >&2; exit 1; }
fi

[ -w "$destination" ] || { echo "docs-archive: $destination is not writable" >&2; exit 1; }

target="$destination/$name"
if [ -e "$target" ]; then
    # Same version on the same day, twice. Overwriting silently would lose the
    # earlier one; a counter keeps both and says which is which.
    counter=2
    while [ -e "$destination/${name%.zip}-$counter.zip" ]; do
        counter=$((counter + 1))
    done
    target="$destination/${name%.zip}-$counter.zip"
fi

mv "$scratch/$name" "$target" || { echo "docs-archive: could not move into place" >&2; exit 1; }

files=$(unzip -l "$target" 2>/dev/null | tail -n 1 | awk '{print $2}')
size=$(du -h "$target" | cut -f1)

echo "$target"
echo "  ${files:-?} files, $size"
