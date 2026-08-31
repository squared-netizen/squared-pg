#!/usr/bin/env fish
#
# sqcart-isolation.fish — prove that sqcart/ is a standalone component.
#
# This script does not trust discipline. It copies sqcart/ to a scratch
# directory outside the repository and builds it there. If anything in the
# component reaches into the parent tree, inherits the root build
# configuration, or reads a fixture from resources/, the copy has no such
# thing and the build fails.
#
# Run it the way you run tools/clang-check.fish and tools/lua-check.fish.
#
# Usage:
#   tools/sqcart-isolation.fish [options]
#
# Options:
#   --keep       Leave the scratch build directory in place and print it.
#   --jobs N     Parallel build jobs. Default: nproc, else 2.
#   --no-build   Static checks only; skip configure, build and test.
#   --make       Build with the Makefile instead of CMake (faster on Termux).
#   --verbose    Show full build output.
#   -h, --help   This text.
#
# Exit status: 0 if every check passed, 1 otherwise.

# --- options ---------------------------------------------------------

set -g opt_keep 0
set -g opt_no_build 0
set -g opt_verbose 0
set -g opt_make 0
set -g opt_jobs ""

set -g argi 1
while test $argi -le (count $argv)
    switch $argv[$argi]
        case --keep
            set opt_keep 1
        case --no-build
            set opt_no_build 1
        case --verbose
            set opt_verbose 1
        case --make
            set opt_make 1
        case --jobs
            set argi (math $argi + 1)
            set opt_jobs $argv[$argi]
        case -h --help
            sed -n '3,26p' (status --current-filename) | sed 's/^# \?//'
            exit 0
        case '*'
            echo "unknown option: $argv[$argi]" >&2
            exit 1
    end
    set argi (math $argi + 1)
end

if test -z "$opt_jobs"
    if type -q nproc
        set opt_jobs (nproc)
    else
        set opt_jobs 2
    end
end

# --- locate ----------------------------------------------------------

set -g tools_dir (realpath (dirname (status --current-filename)))
set -g repo_root (realpath "$tools_dir/..")
set -g component "$repo_root/sqcart"

if not test -d "$component"
    echo "error: no sqcart/ component at $component" >&2
    exit 1
end

# --- reporting -------------------------------------------------------
#
# Counters are global so the helpers can update them. Detail lines are passed
# as separate arguments, never as one newline-joined string: fish splits
# command substitution on newlines and the pieces would land in different
# parameters.

set -g pass_count 0
set -g fail_count 0
set -g warn_count 0

function _detail
    for line in $argv
        if test -n "$line"
            echo "        $line"
        end
    end
end

function _pass
    set -g pass_count (math $pass_count + 1)
    echo "  ok    $argv[1]"
end

function _fail
    set -g fail_count (math $fail_count + 1)
    echo "  FAIL  $argv[1]"
    if test (count $argv) -gt 1
        _detail $argv[2..-1]
    end
end

function _warn
    set -g warn_count (math $warn_count + 1)
    echo "  warn  $argv[1]"
    if test (count $argv) -gt 1
        _detail $argv[2..-1]
    end
end

function _rel
    string replace "$repo_root/" '' -- $argv[1]
end

echo "sqcart isolation gate"
echo "  component: $component"
echo

# =====================================================================
# 1. Layout
# =====================================================================

echo "layout"

if test -f "$component/CMakeLists.txt"
    if grep -qE '^[[:space:]]*project[[:space:]]*\([[:space:]]*sqcart' "$component/CMakeLists.txt"
        _pass "CMakeLists.txt declares its own project()"
    else
        _fail "CMakeLists.txt has no project() call" \
            "it is an add_subdirectory target, not a standalone component"
    end
else
    _fail "CMakeLists.txt is missing"
end

for f in AGENTS.md README.md
    if test -f "$component/$f"
        _pass "$f present"
    else
        _fail "$f missing" "required of every component by repository conventions 1.3"
    end
end

for f in include/sqcart/sqcart.hpp include/sqcart/expected.hpp app/include/sqcart/app/cli.hpp
    if test -f "$component/$f"
        _pass (basename $f)" present"
    else
        _fail "$f missing"
    end
end

echo

# =====================================================================
# 2. Reach — nothing under sqcart/ may name the parent tree
# =====================================================================

echo "reach"

set -g sources (find "$component" -type f \
    \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' -o -name '*.c' \
    -o -name '*.lua' -o -name 'CMakeLists.txt' -o -name '*.cmake' \) \
    -not -path '*/third_party/*' -not -path '*/build*/*' 2>/dev/null)

set -g escapes
for f in $sources
    set -l hits (grep -nE '#include[[:space:]]*[<"]\.\./\.\.' "$f" 2>/dev/null)
    for h in $hits
        set -a escapes (_rel "$f")": $h"
    end
end
if test (count $escapes) -eq 0
    _pass "no include escapes the component root"
else
    _fail "includes reach above sqcart/" $escapes
end

set -g siblings
for f in $sources
    set -l hits (grep -nE '"[^"]*(engine|resources)/' "$f" 2>/dev/null)
    for h in $hits
        set -a siblings (_rel "$f")": $h"
    end
end
if test (count $siblings) -eq 0
    _pass "no string literal names a sibling component"
else
    _warn "sibling component paths appear in string literals" $siblings
end

# Backend types must not reach a public header. Both public include trees.
set -g leaks
for d in "$component/include" "$component/app/include"
    if not test -d "$d"
        continue
    end
    for f in (find "$d" -type f -name '*.hpp' 2>/dev/null)
        set -l hits (grep -nE '#include[[:space:]]*[<"](miniz|yyjson|sha256)' "$f" 2>/dev/null)
        for h in $hits
            set -a leaks (_rel "$f")": $h"
        end
    end
end
if test (count $leaks) -eq 0
    _pass "public headers expose no backend include"
else
    _fail "public header includes a backend header" $leaks
end

set -g fixtures
for f in (find "$component/tests" "$component/app/tests" -type f -name '*.cpp' 2>/dev/null)
    set -l hits (grep -nE '"[^"]*(resources/|third_party/)' "$f" 2>/dev/null)
    for h in $hits
        set -a fixtures (_rel "$f")": $h"
    end
end
if test (count $fixtures) -eq 0
    _pass "tests reference no fixture outside the component"
else
    _fail "test reads a fixture from the parent tree" $fixtures
end

echo

# =====================================================================
# 3. CLI IO isolation
#
# New since the first gate. The CLI's whole testability rests on commands
# taking an Environment rather than naming streams; main.cpp is the sole
# exception. A reviewer will not catch a std::cout that creeps into a command
# six months from now, so check it.
# =====================================================================

echo "cli"

if test -d "$component/app/src"
    set -g stdio
    for f in (find "$component/app/src" -type f -name '*.cpp' 2>/dev/null)
        if test (basename $f) = main.cpp
            continue
        end
        set -l hits (grep -nE '\b(std::cout|std::cerr|std::cin|getenv|::exit|std::exit)\b' "$f" 2>/dev/null \
                     | grep -vE '^[0-9]+:[[:space:]]*(//|\*)')
        for h in $hits
            set -a stdio (_rel "$f")": $h"
        end
    end
    if test (count $stdio) -eq 0
        _pass "no command names a stream directly; all IO goes through Environment"
    else
        _fail "a command touches stdio outside Environment" $stdio \
            "this is what makes app/tests/test_cli.cpp possible; see sqcart/AGENTS.md"
    end

    if test -f "$component/app/src/main.cpp"
        set -l lines (wc -l < "$component/app/src/main.cpp" | string trim)
        if test $lines -le 60
            _pass "main.cpp is $lines lines (thin shell boundary)"
        else
            _warn "main.cpp is $lines lines" \
                "logic may be leaking out of the commands into the shell boundary"
        end
    end
else
    _warn "no app/ directory; CLI checks skipped"
end

echo

# =====================================================================
# 4. Single digest implementation
#
# FR-WRITE-7 requires reader and writer to share one implementation of the
# content digest, not two that agree today. The whole M0 reproducibility
# claim rests on it, and a second copy would look perfectly reasonable in
# review.
# =====================================================================

echo "invariants"

set -g acc_defs (grep -rln "ContentDigest DigestAccumulator::finish" "$component/src" 2>/dev/null)
if test (count $acc_defs) -eq 1
    _pass "exactly one DigestAccumulator implementation"
else if test (count $acc_defs) -eq 0
    _warn "DigestAccumulator::finish not found; digest may have been restructured"
else
    _fail "more than one digest implementation" $acc_defs \
        "reader and writer must share one; see sqcart/AGENTS.md"
end

# The writer must feed the shared accumulator rather than compute its own.
if test -f "$component/src/writer.cpp"
    if grep -q "DigestAccumulator" "$component/src/writer.cpp"
        _pass "writer uses the shared accumulator"
    else
        _fail "writer does not reference DigestAccumulator" \
            "FR-WRITE-7: pack and read must produce the same digest"
    end
end

# Seeds are embedded; the files under assets/ are the source of truth. A seed
# added to one and not the other is invisible until someone runs create.
if test -d "$component/assets/seeds"
    set -l on_disk (count (find "$component/assets/seeds" -name '*.json' 2>/dev/null))
    set -l embedded (grep -c 'SeedEntry{"' "$component/app/src/seeds.cpp" 2>/dev/null)
    test -z "$embedded"; and set embedded 0
    if test "$on_disk" -eq "$embedded"
        _pass "$on_disk seed files, $embedded embedded"
    else
        _fail "seed drift: $on_disk files on disk, $embedded embedded" \
            "tests/test_seeds.cpp checks this properly; mirror the JSON into seeds.cpp"
    end
end

# Symbolic links: the one rule that must not be relaxed by any policy.
if test -f "$component/src/symlink.cpp"
    if grep -q "escapes the cartridge root" "$component/src/symlink.cpp"
        _pass "symlink escape check present"
    else
        _fail "symlink escape check missing from src/symlink.cpp" \
            "a link resolving outside the root must never be packed, whatever the policy"
    end
end

echo

# =====================================================================
# 5. Vendored backend parity
# =====================================================================

echo "backends"

for dep in miniz yyjson
    set -l ours (find "$component/third_party" -maxdepth 1 -type d -name "$dep*" 2>/dev/null | head -n1)
    set -l theirs (find "$repo_root/third_party" -maxdepth 1 -type d -name "$dep*" 2>/dev/null | head -n1)

    if test -z "$ours"
        _fail "sqcart/third_party/$dep* missing" \
            "a standalone build must not fall back to the parent tree"
        continue
    end
    if test -z "$theirs"
        _warn "third_party/$dep* not found at repo root; parity not checked"
        continue
    end

    set -l a (basename $ours)
    set -l b (basename $theirs)
    if test "$a" = "$b"
        _pass "$a vendored identically in both trees"
    else
        _fail "$dep version drift" "sqcart: $a" "repo root: $b"
    end
end

if test -d "$component/third_party/crypto-algorithms"
    _pass "SHA-256 vendored (always local; FIPS 180-4 is frozen)"
else
    _fail "sqcart/third_party/crypto-algorithms missing"
end

echo

# =====================================================================
# 6. Scope — advisory
#
# The reach checks catch structural coupling. They cannot catch sqcart
# growing a resolve_kit() the framework runtime would never want. This is a
# prompt for human judgement, never a failure.
# =====================================================================

echo "scope (advisory)"

set -g creep
for f in (find "$component/include" -type f -name '*.hpp' 2>/dev/null)
    set -l hits (grep -nE '^[^/*]*(resolve|select|materiali[sz]e|generate|workflow)[a-z_]*[[:space:]]*\(' "$f" 2>/dev/null)
    for h in $hits
        set -a creep (_rel "$f")": $h"
    end
end
if test (count $creep) -eq 0
    _pass "no resolution or workflow vocabulary in the public API"
else
    _warn "public API names suggest engine responsibility, not container work" \
        $creep \
        "test: would the Squared framework runtime, which has never heard of" \
        "a template, want this function? if not, it belongs in an engine service"
end

echo

# =====================================================================
# 7. Standalone build
#
# The copy is what makes this meaningful. Building in place would let the
# component find the parent tree by accident.
# =====================================================================

if test $opt_no_build -eq 1
    echo "standalone build: skipped (--no-build)"
else
    echo "standalone build"

    set -g builder cmake
    if test $opt_make -eq 1
        set builder make
    else if not type -q cmake
        if type -q make
            echo "        cmake not found; falling back to make"
            set builder make
        else
            set builder none
        end
    end

    if test "$builder" = none
        _fail "neither cmake nor make found on PATH"
    else
        set -g scratch (mktemp -d)
        set -g src "$scratch/sqcart"
        set -g log "$scratch/log.txt"

        echo "        copying component to scratch (this is the isolation)"
        cp -R "$component" "$src"
        rm -rf "$src/build" "$src/.git"

        # Declared out here deliberately. A `set -l` inside the if-blocks
        # below would not survive past `end`, and the status checks would
        # read an undefined variable.
        set -g cfg_status 1
        set -g bld_status 1
        set -g test_status 1

        if test "$builder" = make
            echo "        building with make -j$opt_jobs (no parent tree present)"
            if test $opt_verbose -eq 1
                make -C "$src" -j $opt_jobs check
                set test_status $status
            else
                make -C "$src" -j $opt_jobs check >$log 2>&1
                set test_status $status
            end
            if test $test_status -eq 0
                _pass "builds and tests standalone (make)"
            else
                _fail "standalone make check failed" (tail -n 30 $log)
            end
        else
            set -g bld "$scratch/build"
            set -g cfg_args -S "$src" -B "$bld" \
                -DCMAKE_BUILD_TYPE=Debug \
                -DSQCART_USE_EXTERNAL_MINIZ=OFF \
                -DSQCART_USE_EXTERNAL_YYJSON=OFF \
                -DSQCART_BUILD_TESTS=ON

            echo "        configuring"
            if test $opt_verbose -eq 1
                cmake $cfg_args
                set cfg_status $status
            else
                cmake $cfg_args >$log 2>&1
                set cfg_status $status
            end

            if test $cfg_status -eq 0
                _pass "configures with no parent tree present"

                echo "        building with -j$opt_jobs (a minute or two on Termux)"
                if test $opt_verbose -eq 1
                    cmake --build "$bld" -j $opt_jobs
                    set bld_status $status
                else
                    cmake --build "$bld" -j $opt_jobs >>$log 2>&1
                    set bld_status $status
                end

                if test $bld_status -eq 0
                    _pass "builds standalone"

                    echo "        running tests"
                    if test $opt_verbose -eq 1
                        ctest --test-dir "$bld" --output-on-failure
                        set test_status $status
                    else
                        ctest --test-dir "$bld" --output-on-failure >>$log 2>&1
                        set test_status $status
                    end

                    if test $test_status -eq 0
                        _pass "tests pass standalone"
                    else
                        _fail "tests failed" (tail -n 30 $log)
                    end
                else
                    _fail "build failed" (tail -n 40 $log)
                end
            else
                _fail "configure failed" (tail -n 40 $log)
            end
        end

        if test $opt_keep -eq 1
            echo "        scratch tree kept at $scratch"
        else
            rm -rf "$scratch"
        end
    end
end

echo

# =====================================================================
# Summary
# =====================================================================

echo "$pass_count passed, $fail_count failed, $warn_count warnings"

if test $fail_count -gt 0
    echo
    echo "sqcart is not standalone. Fix before the boundary sets."
    exit 1
end

exit 0
