#pragma once

#include <cstdint>

union SDL_Event;
struct lua_State;

namespace {{PROJECT_ID}} {

/**
 * @brief Renderer-independent visual state controlled by application logic.
 */
struct VisualState {
    std::uint8_t background_red{8};
    std::uint8_t background_green{24};
    std::uint8_t background_blue{32};
    int tile_x{432};
    int tile_y{222};
    int tile_size{96};
    std::uint8_t tile_red{80};
    std::uint8_t tile_green{220};
    std::uint8_t tile_blue{140};
};

/**
 * @brief Protected Lua lifecycle host for the application plug-in manager.
 *
 * The trusted bootstrap receives the native `host` table and constructs
 * narrower capability proxies for individual plug-ins. Standard filesystem,
 * process, package-loading, and debug libraries are not opened.
 */
class ScriptRuntime final {
public:
    /**
     * @brief Construct a runtime connected to native visual state.
     *
     * @param visual Mutable state rendered by the native application.
     * @param logical_width SDL logical rendering width.
     * @param logical_height SDL logical rendering height.
     */
    ScriptRuntime(
        VisualState& visual,
        int logical_width,
        int logical_height
    ) noexcept;

    ScriptRuntime(const ScriptRuntime&) = delete;
    ScriptRuntime& operator=(const ScriptRuntime&) = delete;

    /** @brief Shut down Lua and release all owned resources. */
    ~ScriptRuntime();

    /**
     * @brief Load the trusted APK bootstrap asset.
     *
     * The bootstrap returns the aggregate application callback table used by
     * the native loop.
     *
     * @param asset_path Asset path such as `lua/bootstrap.lua`.
     * @return `true` when the bootstrap and plug-in registry load.
     */
    [[nodiscard]] bool start(const char* asset_path) noexcept;

    /**
     * @brief Invoke the optional Lua `update(delta_seconds)` callback.
     *
     * @param delta_seconds Frame duration in seconds.
     */
    void update(double delta_seconds) noexcept;

    /**
     * @brief Translate and forward one supported SDL event to Lua.
     *
     * Lua receives `event(kind, first, second)`.
     *
     * @param event SDL event owned by the caller.
     */
    void handle_event(const SDL_Event& event) noexcept;

    /** @brief Invoke `shutdown()` once and close the Lua state. */
    void shutdown() noexcept;

    /**
     * @brief Report whether Lua requested application shutdown.
     *
     * @return `true` after `host.request_quit()`.
     */
    [[nodiscard]] bool quit_requested() const noexcept;

private:
    static int lua_read_asset(lua_State* state);
    static int lua_runtime_version(lua_State* state);
    static int lua_log(lua_State* state);
    static int lua_set_background(lua_State* state);
    static int lua_set_tile(lua_State* state);
    static int lua_logical_size(lua_State* state);
    static int lua_request_quit(lua_State* state);

    void open_safe_libraries() noexcept;
    void install_host_api() noexcept;
    [[nodiscard]] bool prepare_callback(const char* name) noexcept;
    [[nodiscard]] bool finish_callback(int argument_count) noexcept;

    lua_State* state_{nullptr};
    int application_reference_{-2};
    VisualState& visual_;
    int logical_width_;
    int logical_height_;
    bool quit_requested_{false};
    bool started_{false};
    bool shutdown_called_{false};
    const char* active_callback_{nullptr};
};

}  // namespace {{PROJECT_ID}}
