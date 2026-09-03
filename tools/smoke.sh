#!/bin/sh
# End-to-end smoke test: generate a workspace, build it, run it, and check that
# what came out is what the plan promised.
#
# sh rather than fish or bash: this runs in CI, in Termux, and on a desktop, and
# sh is the only one guaranteed to be present in all three. The repository's
# other tooling is fish by preference (§1.7); this one has a portability
# requirement fish does not meet.
#
#   sh tools/smoke.sh [build-dir]

set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
build=${1:-$root/build}
sqpg=$build/sqpg

# $TMPDIR, never a hardcoded /tmp: Termux has no /tmp, and a smoke test that
# only runs on a desktop tests the wrong host.
scratch=${TMPDIR:-/tmp}/squared-pg-smoke.$$
trap 'rm -rf "$scratch"' EXIT INT TERM
mkdir -p "$scratch"

fail() {
    echo "smoke: FAIL: $*" >&2
    exit 1
}

step() {
    printf '\n== %s\n' "$*"
}

[ -x "$sqpg" ] || fail "no sqpg at $sqpg (run make first)"

step "engine reports itself"
"$sqpg" describe || fail "describe"

step "resources are indexed"
"$sqpg" list | grep -q "template.termux.cpp" || fail "template not indexed"
"$sqpg" list | grep -q "kit.terminal" || fail "kit.terminal not indexed"
"$sqpg" list | grep -q "kit.lua" || fail "kit.lua not indexed"

step "planning creates nothing"
cd "$scratch"
"$sqpg" plan phantom --template template.termux.cpp --kit kit.terminal >/dev/null || fail "plan"
[ ! -e "$scratch/phantom" ] || fail "plan created a workspace"

step "a missing required kit is refused before anything is written"
if "$sqpg" plan bare --template template.termux.cpp >/dev/null 2>&1; then
    fail "template.kit.required was not enforced"
fi

step "generate with kit.terminal"
"$sqpg" new hello --template template.termux.cpp --kit kit.terminal || fail "generate"

for required in Makefile mk/squared_generated.mk mk/kit_terminal.mk \
                sq_app/src/main.cpp sq_app/src/app.cpp sq_app/include/app.hpp \
                sq_kit/include/squared/kit/terminal.hpp .squared/metadata.json; do
    [ -f "hello/$required" ] || fail "missing $required"
done

# kit.lua was not selected, so nothing it contributes may appear.
[ ! -e hello/sq_lua ] || fail "sq_lua present without kit.lua"
[ ! -e hello/mk/kit_lua.mk ] || fail "kit_lua.mk present without kit.lua"

# Substitution actually ran.
grep -q "hello::App" hello/sq_app/src/main.cpp || fail "substitution did not run"
! grep -rq "{{" hello/sq_app hello/Makefile hello/mk || fail "unsubstituted placeholder left behind"

# §2.7.13: no absolute host path in the metadata record, so the workspace is
# relocatable and its identity does not depend on where it sits.
! grep -q "$scratch" hello/.squared/metadata.json || fail "absolute host path in metadata"

step "the generated project builds"
( cd hello && make ) || fail "generated project does not build"
[ -x hello/build/hello ] || fail "no binary produced"

step "the generated project runs"
# The prompt has no trailing newline, so echoed output shares its line: match
# the content rather than anchoring, or the test asserts the prompt's shape
# instead of the program's behaviour.
printf 'echo smoke-ok\nquit\n' | ./hello/build/hello | grep -q "smoke-ok" || fail "app did not echo"
printf 'words a b c\nquit\n' | ./hello/build/hello | grep -q "3: c" || fail "regex split failed"
./hello/build/hello "echo one-shot-ok" | grep -q "one-shot-ok" || fail "one-shot mode failed"
./hello/build/hello </dev/null >/dev/null || fail "app did not survive empty input"

step "the generated project does not depend on the generator"
! grep -rq "squared-pg/resources\|$root" hello/Makefile hello/mk hello/sq_app || \
    fail "generated project references the generator tree"

step "regenerating over an existing workspace is refused"
if "$sqpg" new hello --template template.termux.cpp --kit kit.terminal >/dev/null 2>&1; then
    fail "an existing workspace was overwritten"
fi

step "generate with kit.lua as well"
"$sqpg" new scripted --template template.termux.cpp --kit kit.terminal --kit kit.lua || fail "generate scripted"
[ -f scripted/sq_lua/main.lua ] || fail "kit.lua did not seed the script workspace"
[ -f scripted/mk/kit_lua.mk ] || fail "kit.lua did not contribute its build fragment"
( cd scripted && make ) || fail "scripted project does not build"
( cd scripted && make lua-status ) || fail "lua-status target missing"

step "generation is deterministic"
# Same project name, two locations. Two *different* names would not be
# equivalent inputs, and comparing them would assert something §2.7.13 never
# promised.
mkdir -p det/a det/b
( cd det/a && "$sqpg" new twin --template template.termux.cpp --kit kit.terminal --kit kit.lua ) >/dev/null \
    || fail "generate twin (a)"
( cd det/b && "$sqpg" new twin --template template.termux.cpp --kit kit.terminal --kit kit.lua ) >/dev/null \
    || fail "generate twin (b)"
# §2.7.13: equivalent inputs, byte-identical output — metadata included, since
# metadata is generated output and not excused from the rule.
diff -r det/a/twin det/b/twin >/dev/null 2>&1 \
    || fail "two equivalent requests produced different output"

step "the workspace metadata reads back"
"$sqpg" inspect hello | grep -q "template.termux.cpp" || fail "inspect"

printf '\nsmoke: all checks passed\n'
