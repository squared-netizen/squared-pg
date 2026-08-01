--- Touch-controlled orbit example plug-in.
-- @module plugins.orbit

local palette = require("palette")
local plugin = {}
local phase = 0.0
local touch_x
local touch_y
local logical_width = 960
local logical_height = 540

--- Initialize display state through declared capabilities.
function plugin.init()
    logical_width, logical_height = host.logical_size()
    host.log("{{PROJECT_TITLE}} orbit plug-in initialized")
end

--- Advance the animation and publish its visual state.
-- @param delta_seconds Frame duration in seconds, capped by the host.
function plugin.update(delta_seconds)
    phase = phase + delta_seconds

    local size = math.floor(72 + 18 * math.sin(phase * 2.0))
    local x
    local y

    if touch_x and touch_y then
        x = math.floor(touch_x - size / 2)
        y = math.floor(touch_y - size / 2)
    else
        x = math.floor(
            logical_width / 2 +
            math.cos(phase) * logical_width * 0.22 -
            size / 2
        )
        y = math.floor(
            logical_height / 2 +
            math.sin(phase * 1.4) * logical_height * 0.18 -
            size / 2
        )
    end

    local pulse = math.floor(24 + 12 * (1 + math.sin(phase * 0.7)))
    host.set_background(
        palette.background_red,
        pulse,
        palette.background_blue
    )
    host.set_tile(
        x,
        y,
        size,
        palette.tile_red,
        math.floor(
            palette.tile_green - 30 +
            45 * (1 + math.sin(phase)) / 2
        ),
        palette.tile_blue
    )
end

--- Receive one portable event from the native host.
-- @param kind Event name.
-- @param first First numeric payload.
-- @param second Second numeric payload.
function plugin.event(kind, first, second)
    if kind == "touch_down" or kind == "touch_move" then
        touch_x = first
        touch_y = second
    elseif kind == "touch_up" then
        touch_x = nil
        touch_y = nil
    elseif kind == "back" then
        host.request_quit()
    end
end

--- Release plug-in-owned resources.
function plugin.shutdown()
    host.log("Orbit plug-in shutdown")
end

return plugin
