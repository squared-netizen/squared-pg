--- Application scripting entry point.
-- The returned table defines lifecycle callbacks invoked by C++.
-- @script main

local application = {}
local phase = 0.0
local touch_x
local touch_y
local logical_width = 960
local logical_height = 540

--- Initialize script-owned application state.
function application.init()
    logical_width, logical_height = host.logical_size()
    host.log("{{PROJECT_TITLE}} Lua lifecycle initialized")
end

--- Advance script state and publish the next visual state.
-- @param delta_seconds Frame duration in seconds, capped by the host.
function application.update(delta_seconds)
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
    host.set_background(8, pulse, 32)
    host.set_tile(
        x,
        y,
        size,
        80,
        math.floor(190 + 45 * (1 + math.sin(phase)) / 2),
        140
    )
end

--- Receive a portable event translated by the C++ host.
-- @param kind Event name.
-- @param first First numeric payload.
-- @param second Second numeric payload.
function application.event(kind, first, second)
    if kind == "touch_down" or kind == "touch_move" then
        touch_x = first
        touch_y = second
    elseif kind == "touch_up" then
        touch_x = nil
        touch_y = nil
    elseif kind == "back" then
        host.request_quit()
    elseif kind == "background" then
        host.log("Application entered background")
    elseif kind == "foreground" then
        host.log("Application returned to foreground")
    end
end

--- Release script-owned resources.
function application.shutdown()
    host.log("Lua lifecycle shutdown")
end

return application
