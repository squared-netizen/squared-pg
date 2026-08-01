--- Lifecycle diagnostics example plug-in.
-- @module plugins.diagnostics

local plugin = {}

--- Report initialization through the declared logging capability.
function plugin.init()
    host.log(
        "Diagnostics plug-in initialized with API " ..
            tostring(host.api_version)
    )
end

--- Report Android foreground transitions.
-- @param kind Portable event name.
function plugin.event(kind)
    if kind == "background" then
        host.log("Application entered background")
    elseif kind == "foreground" then
        host.log("Application returned to foreground")
    end
end

--- Report orderly plug-in shutdown.
function plugin.shutdown()
    host.log("Diagnostics plug-in shutdown")
end

return plugin
