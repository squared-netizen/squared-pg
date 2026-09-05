#!/bin/sh
# Verify the version has not drifted between the places that hold a copy.
#
# VERSION is the single source. Three other places carry a copy or a
# constraint, and every one of them fails quietly when it disagrees:
#
#   engine/src/engine.cpp   the #ifndef fallback, used when built by hand
#   resources/**/manifest.json   `engine.version` ranges the engine must satisfy
#   lua/workflows/**/workflow.json   `requires_engine`
#
# A stale fallback reports a version the manifests then resolve against, and a
# manifest range that excludes the real version makes every resource refuse to
# load with an error about compatibility rather than about the mistake.

set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"

fail=0
note() { printf '  %s\n' "$*"; }
bad() { printf '  \033[31mFAIL\033[0m %s\n' "$*" >&2; fail=1; }

[ -f VERSION ] || { echo "no VERSION file" >&2; exit 1; }
version=$(tr -d ' \t\n\r' < VERSION)
note "VERSION            $version"

# The numeric core, which is what a range is compared against.
core=$(printf '%s' "$version" | sed 's/[-+].*//')
major=$(printf '%s' "$core" | cut -d. -f1)
minor=$(printf '%s' "$core" | cut -d. -f2)

# --- the compiled-in fallback ---------------------------------------------

fallback=$(sed -n 's/^#  *define SQUARED_PG_VERSION "\(.*\)"/\1/p' engine/src/engine.cpp | head -n 1)
if [ -z "$fallback" ]; then
    bad "engine/src/engine.cpp has no SQUARED_PG_VERSION fallback"
elif [ "$fallback" != "$version" ]; then
    bad "engine.cpp fallback is $fallback, VERSION is $version"
else
    note "engine.cpp         $fallback"
fi

# --- what a built binary actually reports ----------------------------------
#
# This is the check that catches a build system failing to pass the define at
# all — which no amount of reading source files would find.
#
# But a binary that disagrees has two possible causes with opposite remedies,
# and the first version of this check conflated them:
#
#   stale  — built before VERSION changed. Rebuild. Not an error.
#   drift  — built after, and still wrong. The build system is not passing
#            the define, and that is a real failure.
#
# Telling them apart is a timestamp comparison, and getting it wrong means
# either a confusing false alarm or a silently missed defect. `find -newer`
# rather than `test -nt`, because -nt is not POSIX and this runs under
# whatever /bin/sh the host provides.

for candidate in build/sqpg build-cmake/sqpg build-release/sqpg; do
    [ -x "$candidate" ] || continue

    reported=$("$candidate" describe 2>/dev/null | head -n 1 | awk '{print $2}')
    if [ "$reported" = "$version" ]; then
        note "$candidate  $reported"
        continue
    fi

    if [ -n "$(find "$candidate" -newer VERSION 2>/dev/null)" ]; then
        bad "$candidate reports $reported but was built after VERSION changed"
        note "  its build system is not passing -DSQUARED_PG_VERSION"
    else
        # Stale, not wrong. Saying so is the difference between a useful
        # message and one that sends someone editing files that are correct.
        printf '  \033[33mstale\033[0m %s reports %s; rebuild it\n' "$candidate" "$reported"
    fi
done

# --- manifest engine ranges ------------------------------------------------
#
# Not a full range evaluation -- that is the engine's job and it is tested.
# This catches the common drift: a range pinned to a major.minor the version
# has moved past.

for manifest in $(find resources -name manifest.json | sort); do
    range=$(python3 - "$manifest" <<'PY'
import json, sys
m = json.load(open(sys.argv[1]))
print((m.get("engine") or {}).get("version", ""))
PY
)
    [ -n "$range" ] || continue
    # Expect the range to admit this major.minor. A range naming a different
    # one is drift, whatever its exact form.
    case "$range" in
        *"$major.$minor"*) ;;
        *) bad "$manifest engine range '$range' does not mention $major.$minor" ;;
    esac
done

for workflow in $(find lua/workflows -name workflow.json | sort); do
    range=$(python3 - "$workflow" <<'PY'
import json, sys
print(json.load(open(sys.argv[1])).get("requires_engine", ""))
PY
)
    [ -n "$range" ] || continue
    case "$range" in
        *"$major.$minor"*) ;;
        *) bad "$workflow requires_engine '$range' does not mention $major.$minor" ;;
    esac
done

if [ "$fail" -eq 0 ]; then
    printf '  \033[32mok\033[0m   version is consistent\n'
else
    printf '\n  Fix VERSION, or the file that disagrees with it.\n' >&2
fi
exit "$fail"
