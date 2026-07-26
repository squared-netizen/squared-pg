--- Pure Lua 5.4 SHA-256 support for offline dependency verification.
-- @module sha256

local sha256 = {}

local mask = 0xffffffff

local constants = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
}

local function rotate_right(value, amount)
    return ((value >> amount) | (value << (32 - amount))) & mask
end

--- Calculate a SHA-256 digest.
-- @tparam string data Bytes to hash.
-- @treturn string Lowercase hexadecimal digest.
function sha256.digest(data)
    assert(type(data) == "string", "data must be a string")

    local bit_length = #data * 8
    local padding_length = (56 - ((#data + 1) % 64)) % 64
    local message =
        data ..
        "\128" ..
        string.rep("\0", padding_length) ..
        string.pack(">I8", bit_length)

    local h0 = 0x6a09e667
    local h1 = 0xbb67ae85
    local h2 = 0x3c6ef372
    local h3 = 0xa54ff53a
    local h4 = 0x510e527f
    local h5 = 0x9b05688c
    local h6 = 0x1f83d9ab
    local h7 = 0x5be0cd19

    for chunk_start = 1, #message, 64 do
        local words = {}

        for index = 0, 15 do
            words[index] = string.unpack(
                ">I4",
                message,
                chunk_start + index * 4
            )
        end

        for index = 16, 63 do
            local previous_15 = words[index - 15]
            local previous_2 = words[index - 2]
            local sigma0 =
                rotate_right(previous_15, 7) ~
                rotate_right(previous_15, 18) ~
                (previous_15 >> 3)
            local sigma1 =
                rotate_right(previous_2, 17) ~
                rotate_right(previous_2, 19) ~
                (previous_2 >> 10)

            words[index] = (
                words[index - 16] +
                sigma0 +
                words[index - 7] +
                sigma1
            ) & mask
        end

        local a, b, c, d = h0, h1, h2, h3
        local e, f, g, h = h4, h5, h6, h7

        for index = 0, 63 do
            local sum1 =
                rotate_right(e, 6) ~
                rotate_right(e, 11) ~
                rotate_right(e, 25)
            local choice = (e & f) ~ ((~e) & g)
            local temporary1 =
                (h + sum1 + choice + constants[index + 1] + words[index])
                & mask
            local sum0 =
                rotate_right(a, 2) ~
                rotate_right(a, 13) ~
                rotate_right(a, 22)
            local majority = (a & b) ~ (a & c) ~ (b & c)
            local temporary2 = (sum0 + majority) & mask

            h = g
            g = f
            f = e
            e = (d + temporary1) & mask
            d = c
            c = b
            b = a
            a = (temporary1 + temporary2) & mask
        end

        h0 = (h0 + a) & mask
        h1 = (h1 + b) & mask
        h2 = (h2 + c) & mask
        h3 = (h3 + d) & mask
        h4 = (h4 + e) & mask
        h5 = (h5 + f) & mask
        h6 = (h6 + g) & mask
        h7 = (h7 + h) & mask
    end

    return string.format(
        "%08x%08x%08x%08x%08x%08x%08x%08x",
        h0,
        h1,
        h2,
        h3,
        h4,
        h5,
        h6,
        h7
    )
end

--- Calculate the SHA-256 digest of a file.
-- @tparam string path File to read.
-- @treturn string Lowercase hexadecimal digest.
-- @raise If the file cannot be opened or read.
function sha256.file(path)
    local file, open_error = io.open(path, "rb")

    if not file then
        error("cannot open " .. path .. ": " .. tostring(open_error), 2)
    end

    local data, read_error = file:read("*a")
    file:close()

    if not data then
        error("cannot read " .. path .. ": " .. tostring(read_error), 2)
    end

    return sha256.digest(data)
end

return sha256
