-- SPDX-License-Identifier: MIT
--
-- squaredpg.args -- command-line parsing.
--
-- Specification: 2.5.4 and 2.5.5. Argument grammar, switch interpretation and
-- input acquisition are Lua's responsibility. The engine never sees a switch;
-- it receives the structured table this module produces.
--
-- Nothing here calls the engine. Keeping parsing free of engine calls is what
-- lets the same grammar be driven from a config file or a stream later
-- without the parser having to change.

local args = {}

-- Options that may be given more than once accumulate into a list. Everything
-- else takes the last value, because "the last one wins" is what every shell
-- user already expects from a repeated flag.
-- Options that take no value. A switch not listed here swallows the next
-- argument, which for `sqpg promote --explain name` would silently consume
-- the workspace name and then complain that promote needs one.
local switches = {
  help = true,
  json = true,
  quiet = true,
  verbose = true,
  explain = true,
  ["no-git"] = true,
}

local repeatable = {
  kit = true,
  package = true,
  asset = true,
  platform = true,
  param = true,
}

local aliases = {
  t = "template",
  k = "kit",
  o = "out",
  p = "param",
  h = "help",
  j = "jobs",
}

--- Split `key=value` into two parts. Returns nil when there is no `=`.
local function split_pair(text)
  local key, value = text:match("^([^=]+)=(.*)$")
  if key == nil then
    return nil
  end
  return key, value
end

--- Parse a raw argument vector.
--
-- Returns a table:
--   command   string        the first non-option argument
--   positional array        remaining non-option arguments
--   options   table         name -> string, or name -> list for repeatables
--   errors    array         human-readable parse problems
function args.parse(argv)
  local parsed = {
    command = nil,
    positional = {},
    options = {},
    errors = {},
  }

  local index = 1
  while index <= #argv do
    local argument = argv[index]

    if argument == "--" then
      -- Everything after `--` is positional, so a project name that looks
      -- like a switch is still possible to express.
      for rest = index + 1, #argv do
        parsed.positional[#parsed.positional + 1] = argv[rest]
      end
      break
    end

    local name, inline = nil, nil
    if argument:sub(1, 2) == "--" then
      name = argument:sub(3)
      local key, value = split_pair(name)
      if key ~= nil then
        name, inline = key, value
      end
    elseif argument:sub(1, 1) == "-" and #argument > 1 then
      name = aliases[argument:sub(2)] or argument:sub(2)
    end

    if name == nil then
      if parsed.command == nil then
        parsed.command = argument
      else
        parsed.positional[#parsed.positional + 1] = argument
      end
      index = index + 1
    else
      local value = inline
      if value == nil then
        -- Boolean switches take no value. Treating a following argument as a
        -- value for them would swallow the next command.
        if switches[name] then
          value = "true"
        else
          value = argv[index + 1]
          if value == nil then
            parsed.errors[#parsed.errors + 1] = ("--%s requires a value"):format(name)
            break
          end
          index = index + 1
        end
      end

      if repeatable[name] then
        local list = parsed.options[name] or {}
        list[#list + 1] = value
        parsed.options[name] = list
      else
        parsed.options[name] = value
      end
      index = index + 1
    end
  end

  return parsed
end

--- Turn `--param k=v` occurrences into a parameter table.
--
-- Values arrive as strings because a shell has nothing else. `true`, `false`
-- and integers are converted so that a template declaring a `bool` or
-- `integer` parameter can be satisfied from the command line at all; anything
-- else stays a string rather than being guessed at.
function args.parameters(list)
  local parameters = {}
  local errors = {}
  for _, entry in ipairs(list or {}) do
    local key, value = split_pair(entry)
    if key == nil then
      errors[#errors + 1] = ("--param expects key=value, got '%s'"):format(entry)
    elseif value == "true" then
      parameters[key] = true
    elseif value == "false" then
      parameters[key] = false
    elseif value:match("^%-?%d+$") then
      parameters[key] = math.tointeger(tonumber(value))
    else
      parameters[key] = value
    end
  end
  return parameters, errors
end

--- Load a configuration file.
--
-- The file is a Lua chunk returning a table. 2.5.6 makes locating and
-- combining configuration a workflow decision, and a Lua table is the natural
-- shape for a workflow that is itself Lua.
--
-- 2.14.4 note: this is the `trusted` tier -- a file the user pointed at, on
-- the user's own machine. A sandboxed workflow must not gain arbitrary
-- execution this way, so a workflow running sandboxed should not call this.
function args.load_config(path)
  local chunk, message = loadfile(path)
  if chunk == nil then
    return nil, message
  end
  local ok, result = pcall(chunk)
  if not ok then
    return nil, result
  end
  if type(result) ~= "table" then
    return nil, ("%s must return a table"):format(path)
  end
  return result
end

--- Merge configuration sources.
--
-- Precedence is command line over file over defaults, and 2.5.6 requires the
-- workflow to document it rather than leave it to be inferred -- which is
-- what this comment and the usage text do.
function args.merge(defaults, from_file, from_cli)
  local merged = {}
  for _, source in ipairs({ defaults or {}, from_file or {}, from_cli or {} }) do
    for key, value in pairs(source) do
      if value ~= nil then
        merged[key] = value
      end
    end
  end
  return merged
end

return args
