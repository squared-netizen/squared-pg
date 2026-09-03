// SPDX-License-Identifier: MIT
//
// squared/pg/value.hpp — structured data crossing the control surface.
//
// Specification: §2.6.5 (data crossing the boundary), §2.6.8 (configuration
// representation), §2.4.7 (result payloads).
//
// Every value entering or leaving the engine is a Value. The engine never sees
// a lua_State and never sees a JSON document; both are converted to this type
// at their respective edges, which is what keeps the boundary narrow.
//
// Integers and doubles are distinct alternatives on purpose. §2.6.5 requires
// version components, counts and sizes to cross as integers, and forbids
// silent narrowing — a single numeric alternative would make that
// unenforceable.

#ifndef SQUARED_PG_VALUE_HPP
#define SQUARED_PG_VALUE_HPP

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace squared::pg {

class Value;

/// Ordered key/value pairs.
///
/// A vector of pairs rather than std::map for two reasons. First, insertion
/// order is preserved, so serialising a Value twice produces byte-identical
/// output — §2.7.13 requires deterministic generation and metadata is a
/// generated file. Second, std::vector is specified to support incomplete
/// element types (§[vector.overview]); std::map is not, and Value is
/// necessarily incomplete at this point. Objects here hold a handful of keys,
/// so linear lookup is not a cost worth a node-based container.
using Object = std::vector<std::pair<std::string, Value>>;
using Array  = std::vector<Value>;

enum class ValueKind : std::uint8_t { null, boolean, integer, number, string, array, object };

class Value {
public:
    using Storage = std::variant<std::monostate, bool, std::int64_t, double, std::string, Array, Object>;

    Value() = default;
    Value(std::nullptr_t) {}                                  // NOLINT(google-explicit-constructor)
    Value(bool v) : storage_(v) {}                            // NOLINT(google-explicit-constructor)
    Value(int v) : storage_(static_cast<std::int64_t>(v)) {}   // NOLINT(google-explicit-constructor)
    Value(std::int64_t v) : storage_(v) {}                     // NOLINT(google-explicit-constructor)
    Value(double v) : storage_(v) {}                           // NOLINT(google-explicit-constructor)
    Value(std::string v) : storage_(std::move(v)) {}           // NOLINT(google-explicit-constructor)
    Value(std::string_view v) : storage_(std::string(v)) {}    // NOLINT(google-explicit-constructor)
    Value(const char* v) : storage_(std::string(v)) {}         // NOLINT(google-explicit-constructor)
    Value(Array v) : storage_(std::move(v)) {}                 // NOLINT(google-explicit-constructor)
    Value(Object v) : storage_(std::move(v)) {}                // NOLINT(google-explicit-constructor)

    [[nodiscard]] ValueKind kind() const noexcept;
    [[nodiscard]] bool is_null() const noexcept { return kind() == ValueKind::null; }
    [[nodiscard]] bool is_object() const noexcept { return kind() == ValueKind::object; }
    [[nodiscard]] bool is_array() const noexcept { return kind() == ValueKind::array; }

    /// Typed accessors. Each returns an empty optional when the alternative
    /// does not hold, never a coerced value: §2.6.5 makes conversion loss a
    /// structured error rather than something the accessor papers over.
    [[nodiscard]] std::optional<bool>         as_bool() const noexcept;
    [[nodiscard]] std::optional<std::int64_t> as_int() const noexcept;
    [[nodiscard]] std::optional<double>       as_number() const noexcept;
    [[nodiscard]] std::optional<std::string_view> as_string() const noexcept;
    [[nodiscard]] const Array*  as_array() const noexcept;
    [[nodiscard]] const Object* as_object() const noexcept;
    [[nodiscard]] Array*  as_array() noexcept;
    [[nodiscard]] Object* as_object() noexcept;

    /// Object member lookup. Null when absent or when this is not an object.
    [[nodiscard]] const Value* find(std::string_view key) const noexcept;

    /// Object member lookup with a fallback, for optional configuration keys.
    [[nodiscard]] std::string_view string_or(std::string_view key, std::string_view fallback) const noexcept;
    [[nodiscard]] bool bool_or(std::string_view key, bool fallback) const noexcept;
    [[nodiscard]] std::int64_t int_or(std::string_view key, std::int64_t fallback) const noexcept;

    /// Insert or replace an object member, promoting null to an empty object
    /// first. Calling this on a non-null, non-object Value is a no-op, which
    /// keeps result assembly free of defensive branching.
    void set(std::string key, Value value);

    /// Append to an array, promoting null to an empty array first.
    void push(Value value);

    [[nodiscard]] const Storage& storage() const noexcept { return storage_; }

    static Value object() { return Value{Object{}}; }
    static Value array() { return Value{Array{}}; }
    static Value strings(const std::vector<std::string>& items);

private:
    Storage storage_{};
};

[[nodiscard]] std::string_view to_string(ValueKind kind) noexcept;

}  // namespace squared::pg

#endif  // SQUARED_PG_VALUE_HPP
