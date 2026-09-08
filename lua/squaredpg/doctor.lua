-- SPDX-License-Identifier: MIT
--
-- squaredpg.doctor -- what is assembled, what is installed, what is stale.
--
-- This exists because of a specific failure that took three commands to
-- produce and none to notice:
--
--   1. the `squared` repository was flattened and pushed;
--   2. `bootstrap.sh` checked out the pinned revision, which *reverted* the
--      clone to the pre-flatten commit;
--   3. `--update` recorded what was checked out -- the same old hash;
--   4. everything still worked, because bootstrap accepts both layouts;
--   5. a commit went in saying "adopt the flattened layout", which it had not.
--
-- Every individual step reported success. Nothing anywhere said "the pin is
-- behind the remote", because nothing was looking. The tool knew enough to
-- say so and had no place to say it.
--
-- ## What it will not do
--
-- **It does not touch the network.** Offline-first is a hard constraint
-- (Repository invariant 1.13), and a diagnostic that hangs on a dead network
-- is worse than one that admits what it cannot see. It reads the
-- remote-tracking refs git already has, and says when they are old.
--
-- **It does not fix anything.** Every finding names the command that would.
-- A doctor that repaired would be a doctor nobody could predict, and the
-- failure above came from commands that did more than they said.
--
-- **It never fails on a finding.** Exit status reflects severity so it
-- composes into a check, but a broken environment must still be inspectable
-- -- that is the same lesson as D-058.

local env = require("squaredpg.env")

local doctor = {}

local function capture(command)
  local pipe = io.popen(command .. " 2>/dev/null")
  if pipe == nil then return "" end
  local out = pipe:read("*a") or ""
  pipe:close()
  return out
end

local function trim(text)
  return (tostring(text):gsub("^%s+", ""):gsub("%s+$", ""))
end

-- ---------------------------------------------------------------------------
-- Findings
-- ---------------------------------------------------------------------------

local Report = {}
Report.__index = Report

local function new_report(report)
  return setmetatable({ out = report, worst = 0, findings = {} }, Report)
end

--- level: 0 ok, 1 note, 2 warning, 3 problem.
function Report:line(level, label, detail, fix)
  local mark = ({ [0] = "ok  ", [1] = "note", [2] = "warn", [3] = "STOP" })[level]
  self.out.out(("  %s  %-22s %s"):format(mark, label, detail))
  if fix ~= nil then
    self.findings[#self.findings + 1] = fix
  end
  if level > self.worst then self.worst = level end
end

function Report:heading(text)
  self.out.out("")
  self.out.out(text)
end

-- ---------------------------------------------------------------------------
-- Components
-- ---------------------------------------------------------------------------

local function pinned_ref(root, file)
  local handle = io.open(root .. "/" .. file, "r")
  if handle == nil then return nil end
  for line in handle:lines() do
    local text = trim(line)
    if text ~= "" and text:sub(1, 1) ~= "#" then
      handle:close()
      return text
    end
  end
  handle:close()
  return nil
end

--- A component's state, entirely from what git already knows locally.
local function component_state(dir)
  if not env.is_dir(dir) then return { present = false } end
  if not env.is_dir(dir .. "/.git") then return { present = true, repo = false } end

  local q = env.quote(dir)
  local state = { present = true, repo = true }
  state.head = trim(capture("git -C " .. q .. " rev-parse HEAD"))
  state.short = trim(capture("git -C " .. q .. " rev-parse --short HEAD"))
  state.branch = trim(capture("git -C " .. q .. " rev-parse --abbrev-ref HEAD"))
  state.detached = state.branch == "HEAD"
  state.dirty = trim(capture("git -C " .. q .. " status --porcelain")) ~= ""

  -- The remote-tracking ref, not the remote. Whatever the last fetch saw.
  -- Reading it costs nothing and is often enough to catch a stale pin; when
  -- it is not, saying "as of the last fetch" is more honest than silence.
  local upstream = trim(capture("git -C " .. q .. " rev-parse --abbrev-ref origin/HEAD"))
  if upstream == "" then upstream = "origin/main" end
  state.upstream = upstream
  state.remote_head = trim(capture("git -C " .. q .. " rev-parse " .. env.quote(upstream)))
  if state.remote_head ~= "" and state.head ~= "" then
    local behind = trim(capture("git -C " .. q .. " rev-list --count HEAD.."
                                .. env.quote(upstream)))
    state.behind = tonumber(behind) or 0
  end
  return state
end

local function check_components(root, rep)
  rep:heading("components")

  local any = false
  for _, spec in ipairs({ { "sqcart", "sqcart", "SQCART_VERSION" },
                          { "squared", "resources/.squared", "SQUARED_VERSION" } }) do
    local name, rel, pin_file = spec[1], spec[2], spec[3]
    local dir = root .. "/" .. rel
    local pin = pinned_ref(root, pin_file)
    local state = component_state(dir)

    if not state.present then
      rep:line(3, name, "absent", "./tools/bootstrap.sh")
      any = true
    elseif not state.repo then
      rep:line(2, name, rel .. " exists but is not a git checkout")
      any = true
    else
      any = true
      local where = state.detached and ("detached at " .. state.short)
                                    or (state.branch .. " " .. state.short)
      -- A pin that does not match HEAD means bootstrap has not been run since
      -- the pin changed, or someone moved the checkout by hand.
      if pin ~= nil and pin ~= "main" and state.head ~= "" and pin:sub(1, 7) ~= state.head:sub(1, 7) then
        rep:line(2, name, where .. "; pinned to " .. pin:sub(1, 7),
                 "./tools/bootstrap.sh    # check out the pin")
      elseif (state.behind or 0) > 0 then
        -- The case that produced this command. Every step reported success
        -- and the pin was three commits behind what had already been pushed.
        rep:line(2, name,
                 ("%s; %d commit(s) behind %s as of the last fetch")
                   :format(where, state.behind, state.upstream),
                 "./tools/bootstrap.sh --latest   # move the pin forward")
      else
        rep:line(0, name, where .. (pin and ("; pinned " .. pin:sub(1, 7)) or ""))
      end

      if state.dirty then
        rep:line(1, name .. " (working tree)", "uncommitted changes")
      end
    end
  end

  if not any then
    rep:line(1, "components", "none; this looks like an installed tool, not a checkout")
  end
end

-- ---------------------------------------------------------------------------
-- Resources
-- ---------------------------------------------------------------------------

local function check_resources(root, rep, sqpg_table)
  rep:heading("resources")

  local roots = {}
  for _, kind in ipairs({ "templates", "kits", "packages", "assets" }) do
    roots[#roots + 1] = { root .. "/resources/" .. kind, "resources/" .. kind }
    roots[#roots + 1] = { root .. "/resources/generator/" .. kind,
                          "resources/generator/" .. kind }
  end

  local present = 0
  for _, pair in ipairs(roots) do
    if env.is_dir(pair[1]) then
      present = present + 1
      local count = tonumber(trim(capture(
        "find " .. env.quote(pair[1]) .. " -mindepth 1 -maxdepth 1 -type d | wc -l"))) or 0
      local link = ""
      if env.run("test -L " .. env.quote(pair[1])) then
        link = " -> " .. trim(capture("readlink " .. env.quote(pair[1])))
      end
      if count > 0 then
        rep:line(0, pair[2], ("%d resource(s)%s"):format(count, link))
      end
    end
  end

  if present == 0 then
    rep:line(3, "resource roots", "none found",
             "./tools/bootstrap.sh    # assemble the framework resources")
  end

  -- Duplicates. Since D-058 these no longer stop the tool, which means
  -- nothing forces you to notice them -- so this is where they get noticed.
  local seen, dupes = {}, {}
  for _, pair in ipairs(roots) do
    if env.is_dir(pair[1]) then
      local listing = capture("ls -1 " .. env.quote(pair[1]))
      for name in listing:gmatch("[^\n]+") do
        if seen[name] ~= nil then
          dupes[#dupes + 1] = { name, seen[name], pair[2] }
        else
          seen[name] = pair[2]
        end
      end
    end
  end
  for _, d in ipairs(dupes) do
    rep:line(2, "duplicate", ("%s in %s and %s"):format(d[1], d[2], d[3]),
             "remove one, or point the roots at only one of them")
  end
  if #dupes == 0 and present > 0 then
    rep:line(0, "identities", "no duplicates")
  end

  local _ = sqpg_table
end

-- ---------------------------------------------------------------------------
-- Environment
-- ---------------------------------------------------------------------------

local function check_environment(rep, source)
  rep:heading("environment")

  local root, why = env.root()
  if root == nil then
    rep:line(3, "environment", why)
    return
  end

  if not env.initialised(root) then
    rep:line(2, "environment", root .. " is not initialised",
             "sqpg initialize")
    return
  end
  rep:line(0, "environment", root)

  for _, tool in ipairs({ "sqpg", "sqcart" }) do
    local installed = root .. "/.sqpg/bin/" .. tool
    if not env.exists(installed) then
      local fix = tool == "sqcart"
        and "make -C sqcart && sqpg initialize"
        or "sqpg initialize"
      rep:line(2, tool, "not installed in the environment", fix)
    else
      -- Installed copy against the built one. The environment holds a
      -- snapshot; a rebuild in the checkout does not reach it, and nothing
      -- else says so.
      local built = source .. "/build/" .. tool
      if tool == "sqcart" then built = source .. "/sqcart/build/sqcart" end
      if env.exists(built) and not env.run("cmp -s " .. env.quote(built) .. " "
                                           .. env.quote(installed)) then
        rep:line(2, tool, "installed copy differs from the built one",
                 "sqpg initialize    # push the rebuilt tool into the environment")
      else
        rep:line(0, tool, "installed")
      end
    end
  end

  -- On PATH, and pointing where you think.
  local which = trim(capture("command -v sqpg"))
  if which == "" then
    rep:line(1, "PATH", "sqpg is not on your PATH",
             ("ln -sf %s/.sqpg/bin/sqpg <a directory on your PATH>/sqpg"):format(root))
  else
    local resolved = trim(capture("readlink -f " .. env.quote(which)))
    local expected = trim(capture("readlink -f " .. env.quote(root .. "/.sqpg/bin/sqpg")))
    if resolved ~= expected and expected ~= "" then
      rep:line(1, "PATH", which .. " is not the environment's copy")
    else
      rep:line(0, "PATH", which)
    end
  end
end

-- ---------------------------------------------------------------------------

function doctor.run(options, report)
  options = options or {}
  local source = sqpg.installation
  if source == nil or source == "" then source = sqpg.cwd end

  local rep = new_report(report)
  report.out("sqpg " .. (sqpg.version or "") .. "  doctor")
  report.out("installation " .. source)

  check_components(source, rep)
  check_resources(source, rep)
  check_environment(rep, source)

  if #rep.findings > 0 then
    report.out("")
    report.out("suggested:")
    local shown = {}
    for _, fix in ipairs(rep.findings) do
      if not shown[fix] then
        report.out("  " .. fix)
        shown[fix] = true
      end
    end
  else
    report.out("")
    report.out("nothing to report.")
  end

  -- Severity, not success. A warning is worth a non-zero status in a check
  -- target and is not worth stopping a person.
  if rep.worst >= 3 then return 1 end
  if rep.worst >= 2 then return 1 end
  return 0
end

return doctor
