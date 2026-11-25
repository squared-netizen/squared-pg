-- build.lua: {{"{{"}}project_name}}'s build, for sqmake.
--
--   sqmake        build/{{"{{"}}project_name}}
--   sqmake run    run it
--   sqmake test   build and run each sq_app/test/*.cpp
--
-- Its modules (sq_app/src/**/*.cppm) are built once and linked into the
-- program and each test, so a test can import them.

local name = "{{"{{"}}project_name}}"
local modules, srcs = {}, {}
for _, f in ipairs(glob("sq_app/src/**/*.cppm")) do modules[#modules + 1] = f end
for _, f in ipairs(glob("sq_app/src/**/*.cpp")) do srcs[#srcs + 1] = f end

local flags = {"-Wall", "-Wextra", "-Wpedantic"}
local deps = {}
if #modules > 0 then
  deps = {library{name = name .. "_modules", srcs = modules, flags = flags, default = false}}
end
local bin = exe{name = name, srcs = srcs, deps = deps, flags = flags}
verb("run", {needs = {bin}, call = function() return exec{bin} end})

local tests = {}
for _, src in ipairs(glob("sq_app/test/*.cpp")) do
  local t = src:match("([^/]+)%.cpp$")
  tests[#tests + 1] = exe{name = "test/" .. t, srcs = {src}, deps = deps, flags = flags, default = false}
end
verb("test", {needs = tests, call = function()
  if #tests == 0 then
    print("no tests: add .cpp files under sq_app/test/")
    return 0
  end
  local failed = 0
  for _, t in ipairs(tests) do
    if exec{t} ~= 0 then failed = failed + 1 end
  end
  print(failed == 0 and "tests passed" or (failed .. " test(s) failed"))
  return failed == 0 and 0 or 1
end})
