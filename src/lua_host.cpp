#include "lua_host.hpp"

#include <stdexcept>
#include <utility>

LuaHost::LuaHost()
    : state_(luaL_newstate())
{
    if (state_ == nullptr) {
        throw std::runtime_error("luaL_newstate failed");
    }

    luaL_openlibs(state_);
}

LuaHost::~LuaHost()
{
    if (state_ != nullptr) {
        lua_close(state_);
    }
}

void LuaHost::register_module(
    const std::string& name,
    lua_CFunction open_function
)
{
    luaL_requiref(state_, name.c_str(), open_function, 1);
    lua_pop(state_, 1);
}

bool LuaHost::run_test_file(const std::string& path)
{
    if (luaL_loadfile(state_, path.c_str()) != LUA_OK) {
        throw std::runtime_error(pop_error("loading " + path));
    }

    if (lua_pcall(state_, 0, 1, 0) != LUA_OK) {
        throw std::runtime_error(pop_error("executing " + path));
    }

    if (!lua_istable(state_, -1)) {
        lua_pop(state_, 1);
        throw std::runtime_error(
            path + " must return a table containing a run function"
        );
    }

    lua_getfield(state_, -1, "run");

    if (!lua_isfunction(state_, -1)) {
        lua_pop(state_, 2);
        throw std::runtime_error(
            path + " returned a table without a run function"
        );
    }

    if (lua_pcall(state_, 0, 1, 0) != LUA_OK) {
        const std::string message = pop_error("calling run from " + path);
        lua_pop(state_, 1);
        throw std::runtime_error(message);
    }

    if (!lua_isboolean(state_, -1)) {
        lua_pop(state_, 2);
        throw std::runtime_error(
            path + " run function must return a Boolean"
        );
    }

    const bool succeeded = lua_toboolean(state_, -1) != 0;
    lua_pop(state_, 2);

    if (!succeeded) {
        throw std::runtime_error(path + " reported failure");
    }

    return true;
}

lua_State* LuaHost::state() noexcept
{
    return state_;
}

std::string LuaHost::pop_error(const std::string& context)
{
    const char* raw_message = lua_tostring(state_, -1);
    const std::string message =
        raw_message == nullptr ? "unknown Lua error" : raw_message;

    lua_pop(state_, 1);
    return context + ": " + message;
}
