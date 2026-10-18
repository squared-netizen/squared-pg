#!/usr/bin/env bash
# check_template_drift.sh: read-only. Shows where a generated project's build
# files have moved on from the template it was generated from, so fixes made in
# the project can be carried back (see port_to_template.sh).
#
#   ./check_template_drift.sh          one line per file: same / DIFFERS / ...
#   ./check_template_drift.sh --diff   also print each diff (first 120 lines)
#
# Options:
#   [DIR] | --demo DIR  the project (default ~/sqsysroot/project/demo, or
#                       ~/sqsysroot/project/squared-examples/demo)
#   --res DIR           squared-pg resources/.squared (default: the squared-pg
#                       this script sits in, else ~/projects/squared-pg/...)
#   --template ID       template to compare with (default: the one named in the
#                       project's mk/squared_generated.mk)
# The template is rendered with the project's own values before comparing, so
# "same" means byte-identical to what the generator would write today, and a
# DIFFERS line is a real change, never a {{placeholder}}.
# Compared: mk/*.mk (except mk/pkg_*.mk, which packages write), Makefile,
# sq_android/entry.cpp. Changes nothing.
set -euo pipefail

DEMO=
RES=$(cd "$(dirname "$0")/.." && pwd -P)/resources/.squared
[ -d "$RES/templates" ] || RES=$HOME/projects/squared-pg/resources/.squared
TEMPLATE=
SHOW=0
need() { [ $# -ge 2 ] && [ -n "$2" ] || { echo "$1 needs a value" >&2; exit 2; }; }
while [ $# -gt 0 ]; do
  case $1 in
    --diff) SHOW=1 ;;
    --demo) need "$@"; DEMO=$2; shift ;;
    --res) need "$@"; RES=$2; shift ;;
    --template) need "$@"; TEMPLATE=$2; shift ;;
    -h | --help)
      sed -n '2,21p' "$0"
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

GEN=$DEMO/mk/squared_generated.mk
[ -f "$GEN" ] || die "no $GEN; is $DEMO a generated project? (pass --demo DIR)"
val() { awk -v k="$1" '$1 == k && $2 == ":=" { $1 = ""; $2 = ""; sub(/^ +/, ""); print; exit }' "$GEN"; }
tpl_full=$(val SQ_TEMPLATE)
[ -n "$TEMPLATE" ] || TEMPLATE=${tpl_full%@*}
TPL=$RES/templates/$TEMPLATE/tree
[ -d "$TPL" ] || die "no template tree at $TPL"

TMP=$(mktemp -d "${TMPDIR:-/tmp}/sqdrift.XXXXXX")
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
while IFS= read -r f; do files+=("${f#"$DEMO"/}"); done < <(find "$DEMO/mk" -type f -name '*.mk' ! -name 'pkg_*' ! -name '*.bak.*' | sort)
for f in Makefile sq_android/entry.cpp; do [ -f "$DEMO/$f" ] && files+=("$f"); done

echo "project:  $DEMO"
echo "template: $TPL"
for f in "${files[@]}"; do
  if [ ! -f "$TPL/$f" ]; then
    printf '  %-9s %s (project only)\n' new "$f"
    continue
  fi
  render "$TPL/$f" "$TMP/r"
  if cmp -s "$TMP/r" "$DEMO/$f"; then
    printf '  %-9s %s\n' same "$f"
  else
    n=$(diff "$TMP/r" "$DEMO/$f" | grep -c '^[<>]' || true)
    printf '  %-9s %s (%s lines)\n' DIFFERS "$f" "$n"
    if [ "$SHOW" = 1 ]; then
      diff -u --label "template/$f (rendered)" --label "project/$f" "$TMP/r" "$DEMO/$f" | head -120 | sed 's/^/    /' || true
    fi
  fi
done
