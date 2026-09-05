# Shared helpers for the probe scripts. Sourced, never run directly.
#
# POSIX sh. Termux has bash, but these are meant to be runnable on any host
# someone tries to build squared-pg on, and sh is the one that is always there.
#
# Everything here is read-only and never exits non-zero on a missing tool: the
# point is to produce a complete report, not to stop at the first gap.

PROBE_FOUND=0
PROBE_MISSING=0

section() {
    printf '\n\033[1;36m== %s\033[0m\n' "$*"
}

# ok <label> [detail]
ok() {
    PROBE_FOUND=$((PROBE_FOUND + 1))
    printf '  \033[32m+\033[0m %-34s %s\n' "$1" "${2-}"
}

# no <label> [detail]
no() {
    PROBE_MISSING=$((PROBE_MISSING + 1))
    printf '  \033[31m-\033[0m %-34s %s\n' "$1" "${2-}"
}

# note <label> [detail] -- neither a pass nor a fail, just information
note() {
    printf '    %-32s %s\n' "$1" "${2-}"
}

warn() {
    printf '  \033[33m!\033[0m %s\n' "$*"
}

# have <command>
have() {
    command -v "$1" >/dev/null 2>&1
}

# where <command> -- absolute path, or empty
where() {
    command -v "$1" 2>/dev/null || true
}

# first_line <command...> -- first line of output, errors swallowed
first_line() {
    "$@" 2>&1 | head -n 1 || true
}

# Scratch space. $TMPDIR, never /tmp: Termux has no /tmp.
probe_scratch() {
    _base=${TMPDIR:-/tmp}
    _dir="$_base/squared-pg-probe.$$"
    mkdir -p "$_dir" 2>/dev/null || return 1
    printf '%s' "$_dir"
}

summary() {
    printf '\n  %s: %d present, %d absent\n' "$1" "$PROBE_FOUND" "$PROBE_MISSING"
}
