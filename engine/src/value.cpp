// SPDX-License-Identifier: MIT

#include "squared/pg/value.hpp"

#include <algorithm>

namespace squared::pg {

ValueKind Value::kind() const noexcept {
    switch (storage_.index()) {
        case 0: return ValueKind::null;
        case 1: return ValueKind::boolean;
        case 2: return ValueKind::integer;
        case 3: return ValueKind::number;
        case 4: return ValueKind::string;
        case 5: return ValueKind::array;
        default: return ValueKind::object;
    }
}

std::optional<bool> Value::as_bool() const noexcept {
    if (const auto* v = std::get_if<bool>(&storage_)) return *v;
    return std::nullopt;
}

std::optional<std::int64_t> Value::as_int() const noexcept {
    if (const auto* v = std::get_if<std::int64_t>(&storage_)) return *v;
    return std::nullopt;
}

std::optional<double> Value::as_number() const noexcept {
    if (const auto* d = std::get_if<double>(&storage_)) return *d;
    // An integer is a number; the reverse is not true. Widening int64 to
    // double is lossless up to 2^53 and every count the engine reports is far
    // below that, so this direction is safe where the other is not.
    if (const auto* i = std::get_if<std::int64_t>(&storage_)) return static_cast<double>(*i);
    return std::nullopt;
}

std::optional<std::string_view> Value::as_string() const noexcept {
    if (const auto* v = std::get_if<std::string>(&storage_)) return std::string_view{*v};
    return std::nullopt;
}

const Array* Value::as_array() const noexcept { return std::get_if<Array>(&storage_); }
const Object* Value::as_object() const noexcept { return std::get_if<Object>(&storage_); }
Array* Value::as_array() noexcept { return std::get_if<Array>(&storage_); }
Object* Value::as_object() noexcept { return std::get_if<Object>(&storage_); }

const Value* Value::find(std::string_view key) const noexcept {
    const Object* object = as_object();
    if (object == nullptr) return nullptr;
    for (const auto& [name, value] : *object) {
        if (name == key) return &value;
    }
    return nullptr;
}

std::string_view Value::string_or(std::string_view key, std::string_view fallback) const noexcept {
    if (const Value* v = find(key)) {
        if (auto s = v->as_string()) return *s;
    }
    return fallback;
}

bool Value::bool_or(std::string_view key, bool fallback) const noexcept {
    if (const Value* v = find(key)) {
        if (auto b = v->as_bool()) return *b;
    }
    return fallback;
}

std::int64_t Value::int_or(std::string_view key, std::int64_t fallback) const noexcept {
    if (const Value* v = find(key)) {
        if (auto i = v->as_int()) return *i;
    }
    return fallback;
}

void Value::set(std::string key, Value value) {
    if (std::holds_alternative<std::monostate>(storage_)) storage_ = Object{};
    Object* object = as_object();
    if (object == nullptr) return;
    for (auto& entry : *object) {
        if (entry.first == key) {
            entry.second = std::move(value);
            return;
        }
    }
    object->emplace_back(std::move(key), std::move(value));
}

void Value::push(Value value) {
    if (std::holds_alternative<std::monostate>(storage_)) storage_ = Array{};
    if (Array* array = as_array()) array->push_back(std::move(value));
}

Value Value::strings(const std::vector<std::string>& items) {
    Array array;
    array.reserve(items.size());
    for (const auto& item : items) array.emplace_back(item);
    return Value{std::move(array)};
}

std::string_view to_string(ValueKind kind) noexcept {
    switch (kind) {
        case ValueKind::null: return "null";
        case ValueKind::boolean: return "boolean";
        case ValueKind::integer: return "integer";
        case ValueKind::number: return "number";
        case ValueKind::string: return "string";
        case ValueKind::array: return "array";
        case ValueKind::object: return "object";
    }
    return "null";
}

}  // namespace squared::pg
