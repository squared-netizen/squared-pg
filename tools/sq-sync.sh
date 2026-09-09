#!/usr/bin/env bash
# sq-sync.sh — status and publish workflow for squared-pg's vendored repos.
#
# resources/.squared and sqcart/ are separate git repositories checked out at
# refs pinned by SQUARED_VERSION and SQCART_VERSION. Bootstrap leaves them
# detached, so commits made there point at nothing and are one stray checkout
# from being unreachable. This script names that work, publishes it, and
# closes the loop by updating the pin.
#
# Usage:
#   tools/sq-sync.sh                          status of both vendored repos
#   tools/sq-sync.sh --name <branch>          branch the detached work
#   tools/sq-sync.sh --land                   fast-forward main, push
#   tools/sq-sync.sh --pin                    show pin update for squared-pg
#   tools/sq-sync.sh --repo squared --land --apply
#
#   --repo    squared | sqcart | all   (default: all)
#   --apply   perform writes (default: dry-run)
#
# Dry-run by default. Never resets, never force-pushes, never discards commits.

set -euo pipefail

REPO_SEL=all
DO_NAME=""
DO_LAND=0
DO_PIN=0
MODE=dry
ROOT=""

while [ $# -gt 0 ]; do
    case "$1" in
        --repo)  REPO_SEL="${2:?--repo needs squared|sqcart|all}"; shift 2 ;;
        --name)  DO_NAME="${2:?--name needs a branch name}"; shift 2 ;;
        --land)  DO_LAND=1; shift ;;
        --pin)   DO_PIN=1; shift ;;
        --apply) MODE=apply; shift ;;
        --root)  ROOT="${2:?--root needs a path}"; shift 2 ;;
        -h|--help) sed -n '2,22p' "$0"; exit 0 ;;
        -*) printf 'unknown option: %s\n' "$1" >&2; exit 2 ;;
        *)  if [ -z "$ROOT" ]; then ROOT="$1"; shift
            else printf 'unexpected argument: %s\n' "$1" >&2; exit 2; fi ;;
    esac
done

say()  { printf '%s\n' "$*"; }
step() { printf '\n== %s\n' "$*"; }
warn() { printf 'warning: %s\n' "$*" >&2; }
die()  { printf 'error: %s\n' "$*" >&2; exit 1; }
run()  { if [ "$MODE" = apply ]; then "$@"; else printf '  would run: %s\n' "$*"; fi; }

# ---------------------------------------------------------------- repo root

find_root() {
    local d
    d="$(cd "${1:-$PWD}" 2>/dev/null && pwd)" || return 1
    while [ "$d" != "/" ]; do
        [ -d "$d/resources" ] && [ -d "$d/lua/workflows" ] && { printf '%s\n' "$d"; return 0; }
        d="$(dirname "$d")"
    done
    return 1
}

if [ -n "$ROOT" ]; then
    ROOT="$(cd "$ROOT" && pwd)" || die "not a directory"
    [ -d "$ROOT/resources" ] || ROOT="$(find_root "$ROOT")" || die "no checkout at or above it"
else
    ROOT="$(find_root "$PWD")" || die "no squared-pg checkout found above $PWD"
fi

say "squared-pg:  $ROOT"
[ "$MODE" = apply ] && say "mode:        APPLY" || say "mode:        dry-run (pass --apply to write)"

# ------------------------------------------------------- vendored repo table

# name | working tree | pin file
VENDORED=(
    "squared|$ROOT/resources/.squared|$ROOT/SQUARED_VERSION"
    "sqcart|$ROOT/sqcart|$ROOT/SQCART_VERSION"
)

selected() {
    [ "$REPO_SEL" = all ] && return 0
    [ "$REPO_SEL" = "$1" ] && return 0
    return 1
}

# --------------------------------------------------------------- inspection

# Prints: state of one vendored repo. Sets globals for the caller.
REPO_HEAD=""; REPO_BRANCH=""; REPO_DETACHED=0; REPO_DIRTY=0; REPO_ORPHAN=0

inspect() {
    local dir="$1"
    if ! git -C "$dir" rev-parse --verify -q HEAD >/dev/null 2>&1; then
        REPO_HEAD=""; REPO_BRANCH=""; REPO_DETACHED=0; REPO_DIRTY=0; REPO_ORPHAN=0
        return 1
    fi
    REPO_HEAD="$(git -C "$dir" rev-parse --short HEAD)"
    REPO_DIRTY=0; REPO_ORPHAN=0; REPO_BRANCH=""; REPO_DETACHED=0

    if git -C "$dir" symbolic-ref -q HEAD >/dev/null 2>&1; then
        REPO_BRANCH="$(git -C "$dir" rev-parse --abbrev-ref HEAD)"
    else
        REPO_DETACHED=1
    fi

    [ -n "$(git -C "$dir" status --porcelain)" ] && REPO_DIRTY=1

    # Orphan risk: detached, and HEAD is on no branch anywhere in the repo.
    if [ "$REPO_DETACHED" -eq 1 ]; then
        # for-each-ref, not `branch --contains`: the latter emits a
        # "(HEAD detached at ...)" pseudo-entry that reads as a real branch.
        if [ -z "$(git -C "$dir" for-each-ref --contains HEAD --format='%(refname)' refs/heads 2>/dev/null)" ]; then
            REPO_ORPHAN=1
        fi
    fi
}

step "vendored repositories"

ANY_ORPHAN=0
for entry in "${VENDORED[@]}"; do
    IFS='|' read -r name dir pinfile <<<"$entry"
    selected "$name" || continue

    if [ ! -d "$dir" ]; then
        say "  $name: not present at ${dir#"$ROOT"/}"
        continue
    fi
    if ! git -C "$dir" rev-parse --git-dir >/dev/null 2>&1; then
        say "  $name: present but not a git repository"
        continue
    fi

    if ! inspect "$dir"; then
        say "  $name: no commits yet (unborn HEAD)"
        continue
    fi
    pin="(no pin file)"
    [ -f "$pinfile" ] && pin="$(tr -d '[:space:]' < "$pinfile")"

    say "  $name"
    say "    path      ${dir#"$ROOT"/}"
    say "    HEAD      $REPO_HEAD"
    if [ "$REPO_DETACHED" -eq 1 ]; then
        say "    branch    (detached)"
    else
        say "    branch    $REPO_BRANCH"
    fi
    say "    pin       $pin  <- ${pinfile#"$ROOT"/}"
    [ "$REPO_DIRTY" -eq 1 ] && say "    tree      UNCOMMITTED CHANGES"

    if [ "$REPO_ORPHAN" -eq 1 ]; then
        ANY_ORPHAN=1
        warn "$name: HEAD $REPO_HEAD is on no branch — reachable only by SHA"
        warn "  name it before anything else:  tools/sq-sync.sh --repo $name --name <branch> --apply"
    fi
done

# -------------------------------------------------------------------- --name

if [ -n "$DO_NAME" ]; then
    step "naming detached work"
    for entry in "${VENDORED[@]}"; do
        IFS='|' read -r name dir _ <<<"$entry"
        selected "$name" || continue
        [ -d "$dir/.git" ] || [ -f "$dir/.git" ] || continue
        inspect "$dir" || { warn "$name: unborn HEAD; skipping"; continue; }
        if [ "$REPO_DETACHED" -eq 0 ]; then
            say "  $name: already on branch $REPO_BRANCH; nothing to name"
            continue
        fi
        if git -C "$dir" show-ref --verify --quiet "refs/heads/$DO_NAME"; then
            die "$name: branch $DO_NAME already exists"
        fi
        say "  $name: creating $DO_NAME at $REPO_HEAD"
        run git -C "$dir" switch -c "$DO_NAME"
    done
fi

# -------------------------------------------------------------------- --land

if [ "$DO_LAND" -eq 1 ]; then
    step "landing on main"
    for entry in "${VENDORED[@]}"; do
        IFS='|' read -r name dir _ <<<"$entry"
        selected "$name" || continue
        [ -d "$dir" ] || continue
        git -C "$dir" rev-parse --git-dir >/dev/null 2>&1 || continue

        inspect "$dir" || { warn "$name: unborn HEAD; skipping"; continue; }
        if [ "$REPO_DETACHED" -eq 1 ]; then
            warn "$name: detached; run --name first so there is something to land"
            continue
        fi
        if [ "$REPO_DIRTY" -eq 1 ]; then
            warn "$name: uncommitted changes; commit before landing"
            continue
        fi
        branch="$REPO_BRANCH"
        if [ "$branch" = main ]; then
            say "  $name: already on main"
        else
            say "  $name: publishing $branch, then fast-forwarding main"
            run git -C "$dir" push -u origin "$branch"
            run git -C "$dir" fetch origin
            run git -C "$dir" switch main
            # --ff-only: a merge commit here would mean main has diverged,
            # which is a situation for a human, not a script.
            run git -C "$dir" merge --ff-only "$branch"
        fi
        run git -C "$dir" push
    done
fi

# --------------------------------------------------------------------- --pin

if [ "$DO_PIN" -eq 1 ]; then
    step "pin update"
    for entry in "${VENDORED[@]}"; do
        IFS='|' read -r name dir pinfile <<<"$entry"
        selected "$name" || continue
        [ -d "$dir" ] || continue
        git -C "$dir" rev-parse --git-dir >/dev/null 2>&1 || continue
        [ -f "$pinfile" ] || { warn "$name: no pin file at $pinfile"; continue; }

        current="$(tr -d '[:space:]' < "$pinfile")"
        head_full="$(git -C "$dir" rev-parse HEAD)"
        head_short="$(git -C "$dir" rev-parse --short HEAD)"

        say "  $name"
        say "    pin file   ${pinfile#"$ROOT"/}"
        say "    current    $current"
        say "    HEAD       $head_short ($head_full)"

        if [ "$current" = "$head_full" ] || [ "$current" = "$head_short" ]; then
            say "    -> already pinned to HEAD"
            continue
        fi

        # The pin may be a tag or semver rather than a SHA. Writing a raw SHA
        # over a tag would silently change the pin's kind, so say what is
        # there and let the human decide.
        case "$current" in
            [0-9a-f]*) 
                if [ "${#current}" -ge 7 ] && [ "${#current}" -le 40 ]; then
                    if [ "$MODE" = apply ]; then
                        printf '%s\n' "$head_full" > "$pinfile"
                        say "    -> written: $head_full"
                    else
                        say "    -> pin looks like a SHA; would write $head_full"
                    fi
                    continue
                fi
                ;;
        esac
        warn "  $name: pin '$current' is not a SHA (tag or version?)."
        warn "    Not overwriting. Tag the new ref and update the pin by hand,"
        warn "    or re-run once the pin's kind is settled."
    done
fi

# ------------------------------------------------------------------ epilogue

step "next"
if [ "$ANY_ORPHAN" -eq 1 ]; then
    say "  Orphaned commits above. Name them before running anything else."
elif [ "$DO_PIN" -eq 1 ] && [ "$MODE" = apply ]; then
    say "  Pin updated. Commit it in squared-pg so a fresh clone gets the new"
    say "  resources ref:  git add SQUARED_VERSION SQCART_VERSION && git commit"
else
    say "  --name <branch>   name detached work"
    say "  --land            publish the branch and fast-forward main"
    say "  --pin             update squared-pg's pin to the vendored HEAD"
    say "  Add --apply to any of the above to write."
fi
