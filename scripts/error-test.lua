local error_test = {}

function error_test.run()
    error("intentional Lua failure used to verify diagnostics")
end

return error_test
