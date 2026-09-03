-- SPDX-License-Identifier: MIT
--
-- workflow.generate.default -- the reference generation workflow.
--
-- Specification: 2.5 in full, and 2.7.9 for the phase order.
--
-- This file is where generation policy lives. It decides which template is
-- selected, which kits are applied, in what order the engine is called, and
-- what happens when a call fails. The engine decides none of that (2.2.5),
-- and that division is the whole architecture.
--
-- It is also programmable after release (2.14): a user may copy this
-- directory into $XDG_CONFIG_HOME/squared-pg/workflows/, change it, and run
-- their version with --workflow, without rebuilding anything.

local args = require("squaredpg.args")
local report = require("squaredpg.report")

local usage = [[
sqpg -- offline-first project generator for the Squared framework

usage:
  sqpg new <name> --template ID [options]     generate a workspace
  sqpg plan <name> --template ID [options]    show what would be generated
  sqpg list [template|kit|package|asset]      list indexed resources
  sqpg show <id>                              resolve one resource and describe it
  sqpg inspect <workspace>                    read a workspace metadata record
  sqpg describe                               engine version, services, capabilities
  sqpg operations                             enumerate the control surface

options for new and plan:
  -t, --template ID       template identity, optionally id@version-range
  -k, --kit ID            kit identity; repeat for several, applied in order
      --package ID        package identity; repeat for several
      --asset ID          asset bundle identity; repeat for several
      --platform NAME     target platform; repeat for several
      --framework VER     Squared framework version
  -o, --out PATH          workspace location (default: ./<name>)
  -p, --param key=value   template parameter; repeat for several
      --config FILE       Lua file returning a table of the above
      --json              machine-readable output
      --verbose           print informational diagnostics as well as warnings

host options, interpreted before this workflow runs:
      --workflow ID       run a different workflow
      --fast              relax durability to journal transitions only

configuration precedence: command line, then --config file, then defaults.
]]

local parsed = args.parse(sqpg.args)

if #parsed.errors > 0 then
  for _, message in ipairs(parsed.errors) do
    report.err("sqpg: " .. message)
  end
  return 2
end

if parsed.options.help or parsed.command == nil or parsed.command == "help" then
  report.out(usage)
  return parsed.command == nil and 2 or 0
end

local json_output = parsed.options.json ~= nil
local verbose = parsed.options.verbose ~= nil

--- Run an operation and apply this workflow's failure policy.
--
-- 2.5.8: workflows are driven by engine results. The engine reports; this
-- function decides that a failure means "print and stop", which is one policy
-- among several a different workflow could reasonably choose.
local function call(operation, parameters)
  local result = engine.execute({ operation = operation, parameters = parameters })
  report.diagnostics(result, verbose)
  if not result.ok then
    if json_output then
      report.out(report.json(result))
    else
      report.failure(result)
    end
    return nil, result
  end
  return result
end

--- Assemble the generation request from every input source (2.5.6, 2.7.2).
local function build_config(name)
  local from_file = {}
  if parsed.options.config ~= nil then
    local loaded, message = args.load_config(parsed.options.config)
    if loaded == nil then
      report.err("sqpg: " .. tostring(message))
      return nil
    end
    from_file = loaded
  end

  local parameters, parameter_errors = args.parameters(parsed.options.param)
  if #parameter_errors > 0 then
    for _, message in ipairs(parameter_errors) do
      report.err("sqpg: " .. message)
    end
    return nil
  end

  local from_cli = {
    name = name,
    template = parsed.options.template,
    kits = parsed.options.kit,
    packages = parsed.options.package,
    assets = parsed.options.asset,
    platforms = parsed.options.platform,
    framework = parsed.options.framework,
    out = parsed.options.out,
  }

  local config = args.merge({ platforms = { "termux" }, framework = "1.0.0" }, from_file, from_cli)

  -- Merge parameter tables rather than replacing: a --param on the command
  -- line should refine a config file, not discard it.
  config.parameters = config.parameters or {}
  for key, value in pairs(parameters) do
    config.parameters[key] = value
  end

  -- The engine supplies no default for an identity-bearing input (2.7.2), so
  -- defaulting the output location is the workflow's call to make. Deriving
  -- it from the name is what a user typing `sqpg new hello` expects.
  config.output = config.out or ("./" .. name)
  config.out = nil

  if config.template == nil then
    report.err("sqpg: --template is required; try `sqpg list template`")
    return nil
  end

  return config
end

local commands = {}

function commands.describe()
  local result = call("engine.describe", {})
  if result == nil then
    return 1
  end
  if json_output then
    report.out(report.json(result.data))
    return 0
  end
  report.out(("squared-pg %s (%s)"):format(result.data.version, result.data.state))
  report.out(("  boundary api  %d"):format(engine.api_version))
  report.out(("  resources     %d indexed"):format(result.data.resource_count))
  report.out(("  durability    %s"):format(result.data.durability))
  report.out(("  services      %s"):format(table.concat(result.data.services, ", ")))
  report.out("  capabilities")
  for _, capability in ipairs(result.data.capabilities) do
    report.out(("    %-42s %s  %s"):format(capability.token, capability.version, capability.summary))
  end
  return 0
end

function commands.operations()
  local result = call("engine.operations", {})
  if result == nil then
    return 1
  end
  if json_output then
    report.out(report.json(result.data))
    return 0
  end
  for _, descriptor in ipairs(result.data) do
    local marker = descriptor.mutates_workspace and "!" or " "
    report.out(("%s %-20s %s"):format(marker, descriptor.id, descriptor.summary))
    for _, parameter in ipairs(descriptor.parameters) do
      report.out(("    %-12s %-10s %s%s"):format(
        parameter.name, parameter.type, parameter.required and "required  " or "optional  ",
        parameter.summary))
    end
  end
  report.out("")
  report.out("! marks operations that mutate a workspace")
  return 0
end

function commands.list()
  local kind = parsed.positional[1]
  local result = call("resource.list", kind and { kind = kind } or {})
  if result == nil then
    return 1
  end
  if json_output then
    report.out(report.json(result.data.resources))
    return 0
  end
  report.resources(result.data.resources)
  return 0
end

function commands.show()
  local id = parsed.positional[1]
  if id == nil then
    report.err("sqpg: show requires a resource identity")
    return 2
  end

  -- The operation to call follows from the identity's own kind prefix, which
  -- is exactly what 2.6.7 says identities are for.
  local kind = id:match("^([a-z_]+)%.")
  local operation = ({
    ["template"] = "template.resolve",
    kit = "kit.resolve",
    package = "package.resolve",
    asset = "asset.resolve",
  })[kind]
  if operation == nil then
    report.err(("sqpg: '%s' does not name a resolvable resource kind"):format(id))
    return 2
  end

  local result = call(operation, {
    id = id,
    platforms = parsed.options.platform,
    framework = parsed.options.framework,
    ["template"] = parsed.options.template,
  })
  if result == nil then
    return 1
  end
  report.out(report.json(result.data))
  return 0
end

function commands.inspect()
  local workspace = parsed.positional[1]
  if workspace == nil then
    report.err("sqpg: inspect requires a workspace path")
    return 2
  end
  local result = call("workspace.inspect", { workspace = workspace })
  if result == nil then
    return 1
  end
  report.out(report.json(result.data))
  return 0
end

function commands.plan()
  local name = parsed.positional[1]
  if name == nil then
    report.err("sqpg: plan requires a project name")
    return 2
  end
  local config = build_config(name)
  if config == nil then
    return 2
  end

  local result = call("project.plan", config)
  if result == nil then
    return 1
  end
  if json_output then
    report.out(report.json(result.data))
    return 0
  end
  report.plan(result.data)
  return 0
end

function commands.new()
  local name = parsed.positional[1]
  if name == nil then
    report.err("sqpg: new requires a project name")
    return 2
  end
  local config = build_config(name)
  if config == nil then
    return 2
  end

  -- Plan before generating. The engine would resolve again inside
  -- project.generate anyway, but asking for the plan first means a
  -- cross-validation failure is reported before anything is attempted, and
  -- --verbose can show the user what is about to happen (2.7.9).
  local planned = call("project.plan", config)
  if planned == nil then
    return 1
  end
  if verbose then
    report.plan(planned.data)
    report.out("")
  end

  local result = call("project.generate", config)
  if result == nil then
    return 1
  end

  if json_output then
    report.out(report.json(result.data))
    return 0
  end

  report.out(("created %s"):format(result.data.workspace))
  report.out(("  template          %s"):format(result.data["template"]))
  if #result.data.kits > 0 then
    report.out(("  kits              %s"):format(table.concat(result.data.kits, ", ")))
  end
  report.out(("  working directory %s/"):format(result.data.working_directory))
  report.out(("  files             %d in %d directories")
    :format(result.data.files_written, result.data.directories_created))
  report.out("")
  report.out(("write your code in %s/%s/ and run `make` in %s")
    :format(result.data.workspace, result.data.working_directory, result.data.workspace))
  return 0
end

local handler = commands[parsed.command]
if handler == nil then
  report.err(("sqpg: unknown command '%s'"):format(parsed.command))
  report.err("try `sqpg help`")
  return 2
end

return handler()
