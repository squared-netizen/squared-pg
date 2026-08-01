#pragma once

/** @brief Opaque Lua interpreter state declared by the Lua C API. */
struct lua_State;

/**
 * @brief Register the thin native `squared.sq` Lua 5.4 module.
 *
 * The function follows the Lua C module convention and returns one module
 * table. It never transfers native ownership to untyped Lua values; package
 * handles are represented by closeable userdata.
 *
 * @param state Active Lua state owned by the caller.
 * @return Number of Lua results, always one after successful registration.
 */
extern "C" int luaopen_squared_sq(lua_State* state);
