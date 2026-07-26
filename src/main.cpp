#include "lua_host.hpp"

#include <iostream>
#include <stdexcept>

namespace {

int host_add(lua_State* state)
{
    const lua_Integer left = luaL_checkinteger(state, 1);
    const lua_Integer right = luaL_checkinteger(state, 2);

    lua_pushinteger(state, left + right);
    return 1;
}

int host_log(lua_State* state)
{
    const char* message = luaL_checkstring(state, 1);
    std::cout << message << '\n';
    return 0;
}

int host_runtime_version(lua_State* state)
{
    lua_pushstring(state, LUA_VERSION);
    return 1;
}

int open_host_module(lua_State* state)
{
    static const luaL_Reg functions[] = {
        {"add", host_add},
        {"log", host_log},
        {"runtime_version", host_runtime_version},
        {nullptr, nullptr}
    };

    luaL_newlib(state, functions);
    return 1;
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "Usage: lua_embed_smoke SCRIPT.lua\n";
        return 2;
    }

    try {
        LuaHost host;
        host.register_module("host", open_host_module);

        std::cout << "Lua runtime: " << LUA_VERSION << '\n';

        if (!host.run_test_file(argv[1])) {
            std::cerr << "Lua test returned false\n";
            return 1;
        }

        std::cout << "C++ called Lua: test returned true\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ERROR: " << error.what() << '\n';
        return 1;
    }
}
