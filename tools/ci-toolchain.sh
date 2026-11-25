#!/usr/bin/env bash
# ci-toolchain.sh — clang and libc++ for CI, as the build needs them.
#
#   tools/ci-toolchain.sh BINDIR
#
# Installs clang, lld and libc++ $LLVM (default 23: what sqterm's packages
# carry) from apt.llvm.org on Ubuntu, or Homebrew's llvm on macOS, and
# writes two wrappers into BINDIR, `clang++` and `clang`, that build and link
# against that libc++; then sqmake, the build (no make), bootstrapped from its
# source with them (BINDIR/sqmake; $SQMAKE_REF a branch or tag, else its
# main). Put BINDIR first on PATH and pass CXX=BINDIR/clang++: the engine,
# sq.cart (`import std;` from that libc++'s module sources) and the projects
# `sqmake smoke` generates then all use one standard library, as on Termux,
# where clang uses libc++ by default.
#
# CI only: the network is used here, once, before anything builds.

set -euo pipefail

bin="${1:?usage: ci-toolchain.sh BINDIR}"
llvm="${LLVM:-23}"
mkdir -p "$bin"

case "$(uname -s)" in
    Linux)
        . /etc/os-release
        curl -fsSL https://apt.llvm.org/llvm-snapshot.gpg.key \
            | sudo tee /etc/apt/trusted.gpg.d/apt.llvm.org.asc >/dev/null
        echo "deb http://apt.llvm.org/$VERSION_CODENAME/ llvm-toolchain-$VERSION_CODENAME-$llvm main" \
            | sudo tee /etc/apt/sources.list.d/llvm.list >/dev/null
        sudo apt-get update -q
        sudo apt-get install --yes --no-install-recommends clang-"$llvm" lld-"$llvm" \
            libc++-"$llvm"-dev libc++abi-"$llvm"-dev
        prefix="/usr/lib/llvm-$llvm"
        extra="-fuse-ld=lld"
        ;;
    Darwin)
        brew install --quiet llvm
        prefix="$(brew --prefix llvm)"
        extra="-L$prefix/lib/c++ -Wl,-rpath,$prefix/lib/c++"
        ;;
    *)
        echo "ci-toolchain.sh: no recipe for $(uname -s)" >&2
        exit 1
        ;;
esac

printf '#!/bin/sh\nexec %s -stdlib=libc++ %s "$@"\n' "$prefix/bin/clang++" "$extra" > "$bin/clang++"
printf '#!/bin/sh\nexec %s "$@"\n' "$prefix/bin/clang" > "$bin/clang"
chmod +x "$bin/clang++" "$bin/clang"

"$bin/clang++" --version | head -n 1
manifest="$("$bin/clang++" -print-library-module-manifest-path)"
echo "std module manifest: $manifest"
test -f "$manifest" || { echo "ci-toolchain.sh: this libc++ has no std module" >&2; exit 1; }

# sqmake: from its source, with these wrappers (tools/bootstrap.sh needs a
# compiler and a shell, nothing else).
if [ ! -x "$bin/sqmake" ]; then
    src="$bin/sqmake-src"
    rm -rf "$src"
    git clone --quiet --depth 1 ${SQMAKE_REF:+--branch "$SQMAKE_REF"} \
        https://github.com/squared-netizen/sqmake "$src"
    (cd "$src" && CXX="$bin/clang++" CC="$bin/clang" sh tools/bootstrap.sh)
    ln -sf "$src/build/sqmake" "$bin/sqmake"
fi
"$bin/sqmake" --version
