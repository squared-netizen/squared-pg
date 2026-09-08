#!/usr/bin/env bash
#
# bootstrap.sh -- assemble a squared-pg working tree from its sibling
# repositories.
#
#   ./tools/bootstrap.sh          clone or update to the pinned revisions
#   ./tools/bootstrap.sh --check  report what is present and what is pinned
#   ./tools/bootstrap.sh --update record the current checkouts as the new pins
#
# squared-pg is three repositories:
#
#   squared-pg   this one: the engine, the control layer, the CLI, the spec
#   sqcart       the cartridge container library and its tool
#   squared      the framework: templates, kits, packages, assets
#
# ## Why a script rather than submodules
#
# Both work. A submodule pins by construction and clones with one flag; a
# script has to pin deliberately, which is what the version files below are
# for. The script wins on one thing that matters here: a contributor working on
# sqcart gets an ordinary checkout on an ordinary branch, not a detached HEAD
# they have to remember to fix before committing. That case is the daily one
# for this project, because the same person maintains all three.
#
# ## The pins are not optional
#
# Without them this script means "give me whatever main happens to be", and the
# day sqcart moves to cartridge format 3 while this engine still implements 2,
# a fresh clone builds cleanly and then refuses every cartridge -- with an
# error that names the cartridge, not the checkout. The failure would be
# reproducible only by accident.
#
# Network is required here and here only. Once assembled, nothing in the
# generate/build/package chain touches the network (Repository invariant
# 1.13).

set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$here"

SQCART_REMOTE="${SQCART_REMOTE:-https://github.com/squared-netizen/sqcart.git}"
SQUARED_REMOTE="${SQUARED_REMOTE:-https://github.com/squared-netizen/squared.git}"

mode="assemble"
case "${1:-}" in
    --check)  mode="check" ;;
    --update) mode="update" ;;
    -h|--help)
        sed -n '3,9p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
        exit 0 ;;
    "") ;;
    *) printf 'bootstrap.sh: unknown option %s\n' "$1" >&2; exit 2 ;;
esac

# ---------------------------------------------------------------------------

pinned_ref() {
    # A pin file holds one line: a commit, a tag, or a branch name. Missing or
    # empty means "track the default branch", which is the honest reading of a
    # file nobody has filled in -- better than pretending to a precision the
    # repository does not have.
    local file="$1"
    [ -f "$file" ] || { printf 'main\n'; return; }
    local ref
    ref="$(grep -v '^[[:space:]]*#' "$file" | grep -m1 '[^[:space:]]' || true)"
    printf '%s\n' "${ref:-main}"
}

current_ref() {
    git -C "$1" rev-parse --short HEAD 2>/dev/null || printf 'unknown\n'
}

report() {
    local name="$1" dir="$2" pin="$3"
    if [ ! -d "$dir/.git" ] && [ ! -d "$dir" ]; then
        printf '  %-10s absent      pinned to %s\n' "$name" "$pin"
    elif [ ! -d "$dir/.git" ]; then
        # A directory with no repository. Almost always a leftover from before
        # the split, and overwriting it would destroy uncommitted work.
        printf '  %-10s NOT A REPO  %s exists but is not a git checkout\n' "$name" "$dir"
    else
        local now dirty
        now="$(current_ref "$dir")"
        dirty=""
        git -C "$dir" diff --quiet 2>/dev/null || dirty=" (uncommitted changes)"
        printf '  %-10s %-11s pinned to %s%s\n' "$name" "$now" "$pin" "$dirty"
    fi
}

assemble() {
    local name="$1" dir="$2" remote="$3" pin="$4"

    if [ -d "$dir" ] && [ ! -d "$dir/.git" ]; then
        printf 'bootstrap.sh: %s exists and is not a git checkout.\n' "$dir" >&2
        printf '              Refusing to touch it. Move it aside if it is stale.\n' >&2
        return 1
    fi

    if [ ! -d "$dir" ]; then
        printf '  cloning %s\n' "$name"
        git clone --quiet "$remote" "$dir"
    fi

    # Fetch before checking out: the pin may name a commit this clone has never
    # seen, and a checkout that fails after a successful clone is a confusing
    # place to stop.
    git -C "$dir" fetch --quiet --all --tags

    if ! git -C "$dir" diff --quiet 2>/dev/null; then
        printf '  %-10s has uncommitted changes; leaving it alone\n' "$name"
        printf '             (pinned to %s; commit or stash to move it)\n' "$pin"
        return 0
    fi

    printf '  %-10s -> %s\n' "$name" "$pin"
    git -C "$dir" checkout --quiet "$pin"
}

# ---------------------------------------------------------------------------

sqcart_pin="$(pinned_ref SQCART_VERSION)"
squared_pin="$(pinned_ref SQUARED_VERSION)"

case "$mode" in
check)
    printf 'squared-pg components:\n'
    report sqcart  sqcart  "$sqcart_pin"
    report squared squared "$squared_pin"
    ;;

update)
    for pair in "SQCART_VERSION sqcart" "SQUARED_VERSION squared"; do
        set -- $pair
        if [ -d "$2/.git" ]; then
            git -C "$2" rev-parse HEAD > "$1"
            printf '  %-10s pinned to %s\n' "$2" "$(cat "$1")"
        fi
    done
    printf '\nCommit the version files: they are the record of what this tree was\n'
    printf 'tested against.\n'
    ;;

assemble)
    printf 'bootstrap.sh: assembling into %s\n' "$here"
    assemble sqcart  sqcart  "$SQCART_REMOTE"  "$sqcart_pin"
    assemble squared squared "$SQUARED_REMOTE" "$squared_pin"

    # No symlink and no copy. The engine scans squared/resources directly
    # alongside resources/generator, because a link would put the generator's
    # own resources inside the framework repository and a copy would give two
    # trees that drift -- with the drifted one being what the engine reads.

    printf '\nAssembled. Build with:\n\n  make -j4 && make check\n'
    ;;
esac
