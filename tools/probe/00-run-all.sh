#!/bin/sh
# Run every probe and produce one report.
#
#   sh tools/probe/00-run-all.sh > probe-report.txt 2>&1
#
# Read-only. Nothing is installed, nothing outside $TMPDIR is written, and a
# missing tool is reported rather than treated as a failure.

set -u
dir=$(dirname "$0")

printf '\033[1msquared-pg host capability probe\033[0m\n'
printf 'date    %s\n' "$(date 2>/dev/null || echo unknown)"
printf 'host    %s\n' "$(uname -srm 2>/dev/null || echo unknown)"
printf 'shell   %s\n' "${SHELL:-unknown}"

for script in "$dir"/[1-9]*.sh; do
    [ -f "$script" ] || continue
    printf '\n\n\033[1;35m######## %s\033[0m\n' "$(basename "$script")"
    sh "$script" || true
done

printf '\n\n\033[1mdone.\033[0m Send the whole of this output.\n'
