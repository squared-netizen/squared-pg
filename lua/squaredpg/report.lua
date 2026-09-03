-- SPDX-License-Identifier: MIT
--
-- squaredpg.report -- presentation.
--
-- Specification: 2.6.14. The engine provides structured information; it does
-- not write to a user interface. Everything a user reads comes from here.
--
-- The engine's result shape (2.6.11) is stable, so this module is the only
-- place that needs to know what a result looks like on screen. A different
-- host wanting different output replaces this file and nothing else.

local report = {}

local colours_enabled = os.getenv("NO_COLOR") == nil and os.getenv("TERM") ~= "dumb"

local function paint(code, text)
  if not colours_enabled then
    return text
  end
  return ("\27[%sm%s\27[0m"):format(code, text)
end

function report.err(text)
  io.stderr:write(text, "\n")
end

function report.out(text)
  io.stdout:write(text, "\n")
end

--- Print diagnostics and warnings from a result.
--
-- Warnings are printed even on success. 2.4.7 forbids hiding a problem behind
-- a successful status, and silently discarding the warnings the engine went
-- to the trouble of producing would do exactly that.
function report.diagnostics(result, verbose)
  for _, warning in ipairs(result.warnings or {}) do
    report.err(("%s %s"):format(paint("33", "warning:"), warning.message))
  end
  if verbose then
    for _, diagnostic in ipairs(result.diagnostics or {}) do
      report.err(("%s %s"):format(paint("36", "note:"), diagnostic.message))
    end
  end
end

--- Print a structured error the way 2.15.3 argues errors should read: the
--- code, the message, and then the detail that makes it actionable.
function report.failure(result)
  for _, err in ipairs(result.errors or {}) do
    report.err(("%s %s"):format(paint("31", "error:"), err.message))
    report.err(("  code:      %s"):format(err.code))
    if err.resource ~= nil and err.resource ~= "" then
      report.err(("  resource:  %s"):format(err.resource))
    end
    if err.path ~= nil and err.path ~= "" then
      report.err(("  path:      %s"):format(err.path))
    end
    local detail = err.diagnostics
    if type(detail) == "table" then
      -- Diagnostic detail is arbitrarily shaped: 2.15.3 asks for whatever
      -- makes the failure actionable, which is sometimes a list of strings and
      -- sometimes a list of records. Render both rather than assuming one.
      local keys = {}
      for key in pairs(detail) do
        keys[#keys + 1] = key
      end
      table.sort(keys, function(a, b) return tostring(a) < tostring(b) end)

      for _, key in ipairs(keys) do
        local value = detail[key]
        if type(value) ~= "table" then
          report.err(("  %-10s %s"):format(key .. ":", tostring(value)))
        elseif #value > 0 then
          local flat = {}
          local nested = false
          for _, item in ipairs(value) do
            if type(item) == "table" then
              nested = true
            else
              flat[#flat + 1] = tostring(item)
            end
          end
          if not nested then
            report.err(("  %-10s %s"):format(key .. ":", table.concat(flat, ", ")))
          else
            report.err(("  %s:"):format(key))
            for _, item in ipairs(value) do
              report.err("    " .. report.json(item, "    "))
            end
          end
        end
      end
    end
    if err.recoverable then
      report.err("  this failure is recoverable: a different selection may succeed")
    end
  end
end

--- Serialise a value as JSON, for `--json`.
--
-- 2.15.5 requires errors to be machine-readable so automation never has to
-- parse human output. That guarantee needs an actual machine-readable mode,
-- so this exists rather than asking callers to scrape the text above.
function report.json(value, indent)
  indent = indent or ""
  local next_indent = indent .. "  "
  local kind = type(value)

  if value == nil then
    return "null"
  elseif kind == "boolean" then
    return tostring(value)
  elseif kind == "number" then
    if math.type(value) == "integer" then
      return tostring(value)
    end
    return string.format("%.17g", value)
  elseif kind == "string" then
    local escaped = value:gsub('[%c"\\]', function(c)
      local map = { ['"'] = '\\"', ["\\"] = "\\\\", ["\n"] = "\\n", ["\r"] = "\\r", ["\t"] = "\\t" }
      return map[c] or string.format("\\u%04x", string.byte(c))
    end)
    return '"' .. escaped .. '"'
  elseif kind == "table" then
    local count = 0
    for _ in pairs(value) do
      count = count + 1
    end
    if count == 0 then
      return "{}"
    end
    if #value == count then
      local parts = {}
      for _, item in ipairs(value) do
        parts[#parts + 1] = next_indent .. report.json(item, next_indent)
      end
      return "[\n" .. table.concat(parts, ",\n") .. "\n" .. indent .. "]"
    end
    -- Object keys are sorted so two runs over the same data produce the same
    -- bytes; Lua's pairs() order is not specified and would not.
    local keys = {}
    for key in pairs(value) do
      keys[#keys + 1] = tostring(key)
    end
    table.sort(keys)
    local parts = {}
    for _, key in ipairs(keys) do
      parts[#parts + 1] = next_indent .. report.json(key, next_indent) .. ": " ..
        report.json(value[key], next_indent)
    end
    return "{\n" .. table.concat(parts, ",\n") .. "\n" .. indent .. "}"
  end
  return "null"
end

--- Human-readable rendering of a generation plan.
function report.plan(plan)
  report.out(("plan for %s"):format(plan.project_name))
  report.out(("  template          %s@%s"):format(plan["template"], plan.template_version))
  if #plan.kits > 0 then
    for _, kit in ipairs(plan.kits) do
      report.out(("  kit               %s@%s"):format(kit.id, kit.version))
    end
  end
  report.out(("  working directory %s"):format(plan.working_directory))
  report.out(("  workspace         %s"):format(plan.workspace))
  report.out("")

  local widest = 0
  for _, step in ipairs(plan.steps) do
    if #step.path > widest then
      widest = #step.path
    end
  end

  for _, step in ipairs(plan.steps) do
    if step.action == "create_directory" then
      report.out(("  %s %s/"):format(paint("34", "dir "), step.path))
    else
      local ownership = step.ownership
      local colour = ownership == "generated" and "35" or "32"
      report.out(("  %s %-" .. widest .. "s  %s  %s")
        :format(paint("32", "file"), step.path, paint(colour, ownership), step.origin or ""))
    end
  end
  report.out("")
  report.out(("  %d steps"):format(plan.step_count))
end

--- Human-readable rendering of a resource listing.
function report.resources(resources)
  if #resources == 0 then
    report.out("no resources indexed")
    return
  end
  local widest = 0
  for _, resource in ipairs(resources) do
    if #resource.id > widest then
      widest = #resource.id
    end
  end
  for _, resource in ipairs(resources) do
    report.out(("  %-" .. widest .. "s  %-8s  %-8s  %s")
      :format(resource.id, resource.kind, resource.version, resource.title or ""))
  end
end

return report
