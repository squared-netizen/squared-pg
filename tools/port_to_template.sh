#!/usr/bin/env bash
# port_to_template.sh: carry build fixes made in a generated project back into
# the template it was generated from, keeping the template's {{placeholders}}.
#
#   ./port_to_template.sh --dry-run    what would change in the template, as diffs
#   ./port_to_template.sh              write the template files
#   ./port_to_template.sh --only Makefile --only sq_android/entry.cpp
#
# Options:
#   [DIR] | --demo DIR  the project (default ~/sqsysroot/project/demo, or
#                       ~/sqsysroot/project/squared-examples/demo)
#   --res DIR           squared-pg resources/.squared (default: the squared-pg
#                       this script sits in, else ~/projects/squared-pg/...)
#   --only FILE         port just this file (repeatable), path as in the project
#
# How, per file (mk/*.mk except mk/pkg_*.mk, Makefile, sq_android/entry.cpp):
#   1. Render the template with the project's values. Identical to the project
#      file: nothing to port.
#   2. Three-way merge (git merge-file): the project's changes, taken against the
#      rendered template, applied to the template itself. Lines the project
#      didn't touch keep their {{placeholders}}.
#   3. Proof: the merged template, rendered, must be byte-identical to the
#      project file. Anything else (a conflict, a change on a placeholder line,
#      an unknown placeholder) leaves that file alone and says why.
# Each replaced template file is first copied to
# ~/.cache/squared-template-backups/<timestamp>/, outside the template tree, so
# a backup can never be seeded into a new project. Commits nothing.
set -euo pipefail

DEMO=
RES=$(cd "$(dirname "$0")/.." && pwd -P)/resources/.squared
[ -d "$RES/templates" ] || RES=$HOME/projects/squared-pg/resources/.squared
DRY=0
ONLY=()
need() { [ $# -ge 2 ] && [ -n "$2" ] || { echo "$1 needs a value" >&2; exit 2; }; }
while [ $# -gt 0 ]; do
  case $1 in
    --dry-run) DRY=1 ;;
    --demo) need "$@"; DEMO=$2; shift ;;
    --res) need "$@"; RES=$2; shift ;;
    --only) need "$@"; ONLY+=("$2"); shift ;;
    -h | --help)
      sed -n '2,28p' "$0"
      exit 0
      ;;
    -*)
      echo "unknown option: $1 (see --help)" >&2
      exit 2
      ;;
    *) DEMO=$1 ;;
  esac
  shift
done
if [ -z "$DEMO" ]; then
  DEMO=$HOME/sqsysroot/project/demo
  [ -d "$DEMO" ] || DEMO=$HOME/sqsysroot/project/squared-examples/demo
fi
die() { printf 'stop: %s\n' "$*" >&2; exit 1; }
git merge-file -h >/dev/null 2>&1 || [ $? -eq 129 ] || die "git merge-file is not available"

GEN=$DEMO/mk/squared_generated.mk
[ -f "$GEN" ] || die "no $GEN; is $DEMO a generated project? (pass --demo DIR)"
val() { awk -v k="$1" '$1 == k && $2 == ":=" { $1 = ""; $2 = ""; sub(/^ +/, ""); print; exit }' "$GEN"; }
tpl_full=$(val SQ_TEMPLATE)
TEMPLATE=${tpl_full%@*}
TPL=$RES/templates/$TEMPLATE/tree
[ -d "$TPL" ] || die "no template tree at $TPL"
STAMP=$(date +%Y%m%d-%H%M%S)
BACKUP=$HOME/.cache/squared-template-backups/$STAMP

TMP=$(mktemp -d "${TMPDIR:-/tmp}/sqport.XXXXXX")
trap 'rm -rf "$TMP"' EXIT
{
  printf '{{project_name}}\t%s\n' "$(val SQ_PROJECT)"
  printf '{{package_name}}\t%s\n' "$(val SQ_PACKAGE)"
  printf '{{generator_version}}\t%s\n' "$(val SQ_GENERATOR_VERSION)"
  printf '{{template_id}}\t%s\n' "${tpl_full%@*}"
  printf '{{template_version}}\t%s\n' "${tpl_full#*@}"
  printf '{{framework_version}}\t%s\n' "$(val SQ_FRAMEWORK_VERSION)"
  printf '{{working_directory}}\t%s\n' "$(val SQ_WORKING_DIR)"
} >"$TMP/map"

# render <in> <out>: plain text substitution of every {{key}}, no regex, and the
# final newline kept exactly as the input has it (or hasn't).
render() {
  local nl=0
  [ ! -s "$1" ] || [ -n "$(tail -c1 "$1")" ] || nl=1
  awk -v map="$TMP/map" -v nl="$nl" '
    BEGIN { n = 0; while ((getline l < map) > 0) { i = index(l, "\t"); if (i > 1) { k[n] = substr(l, 1, i - 1); v[n] = substr(l, i + 1); n++ } } }
    {
      s = $0
      for (j = 0; j < n; j++) {
        out = ""
        while ((p = index(s, k[j])) > 0) { out = out substr(s, 1, p - 1) v[j]; s = substr(s, p + length(k[j])) }
        s = out s
      }
      if (NR > 1) printf "\n"
      printf "%s", s
    }
    END { if (NR > 0 && nl) printf "\n" }' "$1" >"$2"
}

files=()
if [ ${#ONLY[@]} -gt 0 ]; then
  files=("${ONLY[@]}")
else
  while IFS= read -r f; do files+=("${f#"$DEMO"/}"); done < <(find "$DEMO/mk" -type f -name '*.mk' ! -name 'pkg_*' ! -name '*.bak.*' | sort)
  for f in Makefile sq_android/entry.cpp; do [ -f "$DEMO/$f" ] && files+=("$f"); done
fi

echo "project:  $DEMO"
echo "template: $TPL"
if [ "$DRY" = 1 ]; then echo "dry run: nothing will change"; fi
ported=0
skipped=0
for f in "${files[@]}"; do
  t=$TPL/$f
  d=$DEMO/$f
  [ -f "$d" ] || { printf '  %-9s %s (not in the project)\n' skip "$f"; skipped=$((skipped + 1)); continue; }
  [ -f "$t" ] || { printf '  %-9s %s (project only; copy it into the template by hand if it belongs there)\n' skip "$f"; skipped=$((skipped + 1)); continue; }
  render "$t" "$TMP/base"
  if cmp -s "$TMP/base" "$d"; then
    printf '  %-9s %s\n' same "$f"
    continue
  fi
  if grep -q '{{[A-Za-z_]*}}' "$TMP/base"; then
    printf '  %-9s %s (placeholder this script has no value for: %s)\n' skip "$f" \
      "$(grep -o '{{[A-Za-z_]*}}' "$TMP/base" | sort -u | tr '\n' ' ')"
    skipped=$((skipped + 1))
    continue
  fi
  cp "$t" "$TMP/merged"
  rc=0
  git merge-file -q "$TMP/merged" "$TMP/base" "$d" || rc=$?
  if [ "$rc" -ne 0 ]; then
    printf '  %-9s %s (%s conflict(s): the project changed a line that holds a placeholder, or next to one)\n' skip "$f" "$rc"
    skipped=$((skipped + 1))
    continue
  fi
  render "$TMP/merged" "$TMP/check"
  if ! cmp -s "$TMP/check" "$d"; then
    printf '  %-9s %s (merged template would not render to the project file; left alone)\n' skip "$f"
    skipped=$((skipped + 1))
    continue
  fi
  n=$(diff "$t" "$TMP/merged" | grep -c '^[<>]' || true)
  printf '  %-9s %s (%s lines)\n' port "$f" "$n"
  if [ "$DRY" = 1 ]; then
    diff -u --label "template/$f" --label "template/$f (ported)" "$t" "$TMP/merged" | sed 's/^/    /' || true
  else
    mkdir -p "$BACKUP/$(dirname "$f")"
    cp -p "$t" "$BACKUP/$f"
    cat "$TMP/merged" >"$t"
  fi
  ported=$((ported + 1))
done

echo "== $ported to port, $skipped skipped"
if [ "$DRY" = 0 ] && [ "$ported" -gt 0 ]; then
  echo "  backups: $BACKUP"
  echo "  review:  git -C \"$RES\" diff --stat -- \"templates/$TEMPLATE\""
fi
