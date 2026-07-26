local script_path = arg[0]
local root = script_path:match("^(.*)/tests/[^/]+$") or "."
local sha256 = dofile(root .. "/tools/sha256.lua")

local vectors = {
    {
        input = "",
        expected =
            "e3b0c44298fc1c149afbf4c8996fb924" ..
            "27ae41e4649b934ca495991b7852b855"
    },
    {
        input = "abc",
        expected =
            "ba7816bf8f01cfea414140de5dae2223" ..
            "b00361a396177a9cb410ff61f20015ad"
    },
    {
        input = "The quick brown fox jumps over the lazy dog",
        expected =
            "d7a8fbb307d7809469ca9abcb0082e4f" ..
            "8d5651e46d3cdb762d02d0bf37c9e592"
    }
}

for _, vector in ipairs(vectors) do
    local actual = sha256.digest(vector.input)
    assert(
        actual == vector.expected,
        string.format(
            "SHA-256 mismatch\nexpected: %s\nactual:   %s",
            vector.expected,
            actual
        )
    )
end

print("SHA-256 known vectors: OK")
