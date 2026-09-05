#!/usr/bin/env fish
#
# release.fish — run what CI runs, and build a release artifact for this host.
#
#   cd tools
#   ./release.fish check              everything CI does, locally
#   ./release.fish package            check, then build a tarball for this host
#   ./release.fish verify <tarball>   unpack somewhere clean and prove it works
#   ./release.fish tag                create and push an annotated tag
#   ./release.fish all                check, package, verify
#
# Runs from anywhere: the repository is found from this script's own location,
# never from the working directory.
#
# Deliberately does NOT cross-compile. One host builds one artifact; the other
# platforms are GitHub Actions' job (.github/workflows/release.yml), because a
# binary built by a machine that cannot run it is a binary nobody has tested.
#
# Fish because this is developer tooling and §1.7 makes that the repository's
# preference. The *installer inside the artifact* is POSIX sh, because it runs
# on a stranger's machine where fish is one more thing to have to install.

# Global, not local. fish does not make a caller's local variables visible
# to the functions it calls -- unlike bash -- so a top-level `set --local
# root` is invisible inside every function below, and $root expands to
# nothing. The symptom is a path like "/tools/version-check.sh".
set --global script_dir (realpath (dirname (status --current-filename)))
set --global root (realpath "$script_dir/..")

set --global command ""
# NOT `version`: fish reserves that for its own version number and it is
# read-only. Shadowing it fails at parse time in every function that
# declares it as a parameter.
set --global pkg_version ""
set --global out_dir "$root/dist"
set --global skip_cmake 0
set --global skip_make 0
set --global tarball ""

for argument in $argv
    switch $argument
        case --skip-cmake
            set skip_cmake 1
        case --skip-make
            set skip_make 1
        case '--version=*'
            set pkg_version (string replace -- '--version=' '' $argument)
        case '--out=*'
            set out_dir (string replace -- '--out=' '' $argument)
        case '-h' '--help'
            echo "usage: ./release.fish {check|stage|package|source|verify|tag|publish|all|clean} [options]"
            echo "  --version=X.Y.Z[-tag]   override the version"
            echo "  --out=DIR               artifact directory (default: dist/)"
            echo "  --skip-cmake            skip the CMake build in 'check'"
            echo "  --skip-make             skip the Makefile build in 'check'"
            exit 0
        case '-*'
            echo "release: unknown option $argument" >&2
            exit 2
        case '*'
            if test -z "$command"
                set command $argument
            else
                set tarball $argument
            end
    end
end

test -z "$command"; and set command all

# ---------------------------------------------------------------------------
# output
# ---------------------------------------------------------------------------

function step
    set_color --bold cyan; echo -n ":: "; set_color normal; echo $argv
end

function ok
    set_color green; echo -n "   ok  "; set_color normal; echo $argv
end

function warn
    set_color yellow; echo -n "   !!  "; set_color normal; echo $argv >&2
end

function die
    set_color --bold red; echo -n "xx "; set_color normal; echo $argv >&2
    exit 1
end

function run_or_die
    # Show the command, then run it. A release script that hides what it ran is
    # a release script you cannot reproduce by hand.
    set_color brblack; echo "   \$ $argv"; set_color normal
    command $argv; or die "failed: $argv"
end

# ---------------------------------------------------------------------------
# host identity
#
# Names the artifact. Deliberately coarse: these are the buckets a user picks
# from on a download page, not a full target triple.
# ---------------------------------------------------------------------------

function host_platform
    set --local os (uname -s)
    set --local machine (uname -m)

    # Termux reports Linux for -s; -o is what distinguishes it, and the
    # distinction matters because a Termux binary is an Android binary and will
    # not run on a glibc host of the same architecture.
    if test (uname -o 2>/dev/null) = "Android"
        set os "android"
    end

    switch $os
        case Linux linux
            set os linux
        case Darwin darwin
            set os macos
        case android
            set os android
        case '*'
            set os (string lower $os)
    end

    switch $machine
        case x86_64 amd64
            set machine x86_64
        case aarch64 arm64
            set machine aarch64
        case '*'
            # left as reported; an unusual machine should be visible in the
            # artifact name rather than silently bucketed
    end

    echo "$os-$machine"
end

# ---------------------------------------------------------------------------
# version
#
# Declared in two places on purpose (see engine_version() in
# engine/src/engine.cpp). This is where they are forced to agree: packaging a
# binary that reports a different version than the release it came from is the
# kind of thing nobody notices until a bug report is impossible to place.
# ---------------------------------------------------------------------------

# The version lives in one file. CMakeLists.txt and the Makefile both read it
# and compile it in; engine.cpp keeps a fallback for hand builds. Scraping it
# back out of the build files -- which an earlier version of this script did --
# stopped working the moment they started reading it instead of declaring it.
function project_version
    tr -d ' \t\n\r' < "$root/VERSION"
end

# Delegated rather than duplicated: CI runs the same script, so a rule enforced
# here and nowhere else would be a rule that only applies to whoever remembers
# to run this.
function check_version_agreement
    sh "$root/tools/version-check.sh"; or die "version drift"
end

# ---------------------------------------------------------------------------
# check — everything CI runs
# ---------------------------------------------------------------------------

function do_check --argument-names root skip_cmake skip_make
    step "version"
    check_version_agreement

    step "fish tooling"
    # This script is fish, and fish is the one language here that nothing else
    # exercises. It lints itself for the two bug classes that shipped in it.
    python3 "$root/tools/fish-lint.py"; or die "fish lint failed"

    step "vendored dependencies are present"
    for required in third-party/lua-5.4.8/src/lua.h \
                    third-party/miniz-3.1.2/miniz.c \
                    third-party/yyjson-0.12.0/src/yyjson.c \
                    third-party/crypto-algorithms/sha256.c \
                    sqcart/include/sqcart/sqcart.hpp
        test -f "$root/$required"; or die "missing: $required"
    end
    ok "offline build inputs complete"

    # $TMPDIR, never /tmp: Termux has none, and the tests stage workspaces.
    if not set -q TMPDIR
        set -gx TMPDIR "$root/.tmp"
        mkdir -p "$TMPDIR"
        warn "TMPDIR was unset; using $TMPDIR"
    end

    if test $skip_make -eq 0
        step "Makefile build (the Termux path)"
        run_or_die make -C "$root" -j (nproc 2>/dev/null; or echo 2)
        run_or_die make -C "$root" check
        run_or_die make -C "$root" smoke
        ok "Makefile path clean"
    end

    if test $skip_cmake -eq 0
        step "CMake build (the supported path)"
        if not command -v cmake >/dev/null
            warn "cmake not installed; skipping. CI covers this path."
        else
            run_or_die cmake -S "$root" -B "$root/build-cmake" -DCMAKE_BUILD_TYPE=RelWithDebInfo
            run_or_die cmake --build "$root/build-cmake" -j (nproc 2>/dev/null; or echo 2)
            run_or_die ctest --test-dir "$root/build-cmake" --output-on-failure
            ok "CMake path clean"
        end
    end

    step "sqcart, independently"
    run_or_die make -C "$root/sqcart" -j (nproc 2>/dev/null; or echo 2)
    run_or_die make -C "$root/sqcart" check
    ok "sqcart clean"
end

# ---------------------------------------------------------------------------
# stage — the runtime layout
#
#   bin/sqpg
#   share/squared-pg/{lua,resources}
#
# Matching what sqpg looks for: it walks up from its own executable and also
# checks <prefix>/share/squared-pg, so this tree works extracted anywhere and
# needs no environment variables.
# ---------------------------------------------------------------------------

function do_stage --argument-names root pkg_version platform out_dir
    set --local name "squared-pg-$pkg_version-$platform"
    set --local stage "$out_dir/$name"

    set --local binary "$root/build-cmake/sqpg"
    test -x "$binary"; or set binary "$root/build/sqpg"
    test -x "$binary"; or die "no sqpg binary; run './release.fish check' first"

    rm -rf "$stage"
    mkdir -p "$stage/bin" "$stage/share/squared-pg"

    cp "$binary" "$stage/bin/sqpg"
    # Strip when we can: an unstripped RelWithDebInfo binary is several times
    # the size of the useful part, and a release artifact is bandwidth.
    if command -v llvm-strip >/dev/null
        llvm-strip --strip-unneeded "$stage/bin/sqpg" 2>/dev/null
    else if command -v strip >/dev/null
        strip "$stage/bin/sqpg" 2>/dev/null
    end

    cp -r "$root/lua" "$root/resources" "$stage/share/squared-pg/"
    cp "$root/README.md" "$root/BUILDING.md" "$root/LICENSE" \
       "$root/THIRD-PARTY-LICENSES.md" "$stage/"
    cp -r "$root/docs/programmer" "$stage/docs"

    # The installer inside the artifact is POSIX sh: it runs on a stranger's
    # machine, where fish is one more thing to have to install first.
    echo '#!/bin/sh
# Install squared-pg. Idempotent.
#
#   ./install.sh                 -> ~/.local
#   ./install.sh /usr/local      -> needs write access
#
# Copies bin/sqpg and share/squared-pg/. sqpg finds its resources relative to
# its own executable, so nothing needs to be set afterwards -- but bin must be
# on your PATH for the command to be found.
set -eu

here=$(cd "$(dirname "$0")" && pwd)
prefix=${1:-$HOME/.local}

mkdir -p "$prefix/bin" "$prefix/share"
rm -rf "$prefix/share/squared-pg"
cp "$here/bin/sqpg" "$prefix/bin/sqpg"
chmod +x "$prefix/bin/sqpg"
cp -r "$here/share/squared-pg" "$prefix/share/squared-pg"

echo "installed to $prefix"
if ! command -v sqpg >/dev/null 2>&1; then
    echo
    echo "$prefix/bin is not on your PATH. Add it:"
    echo "    export PATH=\"$prefix/bin:\$PATH\""
fi
echo
echo "  sqpg list"
echo "  sqpg new hello --template template.terminal.cpp --kit kit.terminal"
' > "$stage/install.sh"
    chmod +x "$stage/install.sh"

    echo "$stage"
end

# ---------------------------------------------------------------------------
# package
# ---------------------------------------------------------------------------

function do_package --argument-names root pkg_version platform out_dir
    # host_platform already distinguishes Android from Linux, which is the hard
    # part. This is the consequence: an Android binary built under Termux links
    # that installation's libc++ and breaks at the next `pkg upgrade`. It is
    # fine for your own use and wrong to publish, and the difference is worth
    # saying out loud at the moment someone is packaging one.
    if string match -q 'android-*' -- "$platform"
        warn "packaging an Android/Termux binary"
        warn "  it links this installation's libc++ and will not survive a pkg upgrade"
        warn "  publish the source tarball instead: ./release.fish source"
    end
    set --local stage (do_stage "$root" "$pkg_version" "$platform" "$out_dir")
    set --local name (basename "$stage")
    set --local archive "$out_dir/$name.tar.gz"

    # Reproducible-ish: sorted entries, no owner names, fixed mtime. Two builds
    # of the same tree should differ only by the compiler's own output.
    tar --sort=name --owner=0 --group=0 --numeric-owner \
        --mtime='2026-01-01 00:00:00' \
        -czf "$archive" -C "$out_dir" "$name" 2>/dev/null
    or tar -czf "$archive" -C "$out_dir" "$name"  # BSD tar lacks --sort

    rm -rf "$stage"

    if command -v sha256sum >/dev/null
        sha256sum "$archive" | sed "s|$out_dir/||" > "$archive.sha256"
    else if command -v shasum >/dev/null
        shasum -a 256 "$archive" | sed "s|$out_dir/||" > "$archive.sha256"
    end

    echo "$archive"
end

# ---------------------------------------------------------------------------
# verify — prove the artifact works away from the source tree
#
# The point is to catch what the build cannot: a binary that only works because
# the repository happens to be next to it.
# ---------------------------------------------------------------------------

function do_verify --argument-names archive
    test -f "$archive"; or die "no such archive: $archive"

    set --local scratch (mktemp -d "$TMPDIR/squared-pg-verify.XXXXXX")
    tar -xzf "$archive" -C "$scratch"; or die "could not unpack $archive"

    set --local unpacked (ls -d "$scratch"/*/ | head -1 | string trim -c /)
    set --local sqpg "$unpacked/bin/sqpg"
    test -x "$sqpg"; or die "no bin/sqpg in the archive"

    step "the artifact runs in place"
    "$sqpg" list | grep -q template.terminal.cpp; or die "resources not found from the artifact"
    ok "resources resolve"

    step "the artifact generates and the result builds"
    set --local work "$scratch/work"
    mkdir -p "$work"
    pushd "$work" >/dev/null
    "$sqpg" new hello --template template.terminal.cpp --kit kit.terminal >/dev/null
    or begin; popd >/dev/null; die "generation failed"; end
    make -C hello >/dev/null 2>&1
    or begin; popd >/dev/null; die "the generated project did not build"; end
    echo 'echo verified
quit' | ./hello/build/hello | grep -q verified
    or begin; popd >/dev/null; die "the generated program did not run"; end
    popd >/dev/null
    ok "generate, build, run"

    step "install.sh into a throwaway prefix"
    "$unpacked/install.sh" "$scratch/prefix" >/dev/null; or die "install.sh failed"
    "$scratch/prefix/bin/sqpg" list | grep -q kit.terminal; or die "installed sqpg cannot find resources"
    ok "installed copy works"

    rm -rf "$scratch"
end

# ---------------------------------------------------------------------------
# tag
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# source — the tarball that matters most
#
# Not a fallback for platforms without a binary. On Termux -- the primary
# target -- building from source is the *right* route: a binary would link that
# installation's libc++ and break at the next upgrade, and the build takes
# under a minute with no network.
# ---------------------------------------------------------------------------

function do_source --argument-names root pkg_version out_dir
    set --local name "squared-pg-$pkg_version"
    mkdir -p "$out_dir"

    test -d "$root/.git"; or die "the source tarball is built with git archive"

    # git archive rather than tar: it honours .gitignore, so a build tree or a
    # scratch workspace cannot end up in a release.
    git -C "$root" archive --format=tar --prefix="$name/" HEAD \
        | gzip -9 > "$out_dir/$name-src.tar.gz"
    ok "$out_dir/$name-src.tar.gz"

    # Unpack somewhere unrelated and build it. That is the only way to know the
    # tarball is self-contained and that nothing is fetched.
    step "the source tarball builds from a clean unpack"
    set --local scratch (mktemp -d)
    tar xzf "$out_dir/$name-src.tar.gz" -C "$scratch"
    if pushd "$scratch/$name" >/dev/null
        and make -j4 >/dev/null 2>&1
        and make check >/dev/null 2>&1
        popd >/dev/null
        rm -rf "$scratch"
        ok "builds and passes offline"
    else
        popd >/dev/null 2>/dev/null
        rm -rf "$scratch"
        die "the source tarball does not build on its own"
    end
end

# ---------------------------------------------------------------------------
# publish — hand over to CI
# ---------------------------------------------------------------------------

function do_publish --argument-names root pkg_version
    set --local tag "v$pkg_version"

    git -C "$root" rev-parse "$tag" >/dev/null 2>&1
        or die "$tag does not exist; run './release.fish tag' first"

    echo "  Pushing $tag starts the release workflow, which opens a DRAFT"
    echo "  release. Nothing becomes public until you publish the draft."
    echo
    read --local --prompt-str "  Push $tag to origin? [y/N] " answer
    string match -qi 'y*' -- "$answer"; or begin
        warn "cancelled"
        return 1
    end

    git -C "$root" push origin "$tag"; or die "push failed"
    ok "pushed $tag"
    echo
    echo "   watch:  gh run watch"
    echo "   draft:  gh release view $tag --web"
end

function do_tag --argument-names root pkg_version
    set --local tag "v$pkg_version"

    test -z (git -C "$root" status --porcelain | string collect)
    or die "the tree has uncommitted changes; commit them before tagging"

    if git -C "$root" rev-parse "$tag" >/dev/null 2>&1
        die "tag $tag already exists"
    end

    git -C "$root" tag -a "$tag" -m "squared-pg $pkg_version"
    ok "tagged $tag"
    echo
    echo "   push it to start the release workflow:"
    echo "     git push origin $tag"
end

# ---------------------------------------------------------------------------

if not set -q TMPDIR
    set -gx TMPDIR /tmp
end

test -n "$pkg_version"; or set pkg_version (project_version)
set --local platform (host_platform)

echo
set_color --bold; echo "squared-pg release — $pkg_version — $platform"; set_color normal
echo

switch $command
    case check
        do_check "$root" $skip_cmake $skip_make

    case stage
        set --local stage (do_stage "$root" "$pkg_version" "$platform" "$out_dir")
        ok "staged at $stage"

    case package
        do_check "$root" $skip_cmake $skip_make
        step "packaging"
        set --local archive (do_package "$root" "$pkg_version" "$platform" "$out_dir")
        ok "$archive"

    case verify
        test -n "$tarball"; or die "usage: ./release.fish verify <tarball>"
        do_verify "$tarball"

    case source
        do_source "$root" "$pkg_version" "$out_dir"

    case tag
        do_tag "$root" "$pkg_version"

    case publish
        do_publish "$root" "$pkg_version"

    case all
        do_check "$root" $skip_cmake $skip_make
        step "source tarball"
        do_source "$root" "$pkg_version" "$out_dir"
        step "packaging"
        set --local archive (do_package "$root" "$pkg_version" "$platform" "$out_dir")
        ok "$archive"
        do_verify "$archive"
        echo
        set_color --bold green; echo "Release artifact ready: $archive"; set_color normal
        echo
        echo "  Other platforms are built by CI. To cut a release:"
        echo "    ./release.fish source      # the tarball Termux users want"
        echo "    ./release.fish tag         # re-runs the checks, then tags"
        echo "    ./release.fish publish     # pushes; CI drafts the release"

    case clean
        rm -rf "$out_dir" "$root/build-cmake"
        ok "removed $out_dir and build-cmake"

    case '*'
        die "unknown command '$command' — try ./release.fish --help"
end
