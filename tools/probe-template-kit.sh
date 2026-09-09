#!/usr/bin/env bash
# probe-template-kit.sh — collect what template.kit actually does.
#
# Answers, with evidence rather than inference:
#   1. exact file and directory names in the template tree
#   2. what `sqpg plan` says it will write, and from which origin
#   3. what `sqpg new` actually writes
#   4. whether {{project_name}} substitutes in FILE names
#   5. whether it substitutes in DIRECTORY names  (--dirs, invasive)
#   6. the README.md situation: how many, where they land
#
# Read-only unless --dirs is given. Writes a report to ./template-kit-probe.md
#
# Usage:
#   ./probe-template-kit.sh [--dirs]
#
#   --dirs   also test directory-name substitution. This adds a directory to
#            the INSTALLED resources copy, plans, then removes it. It does not
#            touch the repo. If it dies midway, the fix is:
#              rm -rf ~/sqsysroot/.sqpg/resources && build/sqpg initialize

set -uo pipefail

DIRS_TEST=0
[ "${1:-}" = "--dirs" ] && DIRS_TEST=1

REPORT="$PWD/template-kit-probe.md"
: > "$REPORT"
say() { printf '%s\n' "$*" | tee -a "$REPORT"; }
run() {
	    printf '\n$ %s\n' "$*" >> "$REPORT"
    # shellcheck disable=SC2068
    $@ >> "$REPORT" 2>&1
    printf '  (exit %d)\n' "$?" >> "$REPORT"
}
hdr() { printf '\n## %s\n' "$*" | tee -a "$REPORT"; }

TMP="${TMPDIR:-/data/data/com.termux/files/usr/tmp}/tk-probe.$$"
mkdir -p "$TMP"
trap 'rm -rf "$TMP"' EXIT

SQPG="$(command -v sqpg || echo ./build/sqpg)"
TPL_REPO="resources/generator/templates/template.kit"
TPL_INST="$HOME/sqsysroot/.sqpg/resources/generator/templates/template.kit"

say "# template.kit probe"
say ""
say "- sqpg:      $SQPG"
say "- repo tpl:  $TPL_REPO"
say "- installed: $TPL_INST"
say "- tmp:       $TMP"

# --------------------------------------------------------------- 1. the tree

hdr "1. template tree, exact names"
say ""
say '```'
if [ -d "$TPL_REPO" ]; then
    find "$TPL_REPO" -mindepth 1 | sed "s|^$TPL_REPO/||" | sort | tee -a "$REPORT"
else
    say "MISSING: $TPL_REPO"
fi
say '```'

hdr "1b. every README.md in the template, with size and first line"
say ""
say '```'
find "$TPL_REPO" -name 'README.md' 2>/dev/null | while read -r f; do
    printf '%s\n' "${f#"$TPL_REPO"/}" | tee -a "$REPORT"
    printf '    bytes: %s\n' "$(wc -c < "$f")" | tee -a "$REPORT"
    printf '    first: %s\n' "$(head -1 "$f")" | tee -a "$REPORT"
done
say '```'

hdr "1c. does the installed copy match the repo copy?"
say ""
say '```'
if [ -d "$TPL_INST" ]; then
    if diff -rq "$TPL_REPO" "$TPL_INST" > "$TMP/d" 2>&1; then
        say "identical"
    else
        say "DIFFERS — the probe below reflects the installed copy:"
        cat "$TMP/d" | tee -a "$REPORT"
    fi
else
    say "installed copy absent"
fi
say '```'

# --------------------------------------------------------------- 2. the plan

hdr "2. plan output (this is the important one)"
say ""
say '```'
run "$SQPG" plan probekit -t template.kit -p project_name=probe -p description=Probe --verbose
tail -n +2 "$REPORT" > /dev/null
say '```'

hdr "2b. plan as JSON — shows origin and disposition per step"
say ""
say '```'
run "$SQPG" plan probekit -t template.kit -p project_name=probe -p description=Probe --json
say '```'

# ------------------------------------------------------------ 3. actual generate

hdr "3. what `sqpg new` actually writes"
say ""
say '```'
( cd "$TMP" && "$SQPG" new probekit -t template.kit -p project_name=probe -p description=Probe ) \
    >> "$REPORT" 2>&1
say ""
say "resulting tree:"
if [ -d "$TMP/probekit" ]; then
    find "$TMP/probekit" -mindepth 1 | sed "s|$TMP/probekit/||" | sort | tee -a "$REPORT"
else
    say "(nothing generated)"
fi
say '```'

hdr "3b. filename substitution: did kit_{{project_name}}.mk become kit_probe.mk?"
say ""
say '```'
find "$TMP/probekit" -name '*.mk' 2>/dev/null | sed "s|$TMP/probekit/||" | tee -a "$REPORT"
say ""
say "literal-brace names still present (a bug if any appear):"
find "$TMP/probekit" -name '*{{*' 2>/dev/null | sed "s|$TMP/probekit/||" | tee -a "$REPORT"
say '```'

hdr "3c. README.md files in the generated workspace"
say ""
say '```'
find "$TMP/probekit" -name 'README.md' 2>/dev/null | while read -r f; do
    printf '%s  (%s bytes)\n' "${f#"$TMP/probekit"/}" "$(wc -c < "$f")" | tee -a "$REPORT"
    printf '    first: %s\n' "$(head -1 "$f")" | tee -a "$REPORT"
done
say '```'
say ""
say "Compare 1b with 3c. If the template has two README.md and the workspace"
say "has one, or one has the other's content, that is the collision."

hdr "3d. contents of the generated mk fragment"
say ""
say '```'
find "$TMP/probekit" -path '*mk/kit_*' -type f 2>/dev/null | while read -r f; do
    printf -- '--- %s\n' "${f#"$TMP/probekit"/}" | tee -a "$REPORT"
    cat "$f" | tee -a "$REPORT"
done
say '```'

hdr "3e. the generated cartridge manifest, substituted"
say ""
say '```'
if [ -f "$TMP/probekit/cartridge/SQ-INF/manifest.json" ]; then
    cat "$TMP/probekit/cartridge/SQ-INF/manifest.json" | tee -a "$REPORT"
fi
say '```'

hdr "3f. does the authored cartridge pack as-is?"
say ""
say '```'
( cd "$TMP/probekit" && sqcart pack cartridge -o "$TMP/probe.sq" ) >> "$REPORT" 2>&1
printf '  (exit %d)\n' "$?" >> "$REPORT"
[ -f "$TMP/probe.sq" ] && ( sqcart verify "$TMP/probe.sq" >> "$REPORT" 2>&1; \
    printf '  verify exit %d\n' "$?" >> "$REPORT" )
say '```'

# ------------------------------------------------- 4. directory substitution

if [ "$DIRS_TEST" -eq 1 ]; then
    hdr "4. directory-name substitution (invasive test)"
    say ""
    say '```'
    PROBE_DIR="$TPL_INST/tree/cartridge/tree/sq_kit/include/{{project_name}}"
    if [ -d "$TPL_INST" ]; then
        mkdir -p "$PROBE_DIR"
        printf '// probe header for {{project_name}}\n' > "$PROBE_DIR/probe.hpp"
        say "added to installed copy: tree/cartridge/tree/sq_kit/include/{{project_name}}/probe.hpp"
        say ""
        ( cd "$TMP" && rm -rf dirprobe && "$SQPG" plan dirprobe -t template.kit \
            -p project_name=probe -p description=D ) 2>&1 | grep -i 'sq_kit\|probe.hpp' | tee -a "$REPORT"
        say ""
        say "If the path above reads sq_kit/include/probe/probe.hpp the processor"
        say "substitutes directory names. If it reads {{project_name}} literally,"
        say "it does not, and the layout cannot be templated by name."
        rm -rf "$TPL_INST/tree/cartridge/tree/sq_kit"
        say ""
        say "removed probe directory from installed copy"
    else
        say "installed copy absent; cannot run this test"
    fi
    say '```'
else
    hdr "4. directory-name substitution"
    say ""
    say "Not tested. Re-run with --dirs to test it."
    say ""
    say "This matters because D-057 wants sq_kit/include/<kitname>/, and the"
    say "template can only scaffold that if {{project_name}} substitutes in a"
    say "directory name. Filename substitution is already proven by"
    say "kit_{{project_name}}.mk; directory substitution is not."
fi

# --------------------------------------------------------------- 5. context

hdr "5. context"
say ""
say '```'
run "$SQPG" show template.kit
say '```'

say ""
say "---"
say ""
say "Report written to $REPORT"
printf '\nDone. Paste %s back.\n' "$REPORT"
}}'
}
