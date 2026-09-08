#!/usr/bin/env bash
#
# bootstrap.sh -- assemble a squared-pg working tree from its sibling
# repositories.
#
#   ./tools/bootstrap.sh          clone or update to the pinned revisions
#   ./tools/bootstrap.sh --check  report what is present and what is pinned
#   ./tools/bootstrap.sh --update record the current checkouts as the new pins
#   ./tools/bootstrap.sh --latest fetch, move each component to its default
#                                 branch, and record that as the new pin
#
# squared-pg is three repositories:
#
#   squared-pg   this one: the engine, the control layer, the CLI, the spec
#   sqcart       the cartridge container library and its tool
#   squared      the framework: templates, kits, packages, assets
#
# They are assembled as:
#
#   sqcart/                  a sibling of engine/ -- it is vendored source,
#                            compiled into this build.
#   resources/.squared/      the squared clone, hidden.
#   resources/templates/     -> .squared/resources/templates
#   resources/kits/          -> .squared/resources/kits
#   resources/packages/      -> .squared/resources/packages
#   resources/assets/        -> .squared/resources/assets
#
# The asymmetry is the point. sqcart is code this project links; squared is
# data this project indexes, and data belongs where the engine looks for it.
#
# ## Why symlinks
#
# `resources/` must read as the resource directory:
#
#     resources/assets  generator  kits  packages  templates
#
# The `squared` repository owns a `resources/` directory of its own, so
# cloning it under resources/ would give `resources/squared/resources/kits` --
# three levels to say one thing, and a layout that only makes sense once you
# know which repository each level came from.
#
# The clone is therefore hidden and the four namespace directories are linked
# beside `generator/`. The engine tests `is_directory`, which follows
# symlinks, so a linked namespace scans exactly like a real one and the index
# cannot tell the difference.
#
# Moving files out of the clone would have been the other option. It would
# leave the clone permanently dirty and break `git pull`, which makes updating
# the framework a manual reconciliation rather than one command.
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
    --latest)
    # The mode that was missing.
    #
    # Without it there was no way to move a pin forward: `bootstrap.sh`
    # checks out the pin, and `--update` records whatever is checked out --
    # so running them in sequence writes the old pin straight back. That is
    # circular, and it silently reverted a pushed change while every
    # individual step reported success (D-060).
    #
    # `git pull` was not the answer either: bootstrap leaves each component
    # on a detached HEAD at the pinned commit, and pull refuses there.
    for pair in "sqcart sqcart SQCART_VERSION" \
                "squared resources/.squared SQUARED_VERSION"; do
        set -- $pair
        name="$1"; dir="$2"; pin_file="$3"
        [ -d "$dir/.git" ] || { printf '  %-10s absent; run without --latest first\n' "$name"; continue; }

        if ! git -C "$dir" diff --quiet 2>/dev/null; then
            printf '  %-10s uncommitted changes; leaving it alone\n' "$name"
            continue
        fi

        git -C "$dir" fetch --quiet --all --tags
        # The remote's own default branch, not an assumed "main": a component
        # is free to call it something else, and guessing would fail in a way
        # that looks like a network problem.
        head_ref="$(git -C "$dir" rev-parse --abbrev-ref origin/HEAD 2>/dev/null || echo origin/main)"
        branch="${head_ref#origin/}"
        git -C "$dir" checkout --quiet "$branch" 2>/dev/null \
            || git -C "$dir" checkout --quiet -B "$branch" "$head_ref"
        git -C "$dir" merge --quiet --ff-only "$head_ref" 2>/dev/null || true
        git -C "$dir" rev-parse HEAD > "$pin_file"
        printf '  %-10s -> %s (%s)\n' "$name" "$(git -C "$dir" rev-parse --short HEAD)" "$branch"
    done

    printf '\nRe-linking.\n'
    "$0" >/dev/null
    printf '\nPins updated. Commit the version files, and check the build:\n\n'
    printf '  make -j4 && make check\n'
    ;;

update) mode="update" ;;
    --latest) mode="latest" ;;
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
    report squared resources/.squared "$squared_pin"
    ;;

latest)
    # The mode that was missing.
    #
    # Without it there was no way to move a pin forward: `bootstrap.sh`
    # checks out the pin, and `--update` records whatever is checked out --
    # so running them in sequence writes the old pin straight back. That is
    # circular, and it silently reverted a pushed change while every
    # individual step reported success (D-060).
    #
    # `git pull` was not the answer either: bootstrap leaves each component
    # on a detached HEAD at the pinned commit, and pull refuses there.
    for pair in "sqcart sqcart SQCART_VERSION" \
                "squared resources/.squared SQUARED_VERSION"; do
        set -- $pair
        name="$1"; dir="$2"; pin_file="$3"
        [ -d "$dir/.git" ] || { printf '  %-10s absent; run without --latest first\n' "$name"; continue; }

        if ! git -C "$dir" diff --quiet 2>/dev/null; then
            printf '  %-10s uncommitted changes; leaving it alone\n' "$name"
            continue
        fi

        git -C "$dir" fetch --quiet --all --tags
        # The remote's own default branch, not an assumed "main": a component
        # is free to call it something else, and guessing would fail in a way
        # that looks like a network problem.
        head_ref="$(git -C "$dir" rev-parse --abbrev-ref origin/HEAD 2>/dev/null || echo origin/main)"
        branch="${head_ref#origin/}"
        git -C "$dir" checkout --quiet "$branch" 2>/dev/null \
            || git -C "$dir" checkout --quiet -B "$branch" "$head_ref"
        git -C "$dir" merge --quiet --ff-only "$head_ref" 2>/dev/null || true
        git -C "$dir" rev-parse HEAD > "$pin_file"
        printf '  %-10s -> %s (%s)\n' "$name" "$(git -C "$dir" rev-parse --short HEAD)" "$branch"
    done

    printf '\nRe-linking.\n'
    "$0" >/dev/null
    printf '\nPins updated. Commit the version files, and check the build:\n\n'
    printf '  make -j4 && make check\n'
    ;;

update)
    for pair in "SQCART_VERSION sqcart sqcart" \
                "SQUARED_VERSION resources/.squared squared"; do
        set -- $pair
        # Three fields: the pin file, the directory, and the component's name.
        # The name is carried rather than derived, because basename of
        # resources/.squared is ".squared", which is a directory rather than a
        # component and reads like a typo in the output.
        if [ -d "$2/.git" ]; then
            git -C "$2" rev-parse HEAD > "$1"
            printf '  %-10s pinned to %s\n' "$3" "$(cat "$1")"
        fi
    done
    printf '\nCommit the version files: they are the record of what this tree was\n'
    printf 'tested against.\n'
    ;;

assemble)
    printf 'bootstrap.sh: assembling into %s\n' "$here"
    assemble sqcart  sqcart  "$SQCART_REMOTE"  "$sqcart_pin"
    assemble squared resources/.squared "$SQUARED_REMOTE" "$squared_pin"

    # Link the namespace directories beside generator/, so `resources/` reads
    # as the resource directory rather than as a place two repositories meet.
    #
    # Two layouts accepted. The `squared` repository originally held its four
    # kinds under a `resources/` directory of its own, which made the link
    # target `.squared/resources/kits`; flattening them to its root makes it
    # `.squared/kits`. Both are supported so the two repositories can be
    # updated in either order -- the pins already make their versions
    # independent, and a bootstrap that only understood one of them would turn
    # a `git mv` in another repository into a coordinated release.
    inner=""
    if [ -d resources/.squared/templates ]; then
        inner=""
    elif [ -d resources/.squared/resources/templates ]; then
        inner="resources/"
    fi

    if [ -d resources/.squared ] && [ -d "resources/.squared/${inner}templates" ]; then
        for kind in templates kits packages assets; do
            target="resources/$kind"
            if [ -e "$target" ] && [ ! -L "$target" ]; then
                printf '  %-10s %s exists and is not a link; leaving it\n' "$kind" "$target"
                continue
            fi
            ln -sfn ".squared/${inner}$kind" "$target"
        done
        printf '  linked     templates kits packages assets -> .squared/%s\n' "$inner"
    fi

    # No symlink and no copy. The engine scans squared/resources directly
    # alongside resources/generator, because a link would put the generator's
    # own resources inside the framework repository and a copy would give two
    # trees that drift -- with the drifted one being what the engine reads.

    printf '\nAssembled. Build with:\n\n  make -j4 && make check\n'
    ;;
esac
