-- SPDX-License-Identifier: MIT
--
-- squaredpg.drive -- the workflow verbs.
--
-- format, lint, check, test, docs, dist.
--
-- **These are drivers, not implementations.** `sqpg lint` does not know what
-- a linter is. It finds the workspace and runs `make lint`. The target is
-- contributed by a kit, the configuration lives next to the code it governs,
-- and squared-pg's part is finding the right directory and reporting the
-- exit status.
--
-- Three things follow from that, and they are the reason for it:
--
--   1. **The engine keeps its no-external-dependency property.** A driver's
--      whole job is executing other people's tools. If that lived in the
--      engine, the engine would depend on clang-format being installed.
--
--   2. **A generated workspace stays independent of squared-pg.** `make lint`
--      works with no `sqpg` on the machine, which is the invariant written
--      into the 2.7.11 stub: squared-pg is required to *change* a workspace's
--      shape, never to build, test or ship one. `sqpg lint` is a convenience
--      over `make lint`, and must never become the only way to reach it.
--
--   3. **The vocabulary is fixed and the implementation is not.** Six verbs,
--      identical in every workspace regardless of language, kit or platform.
--      All of the value is in the names never changing and the defaults being
--      chosen already; whether a verb is a make target, a script or a future
--      binary is nobody's problem but the kit author's.
--
-- What is deliberately absent: profile and debug. They are interactive,
-- host-specific and session-shaped, and forcing them into a verb that runs
-- unattended and exits with a status is where this design would go wrong. A
-- kit that wants `make debug` may ship one; squared-pg will not pretend to
-- abstract a debugger session.
--
-- --explain prints the command instead of running it. That is what keeps this
-- from becoming a black box experts route around, and it is also the teaching
-- mechanism: a beginner learns what `make lint` does by reading what it ran.

local env = require("squaredpg.env")

local drive = {}

--- The verb vocabulary. Closed on purpose.
--
-- Adding a verb is a decision about the vocabulary every workspace shares,
-- not a convenience. A kit needing something outside these six ships a make
-- target with its own name, which a user reaches with plain `make`.
drive.verbs = {
  { name = "format", summary = "rewrite sources to the project's style" },
  { name = "lint",   summary = "report style and correctness complaints" },
  { name = "check",  summary = "static analysis" },
  { name = "test",   summary = "run the test suite" },
  { name = "docs",   summary = "build the documentation" },
  { name = "dist",   summary = "produce a distributable artifact" },
}

function drive.is_verb(name)
  for _, verb in ipairs(drive.verbs) do
    if verb.name == name then return true end
  end
  return false
end

--- Does the workspace's Makefile offer this target?
--
-- Read rather than executed. `make -n` would be authoritative but runs the
-- prerequisites' recipes in some cases, and a target-existence check must not
-- have side effects. A grep for the target's rule is approximate, and the
-- approximation fails safe: a false positive lands in `make`'s own error
-- message, which is a perfectly good one.
local function offers_target(workspace, verb)
  local handle = io.open(workspace .. "/Makefile", "r")
  if handle == nil then return false end
  local pattern = "^" .. verb .. ":"
  for line in handle:lines() do
    if line:match(pattern) then
      handle:close()
      return true
    end
  end
  handle:close()

  -- Kits contribute fragments under mk/, included by the workspace Makefile.
  -- A verb's target usually lives in one of those rather than in the root
  -- file, so a root-only search would report almost everything as missing.
  local pipe = io.popen("grep -lE " .. env.quote("^" .. verb .. ":") .. " "
                        .. env.quote(workspace) .. "/mk/*.mk 2>/dev/null")
  if pipe == nil then return false end
  local found = pipe:read("*l")
  pipe:close()
  return found ~= nil and found ~= ""
end

--- Run one verb against a workspace.
--
-- Returns an exit code. `make`'s status is passed through unchanged: a lint
-- failure must be a lint failure to whatever called sqpg, and rewriting it
-- would break every use of these verbs in a script or a CI step.
function drive.run(verb, options, report)
  options = options or {}

  local workspace = options.workspace
  if workspace == nil then
    workspace = env.find_workspace(".")
  end
  if workspace == nil then
    report.err("sqpg: no workspace here or above")
    report.err("      run this inside a generated workspace, or pass --workspace PATH")
    return 2
  end

  local command = "make -C " .. env.quote(workspace) .. " " .. verb
  if options.jobs ~= nil then
    command = command .. " -j" .. tostring(options.jobs)
  end

  if options.explain then
    report.out(command)
    return 0
  end

  if not offers_target(workspace, verb) then
    report.err(("sqpg: %s has no `%s` target"):format(env.basename(workspace), verb))
    report.err(("      %s is contributed by a kit; this workspace resolved none that provides it")
      :format(verb))
    report.err(("      `sqpg %s --explain` shows the command that would run"):format(verb))
    return 1
  end

  local ok, code = env.run(command)
  if ok then return 0 end
  return type(code) == "number" and code or 1
end

return drive
