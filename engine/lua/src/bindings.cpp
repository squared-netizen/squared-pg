// SPDX-License-Identifier: MIT

#include "squared/pg/lua/bindings.hpp"

#include <cstring>
#include <string>
#include <vector>

extern "C" {
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}

namespace squared::pg::lua {
namespace {

constexpr const char* kEngineRegistryKey = "squared.pg.engine";

[[nodiscard]] Engine& engine_from(lua_State* state) {
    lua_getfield(state, LUA_REGISTRYINDEX, kEngineRegistryKey);
    auto* engine = static_cast<Engine*>(lua_touserdata(state, -1));
    lua_pop(state, 1);
    // The upvalue is installed by open_engine before any bound function can
    // be reached, so this cannot be null in a correctly built state. If it
    // ever were, luaL_error is still preferable to dereferencing.
    if (engine == nullptr) luaL_error(state, "squared-pg: engine binding is not installed");
    return *engine;
}

/// A Lua table is a sequence when its keys are exactly the integers 1..n.
/// Anything else — a string key, a hole, a zero index — is an object. Lua
/// draws no distinction between the two, so the boundary has to pick one, and
/// picking by inspection is the only option that round-trips both shapes.
[[nodiscard]] bool is_sequence(lua_State* state, int index) {
    const lua_Integer length = luaL_len(state, index);
    if (length < 0) return false;

    lua_Integer counted = 0;
    lua_pushnil(state);
    while (lua_next(state, index) != 0) {
        if (!lua_isinteger(state, -2)) {
            lua_pop(state, 2);
            return false;
        }
        const lua_Integer key = lua_tointeger(state, -2);
        if (key < 1 || key > length) {
            lua_pop(state, 2);
            return false;
        }
        ++counted;
        lua_pop(state, 1);
    }
    return counted == length;
}

}  // namespace

Value from_lua(lua_State* state, int index) {
    index = lua_absindex(state, index);

    switch (lua_type(state, index)) {
        case LUA_TNIL:
        case LUA_TNONE:
            return {};
        case LUA_TBOOLEAN:
            return Value{lua_toboolean(state, index) != 0};
        case LUA_TNUMBER:
            // §2.6.5: Lua 5.4 distinguishes integers from floats and the
            // boundary must not silently narrow. Preserving the distinction
            // here is what lets version components and counts stay integers.
            if (lua_isinteger(state, index)) {
                return Value{static_cast<std::int64_t>(lua_tointeger(state, index))};
            }
            return Value{static_cast<double>(lua_tonumber(state, index))};
        case LUA_TSTRING: {
            std::size_t length = 0;
            const char* text   = lua_tolstring(state, index, &length);
            return Value{std::string{text, length}};
        }
        case LUA_TTABLE: {
            if (is_sequence(state, index)) {
                Array array;
                const lua_Integer length = luaL_len(state, index);
                array.reserve(static_cast<std::size_t>(length));
                for (lua_Integer i = 1; i <= length; ++i) {
                    lua_geti(state, index, i);
                    array.push_back(from_lua(state, -1));
                    lua_pop(state, 1);
                }
                return Value{std::move(array)};
            }
            Object object;
            lua_pushnil(state);
            while (lua_next(state, index) != 0) {
                // lua_tolstring on a key would rewrite a number key in place
                // and confuse lua_next, so the key is copied before reading.
                lua_pushvalue(state, -2);
                std::size_t length = 0;
                const char* key    = lua_tolstring(state, -1, &length);
                if (key != nullptr) {
                    // Stack is [key, value, key-copy]; the value is at -2.
                    object.emplace_back(std::string{key, length}, from_lua(state, -2));
                }
                lua_pop(state, 2);
            }
            return Value{std::move(object)};
        }
        default:
            // Functions, userdata, threads and light userdata have no
            // representation on this boundary (§2.6.5). Dropping them to null
            // is deliberate: converting them would mean exposing native
            // memory, which is precisely what the boundary exists to prevent.
            return {};
    }
}

void push_value(lua_State* state, const Value& value) {
    switch (value.kind()) {
        case ValueKind::null:
            lua_pushnil(state);
            return;
        case ValueKind::boolean:
            lua_pushboolean(state, *value.as_bool() ? 1 : 0);
            return;
        case ValueKind::integer:
            lua_pushinteger(state, static_cast<lua_Integer>(*value.as_int()));
            return;
        case ValueKind::number:
            lua_pushnumber(state, static_cast<lua_Number>(*value.as_number()));
            return;
        case ValueKind::string: {
            const std::string_view text = *value.as_string();
            lua_pushlstring(state, text.data(), text.size());
            return;
        }
        case ValueKind::array: {
            const Array& array = *value.as_array();
            lua_createtable(state, static_cast<int>(array.size()), 0);
            for (std::size_t i = 0; i < array.size(); ++i) {
                push_value(state, array[i]);
                lua_seti(state, -2, static_cast<lua_Integer>(i + 1));
            }
            return;
        }
        case ValueKind::object: {
            const Object& object = *value.as_object();
            lua_createtable(state, 0, static_cast<int>(object.size()));
            for (const auto& [key, member] : object) {
                push_value(state, member);
                lua_setfield(state, -2, key.c_str());
            }
            return;
        }
    }
}

namespace {

/// engine.execute{ operation = "...", parameters = { ... } }
///
/// The explicit form of §2.4.6. The sugar below maps onto this, so both forms
/// reach one registry entry and behave identically.
int l_execute(lua_State* state) {
    Engine& engine = engine_from(state);

    std::string operation;
    Value       parameters = Value::object();

    if (lua_isstring(state, 1) != 0) {
        operation = lua_tostring(state, 1);
        if (!lua_isnoneornil(state, 2)) parameters = from_lua(state, 2);
    } else {
        luaL_checktype(state, 1, LUA_TTABLE);
        lua_getfield(state, 1, "operation");
        if (lua_isstring(state, -1) == 0) {
            lua_pop(state, 1);
            return luaL_error(state, "engine.execute: 'operation' must be a string");
        }
        operation = lua_tostring(state, -1);
        lua_pop(state, 1);

        lua_getfield(state, 1, "parameters");
        if (!lua_isnoneornil(state, -1)) parameters = from_lua(state, -1);
        lua_pop(state, 1);
    }

    // §2.6.12: no exception crosses this boundary and the engine never
    // terminates the host. A failure is a value, exactly like a success.
    const OperationResult result = engine.execute(operation, parameters);
    push_value(state, result.to_value());
    return 1;
}

/// Sugar: engine.<namespace>.<name>(argument).
///
/// The operation identity is carried as an upvalue rather than reconstructed
/// from the table path, so a workflow that copies the function out of the
/// table still calls the operation it came from.
int l_sugar(lua_State* state) {
    Engine&           engine    = engine_from(state);
    const std::string operation = lua_tostring(state, lua_upvalueindex(1));

    Value parameters = Value::object();
    if (lua_type(state, 1) == LUA_TTABLE) {
        parameters = from_lua(state, 1);
    } else if (lua_isstring(state, 1) != 0) {
        // §2.5.3 and §2.6.4 both write `engine.template.resolve("template.x")`.
        // Binding a lone string to the operation's first required parameter
        // makes that read naturally without giving the sugar a second meaning:
        // it still produces the same parameter table the explicit form would.
        const char* bind = lua_tostring(state, lua_upvalueindex(2));
        if (bind == nullptr || std::strlen(bind) == 0) {
            return luaL_error(state, "%s takes a table of parameters", operation.c_str());
        }
        parameters.set(bind, std::string{lua_tostring(state, 1)});
    } else if (!lua_isnoneornil(state, 1)) {
        return luaL_error(state, "%s takes a table or a string", operation.c_str());
    }

    const OperationResult result = engine.execute(operation, parameters);
    push_value(state, result.to_value());
    return 1;
}

int l_state(lua_State* state) {
    Engine& engine = engine_from(state);
    const std::string_view text = to_string(engine.state());
    lua_pushlstring(state, text.data(), text.size());
    return 1;
}

int l_version(lua_State* state) {
    Engine& engine = engine_from(state);
    const std::string text = engine.version().to_string();
    lua_pushlstring(state, text.data(), text.size());
    return 1;
}

/// Split "project.generate" into ("project", "generate"); the `engine.`
/// namespace is flattened so `engine.describe()` reads better than
/// `engine.engine.describe()`.
void split_operation(const std::string& id, std::string& table_name, std::string& function_name) {
    const std::size_t dot = id.rfind('.');
    if (dot == std::string::npos) {
        table_name.clear();
        function_name = id;
        return;
    }
    table_name    = id.substr(0, dot);
    function_name = id.substr(dot + 1);
    if (table_name == "engine") table_name.clear();
}

}  // namespace

void open_engine(lua_State* state, Engine& engine) {
    lua_pushlightuserdata(state, &engine);
    lua_setfield(state, LUA_REGISTRYINDEX, kEngineRegistryKey);

    lua_newtable(state);  // engine

    lua_pushcfunction(state, l_execute);
    lua_setfield(state, -2, "execute");
    lua_pushcfunction(state, l_state);
    lua_setfield(state, -2, "state");
    lua_pushcfunction(state, l_version);
    lua_setfield(state, -2, "version");
    lua_pushinteger(state, kApiVersion);
    lua_setfield(state, -2, "api_version");

    // §2.6.15: a workflow may query the capability set and declare what it
    // needs, so it fails at load rather than midway through generation.
    lua_newtable(state);
    int index = 1;
    for (const Capability& capability : engine.capabilities()) {
        push_value(state, capability.to_value());
        lua_seti(state, -2, index++);
    }
    lua_setfield(state, -2, "capabilities");

    // The call surface, generated from the registry (§2.6.3).
    for (const OperationDescriptor& descriptor : engine.operations()) {
        std::string table_name;
        std::string function_name;
        split_operation(descriptor.id, table_name, function_name);

        std::string bind_parameter;
        for (const ParameterSpec& spec : descriptor.parameters) {
            if (spec.required) {
                bind_parameter = spec.name;
                break;
            }
        }

        // Absolute indices throughout. Relative ones would shift under the
        // upvalues pushed below, and the resulting setfield would install the
        // function on itself rather than on the namespace table.
        const int engine_index = lua_gettop(state);
        int       target_index = engine_index;

        if (!table_name.empty()) {
            lua_getfield(state, engine_index, table_name.c_str());
            if (lua_istable(state, -1) == 0) {
                lua_pop(state, 1);
                lua_newtable(state);
                lua_pushvalue(state, -1);
                lua_setfield(state, engine_index, table_name.c_str());
            }
            target_index = lua_gettop(state);
        }

        lua_pushstring(state, descriptor.id.c_str());
        lua_pushstring(state, bind_parameter.c_str());
        lua_pushcclosure(state, l_sugar, 2);
        lua_setfield(state, target_index, function_name.c_str());

        if (!table_name.empty()) lua_pop(state, 1);
    }

    lua_setglobal(state, "engine");
}

void open_host(lua_State* state, const HostInfo& info) {
    lua_newtable(state);

    lua_createtable(state, static_cast<int>(info.args.size()), 0);
    for (std::size_t i = 0; i < info.args.size(); ++i) {
        lua_pushlstring(state, info.args[i].data(), info.args[i].size());
        lua_seti(state, -2, static_cast<lua_Integer>(i + 1));
    }
    lua_setfield(state, -2, "args");

    const auto set_string = [&](const char* key, const std::string& text) {
        lua_pushlstring(state, text.data(), text.size());
        lua_setfield(state, -2, key);
    };
    set_string("program", info.program);
    set_string("workflow_id", info.workflow_id);
    set_string("cwd", info.cwd);

    lua_createtable(state, static_cast<int>(info.workflow_path.size()), 0);
    for (std::size_t i = 0; i < info.workflow_path.size(); ++i) {
        lua_pushlstring(state, info.workflow_path[i].data(), info.workflow_path[i].size());
        lua_seti(state, -2, static_cast<lua_Integer>(i + 1));
    }
    lua_setfield(state, -2, "workflow_path");

    lua_setglobal(state, "sqpg");
}

}  // namespace squared::pg::lua
