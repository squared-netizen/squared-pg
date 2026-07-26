--- Minimal test helpers for the offline generator suite.
-- @module testlib

local testlib = {}

function testlib.equal(actual, expected, label)
    if actual ~= expected then
        error(string.format(
            "%s\nexpected: %s\nactual:   %s",
            label or "values differ",
            tostring(expected),
            tostring(actual)
        ), 0)
    end
end

function testlib.truthy(value, label)
    if not value then
        error(label or "expected a truthy value", 0)
    end
end

function testlib.fails(callback, pattern)
    local ok, message = pcall(callback)
    if ok then
        error("expected operation to fail", 0)
    end

    if pattern and not tostring(message):find(pattern, 1, true) then
        error(
            "failure did not contain '" ..
            pattern ..
            "': " ..
            tostring(message),
            0
        )
    end
end

return testlib
