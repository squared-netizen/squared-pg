#!/usr/bin/env bash
#
# tools/release.sh — run what CI runs, and build a release artifact for this host.
#
#   tools/release.sh check              everything CI does, locally
#   tools/release.sh stage              lay out the runtime tree
#   tools/release.sh package            check, then build a tarball for this host
#   tools/release.sh source             the source tarball, built and verified
#   tools/release.sh verify <tarball>   unpack somewhere clean and prove it works
#   tools/release.sh tag                check, then create the annotated tag
#   tools/release.sh publish            push the tag; CI drafts the release
#   tools/release.sh all                check, source, package, verify
#   tools/release.sh status             where the release stands
#   tools/release.sh clean
#
# Options:
#   --version=X.Y.Z   override the version
#   --out=DIR         artifact directory (default: dist/)
#   --skip-cmake      skip the CMake build in `check`
#   --skip-make       skip the Makefile build in `check`
#
# Runs from anywhere: the repository is found from this script's own location,
# never from the working directory.
#
# Deliberately does NOT cross-compile. One host builds one artifact; the other
# platforms are GitHub Actions' job, because a binary built by a machine that
# cannot run it is a binary nobody has tested.
#
# Bash, not fish. Two real bugs shipped in the fish version because neither the
# author nor CI could run it — shadowing fish's read-only $version, and reading
# a top-level `set --local` from inside a function. Bash is what the sandbox,
# CI and every target host can all execute, so the script can be tested by the
# person writing it. Recorded as D-043.
#
# Bash 3.2 compatible: macOS ships that and CI runs macOS. No associative
# arrays, no `mapfile`, no `${var,,}`.

set -uo pipefail

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
root=$(cd "$script_dir/.." && pwd)

command_name=""
pkg_version=""
out_dir="$root/dist"
skip_cmake=0
skip_make=0
tarball=""

for argument in "$@"; do
    case "$argument" in
        --skip-cmake) skip_cmake=1 ;;
        --skip-make)  skip_make=1 ;;
        --version=*)  pkg_version="${argument#--version=}" ;;
        --out=*)      out_dir="${argument#--out=}" ;;
        -h|--help)
            sed -n '3,25p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
            exit 0 ;;
        -*)
            echo "release: unknown option $argument" >&2
            exit 2 ;;
        *)
            if [ -z "$command_name" ]; then command_name="$argument"; else tarball="$argument"; fi ;;
    esac
done

# --- output ----------------------------------------------------------------

if [ -t 1 ] && [ -z "${NO_COLOR:-}" ]; then
    C_STEP=$'\033[1;36m'; C_OK=$'\033[32m'; C_WARN=$'\033[33m'; C_ERR=$'\033[31m'; C_OFF=$'\033[0m'
else
    C_STEP=""; C_OK=""; C_WARN=""; C_ERR=""; C_OFF=""
fi

step() { printf '%s:: %s%s\n' "$C_STEP" "$*" "$C_OFF"; }
ok()   { printf '   %sok%s  %s\n' "$C_OK" "$C_OFF" "$*"; }
warn() { printf '   %s!!%s  %s\n' "$C_WARN" "$C_OFF" "$*" >&2; }
die()  { printf '   %sxx%s  %s\n' "$C_ERR" "$C_OFF" "$*" >&2; exit 1; }

run_or_die() {
    printf '   $ %s\n' "$*"
    "$@" || die "failed: $*"
}

jobs_count() { nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2; }

# --- host ------------------------------------------------------------------

host_platform() {
    local os machine
    os=$(uname -s)
    machine=$(uname -m)

    # Termux reports Linux for -s; -o is what distinguishes it, and the
    # distinction matters because a Termux binary is an Android binary and will
    # not run on a glibc host of the same architecture.
    if [ "$(uname -o 2>/dev/null || true)" = "Android" ]; then
        os="android"
    fi

    case "$os" in
        Linux|linux)   os=linux ;;
        Darwin|darwin) os=macos ;;
        android)       os=android ;;
        *)             os=$(printf '%s' "$os" | tr '[:upper:]' '[:lower:]') ;;
    esac

    case "$machine" in
        x86_64|amd64)   machine=x86_64 ;;
        aarch64|arm64)  machine=aarch64 ;;
        # anything else is left as reported: an unusual machine should be
        # visible in the artifact name rather than silently bucketed
    esac

    printf '%s-%s\n' "$os" "$machine"
}

project_version() {
    # The version lives in one file. CMakeLists.txt and the Makefile both read
    # it and compile it in. Scraping it back out of the build files stopped
    # working the moment they started reading it instead of declaring it.
    tr -d ' \t\n\r' < "$root/VERSION"
}

check_version_agreement() {
    # Delegated rather than duplicated: CI runs the same script, so a rule
    # enforced here and nowhere else would only apply to whoever remembers.
    sh "$root/tools/version-check.sh" || die "version drift"
}

# --- check -----------------------------------------------------------------

do_check() {
    step "version"
    check_version_agreement

    step "script tooling"
    # Shell scripts are the one part of the tree nothing else exercises.
    for candidate in "$root"/tools/*.sh; do
        [ -f "$candidate" ] || continue
        bash -n "$candidate" || die "syntax error: $candidate"
    done
    ok "shell scripts parse"
    if [ -f "$root/tools/fish-lint.py" ]; then
        python3 "$root/tools/fish-lint.py" || die "fish lint failed"
    fi

    step "vendored dependencies are present"
    for required in third-party/lua-5.4.8/src/lua.h \
                    third-party/miniz-3.1.2/miniz.c \
                    third-party/yyjson-0.12.0/src/yyjson.c \
                    third-party/crypto-algorithms/sha256.c \
                    sqcart/include/sqcart/sqcart.hpp \
                    LICENSE VERSION; do
        [ -f "$root/$required" ] || die "missing: $required"
    done
    ok "offline build inputs complete"

    # $TMPDIR, never /tmp: Termux has none, and the tests stage workspaces.
    if [ -z "${TMPDIR:-}" ]; then
        TMPDIR="$root/.tmp"
        export TMPDIR
        mkdir -p "$TMPDIR"
        warn "TMPDIR was unset; using $TMPDIR"
    fi

    if [ "$skip_make" -eq 0 ]; then
        step "Makefile build (the Termux path)"
        run_or_die make -C "$root" -j "$(jobs_count)"
        run_or_die make -C "$root" check
        run_or_die make -C "$root" smoke
        ok "Makefile path clean"
    fi

    if [ "$skip_cmake" -eq 0 ]; then
        step "CMake build (the supported path)"
        if ! command -v cmake >/dev/null 2>&1; then
            warn "cmake not installed; skipping. CI covers this path."
        else
            run_or_die cmake -S "$root" -B "$root/build-cmake" -DCMAKE_BUILD_TYPE=RelWithDebInfo
            run_or_die cmake --build "$root/build-cmake" -j "$(jobs_count)"
            run_or_die ctest --test-dir "$root/build-cmake" --output-on-failure
            ok "CMake path clean"
        fi
    fi

    step "sqcart, independently"
    run_or_die make -C "$root/sqcart" -j "$(jobs_count)"
    run_or_die make -C "$root/sqcart" check
    ok "sqcart clean"
}

# --- stage -----------------------------------------------------------------
#
#   bin/sqpg
#   share/squared-pg/{lua,resources}
#
# Matching what sqpg looks for: it walks up from its own executable and also
# checks <prefix>/share/squared-pg, so this tree works extracted anywhere and
# needs no environment variables.

do_stage() {
    local version="$1" platform="$2" target="$3"
    local name="squared-pg-$version-$platform"
    local stage="$target/$name"

    local binary="$root/build-cmake/sqpg"
    [ -x "$binary" ] || binary="$root/build/sqpg"
    [ -x "$binary" ] || die "no sqpg binary; run './release.sh check' first"

    rm -rf "$stage"
    mkdir -p "$stage/bin" "$stage/share/squared-pg"

    cp "$binary" "$stage/bin/sqpg"
    # Strip when we can: an unstripped RelWithDebInfo binary is several times
    # the size of the useful part, and a release artifact is bandwidth.
    if command -v llvm-strip >/dev/null 2>&1; then
        llvm-strip --strip-unneeded "$stage/bin/sqpg" 2>/dev/null || true
    elif command -v strip >/dev/null 2>&1; then
        strip "$stage/bin/sqpg" 2>/dev/null || true
    fi

    cp -r "$root/lua" "$root/resources" "$stage/share/squared-pg/"
    # Only files that exist. The fish version listed THIRD-PARTY-LICENSES.md,
    # which was never created — the vendored licences live inside LICENSE — so
    # staging failed at the last step of a release.
    for document in README.md BUILDING.md LICENSE VERSION THIRD-PARTY-LICENSES.md; do
        [ -f "$root/$document" ] && cp "$root/$document" "$stage/"
    done
    [ -d "$root/docs/programmer" ] && cp -r "$root/docs/programmer" "$stage/docs"

    cat > "$stage/install.sh" <<'INSTALLER'
#!/bin/sh
# Install squared-pg. Idempotent.
#
#   ./install.sh                 -> ~/.local
#   ./install.sh /usr/local      -> needs write access
#
# Copies bin/sqpg and share/squared-pg/. sqpg finds its resources relative to
# its own executable, so nothing needs to be set afterwards — but bin must be
# on your PATH for the command to be found.
#
# POSIX sh, not bash: this runs on a stranger's machine, where every additional
# requirement is one more thing that can be missing.
set -eu

here=$(cd "$(dirname "$0")" && pwd)
prefix=${1:-$HOME/.local}

mkdir -p "$prefix/bin" "$prefix/share"
rm -rf "$prefix/share/squared-pg"
cp "$here/bin/sqpg" "$prefix/bin/sqpg"
chmod +x "$prefix/bin/sqpg"
cp -r "$here/share/squared-pg" "$prefix/share/squared-pg"

echo "installed to $prefix"
"$prefix/bin/sqpg" describe >/dev/null 2>&1 \
    || { echo "warning: the installed binary could not start" >&2; exit 1; }
echo "  $("$prefix/bin/sqpg" describe | head -n 1)"

case ":$PATH:" in
    *":$prefix/bin:"*) ;;
    *) echo "  note: $prefix/bin is not on your PATH" ;;
esac
INSTALLER
    chmod +x "$stage/install.sh"

    printf '%s\n' "$stage"
}

# --- package ---------------------------------------------------------------

do_package() {
    local version="$1" platform="$2" target="$3"

    # host_platform already distinguishes Android from Linux, which is the hard
    # part. This is the consequence: an Android binary built under Termux links
    # that installation's libc++ and breaks at the next `pkg upgrade`. Fine for
    # your own use, wrong to publish, and worth saying at the moment someone is
    # packaging one.
    case "$platform" in
        android-*)
            warn "packaging an Android/Termux binary"
            warn "  it links this installation's libc++ and will not survive a pkg upgrade"
            warn "  publish the source tarball instead: ./release.sh source" ;;
    esac

    local stage
    stage=$(do_stage "$version" "$platform" "$target") || return 1
    local name
    name=$(basename "$stage")

    tar czf "$target/$name.tar.gz" -C "$target" "$name"
    rm -rf "$stage"
    printf '%s\n' "$target/$name.tar.gz"
}

# --- source ----------------------------------------------------------------
#
# Not a fallback for platforms without a binary. On Termux — the primary
# target — building from source is the *right* route: a binary would link that
# installation's libc++ and break at the next upgrade, and the build takes
# under a minute with no network.

do_source() {
    local version="$1" target="$2"
    local name="squared-pg-$version"

    [ -d "$root/.git" ] || die "the source tarball is built with git archive"
    mkdir -p "$target"

    # git archive rather than tar: it honours .gitignore, so a build tree or a
    # scratch workspace cannot end up in a release.
    git -C "$root" archive --format=tar --prefix="$name/" HEAD | gzip -9 > "$target/$name-src.tar.gz" \
        || die "git archive failed"
    ok "$target/$name-src.tar.gz"

    step "the source tarball builds from a clean unpack"
    local scratch
    scratch=$(mktemp -d)
    tar xzf "$target/$name-src.tar.gz" -C "$scratch"
    if ( cd "$scratch/$name" && make -j"$(jobs_count)" >/dev/null 2>&1 && make check >/dev/null 2>&1 ); then
        rm -rf "$scratch"
        ok "builds and passes offline"
    else
        rm -rf "$scratch"
        die "the source tarball does not build on its own"
    fi
}

# --- verify ----------------------------------------------------------------

do_verify() {
    local archive="$1"
    [ -f "$archive" ] || die "no such archive: $archive"

    step "verifying $(basename "$archive")"
    local scratch
    scratch=$(mktemp -d)
    tar xzf "$archive" -C "$scratch" || die "could not unpack"

    local unpacked
    unpacked=$(find "$scratch" -mindepth 1 -maxdepth 1 -type d | head -n 1)
    [ -n "$unpacked" ] || die "the archive contains no directory"

    # A source tarball is verified by building it; a binary bundle by running
    # it. Telling them apart by looking rather than by naming convention.
    if [ -x "$unpacked/bin/sqpg" ]; then
        ( cd "$unpacked" && ./bin/sqpg list >/dev/null ) \
            || { rm -rf "$scratch"; die "the bundled binary cannot find its resources"; }
        ok "the bundled binary runs and finds its resources"

        # And that installing it works, since that is what a user does next.
        local prefix="$scratch/prefix"
        ( cd "$unpacked" && ./install.sh "$prefix" >/dev/null ) \
            || { rm -rf "$scratch"; die "install.sh failed"; }
        "$prefix/bin/sqpg" list >/dev/null \
            || { rm -rf "$scratch"; die "the installed binary cannot find its resources"; }
        ok "install.sh works, and the installed binary runs"
    elif [ -f "$unpacked/Makefile" ]; then
        ( cd "$unpacked" && make -j"$(jobs_count)" >/dev/null 2>&1 && make check >/dev/null 2>&1 ) \
            || { rm -rf "$scratch"; die "the source tarball does not build"; }
        ok "the source tarball builds and passes"
    else
        rm -rf "$scratch"
        die "unrecognised archive layout"
    fi

    rm -rf "$scratch"
}

# --- tag and publish -------------------------------------------------------

do_tag() {
    local version="$1"
    local tag="v$version"

    [ -z "$(git -C "$root" status --porcelain)" ] \
        || die "the tree has uncommitted changes; commit them before tagging"

    if git -C "$root" rev-parse "$tag" >/dev/null 2>&1; then
        die "tag $tag already exists"
    fi

    do_check

    git -C "$root" tag -a "$tag" -m "squared-pg $version" || die "could not tag"
    ok "tagged $tag"
    echo
    echo "   Nothing has been pushed. When you are ready:"
    echo "     tools/release.sh publish"
}

do_publish() {
    local version="$1"
    local tag="v$version"

    git -C "$root" rev-parse "$tag" >/dev/null 2>&1 \
        || die "$tag does not exist; run './release.sh tag' first"

    echo "   Pushing $tag starts the release workflow, which opens a DRAFT"
    echo "   release. Nothing becomes public until you publish the draft."
    echo
    printf '   Push %s to origin? [y/N] ' "$tag"
    read -r answer
    case "$answer" in
        y|Y|yes|YES) ;;
        *) warn "cancelled"; return 1 ;;
    esac

    git -C "$root" push origin "$tag" || die "push failed"
    ok "pushed $tag"
    echo
    echo "   watch:  gh run watch"
    echo "   draft:  gh release view $tag --web"
}

# --- status ----------------------------------------------------------------

do_status() {
    local version="$1" platform="$2"
    printf '   %-11s %s\n' "version" "$version"
    printf '   %-11s %s\n' "platform" "$platform"
    if [ -d "$root/.git" ]; then
        printf '   %-11s %s\n' "branch" "$(git -C "$root" rev-parse --abbrev-ref HEAD)"
        printf '   %-11s %s\n' "commit" "$(git -C "$root" rev-parse --short HEAD)"
        local dirty
        dirty=$(git -C "$root" status --porcelain | wc -l | tr -d ' ')
        if [ "$dirty" = "0" ]; then
            printf '   %-11s %s\n' "tree" "clean"
        else
            printf '   %-11s %s\n' "tree" "$dirty change(s), uncommitted"
        fi
        if git -C "$root" rev-parse "v$version" >/dev/null 2>&1; then
            printf '   %-11s %s\n' "tag" "v$version exists"
        else
            printf '   %-11s %s\n' "tag" "v$version not created"
        fi
    fi
    if [ -d "$out_dir" ]; then
        echo "   artifacts"
        for artifact in "$out_dir"/*.tar.gz; do
            [ -f "$artifact" ] || continue
            printf '     %s  %s\n' "$(basename "$artifact")" "$(du -h "$artifact" | cut -f1)"
        done
    fi
}

# ---------------------------------------------------------------------------

[ -n "$pkg_version" ] || pkg_version=$(project_version)
platform=$(host_platform)

echo
printf '%ssquared-pg release — %s — %s%s\n' "${C_STEP}" "$pkg_version" "$platform" "${C_OFF}"
echo

case "$command_name" in
    check)
        do_check ;;

    stage)
        stage=$(do_stage "$pkg_version" "$platform" "$out_dir") && ok "staged at $stage" ;;

    package)
        do_check
        step "packaging"
        archive=$(do_package "$pkg_version" "$platform" "$out_dir") && ok "$archive" ;;

    source)
        step "source tarball"
        do_source "$pkg_version" "$out_dir" ;;

    verify)
        [ -n "$tarball" ] || die "usage: ./release.sh verify <tarball>"
        do_verify "$tarball" ;;

    tag)
        do_tag "$pkg_version" ;;

    publish)
        do_publish "$pkg_version" ;;

    all)
        do_check
        step "source tarball"
        do_source "$pkg_version" "$out_dir"
        step "packaging"
        archive=$(do_package "$pkg_version" "$platform" "$out_dir") || die "packaging failed"
        ok "$archive"
        do_verify "$archive"
        echo
        printf '%sRelease artifacts ready in %s%s\n' "$C_OK" "$out_dir" "$C_OFF"
        echo
        echo "  Other platforms are built by CI. To cut a release:"
        echo "    tools/release.sh tag       # re-runs the checks, then tags"
        echo "    tools/release.sh publish   # pushes; CI drafts the release" ;;

    status)
        do_status "$pkg_version" "$platform" ;;

    clean)
        rm -rf "$out_dir" "$root/build-cmake"
        ok "removed $out_dir and build-cmake" ;;

    ""|help)
        sed -n '3,25p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//' ;;

    *)
        die "unknown command '$command_name' — try ./release.sh --help" ;;
esac
