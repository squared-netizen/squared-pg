#include <{{PROJECT_ID}}/script_runtime.hpp>

#include <SDL.h>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace {{PROJECT_ID}} {
namespace {

ScriptRuntime* runtime_from_upvalue(lua_State* state)
{
    return static_cast<ScriptRuntime*>(
        lua_touserdata(state, lua_upvalueindex(1))
    );
}

std::uint8_t color_component(lua_State* state, int index)
{
    const lua_Integer value = luaL_checkinteger(state, index);
    return static_cast<std::uint8_t>(
        std::clamp<lua_Integer>(value, 0, 255)
    );
}

constexpr std::size_t kMaximumLuaAssetBytes = 1024U * 1024U;

bool valid_lua_asset_path(std::string_view asset_path)
{
    if (!asset_path.starts_with("lua/") ||
        !asset_path.ends_with(".lua") ||
        asset_path.size() > 256 ||
        asset_path.find('\\') != std::string_view::npos) {
        return false;
    }

    std::size_t segment_start = 0;
    while (segment_start <= asset_path.size()) {
        const std::size_t separator =
            asset_path.find('/', segment_start);
        const std::size_t segment_end =
            separator == std::string_view::npos
                ? asset_path.size()
                : separator;
        const std::string_view segment =
            asset_path.substr(
                segment_start,
                segment_end - segment_start
            );

        if (segment.empty() || segment == "." || segment == "..") {
            return false;
        }
        if (separator == std::string_view::npos) {
            break;
        }
        segment_start = separator + 1;
    }

    return true;
}

bool read_asset(
    const char* asset_path,
    std::vector<char>& destination,
    std::string& error
)
{
    SDL_RWops* stream = SDL_RWFromFile(asset_path, "rb");
    if (!stream) {
        error =
            "cannot open Lua asset " +
            std::string(asset_path) +
            ": " +
            SDL_GetError();
        return false;
    }

    const Sint64 length = SDL_RWsize(stream);
    if (length <= 0 ||
        static_cast<Uint64>(length) >
            static_cast<Uint64>(kMaximumLuaAssetBytes) ||
        static_cast<Uint64>(length) >
            static_cast<Uint64>(
                std::numeric_limits<std::size_t>::max()
            )) {
        error =
            "invalid Lua asset size for " + std::string(asset_path);
        SDL_RWclose(stream);
        return false;
    }

    destination.resize(static_cast<std::size_t>(length));
    const std::size_t received =
        SDL_RWread(stream, destination.data(), 1, destination.size());
    SDL_RWclose(stream);

    if (received != destination.size()) {
        error =
            "short read from Lua asset " + std::string(asset_path);
        return false;
    }

    return true;
}

}  // namespace

ScriptRuntime::ScriptRuntime(
    VisualState& visual,
    int logical_width,
    int logical_height
) noexcept
    : visual_(visual),
      logical_width_(logical_width),
      logical_height_(logical_height)
{
}

ScriptRuntime::~ScriptRuntime()
{
    shutdown();
}

bool ScriptRuntime::start(const char* asset_path) noexcept
{
    if (state_) {
        SDL_Log("Lua runtime was already started");
        return false;
    }

    state_ = luaL_newstate();
    if (!state_) {
        SDL_Log("Cannot allocate Lua state");
        return false;
    }

    application_reference_ = LUA_NOREF;
    quit_requested_ = false;
    shutdown_called_ = false;
    active_callback_ = nullptr;

    open_safe_libraries();
    install_host_api();

    if (!asset_path || !valid_lua_asset_path(asset_path)) {
        SDL_Log("Invalid Lua bootstrap asset path");
        shutdown();
        return false;
    }

    std::vector<char> source;
    std::string read_error;
    if (!read_asset(asset_path, source, read_error)) {
        SDL_Log("%s", read_error.c_str());
        shutdown();
        return false;
    }

    if (luaL_loadbuffer(
            state_,
            source.data(),
            source.size(),
            asset_path
        ) != LUA_OK ||
        lua_pcall(state_, 0, 1, 0) != LUA_OK) {
        const char* message = lua_tostring(state_, -1);
        SDL_Log(
            "Lua startup error: %s",
            message ? message : "non-string Lua error"
        );
        lua_pop(state_, 1);
        shutdown();
        return false;
    }

    if (!lua_istable(state_, -1)) {
        SDL_Log("Lua startup asset must return a callback table");
        lua_pop(state_, 1);
        shutdown();
        return false;
    }

    application_reference_ = luaL_ref(state_, LUA_REGISTRYINDEX);
    started_ = true;

    if (prepare_callback("init")) {
        static_cast<void>(finish_callback(0));
    }

    return true;
}

void ScriptRuntime::update(double delta_seconds) noexcept
{
    if (prepare_callback("update")) {
        lua_pushnumber(state_, delta_seconds);
        static_cast<void>(finish_callback(1));
    }
}

void ScriptRuntime::handle_event(const SDL_Event& event) noexcept
{
    const char* kind = nullptr;
    double first = 0.0;
    double second = 0.0;

    switch (event.type) {
    case SDL_FINGERDOWN:
        kind = "touch_down";
        first = event.tfinger.x * logical_width_;
        second = event.tfinger.y * logical_height_;
        break;
    case SDL_FINGERMOTION:
        kind = "touch_move";
        first = event.tfinger.x * logical_width_;
        second = event.tfinger.y * logical_height_;
        break;
    case SDL_FINGERUP:
        kind = "touch_up";
        first = event.tfinger.x * logical_width_;
        second = event.tfinger.y * logical_height_;
        break;
    case SDL_KEYDOWN:
        if (event.key.keysym.sym == SDLK_AC_BACK) {
            kind = "back";
        }
        break;
    case SDL_APP_DIDENTERBACKGROUND:
        kind = "background";
        break;
    case SDL_APP_DIDENTERFOREGROUND:
        kind = "foreground";
        break;
    default:
        break;
    }

    if (kind && prepare_callback("event")) {
        lua_pushstring(state_, kind);
        lua_pushnumber(state_, first);
        lua_pushnumber(state_, second);
        static_cast<void>(finish_callback(3));
    }
}

void ScriptRuntime::shutdown() noexcept
{
    if (!state_) {
        return;
    }

    if (started_ && !shutdown_called_) {
        shutdown_called_ = true;
        if (prepare_callback("shutdown")) {
            static_cast<void>(finish_callback(0));
        }
    }

    if (application_reference_ != LUA_NOREF) {
        luaL_unref(
            state_,
            LUA_REGISTRYINDEX,
            application_reference_
        );
        application_reference_ = LUA_NOREF;
    }

    lua_close(state_);
    state_ = nullptr;
    started_ = false;
}

bool ScriptRuntime::quit_requested() const noexcept
{
    return quit_requested_;
}

int ScriptRuntime::lua_read_asset(lua_State* state)
{
    std::size_t asset_path_length = 0;
    const char* asset_path =
        luaL_checklstring(state, 1, &asset_path_length);
    const std::string_view asset_path_view(
        asset_path,
        asset_path_length
    );

    if (asset_path_view.find('\0') != std::string_view::npos ||
        !valid_lua_asset_path(asset_path_view)) {
        lua_pushnil(state);
        lua_pushliteral(state, "invalid Lua asset path");
        return 2;
    }

    std::vector<char> source;
    std::string read_error;
    if (!read_asset(asset_path, source, read_error)) {
        lua_pushnil(state);
        lua_pushlstring(
            state,
            read_error.data(),
            read_error.size()
        );
        return 2;
    }

    lua_pushlstring(state, source.data(), source.size());
    return 1;
}

int ScriptRuntime::lua_runtime_version(lua_State* state)
{
    lua_pushliteral(state, LUA_VERSION);
    return 1;
}

int ScriptRuntime::lua_log(lua_State* state)
{
    const char* message = luaL_checkstring(state, 1);
    SDL_Log("[Lua] %s", message);
    return 0;
}

int ScriptRuntime::lua_set_background(lua_State* state)
{
    ScriptRuntime* runtime = runtime_from_upvalue(state);
    runtime->visual_.background_red = color_component(state, 1);
    runtime->visual_.background_green = color_component(state, 2);
    runtime->visual_.background_blue = color_component(state, 3);
    return 0;
}

int ScriptRuntime::lua_set_tile(lua_State* state)
{
    ScriptRuntime* runtime = runtime_from_upvalue(state);
    runtime->visual_.tile_x =
        static_cast<int>(luaL_checkinteger(state, 1));
    runtime->visual_.tile_y =
        static_cast<int>(luaL_checkinteger(state, 2));
    runtime->visual_.tile_size = static_cast<int>(
        std::clamp<lua_Integer>(
            luaL_checkinteger(state, 3),
            1,
            512
        )
    );
    runtime->visual_.tile_red = color_component(state, 4);
    runtime->visual_.tile_green = color_component(state, 5);
    runtime->visual_.tile_blue = color_component(state, 6);
    return 0;
}

int ScriptRuntime::lua_logical_size(lua_State* state)
{
    ScriptRuntime* runtime = runtime_from_upvalue(state);
    lua_pushinteger(state, runtime->logical_width_);
    lua_pushinteger(state, runtime->logical_height_);
    return 2;
}

int ScriptRuntime::lua_request_quit(lua_State* state)
{
    runtime_from_upvalue(state)->quit_requested_ = true;
    return 0;
}

void ScriptRuntime::open_safe_libraries() noexcept
{
    struct Library {
        const char* name;
        lua_CFunction open;
    };

    for (const Library& library : {
             Library{LUA_GNAME, luaopen_base},
             Library{LUA_COLIBNAME, luaopen_coroutine},
             Library{LUA_TABLIBNAME, luaopen_table},
             Library{LUA_STRLIBNAME, luaopen_string},
             Library{LUA_MATHLIBNAME, luaopen_math},
             Library{LUA_UTF8LIBNAME, luaopen_utf8}
         }) {
        luaL_requiref(state_, library.name, library.open, 1);
        lua_pop(state_, 1);
    }

    for (const char* blocked : {"dofile", "loadfile"}) {
        lua_pushnil(state_);
        lua_setglobal(state_, blocked);
    }
}

void ScriptRuntime::install_host_api() noexcept
{
    lua_newtable(state_);

    struct Function {
        const char* name;
        lua_CFunction callback;
    };

    for (const Function& function : {
             Function{"_read_asset", lua_read_asset},
             Function{"runtime_version", lua_runtime_version},
             Function{"log", lua_log},
             Function{"set_background", lua_set_background},
             Function{"set_tile", lua_set_tile},
             Function{"logical_size", lua_logical_size},
             Function{"request_quit", lua_request_quit}
         }) {
        lua_pushlightuserdata(state_, this);
        lua_pushcclosure(state_, function.callback, 1);
        lua_setfield(state_, -2, function.name);
    }

    lua_setglobal(state_, "host");
}

bool ScriptRuntime::prepare_callback(const char* name) noexcept
{
    if (!state_ || !started_ ||
        application_reference_ == LUA_NOREF) {
        active_callback_ = nullptr;
        return false;
    }

    lua_rawgeti(
        state_,
        LUA_REGISTRYINDEX,
        application_reference_
    );
    lua_getfield(state_, -1, name);
    lua_remove(state_, -2);

    if (lua_isnil(state_, -1)) {
        lua_pop(state_, 1);
        active_callback_ = nullptr;
        return false;
    }

    if (!lua_isfunction(state_, -1)) {
        SDL_Log("Lua callback '%s' is not a function", name);
        lua_pop(state_, 1);
        active_callback_ = nullptr;
        return false;
    }

    active_callback_ = name;
    return true;
}

bool ScriptRuntime::finish_callback(int argument_count) noexcept
{
    if (lua_pcall(state_, argument_count, 0, 0) == LUA_OK) {
        active_callback_ = nullptr;
        return true;
    }

    const char* message = lua_tostring(state_, -1);
    SDL_Log(
        "Lua callback error: %s",
        message ? message : "non-string Lua error"
    );
    lua_pop(state_, 1);

    if (active_callback_) {
        lua_rawgeti(
            state_,
            LUA_REGISTRYINDEX,
            application_reference_
        );
        lua_pushnil(state_);
        lua_setfield(state_, -2, active_callback_);
        lua_pop(state_, 1);
        SDL_Log(
            "Disabled failing Lua callback: %s",
            active_callback_
        );
    }
    active_callback_ = nullptr;
    return false;
}

}  // namespace {{PROJECT_ID}}
