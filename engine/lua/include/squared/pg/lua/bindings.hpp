// SPDX-License-Identifier: MIT
//
// squared/pg/lua/bindings.hpp — the Engine/Lua boundary.
//
// Specification: §2.6 in full, and §2.5.7 (Lua reaches the engine only
// through the public API).
//
// This is the *only* place a lua_State meets the engine. §2.6.2 forbids the
// binding layer from becoming a workflow layer: nothing here selects a
// template, decides an order, or implements policy. It converts values,
// forwards a call to the registry, and converts the result back.
//
// The binding is built by walking Engine::operations() at install time, so
// the set of callable names cannot drift from the set of registered
// operations (§2.6.3). Adding an operation to the registry makes it callable
// from Lua with no edit here.
//
// Handles (§2.6.6): there are none. Everything crossing the boundary is
// plain data copied into Lua tables, so there is no native lifetime for a
// script to get wrong and no stale-handle case to defend against.

#ifndef SQUARED_PG_LUA_BINDINGS_HPP
#define SQUARED_PG_LUA_BINDINGS_HPP

#include "squared/pg/engine.hpp"
#include "squared/pg/value.hpp"

#include <string>
#include <vector>

struct lua_State;

namespace squared::pg::lua {

/// Boundary contract version, exposed to workflows as `engine.api_version`.
/// §2.6.15 requires the boundary to advertise a version a workflow may
/// require; a breaking change to the shape of a result bumps this.
inline constexpr int kApiVersion = 1;

/// Install the `engine` table.
///
/// The engine must outlive the lua_State. Ownership does not cross: §2.6.9
/// keeps native objects engine-owned, and Lua receives copies.
void open_engine(lua_State* state, Engine& engine);

/// What the host tells the workflow about its own invocation.
///
/// This is host information, not engine information — which is why it is a
/// separate table. §2.5.4 makes argument interpretation Lua's job; the host
/// hands over the raw strings and stops there.
struct HostInfo {
    std::vector<std::string> args;          ///< argv after the program name, unparsed
    std::string              program;       ///< argv[0], for usage messages
    std::string              workflow_id;   ///< workflow the host selected
    std::vector<std::string> workflow_path; ///< search path, in declared order
    std::string              cwd;           ///< where the host was invoked
};

/// Install the `sqpg` host table.
void open_host(lua_State* state, const HostInfo& info);

/// Convert a Lua value at `index` into a Value. Tables whose keys are exactly
/// 1..n become arrays; every other table becomes an object.
[[nodiscard]] Value from_lua(lua_State* state, int index);

/// Push a Value onto the Lua stack.
void push_value(lua_State* state, const Value& value);

}  // namespace squared::pg::lua

#endif  // SQUARED_PG_LUA_BINDINGS_HPP
