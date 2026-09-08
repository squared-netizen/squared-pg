#!/usr/bin/env bash
#
# split.sh -- ONE TIME. Carve sqcart/ and the framework resources out of this
# repository's history into two branches you can pull into their own clones.
#
#   ./tools/split.sh --dry-run    show what would happen, touch nothing
#   ./tools/split.sh              create the branches
#
# ## It creates branches, not repositories
#
# An earlier draft cloned into sibling directories and stripped their remotes.
# That was wrong for the situation this project is actually in: the `squared`
# repository **already exists**, with commits, a README, a licence and a CI
# workflow. A fresh clone would have had to replace it, and replacing a
# repository to avoid a merge commit is a bad trade -- the CI and the clang
# configs there are exactly what kit.format will eventually own.
#
# So this produces two local branches and stops. You pull them into the clones
# you already have, from the side that owns the history. Nothing here touches
# a network, and nothing here can lose a commit.
#
# ## What moves
#
#   sqcart/                     -> branch split/sqcart, contents at the root
#   resources/{templates,kits,
#              packages,assets} -> branch split/squared, under resources/
#
# ## What stays
#
#   resources/generator/        template.kit and its successors. They produce
#                               cartridges rather than programs, and mean
#                               something to someone who has never heard of
#                               the Squared framework -- which is the test.
#   everything else             engine, app, lua, docs, tools, third-party.
#
# `git subtree split` rather than filter-repo: it ships with git, needs no
# plugin, and keeps the commit history. That history is the most valuable
# thing sqcart has -- cartridge format 2 exists because of a specific
# argument, and the argument is in the log.

set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$here"

dry=0
[ "${1:-}" = "--dry-run" ] && dry=1

run() {
    if [ "$dry" -eq 1 ]; then
        printf '  would run: %s\n' "$*"
    else
        printf '  %s\n' "$*"
        "$@"
    fi
}

# ---------------------------------------------------------------------------
# Preconditions
# ---------------------------------------------------------------------------
#
# A split reads committed history. Work that is not committed does not travel,
# and the discrepancy surfaces later, in the new repository, where it looks
# like the split lost it.

[ -d .git ] || { printf 'split.sh: not a git repository\n' >&2; exit 1; }

if ! git diff --quiet || ! git diff --cached --quiet; then
    printf 'split.sh: this tree has uncommitted changes.\n' >&2
    printf '          Commit or stash them first: the split reads committed\n' >&2
    printf '          history, so anything uncommitted would silently not travel.\n' >&2
    exit 1
fi

if [ -n "$(git status --porcelain --untracked-files=normal)" ]; then
    printf 'split.sh: untracked files present. They will NOT travel.\n\n' >&2
    git status --porcelain --untracked-files=normal | sed 's/^/          /' >&2
    printf '\n          Commit what should move, delete what should not.\n' >&2
    exit 1
fi

printf 'split.sh: source %s\n' "$here"
printf '          HEAD %s\n\n' "$(git rev-parse --short HEAD)"

# ---------------------------------------------------------------------------

printf 'sqcart -> branch split/sqcart\n'
git branch -D split/sqcart >/dev/null 2>&1 || true
run git subtree split --prefix=sqcart -b split/sqcart
printf '\n'

printf 'framework resources -> branch split/squared\n'
git branch -D split/squared >/dev/null 2>&1 || true
run git subtree split --prefix=resources -b split/squared
printf '\n'

if [ "$dry" -eq 1 ]; then
    printf 'Dry run. Nothing was written.\n'
    exit 0
fi

# split/squared has templates/ kits/ packages/ assets/ AND generator/ at its
# root, because subtree split takes one prefix and the framework resources are
# four siblings of a directory that must not travel.
#
# Reshaping happens in a commit on the branch, rather than by excluding paths
# during the split. An exclusion would be invisible afterwards; a commit says
# what it did and why, to anyone auditing how the repository came to be.
printf 'reshaping split/squared\n'
work="$(mktemp -d "${TMPDIR:-/tmp}/split-squared.XXXXXX")"
rmdir "$work"
git worktree add --quiet "$work" split/squared
(
    cd "$work"
    mkdir -p resources
    for d in templates kits packages assets; do
        [ -e "$d" ] && git mv "$d" "resources/$d"
    done
    [ -e generator ] && git rm -rq generator
    git -c user.email=split@local -c user.name=split commit -q -m \
"Reshape into the squared repository layout

Move the four resource kinds under resources/, and drop generator/.

generator/ holds template.kit and its successors, which produce cartridges
rather than programs and mean something to a consumer that has never heard of
the Squared framework. Those stay with squared-pg, which owns the formats they
describe." || true
)
git worktree remove --force "$work"
printf '  done\n\n'

cat <<'EOF'
Two branches exist in this repository. Nothing has been pushed.

Look at them first:

  git log  --oneline split/sqcart  | head
  git log  --oneline split/squared | head
  git ls-tree --name-only split/squared

--- sqcart -------------------------------------------------------------

Create the repository on your host, clone it, then pull the branch in:

  cd ~/github
  git clone <sqcart url> sqcart
  cd sqcart
  git pull ~/projects/squared-pg split/sqcart --allow-unrelated-histories
  git push

An empty new repository has nothing to merge with, so this is a
fast-forward rather than a merge.

--- squared ------------------------------------------------------------

This one already has commits, so it is a real merge:

  cd ~/github/squared
  git pull ~/projects/squared-pg split/squared --allow-unrelated-histories
  git push

The merge joins two unrelated roots and will look odd in the log forever.
It is the right trade: that repository already holds the README, the
licence, the CI workflow and the clang configs, and replacing it to keep a
tidy log would throw those away.

--- then, here ---------------------------------------------------------

  git rm -r --cached sqcart resources/templates resources/kits \
                     resources/packages resources/assets
  rm -rf sqcart resources/templates resources/kits \
         resources/packages resources/assets
  printf 'sqcart/\nsquared/\n' >> .gitignore
  ./tools/bootstrap.sh                # clone them back as siblings
  ./tools/bootstrap.sh --update       # record what you just pushed
  make -j4 && make check
  git add -A && git commit -m "Split sqcart and squared into their own repositories"

Delete the split branches once the pushes have landed:

  git branch -D split/sqcart split/squared
EOF
