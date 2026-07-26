---@meta

---@class HostModule
local host = {}

---Add two integers in C++.
---@param left integer
---@param right integer
---@return integer
function host.add(left, right) end

---Write a message through the C++ host logger.
---@param message string
function host.log(message) end

---Return the embedded Lua runtime version.
---@return string
function host.runtime_version() end

return host
