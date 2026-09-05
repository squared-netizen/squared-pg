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
"$sqpg" list | grep -q "template.terminal.cpp" || fail "template not indexed"
"$sqpg" list | grep -q "kit.terminal" || fail "kit.terminal not indexed"
"$sqpg" list | grep -q "kit.lua" || fail "kit.lua not indexed"

step "planning creates nothing"
cd "$scratch"
"$sqpg" plan phantom --template template.terminal.cpp --kit kit.terminal >/dev/null || fail "plan"
[ ! -e "$scratch/phantom" ] || fail "plan created a workspace"

step "a missing required kit is refused before anything is written"
if "$sqpg" plan bare --template template.terminal.cpp >/dev/null 2>&1; then
    fail "template.kit.required was not enforced"
fi

step "generate with kit.terminal"
"$sqpg" new hello --template template.terminal.cpp --kit kit.terminal || fail "generate"

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
if "$sqpg" new hello --template template.terminal.cpp --kit kit.terminal >/dev/null 2>&1; then
    fail "an existing workspace was overwritten"
fi

step "generate with kit.lua as well"
"$sqpg" new scripted --template template.terminal.cpp --kit kit.terminal --kit kit.lua || fail "generate scripted"
[ -f scripted/sq_lua/main.lua ] || fail "kit.lua did not seed the script workspace"
[ -f scripted/mk/kit_lua.mk ] || fail "kit.lua did not contribute its build fragment"
( cd scripted && make ) || fail "scripted project does not build"
( cd scripted && make lua-status ) || fail "lua-status target missing"

step "generation is deterministic"
# Same project name, two locations. Two *different* names would not be
# equivalent inputs, and comparing them would assert something §2.7.13 never
# promised.
mkdir -p det/a det/b
( cd det/a && "$sqpg" new twin --template template.terminal.cpp --kit kit.terminal --kit kit.lua ) >/dev/null \
    || fail "generate twin (a)"
( cd det/b && "$sqpg" new twin --template template.terminal.cpp --kit kit.terminal --kit kit.lua ) >/dev/null \
    || fail "generate twin (b)"
# §2.7.13: equivalent inputs, byte-identical output — metadata included, since
# metadata is generated output and not excused from the rule.
diff -r det/a/twin det/b/twin >/dev/null 2>&1 \
    || fail "two equivalent requests produced different output"

step "the workspace metadata reads back"
"$sqpg" inspect hello | grep -q "template.terminal.cpp" || fail "inspect"

# ---------------------------------------------------------------------------
# Android
#
# Generation only. Building the .so needs the NDK and packaging needs the SDK;
# both are host facts this test must not require, so it checks what the
# generator produced and leaves compilation to the device.
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# kit.termux
#
# The kit ships a JSON parser, which is the only place in the repository where
# a resource contains real logic rather than glue. Engine tests cannot reach it
# — it is a header inside a cartridge — so it is tested the only way it can be:
# generate a project with it, compile a test against it, and run that.
# ---------------------------------------------------------------------------

step "kit.termux generates and its JSON parser is correct"
"$sqpg" new phone --template template.terminal.cpp --kit kit.terminal --kit kit.termux \
    >/dev/null || fail "generate with kit.termux"
[ -f phone/sq_kit/include/squared/kit/termux.hpp ] || fail "kit.termux header missing"
[ -f phone/mk/kit_termux.mk ] || fail "kit.termux fragment missing"

rm -f phone/sq_app/src/app.cpp phone/sq_app/include/app.hpp
cat > phone/sq_app/src/main.cpp <<'TERMUXTEST'
#include <squared/kit/termux.hpp>
#include <cstdio>
#include <string>

static int failures = 0;
#define CHECK(e) do { if (!(e)) { std::printf("FAIL %d: %s\n", __LINE__, #e); ++failures; } } while (0)

using sq::termux::Json;

int main() {
    // Shapes taken from real termux-api output.
    auto battery = Json::parse(R"({"health":"GOOD","percentage":78,"status":"DISCHARGING",
                                   "temperature":29.9,"current":-451000})");
    CHECK(battery.has_value());
    CHECK(battery->int_or("percentage") == 78);
    CHECK(battery->int_or("current") == -451000);
    CHECK(battery->string_or("absent", "dflt") == "dflt");

    auto wifi = Json::parse(R"({"mac_address":null,"ssid_hidden":false,"rssi":-52})");
    CHECK(wifi.has_value());
    CHECK((*wifi)["mac_address"].is_null());
    CHECK((*wifi)["ssid_hidden"].as_bool() == false);

    auto sensor = Json::parse(R"({"Accel":{"values":[0.31,-0.07,9.79]}})");
    CHECK(sensor.has_value());
    CHECK((*sensor)["Accel"]["values"].size() == 3);
    // Chaining through absent keys must yield null, not crash.
    CHECK((*sensor)["nope"]["also"][7].is_null());

    auto dialog = Json::parse(R"({"code":-1,"text":"a\nb \"q\" \u00e9\u4e2d"})");
    CHECK(dialog.has_value());
    CHECK(dialog->string_or("text").find("\xc3\xa9") != std::string::npos);
    CHECK(dialog->string_or("text").find("\xe4\xb8\xad") != std::string::npos);
    CHECK(dialog->string_or("text").find("\n") != std::string::npos);

    // Malformed input must be rejected outright, never half-accepted.
    for (const char* bad : {"", "{", "{\"a\":}", "[1,]", "tru", "{\"a\":1}junk"}) {
        if (Json::parse(bad).has_value()) { std::printf("FAIL accepted: %s\n", bad); ++failures; }
    }

    // Accessors never coerce across types.
    auto types = Json::parse(R"({"n":5,"s":"5"})");
    CHECK(!(*types)["n"].as_string().has_value());
    CHECK(!(*types)["s"].as_number().has_value());

    // Shell quoting is the injection surface: every argument passes through it.
    using sq::termux::detail::shell_quote;
    CHECK(shell_quote("it's") == R"('it'\''s')");
    CHECK(shell_quote("; rm -rf /") == "'; rm -rf /'");
    CHECK(shell_quote("$(whoami)") == "'$(whoami)'");

    // Off a device the API is absent, and that must be reported, not crash.
    if (!sq::termux::available()) {
        const auto probe = sq::termux::probe();
        CHECK(!probe.ok);
        CHECK(!probe.error.empty());
    }

    std::printf(failures == 0 ? "kit.termux json ok\n" : "kit.termux json FAILED\n");
    return failures == 0 ? 0 : 1;
}
TERMUXTEST

( cd phone && make ) >/dev/null 2>&1 || fail "the kit.termux test did not compile"
./phone/build/phone | grep -q "kit.termux json ok" || fail "the kit.termux JSON parser is wrong"

step "no platform is assumed when none is given"
# The workflow supplies no platform default. A resource restricted to another
# platform must still be refused, and an unrestricted request must still work.
"$sqpg" plan anyplat --template template.terminal.cpp --kit kit.terminal >/dev/null \
    || fail "generation without an explicit platform"

step "generate an android workspace"
"$sqpg" new droid --template template.android.cpp --kit kit.opengl \
    --platform android --param package_name=com.example.droid || fail "generate droid"

for required in Makefile mk/squared_generated.mk mk/squared_android_package.mk \
                mk/kit_opengl.mk sq_app/src/app.cpp sq_app/include/app.hpp \
                sq_android/entry.cpp sq_android/AndroidManifest.xml \
                sq_android/res/values/strings.xml \
                sq_android/res/drawable/ic_launcher_foreground.xml \
                sq_android/res/mipmap-anydpi-v26/ic_launcher.xml \
                sq_android/res/mipmap-hdpi/ic_launcher.png \
                sq_kit/include/squared/kit/gl.hpp .squared/metadata.json; do
    [ -f "droid/$required" ] || fail "missing $required"
done

! grep -rq "{{" droid/sq_app droid/sq_android/entry.cpp droid/Makefile droid/mk \
    || fail "unsubstituted placeholder in the android workspace"
grep -q 'package="com.example.droid"' droid/sq_android/AndroidManifest.xml \
    || fail "package name did not substitute"

# app_label declares default_from: project_name, and project_name is an engine
# built-in -- so declared defaults must be applied after the built-ins.
grep -q '<string name="app_name">droid</string>' droid/sq_android/res/values/strings.xml \
    || fail "default_from did not resolve against a built-in"

step "the icon survived as a binary payload"
# §2.8.8: substitution into a PNG would corrupt it in a way nothing downstream
# notices. Byte-for-byte, or the NUL-byte guard is not doing its job.
cmp -s droid/sq_android/res/mipmap-hdpi/ic_launcher.png \
       "$root/resources/templates/template.android.cpp/tree/sq_android/res/mipmap-hdpi/ic_launcher.png" \
    || fail "the launcher icon was altered in transit"

step "the platform layer is the user's"
"$sqpg" plan droid2 --template template.android.cpp --kit kit.opengl \
    --platform android --param package_name=com.example.droid --json > plan.json || fail "plan"
# Nothing generator-owned may land in sq_app/ or sq_android/: both are yours to
# edit, and an update that overwrote them would break that promise.
python3 - plan.json <<'PYEOF' || fail "ownership violation in the android plan"
import json, sys
plan = json.load(open(sys.argv[1]))
bad = [s for s in plan["steps"]
       if s["ownership"] == "generated"
       and (s["path"].startswith("sq_app") or s["path"].startswith("sq_android"))]
sys.exit(1 if bad else 0)
PYEOF

step "android builds without a rendering kit"
"$sqpg" new bare --template template.android.cpp --platform android \
    --param package_name=com.example.bare >/dev/null || fail "generate without a kit"
[ ! -e bare/sq_kit ] || fail "sq_kit present without a rendering kit"
[ ! -e bare/mk/kit_opengl.mk ] || fail "kit fragment present without the kit"
grep -q "__has_include(<squared/kit/gl.hpp>)" bare/sq_android/entry.cpp \
    || fail "the renderer include is not guarded"

step "two renderers on a single-arity area are refused"
if "$sqpg" plan clash --template template.android.cpp --kit kit.opengl --kit kit.opengl \
        --platform android --param package_name=com.example.clash >/dev/null 2>&1; then
    fail "render.backend accepted two contributors"
fi

step "a terminal kit is refused on the android template"
if "$sqpg" plan wrong --template template.android.cpp --kit kit.terminal \
        --platform android --param package_name=com.example.wrong >/dev/null 2>&1; then
    fail "kit.terminal was accepted on template.android.cpp"
fi

step "an invalid application id is refused"
if "$sqpg" plan badid --template template.android.cpp --kit kit.opengl \
        --platform android --param package_name=notapackage >/dev/null 2>&1; then
    fail "an invalid java package was accepted"
fi

printf '\nsmoke: all checks passed\n'
