#!/usr/bin/env bash
#
# split.sh -- ONE TIME. Carve sqcart/ and the framework resources out of
# squared-pg into their own repositories, keeping their history.
#
#   ./tools/split.sh --dry-run    show what would happen, touch nothing
#   ./tools/split.sh              do it, into ../sqcart-split and ../squared-split
#
# It writes to sibling directories and pushes nothing. Reviewing the result
# and pushing is yours -- a script that force-pushes to a remote it inferred
# is not a script anyone should run once, let alone twice.
#
# ## What moves
#
#   sqcart/                     -> the sqcart repository, contents at its root
#   resources/{templates,kits,
#              packages,assets} -> the squared repository, under resources/
#
# ## What stays
#
#   resources/generator/        template.kit and its successors. They produce
#                               cartridges, not programs, and mean something
#                               to someone who has never heard of the Squared
#                               framework -- which is the test.
#   everything else             engine, app, lua, docs, tools, third-party.
#
# ## Why filter-branch and not a fresh repo
#
# A fresh repository is one command and loses every commit message explaining
# why the cartridge format is the way it is. That history is the most valuable
# thing sqcart has: format 2 exists because of a specific argument, and the
# argument is in the log.
#
# `git subtree split` is used rather than `filter-branch` or `filter-repo`:
# it ships with git, needs no plugin, and produces exactly one branch of the
# subdirectory's history.

set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$here"

dry=0
[ "${1:-}" = "--dry-run" ] && dry=1

say() { printf '%s\n' "$*"; }
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
# A split reads history and writes new repositories. Doing that from a tree
# with uncommitted work means the split does not contain the work, and the
# discrepancy will not show up until much later.

[ -d .git ] || { say "split.sh: not a git repository"; exit 1; }

if ! git diff --quiet || ! git diff --cached --quiet; then
    say "split.sh: this tree has uncommitted changes."
    say "          Commit or stash them: the split reads committed history,"
    say "          so anything uncommitted would silently not travel."
    exit 1
fi

if [ -n "$(git status --porcelain --untracked-files=normal)" ]; then
    say "split.sh: untracked files present. They will NOT travel with the split."
    say ""
    git status --porcelain --untracked-files=normal | sed 's/^/          /'
    say ""
    say "          Commit what should move, delete what should not, then re-run."
    exit 1
fi

say "split.sh: source $here"
say "          HEAD $(git rev-parse --short HEAD)"
say ""

# ---------------------------------------------------------------------------
# sqcart
# ---------------------------------------------------------------------------

say "sqcart -> ../sqcart-split"
run git subtree split --prefix=sqcart -b split/sqcart
run rm -rf ../sqcart-split
run git clone --quiet --branch split/sqcart --single-branch . ../sqcart-split
if [ "$dry" -eq 0 ]; then
    # The clone carries a remote pointing back at this working tree, which
    # would be wrong the first time anyone pushed.
    git -C ../sqcart-split remote remove origin
    git -C ../sqcart-split branch -m split/sqcart main
fi
say ""

# ---------------------------------------------------------------------------
# squared
# ---------------------------------------------------------------------------
#
# Harder than sqcart: the framework resources are four directories under
# resources/, not one prefix, and resources/generator/ must stay behind.
# `subtree split` takes one prefix, so this splits `resources` whole and then
# removes the generator tree in a commit of its own -- which keeps the history
# of every template and kit and costs one extra commit that says what it did.

say "squared -> ../squared-split"
run git subtree split --prefix=resources -b split/squared
run rm -rf ../squared-split
run git clone --quiet --branch split/squared --single-branch . ../squared-split
if [ "$dry" -eq 0 ]; then
    git -C ../squared-split remote remove origin
    git -C ../squared-split branch -m split/squared main

    # Reshape: the split branch has templates/ and kits/ at its root, and the
    # squared repository wants them under resources/.
    cd ../squared-split
    mkdir -p resources
    for d in templates kits packages assets; do
        [ -e "$d" ] && git mv "$d" "resources/$d"
    done
    # generator/ belongs to squared-pg, not to the framework.
    [ -e generator ] && git rm -rq generator
    git -c user.email=split@local -c user.name=split commit -q \
        -m "Reshape into the squared repository layout

Move the four resource kinds under resources/, and drop generator/ --
template.kit and its successors produce cartridges rather than programs and
stay with squared-pg." || true
    cd "$here"
fi
say ""

# ---------------------------------------------------------------------------

if [ "$dry" -eq 1 ]; then
    say "Dry run. Nothing was written."
    exit 0
fi

cat <<'EOF'
Done. Two new repositories, neither pushed:

  ../sqcart-split
  ../squared-split

Check them before pushing anything:

  git -C ../sqcart-split  log --oneline | head
  git -C ../squared-split log --oneline | head
  ls ../squared-split/resources

Then, for each:

  git remote add origin <url>
  git push -u origin main

Afterwards, in this repository:

  git rm -r --cached sqcart resources/templates resources/kits \
                     resources/packages resources/assets
  echo 'sqcart/'  >> .gitignore
  echo 'squared/' >> .gitignore
  ./tools/bootstrap.sh --update     # record what you just pushed
  git add -A && git commit -m "Split sqcart and squared into their own repositories"

The local branches split/sqcart and split/squared are left in place. Delete
them once you are satisfied:

  git branch -D split/sqcart split/squared
EOF
