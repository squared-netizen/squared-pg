-- SPDX-License-Identifier: MIT
--
-- squaredpg.env -- finding the workspace a command was run in.
--
-- sqpg once kept an environment of its own (~/sqsysroot, with sandbox and
-- project tiers, and promote, demote, quarantine and selfdestruct to move
-- workspaces between them). sqterm's pkg installs sqpg now, and a workspace
-- lives wherever it was made, so that went (2026-11-23). What remains is
-- the one question a command still asks: which workspace am I in?

local env = {}

local function quote(path)
  -- Single quotes with the standard '"'"' escape: paths come from argv.
  return "'" .. tostring(path):gsub("'", "'\\''") .. "'"
end

local function run(command)
  local ok, how, code = os.execute(command)
  if type(ok) == "number" then           -- Lua 5.1 style, kept for safety
    return ok == 0, ok
  end
  return ok == true, code or how
end

function env.exists(path)
  local handle = io.open(path, "r")
  if handle ~= nil then
    handle:close()
    return true
  end
  -- io.open fails on directories on some hosts, so fall back to a test that
  -- does not care what kind of node it is.
  return run("test -e " .. quote(path))
end

--- Absolute, symlink-free path, or nil if it does not exist.
function env.resolve(path)
  local pipe = io.popen("cd " .. quote(path) .. " 2>/dev/null && pwd -P")
  if pipe == nil then return nil end
  local out = pipe:read("*l")
  pipe:close()
  if out == nil or out == "" then return nil end
  return out
end

--- Nearest enclosing workspace, searching upward from `start`, recognised by
--- its metadata record and by nothing else (D-055): a Makefile beside an
--- `mk/` folder describes half the C projects in existence.
function env.find_workspace(start)
  local here = env.resolve(start or ".")
  if here == nil then return nil end
  while true do
    if env.exists(here .. "/.squared/metadata.json") then
      return here
    end
    local parent = here:match("^(.*)/[^/]+$")
    if parent == nil or parent == "" or parent == here then return nil end
    here = parent
  end
end

return env
