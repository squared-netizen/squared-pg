-- build.lua: squared-pg's build, for sqmake.
--
--   sqmake          build/sqpg, build/sqcart3 (sqcart's CLI, which travels
--                   with sqpg) and the engine's tests' programs
--   sqmake test     the engine's tests
--   sqmake smoke    generate a project and build it, end to end (tools/smoke.sh)
--   sqmake --debug  -O0 (build/debug); OPT=-O2 sqmake, a release's
--
-- The compiler is $CXX (and $CC): clang and libc++ 19 or later (23 is what
-- sqterm and CI use), C++23: the engine reads cartridges through sq.cart
-- (sqcart 3.0), built by sqcart/v3/build.lua. Only
-- engine/src/services/cartridge.cpp imports it. The tree is assembled first
-- (tools/bootstrap.sh: sqcart/ and resources/.squared at their pins).
-- Offline throughout (§1.13): everything it compiles is already in the tree.

if #glob("sqcart/v3/build.lua") == 0 then
  error("sqcart/ is not present, or is older than its build.lua: run ./tools/bootstrap.sh", 0)
end

-- -O1 unless $OPT says (a release builds -O2); --debug: -O0.
local opt = mode.debug and {"-O0", "-g"} or {os.getenv("OPT") or "-O1", "-g"}
toolchain.cxxflags = {"-std=c++23", "-stdlib=libc++", opt}
toolchain.cflags = {opt}
toolchain.ldflags = {"-stdlib=libc++", "-lpthread"}

local version = (read_file("VERSION") or "0.0.0"):match("^%s*(.-)%s*$")
local third = "third-party"
local lua_dir = third .. "/lua-5.4.8/src"
local includes = {"-Iengine/include", "-Iengine/src", "-Iengine/lua/include",
                  "-isystem", third .. "/yyjson-0.12.0/src", "-isystem", third .. "/crypto-algorithms",
                  "-isystem", lua_dir}
local cxx = {"-Wall", "-Wextra", "-Wpedantic", "-DSQUARED_PG_VERSION=\"" .. version .. "\"", includes}

-- sq.cart, and its own sq.data and yyjson (one copy: the engine's yyjson
-- is that one).
sqcart_settings = {root = "sqcart/v3"}
local cart = include("sqcart/v3/build.lua")

-- Lua 5.4, as C: everything but the standalone interpreter and compiler.
local lua_srcs = {}
for _, f in ipairs(glob(lua_dir .. "/*.c")) do
  if not f:match("/luac?%.c$") then lua_srcs[#lua_srcs + 1] = f end
end
local lua = library{name = "lua", srcs = lua_srcs, flags = {"-w", "-DLUA_USE_POSIX"}}

local sha256 = library{name = "sha256", srcs = {third .. "/crypto-algorithms/sha256.c"}, flags = {"-w"}}

local engine = library{name = "squaredpg", deps = {cart.read, sha256}, flags = cxx, srcs = {
  "engine/src/value.cpp", "engine/src/version.cpp", "engine/src/identity.cpp", "engine/src/error.cpp",
  "engine/src/engine.cpp", "engine/src/support/support.cpp",
  "engine/src/services/filesystem_service.cpp", "engine/src/services/resource_service.cpp",
  "engine/src/services/resolve_services.cpp", "engine/src/services/validation_service.cpp",
  "engine/src/services/project_service.cpp", "engine/src/services/transaction_service.cpp",
  "engine/src/services/cartridge.cpp",
}}

local sqpg = exe{name = "sqpg", srcs = {"app/src/main.cpp", "engine/lua/src/bindings.cpp"},
                 deps = {engine, lua}, flags = cxx}

-- sqcart's CLI travels with sqpg (D-055: a fresh clone authors a cartridge
-- without $SQCART set by hand): sqcart3, by sqcart's own build.
exe{name = "sqcart3", srcs = {"sqcart/v3/tools/sqcart3.cpp"}, deps = {cart.write}}

-- The tests reach the real resources/ tree, and test_cli runs build/sqpg:
-- they are told where the source is (this build file's folder).
local source = os.getenv("PWD") or "."
local tests = {}
for _, name in ipairs({"test_version", "test_identity", "test_value", "test_support", "test_lifecycle",
                       "test_generate", "test_android", "test_cli"}) do
  tests[#tests + 1] = exe{name = name, srcs = {"engine/tests/" .. name .. ".cpp"}, deps = {engine},
                          flags = {cxx, "-Iengine/tests", "-DSQUARED_PG_TEST_SOURCE_DIR=\"" .. source .. "\""}}
end

verb("test", {needs = {sqpg, tests}, call = function()
  local failed = 0
  for _, t in ipairs(tests) do
    if exec{t} ~= 0 then failed = failed + 1 end
  end
  print(failed == 0 and "all engine tests passed" or "FAILURES")
  return failed == 0 and 0 or 1
end})

action{name = "smoke", needs = {sqpg}, desc = "generate a project and build it, end to end",
       cmd = {"sh", "tools/smoke.sh"}}
