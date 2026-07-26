#pragma once

#include <lua.hpp>

#include <string>

/**
 * @file lua_host.hpp
 * @brief Minimal, SDL-independent Lua 5.4 host.
 */

/**
 * @brief Owns one Lua state and provides checked script execution.
 *
 * LuaHost is intentionally small. Application-specific APIs are registered as
 * Lua modules rather than being built into this class.
 */
class LuaHost final {
public:
    /**
     * @brief Creates a Lua state and opens the standard libraries.
     *
     * @throws std::runtime_error if the Lua state cannot be allocated.
     */
    LuaHost();

    /**
     * @brief Closes the owned Lua state.
     */
    ~LuaHost();

    LuaHost(const LuaHost&) = delete;
    LuaHost& operator=(const LuaHost&) = delete;
    LuaHost(LuaHost&&) = delete;
    LuaHost& operator=(LuaHost&&) = delete;

    /**
     * @brief Registers a C module and makes it available through `require`.
     *
     * @param name Lua module name.
     * @param open_function Lua-compatible module opening function.
     */
    void register_module(
        const std::string& name,
        lua_CFunction open_function
    );

    /**
     * @brief Loads a Lua file, calls its exported `run` function, and validates
     * the Boolean result.
     *
     * @param path Path to a Lua file returning a table containing `run`.
     * @return `true` when the Lua test reports success.
     * @throws std::runtime_error on load, execution, contract, or test failure.
     */
    [[nodiscard]] bool run_test_file(const std::string& path);

    /**
     * @brief Returns the underlying Lua state for module registration.
     *
     * @return Non-owning pointer valid for the lifetime of this host.
     */
    [[nodiscard]] lua_State* state() noexcept;

private:
    [[nodiscard]] std::string pop_error(const std::string& context);

    lua_State* state_{nullptr};
};
