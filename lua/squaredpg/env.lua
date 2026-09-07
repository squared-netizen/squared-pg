-- SPDX-License-Identifier: MIT
--
-- squaredpg.env -- the sqsysroot environment.
--
-- "Environment", not "workspace": a workspace is one generated project
-- (§2.7.3) and the word is spoken for. The environment is the place
-- workspaces live.
--
--   ~/sqsysroot/
--     project/      promoted work. Version controlled, backed up, precious.
--     sandbox/      experiments. Nothing here is irreplaceable.
--     .squared/     tools. Binaries, resources, workflows.
--
-- The invariant that generates every rule below is: **nothing in sandbox is
-- irreplaceable**. Not "sandbox has no git" -- that is a consequence, and
-- stating the consequence as the rule invites arguing about it case by case.
-- Everything in sandbox is either regenerable from a template or discardable,
-- which is what makes it safe to point an agent at, and what makes demotion
-- the dangerous direction rather than promotion.
--
-- Why this layer and not the engine: the engine generates workspaces and has
-- no external runtime dependency, deliberately. Creating directories and
-- moving trees is host interaction, which is the Lua layer's job -- the same
-- layer that already owns argv, stdio and configuration. Putting `mkdir` in
-- the engine would trade that property for nothing.

local env = {}

-- ---------------------------------------------------------------------------
-- Host primitives
-- ---------------------------------------------------------------------------
--
-- Shelling out for mkdir/mv rather than binding a filesystem API. Two
-- reasons: this layer is already the host boundary, and every command here is
-- one a user could have typed, which matters for `--explain`. A user who runs
-- `sqpg promote --explain` sees `mv`, not an opaque engine operation.

local function quote(path)
  -- Single quotes with the standard '"'"' escape. Paths come from argv and
  -- from $HOME, so neither is trusted to be free of spaces or quotes.
  return "'" .. tostring(path):gsub("'", "'\\''") .. "'"
end

env.quote = quote

local function run(command)
  local ok, how, code = os.execute(command)
  if type(ok) == "number" then           -- Lua 5.1 style, kept for safety
    return ok == 0, ok
  end
  return ok == true, code or how
end

env.run = run

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

function env.is_dir(path)
  return run("test -d " .. quote(path))
end

function env.mkdir_p(path)
  return run("mkdir -p " .. quote(path))
end

function env.rename(from, to)
  -- os.rename rather than `mv`: it fails rather than falling back to a
  -- copy-and-delete across filesystems, and a silent copy is exactly what
  -- must not happen to a tree being promoted. A cross-device promotion should
  -- report that plainly, not half-succeed.
  return os.rename(from, to)
end

-- ---------------------------------------------------------------------------
-- Layout
-- ---------------------------------------------------------------------------

--- Root of the environment.
--
-- $SQSYSROOT wins so a second environment can be used without reconfiguring
-- anything -- for testing this file, among other things. Otherwise
-- ~/sqsysroot, which is the documented default and the one every beginner
-- gets without making a decision.
function env.root()
  local declared = os.getenv("SQSYSROOT")
  if declared ~= nil and declared ~= "" then
    return declared
  end
  local home = os.getenv("HOME")
  if home == nil or home == "" then
    return nil, "neither $SQSYSROOT nor $HOME is set; cannot locate the environment"
  end
  return home .. "/sqsysroot"
end

--- The four directories `initialize` creates, in creation order.
--
-- `.squared` last: it is the marker an initialised environment is recognised
-- by, so creating it before the tiers exist would let a failure halfway
-- through leave something that looks complete.
function env.layout(root)
  return {
    { path = root,                 name = "",         summary = "environment root" },
    { path = root .. "/project",   name = "project",  summary = "promoted work; version controlled" },
    { path = root .. "/sandbox",   name = "sandbox",  summary = "experiments; nothing here is irreplaceable" },
    { path = root .. "/.squared",  name = ".squared", summary = "tools, resources and workflows" },
  }
end

function env.initialised(root)
  return env.is_dir(root .. "/.squared")
      and env.is_dir(root .. "/project")
      and env.is_dir(root .. "/sandbox")
end

-- ---------------------------------------------------------------------------
-- Tiers
-- ---------------------------------------------------------------------------

env.tiers = { project = "project", sandbox = "sandbox" }

--- Which tier a path sits in, or nil if it is outside the environment.
--
-- Prefix matching on the resolved path. Deliberately not a metadata lookup:
-- the tier IS the location (D-046), so that `ls ~/sqsysroot/sandbox` tells
-- the truth without consulting anything. A metadata field could disagree with
-- the filesystem; a directory cannot disagree with itself.
function env.tier_of(root, path)
  local resolved = env.resolve(path)
  if resolved == nil then return nil end
  for _, tier in pairs(env.tiers) do
    local prefix = root .. "/" .. tier .. "/"
    if resolved:sub(1, #prefix) == prefix then
      return tier, resolved
    end
  end
  return nil, resolved
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

--- The last path segment, which is a workspace's name in its tier.
function env.basename(path)
  return (tostring(path):gsub("/+$", ""):match("([^/]+)$"))
end

--- Nearest enclosing workspace, searching upward from `start`.
--
-- A workspace is recognised by its Makefile plus its metadata directory. The
-- Makefile alone would match any C project; the metadata alone would match a
-- workspace whose build file a user deleted, which is a case worth failing
-- loudly rather than half-supporting.
function env.find_workspace(start)
  local here = env.resolve(start or ".")
  if here == nil then return nil end
  while true do
    if env.exists(here .. "/Makefile") and env.is_dir(here .. "/.squared-pg") then
      return here
    end
    if env.exists(here .. "/Makefile") and env.is_dir(here .. "/mk") then
      -- Generated workspaces before metadata was recorded, and workspaces a
      -- user has adopted by hand. Accepted, because refusing to lint a
      -- project over a missing bookkeeping directory would be officious.
      return here
    end
    local parent = here:match("^(.*)/[^/]+$")
    if parent == nil or parent == "" or parent == here then return nil end
    here = parent
  end
end

return env
