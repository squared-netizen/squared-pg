local host = require("host")

local smoke_test = {}

function smoke_test.run()
    assert(host.runtime_version() == "Lua 5.4")

    local result = host.add(20, 22)
    assert(result == 42)

    host.log("Lua called C++: 20 + 22 = 42")
    host.log("Lua embedding test passed")

    return true
end

return smoke_test
