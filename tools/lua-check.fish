#!/usr/bin/env fish
#
# Lua validation and formatting checks for squared-pg.
#
# Usage:
#   tools/lua-check.fish
#   tools/lua-check.fish --fix
#   tools/lua-check.fish --full
#   tools/lua-check.fish --help
#
# The script may be invoked from any directory inside the repository.
#

function usage
    echo "Usage: "(status basename)" [--fix] [--full]"
    echo
    echo "Run the squared-pg Lua tooling checks."
    echo
    echo "  --fix    Format tracked Lua source with StyLua before checking."
    echo "  --full   Also run the repository Lua test suite."
    echo "  --help   Show this help."
end

set -l script_dir (dirname (status --current-filename))
set -l repo_root (realpath "$script_dir/..")

# Find the repository root from the script location, while allowing the
# script to be invoked through a symlink.
if not test -d "$repo_root/.git"
    echo "lua-check: repository root not found: $repo_root" >&2
    exit 2
end

set -l fix 0
set -l full 0

for arg in $argv
    switch $arg
        case --fix
            set fix 1
        case --full
            set full 1
        case --help -h
            usage
            exit 0
        case '*'
            echo "lua-check: unknown option: $arg" >&2
            usage >&2
            exit 2
    end
end

set -l failures 0
set -l checked 0

function require_command
    set -l command_name $argv[1]
    if not command -q $command_name
        echo "lua-check: required command not found: $command_name" >&2
        echo "Install it with: pkg install lua-language-server tree-sitter tree-sitter-lua stylua" >&2
        return 1
    end
    return 0
end

for command_name in lua-language-server tree-sitter stylua
    if not require_command $command_name
        exit 2
    end
end

echo "== squared-pg Lua checks =="
echo "Repository: $repo_root"
echo

# Only operate on Lua files in the source/test/tool portions of this checkout.
# Generated/private build trees are deliberately excluded.
set -l lua_roots
for candidate in lua tests tools
    if test -d "$repo_root/$candidate"
        set -a lua_roots "$repo_root/$candidate"
    end
end

if test (count $lua_roots) -eq 0
    echo "lua-check: no Lua source directories found." >&2
    exit 2
end

set -l lua_files
for lua_root in $lua_roots
    for file in (find "$lua_root" -type f -name '*.lua' 2>/dev/null)
        set -a lua_files $file
    end
end

set checked (count $lua_files)

if test $checked -eq 0
    echo "No Lua files found."
    exit 0
end

echo "Lua files: $checked"
echo

# ---------------------------------------------------------------------------
# StyLua
# ---------------------------------------------------------------------------
echo "-- StyLua --"

if test $fix -eq 1
    stylua $lua_roots
    if test $status -ne 0
        echo "lua-check: StyLua formatting failed." >&2
        set failures (math $failures + 1)
    end
else
    stylua --check $lua_roots
    if test $status -ne 0
        echo "lua-check: StyLua check failed." >&2
        set failures (math $failures + 1)
    end
end

echo

# ---------------------------------------------------------------------------
# LuaLS
# ---------------------------------------------------------------------------
echo "-- LuaLS --"

set -l report_dir "$repo_root/build/lua-check"
mkdir -p "$report_dir"

# LuaLS's --check mode uses the workspace configuration when run against
# the repository root. Keep its generated report out of the source tree.
lua-language-server \
    --check="$repo_root" \
    --checklevel=Information \
    --logpath="$report_dir" \
    --loglevel=warn

set -l luals_status $status
if test $luals_status -ne 0
    echo "lua-check: LuaLS reported diagnostics (exit status $luals_status)." >&2
    set failures (math $failures + 1)
else
    echo "LuaLS: no errors reported."
end

echo

# ---------------------------------------------------------------------------
# Tree-sitter parse validation
# ---------------------------------------------------------------------------
echo "-- Tree-sitter --"

# tree-sitter-lua is installed as a Termux grammar package. The CLI discovers
# installed grammars through its normal configuration/search mechanism.
# Parse every Lua file individually so one malformed file does not hide the
# location of another.
set -l parse_failures 0

for file in $lua_files
    tree-sitter parse "$file" >/dev/null 2>&1
    if test $status -ne 0
        echo "Tree-sitter parse failure: "(string replace "$repo_root/" "" "$file") >&2
        set parse_failures (math $parse_failures + 1)
    end
end

if test $parse_failures -ne 0
    echo "Tree-sitter: $parse_failures file(s) failed to parse." >&2
    set failures (math $failures + $parse_failures)
else
    echo "Tree-sitter: all $checked Lua file(s) parsed successfully."
end

# ---------------------------------------------------------------------------
# Optional full test suite
# ---------------------------------------------------------------------------
if test $full -eq 1
    echo
    echo "-- Lua tests --"

    if test -f "$repo_root/tests/run.lua"
        lua "$repo_root/tests/run.lua"
        if test $status -ne 0
            echo "lua-check: Lua test suite failed." >&2
            set failures (math $failures + 1)
        end
    else
        echo "lua-check: tests/run.lua not found; skipping." >&2
    end
end

echo
if test $failures -eq 0
    echo "Lua checks passed."
    exit 0
else
    echo "Lua checks failed: $failures check(s)." >&2
    exit 1
end
