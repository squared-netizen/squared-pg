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
#   resources/templates/     -> .squared/templates
#   resources/kits/          -> .squared/kits
#   resources/packages/      -> .squared/packages
#   resources/assets/        -> .squared/assets
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
# ## What a pin file may contain (D-063)
#
# One line: a semantic version, a commit SHA, a tag, or a branch name.
#
# A semantic version -- `0.2.0` -- is resolved to a tag, `v0.2.0` or `0.2.0`,
# whichever the component carries. This is the preferred form: it is the one
# thing a human can read, compare against a manifest's `requires` range, and
# edit in one place. It names an exact tag; it is not a range. Ranges belong
# in cartridge manifests, where they describe compatibility, not in a pin
# file, where the job is to say precisely which tree was tested.
#
# A SHA still works and stays exact. It is what `--update` records when the
# component has no tag at HEAD, because inventing one would be a release
# decision this script has no business making.
#
# A semver pin is reproducible only so long as tags do not move. That is now
# a stated invariant of this repository: a published tag in sqcart or squared
# is never repointed. Re-tagging silently changes what every past commit of
# squared-pg builds against.
#
# ## Why the components sit on a branch (D-062)
#
# `git checkout <sha>` leaves a detached HEAD. A commit made there belongs to
# no branch and is reachable only by its SHA -- one stray checkout from being
# unreachable, with no warning at any point. That happened: the D-057 kit
# include flatten was committed detached and survived by luck.
#
# So each component is placed on a branch named `pin/<ref>` pointing at the
# pinned commit. Edits land somewhere durable by default, `git push` has a
# target, and the pin file still says exactly which commit the branch started
# from.
#
# If that branch already exists and carries commits ahead of the pin, this
# script says so and leaves it alone. Moving it would discard work, which is
# the failure this change exists to prevent.
#
# Network is required here and here only. Once assembled, nothing in the
# generate/build/package chain touches the network (Repository invariant
# 1.13).

set -eu

here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$here"

SQCART_REMOTE="${SQCART_REMOTE:-https://github.com/squared-netizen/sqcart.git}"
SQUARED_REMOTE="${SQUARED_REMOTE:-https://github.com/squared-netizen/squared.git}"

mode="assemble"
case "${1:-}" in
    --check)  mode="check" ;;
    --update) mode="update" ;;
    --latest) mode="latest" ;;
    -h|--help)
        sed -n '3,10p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
        exit 0 ;;
    "") ;;
    *) printf 'bootstrap.sh: unknown option %s\n' "$1" >&2; exit 2 ;;
esac

# ---------------------------------------------------------------------------

pinned_ref() {
    # A pin file holds one line: a semantic version, a commit, a tag, or a
    # branch name. Missing or empty means "track the default branch", which is
    # the honest reading of a file nobody has filled in -- better than
    # pretending to a precision the repository does not have.
    local file="$1"
    [ -f "$file" ] || { printf 'main\n'; return; }
    local ref
    ref="$(grep -v '^[[:space:]]*#' "$file" | grep -m1 '[^[:space:]]' || true)"
    printf '%s\n' "${ref:-main}"
}

is_semver() {
    # X.Y.Z, with an optional pre-release or build suffix. Deliberately not a
    # range: see the pin-file note in the header.
    [[ "$1" =~ ^[0-9]+\.[0-9]+\.[0-9]+([-+][0-9A-Za-z.-]+)?$ ]]
}

resolve_pin() {
    # Map a pin to something git can check out. Only semver needs mapping; a
    # SHA, tag or branch is already a ref.
    local dir="$1" pin="$2"
    if ! is_semver "$pin"; then
        printf '%s\n' "$pin"
        return 0
    fi
    local candidate
    for candidate in "v$pin" "$pin"; do
        if git -C "$dir" rev-parse --verify -q "refs/tags/$candidate" >/dev/null 2>&1; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done
    printf 'bootstrap.sh: %s is pinned to version %s, but neither tag v%s nor %s\n' \
        "$dir" "$pin" "$pin" "$pin" >&2
    printf '              exists in that component. Tag the release there first,\n' >&2
    printf '              or pin to a commit.\n' >&2
    return 1
}

tag_at_head() {
    # The exact tag naming HEAD, if there is one. Used by --update so a semver
    # pin stays a semver pin.
    git -C "$1" describe --tags --exact-match HEAD 2>/dev/null || true
}

current_ref() {
    git -C "$1" rev-parse --short HEAD 2>/dev/null || printf 'unknown\n'
}

pin_branch() {
    # Branch name for a pin. Slashes are legal in branch names and the prefix
    # keeps these out of the way of ordinary topic branches. A full SHA is
    # abbreviated, because pin/61b4a53 is a name a person can type and
    # pin/61b4a5360abddec37583d1cf7e438690ac167e43 is not.
    local pin="$1"
    if [[ "$pin" =~ ^[0-9a-f]{40}$ ]]; then
        printf 'pin/%s\n' "${pin:0:7}"
    else
        printf 'pin/%s\n' "$pin"
    fi
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
        local now dirty branch
        now="$(current_ref "$dir")"
        dirty=""
        git -C "$dir" diff --quiet 2>/dev/null || dirty=" (uncommitted changes)"
        branch="$(git -C "$dir" rev-parse --abbrev-ref HEAD 2>/dev/null || echo '?')"
        if [ "$branch" = HEAD ]; then
            branch="DETACHED"
            dirty="$dirty (detached: commits here belong to no branch)"
        fi
        printf '  %-10s %-11s %-22s pinned to %s%s\n' "$name" "$now" "$branch" "$pin" "$dirty"
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

    # Fetch before resolving: the pin may name a commit or tag this clone has
    # never seen, and a checkout that fails after a successful clone is a
    # confusing place to stop.
    git -C "$dir" fetch --quiet --all --tags

    if ! git -C "$dir" diff --quiet 2>/dev/null; then
        printf '  %-10s has uncommitted changes; leaving it alone\n' "$name"
        printf '             (pinned to %s; commit or stash to move it)\n' "$pin"
        return 0
    fi

    local ref branch target
    ref="$(resolve_pin "$dir" "$pin")" || return 1
    branch="$(pin_branch "$pin")"
    target="$(git -C "$dir" rev-parse --verify -q "${ref}^{commit}")" || {
        printf 'bootstrap.sh: %s: cannot resolve %s to a commit\n' "$name" "$ref" >&2
        return 1
    }

    if git -C "$dir" show-ref --verify --quiet "refs/heads/$branch"; then
        local tip
        tip="$(git -C "$dir" rev-parse "refs/heads/$branch")"
        git -C "$dir" checkout --quiet "$branch"
        if [ "$tip" != "$target" ]; then
            # Ahead, behind or diverged -- all three mean somebody worked here.
            # Resetting would discard it, which is the whole reason this script
            # stopped leaving components detached.
            printf '  %-10s on %s at %s, which is not the pinned %s\n' \
                "$name" "$branch" "$(git -C "$dir" rev-parse --short HEAD)" "$(git -C "$dir" rev-parse --short "$target")"
            printf '             Leaving it. Push or reconcile it, then re-run.\n'
            return 0
        fi
    else
        git -C "$dir" checkout --quiet -b "$branch" "$target"
    fi

    printf '  %-10s -> %s on %s\n' "$name" "$pin" "$branch"
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
    # Without this mode there was no way to move a pin forward: bootstrap
    # checks out the pin, and --update records whatever is checked out -- so
    # running them in sequence writes the old pin straight back. That is
    # circular, and it silently reverted a pushed change while every
    # individual step reported success.
    #
    # `git pull` was not the answer either: before D-062 bootstrap left each
    # component on a detached HEAD, and pull refuses there.
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

        # Prefer a tag at the new HEAD so the pin stays readable. Falling back
        # to a SHA is correct but loses the property that made semver pins
        # worth having.
        new_tag="$(tag_at_head "$dir")"
        if [ -n "$new_tag" ]; then
            printf '%s\n' "${new_tag#v}" > "$pin_file"
        else
            git -C "$dir" rev-parse HEAD > "$pin_file"
        fi
        printf '  %-10s -> %s (%s)\n' "$name" "$(cat "$pin_file")" "$branch"
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
        pin_file="$1"; dir="$2"; name="$3"
        [ -d "$dir/.git" ] || continue

        old=""
        [ -f "$pin_file" ] && old="$(pinned_ref "$pin_file")"
        tag="$(tag_at_head "$dir")"

        if [ -n "$tag" ]; then
            # A tag at HEAD is the most durable thing to record, whatever the
            # previous pin looked like.
            printf '%s\n' "${tag#v}" > "$pin_file"
            printf '  %-10s pinned to %s (tag %s)\n' "$name" "$(cat "$pin_file")" "$tag"
        elif is_semver "$old"; then
            # Refusing rather than writing a SHA: overwriting a version pin
            # with a commit silently changes the pin's kind, and the next
            # reader has no way to tell it was not deliberate.
            printf '  %-10s pinned to version %s but HEAD %s carries no tag.\n' \
                "$name" "$old" "$(current_ref "$dir")" >&2
            printf '             Not overwriting. Tag the release, or pin to a\n' >&2
            printf '             commit by writing the SHA yourself.\n' >&2
        else
            git -C "$dir" rev-parse HEAD > "$pin_file"
            printf '  %-10s pinned to %s\n' "$name" "$(cat "$pin_file")"
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
