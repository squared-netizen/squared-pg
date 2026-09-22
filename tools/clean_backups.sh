#!/usr/bin/env bash
# Remove the timestamped backups squared installers leave behind.
#
#   clean_backups.sh <dir>            list what would go, delete nothing
#   clean_backups.sh <dir> --delete   delete them
#
# Only files named exactly  <name>.bak.YYYYMMDD-HHMMSS  are considered - the
# pattern the installers write. A file that merely contains ".bak" is never
# touched.
#
# A backup whose original no longer exists is KEPT and reported. It may be the
# only copy of something, and deleting it would be the one irreversible thing
# this script can do.
#
# Listing is the default because deletion is permanent. .git is never entered.
set -euo pipefail

usage() {
  echo "usage: $0 <directory> [--delete]" >&2
  exit 2
}

[ "$#" -ge 1 ] || usage
target="$1"
mode="${2:-list}"

case "$mode" in
  list|--delete) ;;
  *) usage ;;
esac

if [ ! -d "$target" ]; then
  echo "error: $target is not a directory" >&2
  exit 1
fi
target="$(cd "$target" && pwd)"

# A guard against a mistyped argument sweeping far more than intended.
case "$target" in
  /|"$HOME"|/data|/data/data|/data/data/com.termux|/data/data/com.termux/files)
    echo "error: refusing to run on $target" >&2
    exit 1
    ;;
esac

# Eight digits, a hyphen, six digits: date +%Y%m%d-%H%M%S.
stamp='[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]-[0-9][0-9][0-9][0-9][0-9][0-9]'

redundant=0
orphaned=0
list="$(mktemp)"
trap 'rm -f "$list"' EXIT

while IFS= read -r backup; do
  original="${backup%.bak.*}"
  if [ -e "$original" ]; then
    printf '%s\n' "$backup" >> "$list"
    redundant=$((redundant + 1))
  else
    echo "  keeping  ${backup#"$target"/}  (no original - may be the only copy)"
    orphaned=$((orphaned + 1))
  fi
done < <(find "$target" -name .git -prune -o -type f -name "*.bak.$stamp" -print | sort)

if [ "$redundant" -eq 0 ]; then
  echo "no redundant backups under $target"
  [ "$orphaned" -eq 0 ] || echo "$orphaned orphaned backup(s) kept"
  exit 0
fi

if [ "$mode" = "--delete" ]; then verb="removing"; else verb="would remove"; fi
while IFS= read -r backup; do
  echo "  $verb  ${backup#"$target"/}"
done < "$list"

if [ "$mode" = "--delete" ]; then
  while IFS= read -r backup; do
    rm -f -- "$backup"
  done < "$list"
  echo "removed $redundant backup(s)"
else
  echo "$redundant backup(s) would be removed. Re-run with --delete to remove them."
fi
[ "$orphaned" -eq 0 ] || echo "$orphaned orphaned backup(s) kept"
