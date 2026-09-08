-- SPDX-License-Identifier: MIT
--
-- squaredpg.destruct -- deliberate destruction.
--
-- This exists because `rm -rf ~/sqsysroot` is one tab-completion away and has
-- already cost one project. A safer command does not remove that danger; it
-- only competes with it. So it has to be worth reaching for, which means the
-- confirmation flow must tell you something `rm` cannot:
--
--   **what in there is not backed up anywhere else.**
--
-- The inventory reports uncommitted changes, unpushed commits and missing
-- remotes for every repository it is about to delete. That is the part `rm`
-- has never done and the reason to type this instead.
--
-- ## Two factors
--
-- 1. **A derived token.** The first invocation prints the inventory and a
--    token computed from what it found -- the paths, the file counts, the git
--    state. The second invocation must supply it. This proves you saw the
--    inventory, and because the token is derived rather than stored, it
--    changes whenever *the inventory* changes: a workspace appearing or
--    vanishing, a file count moving, a repository becoming dirty or gaining a
--    remote.
--
--    It does not change when a file's contents change without moving any of
--    those. That is the correct scope, not a gap: the token certifies that
--    what you were shown is still true, and "65 files, uncommitted changes,
--    no remote" is still true after another line is added to one of them.
--    Claiming more than that in the user-facing text would be a lie the
--    implementation could not keep.
--
-- 2. **A typed phrase on stdin.** The exact target, typed out. Nothing a
--    shell completes for you produces `sandbox/hello`.
--
-- There is no `--yes`, no `--force`, and no environment variable that skips
-- either factor. A bypass would be used, and then this would be `rm` with
-- extra steps.
--
-- Non-interactive use is permitted: a pipe can supply the phrase. The
-- protection is not "a human must be present" -- it is that neither factor
-- can be produced by accident.

local env = require("squaredpg.env")

local destruct = {}

-- ---------------------------------------------------------------------------
-- Inventory
-- ---------------------------------------------------------------------------

local function capture(command)
  local pipe = io.popen(command .. " 2>/dev/null")
  if pipe == nil then return "" end
  local out = pipe:read("*a") or ""
  pipe:close()
  return out
end

local function count_files(path)
  local text = capture("find " .. env.quote(path) .. " -type f | wc -l")
  return tonumber(text:match("%d+")) or 0
end

--- What git knows about a tree, or nil if it is not a repository.
--
-- Three questions, in order of how much they should worry you:
-- uncommitted changes are work that exists only here; unpushed commits are
-- work that exists only on this machine; no remote means the whole history
-- is local. Any of them makes deletion unrecoverable in a way the file count
-- does not convey.
local function git_state(path)
  if not env.is_dir(path .. "/.git") then return nil end

  local state = { dirty = false, unpushed = 0, remote = nil }

  local status = capture("git -C " .. env.quote(path) .. " status --porcelain")
  state.dirty = status:match("%S") ~= nil

  local remote = capture("git -C " .. env.quote(path) .. " remote"):match("^%s*(%S+)")
  state.remote = remote

  if remote ~= nil then
    -- Commits on the current branch with no upstream equivalent. Counts as
    -- unpushed whether the branch tracks anything or not: a branch with no
    -- upstream has *all* its commits here only.
    local ahead = capture("git -C " .. env.quote(path)
                          .. " rev-list --count @{upstream}..HEAD")
    if ahead:match("%d") then
      state.unpushed = tonumber(ahead:match("%d+")) or 0
    else
      local total = capture("git -C " .. env.quote(path) .. " rev-list --count HEAD")
      state.unpushed = tonumber(total:match("%d+")) or 0
      state.no_upstream = true
    end
  end

  return state
end

--- Everything that would be removed, as a list of entries.
local function survey(root, scope, name)
  local entries = {}

  local function add(path, label)
    if not env.is_dir(path) then return end
    entries[#entries + 1] = {
      path  = path,
      label = label,
      files = count_files(path),
      git   = git_state(path),
    }
  end

  if scope == "workspace" then
    for _, tier in ipairs({ "sandbox", "project" }) do
      add(root .. "/" .. tier .. "/" .. name, tier .. "/" .. name)
    end
  elseif scope == "sandbox" then
    local listing = capture("ls -1 " .. env.quote(root .. "/sandbox"))
    for line in listing:gmatch("[^\n]+") do
      add(root .. "/sandbox/" .. line, "sandbox/" .. line)
    end
  elseif scope == "environment" then
    local listing = capture("ls -1 " .. env.quote(root .. "/sandbox") .. " "
                            .. env.quote(root .. "/project"))
    -- Tiers first, then the tool directory, so the report reads worst-first.
    for _, tier in ipairs({ "sandbox", "project" }) do
      local names = capture("ls -1 " .. env.quote(root .. "/" .. tier))
      for line in names:gmatch("[^\n]+") do
        add(root .. "/" .. tier .. "/" .. line, tier .. "/" .. line)
      end
    end
    add(root .. "/.sqpg", ".sqpg (the installed tool)")
    local _ = listing
  end

  return entries
end

-- ---------------------------------------------------------------------------
-- The token
-- ---------------------------------------------------------------------------

--- FNV-1a over a description of what was found.
--
-- Derived, never stored. Two consequences, both wanted: there is no state
-- file to go stale or to be forged, and the token changes the moment the
-- contents change -- so a `--confirm` copied from an earlier session is
-- rejected rather than silently acting on a tree that has moved on.
local function token_for(entries, scope)
  local material = scope
  for _, e in ipairs(entries) do
    material = material .. "|" .. e.path .. ":" .. tostring(e.files)
    if e.git ~= nil then
      material = material .. ":" .. tostring(e.git.dirty)
                          .. ":" .. tostring(e.git.unpushed)
                          .. ":" .. tostring(e.git.remote)
    end
  end

  local hash = 2166136261
  for i = 1, #material do
    hash = hash ~ material:byte(i)
    -- 32-bit FNV prime, kept in range by hand: Lua 5.4 integers are 64-bit
    -- and would otherwise grow without bound.
    hash = (hash * 16777619) & 0xFFFFFFFF
  end
  return string.format("%08x", hash)
end

-- ---------------------------------------------------------------------------
-- Reporting
-- ---------------------------------------------------------------------------

local function report_inventory(entries, report)
  local total, at_risk = 0, {}

  for _, e in ipairs(entries) do
    total = total + e.files
    local note = ""
    if e.git ~= nil then
      local flags = {}
      if e.git.dirty then flags[#flags + 1] = "uncommitted changes" end
      if e.git.remote == nil then
        flags[#flags + 1] = "no remote"
      elseif e.git.no_upstream then
        flags[#flags + 1] = ("%d commit(s), no upstream"):format(e.git.unpushed)
      elseif e.git.unpushed > 0 then
        flags[#flags + 1] = ("%d unpushed commit(s)"):format(e.git.unpushed)
      end
      if #flags > 0 then
        note = "  <- " .. table.concat(flags, ", ")
        at_risk[#at_risk + 1] = e
      else
        note = "  (pushed)"
      end
    end
    report.out(("  %-34s %5d files%s"):format(e.label, e.files, note))
  end

  report.out("")
  report.out(("%d director%s, %d files"):format(#entries,
                                                #entries == 1 and "y" or "ies", total))

  if #at_risk > 0 then
    report.out("")
    report.out(("%d of these hold work that exists nowhere else:"):format(#at_risk))
    for _, e in ipairs(at_risk) do
      report.out("  " .. e.label)
    end
    report.out("")
    report.out("this is the part `rm -rf` would not have told you.")
  end

  return total, at_risk
end

-- ---------------------------------------------------------------------------
-- The command
-- ---------------------------------------------------------------------------

function destruct.run(options, report)
  options = options or {}

  local root, why = env.root()
  if root == nil then
    report.err("sqpg: " .. why)
    return 1
  end
  if options.root ~= nil then root = options.root end

  if not env.initialised(root) then
    report.err("sqpg: " .. root .. " is not an initialised environment")
    return 1
  end

  -- Scope. Exactly one, and never inferred: a destructive command must not
  -- guess what you meant.
  local scope, name, phrase
  if options.environment then
    scope, phrase = "environment", "destroy the environment"
  elseif options.sandbox then
    scope, phrase = "sandbox", "destroy the sandbox"
  elseif options.name ~= nil then
    scope, name = "workspace", options.name
    phrase = "destroy " .. options.name
  else
    report.err("sqpg: selfdestruct needs a target")
    report.err("      sqpg selfdestruct <name>      one workspace, in either tier")
    report.err("      sqpg selfdestruct --sandbox   everything in sandbox")
    report.err("      sqpg selfdestruct --environment")
    return 2
  end

  local entries = survey(root, scope, name)
  if #entries == 0 then
    report.err(("sqpg: nothing to destroy: no '%s' in this environment"):format(name or scope))
    return 1
  end

  local expected = token_for(entries, scope)

  -- First invocation: inventory and stop. No prompt, because a prompt is a
  -- thing you dismiss; a second command is a thing you decide.
  if options.confirm == nil then
    report.out(("about to destroy: %s"):format(scope == "workspace" and name or scope))
    report.out(("environment %s"):format(root))
    report.out("")
    report_inventory(entries, report)
    report.out("")
    report.out("nothing has been deleted. to proceed:")
    report.out("")
    if scope == "workspace" then
      report.out(("  echo '%s' | sqpg selfdestruct %s --confirm %s"):format(phrase, name, expected))
    else
      report.out(("  echo '%s' | sqpg selfdestruct --%s --confirm %s"):format(phrase, scope, expected))
    end
    report.out("")
    report.out("the token is computed from the inventory above -- the paths, the")
    report.out("file counts, the git state. If any of that changes the token stops")
    report.out("matching, so a command copied from an earlier session will not run")
    report.out("against a tree that has moved on.")
    return 1
  end

  -- Second invocation: the token must match what is there NOW.
  if options.confirm ~= expected then
    report.err("sqpg: confirmation token does not match")
    report.err(("      given    %s"):format(options.confirm))
    report.err(("      expected %s"):format(expected))
    report.err("")
    report.err("      the token is derived from the contents, so a mismatch means")
    report.err("      either it was mistyped or the target changed since you looked.")
    report.err("      run selfdestruct again with no --confirm to see it now.")
    return 1
  end

  -- Second factor: the phrase, typed. Read from stdin so a pipe can supply
  -- it -- the protection is not that a human is present, it is that neither
  -- factor can be produced by accident or by autocompletion.
  report.out(("type the phrase to confirm: %s"):format(phrase))
  local typed = io.read("l")
  if typed == nil then
    report.err("sqpg: no confirmation phrase supplied; nothing deleted")
    return 1
  end
  if typed ~= phrase then
    report.err("sqpg: phrase does not match; nothing deleted")
    return 1
  end

  local removed = 0
  for _, e in ipairs(entries) do
    if env.run("rm -rf " .. env.quote(e.path)) then
      report.out(("  removed  %s"):format(e.label))
      removed = removed + 1
    else
      report.err(("sqpg: could not remove %s"):format(e.path))
    end
  end

  -- The tiers are recreated, the tool directory is not. An environment
  -- without its tiers is broken; an environment without its tool is one
  -- `sqpg initialize` away, and recreating it here would mean copying a
  -- binary the caller just asked to delete.
  if scope ~= "workspace" then
    env.mkdir_p(root .. "/sandbox")
    if scope == "environment" then
      env.mkdir_p(root .. "/project")
      report.out("")
      report.out("the tool is gone. reinstall it with:")
      report.out("  cd <squared-pg repo> && ./build/sqpg initialize")
    end
  end

  report.out("")
  report.out(("destroyed %d director%s"):format(removed, removed == 1 and "y" or "ies"))
  return 0
end

return destruct
