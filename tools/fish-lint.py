#!/usr/bin/env python3
"""Lint the repository's fish scripts.

Two bug classes, both found the hard way in tools/release.fish, and both
invisible to anyone who cannot run fish:

  1. Shadowing a fish special variable. `version` is fish's own version number
     and is read-only, so `--argument-names root version` is a *parse-time*
     error in every function that declares it. The script fails before running
     a single line, and the banner prints fish's version instead of yours.

  2. Referencing a top-level `set --local` variable from inside a function.
     fish, unlike bash, does not expose a caller's locals to the functions it
     calls. `set --local root` at the top of a script is invisible inside every
     function below it, and `$root` silently expands to nothing -- so a path
     becomes "/tools/version-check.sh" and the failure names the wrong thing.

Where fish is installed, `fish --no-execute` is run first: the real parser
catches class 1 and everything else about syntax. The static analysis below
catches class 2, which no parser can see because it is not a syntax error.

Usage: tools/fish-lint.py [file ...]      (default: every .fish under tools/)
"""

from __future__ import annotations

import pathlib
import re
import shutil
import subprocess
import sys

# Read-only or reserved in fish. Shadowing any of these is an error or a
# subtle bug; `status` and `argv` in particular read as ordinary names.
FISH_SPECIAL = {
    "version", "FISH_VERSION", "status", "pipestatus", "argv", "history",
    "PWD", "SHLVL", "hostname", "USER", "CMD_DURATION", "fish_pid",
    "last_pid", "umask", "COLUMNS", "LINES",
}

DECLARE_LOCAL = re.compile(r"^\s*set\s+(?:--local|-l)\s+(\w+)")
DECLARE_GLOBAL = re.compile(r"^\s*set\s+(?:--global|-g|--universal|-U|-gx|--export)\s*[a-z-]*\s+(\w+)")
FUNCTION_START = re.compile(r"^\s*function\s+([\w.:-]+)(.*)$")
ARGUMENT_NAMES = re.compile(r"--argument-names\s+([\w\s]+?)(?:\s*--|\s*$)")
BLOCK_END = re.compile(r"^\s*end\s*$")
VARIABLE_USE = re.compile(r"\$(\w+)")


class Finding:
    def __init__(self, path: pathlib.Path, line: int, message: str, hint: str = ""):
        self.path, self.line, self.message, self.hint = path, line, message, hint

    def render(self) -> str:
        out = f"{self.path}:{self.line}: {self.message}"
        if self.hint:
            out += f"\n    {self.hint}"
        return out


def parse_fish(path: pathlib.Path) -> list[Finding]:
    findings: list[Finding] = []
    lines = path.read_text().splitlines()

    top_level_locals: dict[str, int] = {}
    # (name, start_line, body_lines, declared_names)
    functions: list[tuple[str, int, list[tuple[int, str]], set[str]]] = []

    depth = 0
    current: tuple[str, int, list[tuple[int, str]], set[str]] | None = None

    for number, raw in enumerate(lines, start=1):
        line = raw.split("#", 1)[0] if not raw.lstrip().startswith("#") else ""

        start = FUNCTION_START.match(line)
        if start and depth == 0:
            name, rest = start.group(1), start.group(2)
            declared = set()
            args = ARGUMENT_NAMES.search(rest)
            if args:
                declared.update(args.group(1).split())
                for shadowed in declared & FISH_SPECIAL:
                    findings.append(Finding(
                        path, number,
                        f"parameter '{shadowed}' shadows a fish special variable",
                        f"fish reserves ${shadowed}; this is a parse-time error. Rename it."))
            current = (name, number, [], declared)
            functions.append(current)
            depth = 1
            continue

        if depth > 0:
            # Nested blocks keep their own `end`; only the outermost closes the
            # function.
            if re.match(r"^\s*(if|for|while|switch|begin|function)\b", line):
                depth += 1
            elif BLOCK_END.match(line):
                depth -= 1
                if depth == 0:
                    current = None
                    continue
            if current is not None:
                current[2].append((number, line))
                declared = DECLARE_LOCAL.match(line) or DECLARE_GLOBAL.match(line)
                if declared:
                    current[3].add(declared.group(1))
                    if declared.group(1) in FISH_SPECIAL:
                        findings.append(Finding(
                            path, number,
                            f"'{declared.group(1)}' shadows a fish special variable",
                            f"fish reserves ${declared.group(1)}."))
            continue

        # Top level.
        local = DECLARE_LOCAL.match(line)
        if local:
            top_level_locals[local.group(1)] = number
            if local.group(1) in FISH_SPECIAL:
                findings.append(Finding(
                    path, number,
                    f"'{local.group(1)}' shadows a fish special variable",
                    f"fish reserves ${local.group(1)}; it is read-only."))

    # Class 2: a function reading a variable that only exists as a top-level
    # local. fish gives the function nothing.
    for name, start_line, body, declared in functions:
        reported: set[str] = set()
        for number, line in body:
            for used in VARIABLE_USE.findall(line):
                if used in declared or used in reported:
                    continue
                if used in top_level_locals:
                    reported.add(used)
                    findings.append(Finding(
                        path, number,
                        f"function '{name}' reads ${used}, which is a top-level "
                        f"`set --local` (line {top_level_locals[used]})",
                        "fish does not expose a caller's locals to called functions. "
                        "Use `set --global`, or pass it as a parameter."))

    return findings


def parse_check(path: pathlib.Path) -> list[Finding]:
    """Run the real fish parser, when there is one."""
    if shutil.which("fish") is None:
        return []
    result = subprocess.run(["fish", "--no-execute", str(path)],
                            capture_output=True, text=True)
    if result.returncode == 0:
        return []
    detail = (result.stderr or result.stdout).strip().splitlines()
    return [Finding(path, 0, "fish --no-execute rejected this file",
                    "\n    ".join(detail[:12]))]


def main(argv: list[str]) -> int:
    if argv:
        targets = [pathlib.Path(a) for a in argv]
    else:
        root = pathlib.Path(__file__).resolve().parent.parent
        targets = sorted(root.glob("tools/*.fish"))

    if not targets:
        print("fish-lint: no files to check")
        return 0

    findings: list[Finding] = []
    for path in targets:
        if not path.exists():
            print(f"fish-lint: no such file: {path}", file=sys.stderr)
            return 2
        findings += parse_check(path)
        findings += parse_fish(path)

    if not findings:
        checked = ", ".join(p.name for p in targets)
        note = "" if shutil.which("fish") else "  (fish not installed; syntax unchecked)"
        print(f"  ok   fish-lint: {checked}{note}")
        return 0

    for finding in findings:
        print(finding.render(), file=sys.stderr)
    print(f"\n  {len(findings)} finding(s)", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
