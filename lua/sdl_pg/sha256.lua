--- Load the generator's bundled pure-Lua SHA-256 module.
-- @module sdl_pg.sha256

local generator_root =
    rawget(_G, "SQUARED_PG_ROOT") or rawget(_G, "SDL_PG_ROOT")

if not generator_root then
    local source = debug.getinfo(1, "S").source
    source = source:sub(1, 1) == "@" and source:sub(2) or source
    generator_root =
        source:match("^(.*)/lua/sdl_pg/[^/]+%.lua$") or "."
end

return dofile(generator_root .. "/tools/sha256.lua")
