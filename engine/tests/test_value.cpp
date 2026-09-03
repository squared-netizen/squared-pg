// SPDX-License-Identifier: MIT
// The boundary value type and its JSON round trip.

#include "squared/pg/value.hpp"
#include "support/support.hpp"

#include "check.hpp"

using namespace squared::pg;

namespace {

void kinds() {
    CHECK(Value{}.kind() == ValueKind::null);
    CHECK(Value{true}.kind() == ValueKind::boolean);
    CHECK(Value{std::int64_t{7}}.kind() == ValueKind::integer);
    CHECK(Value{1.5}.kind() == ValueKind::number);
    CHECK(Value{"text"}.kind() == ValueKind::string);

    // §2.6.5: integers and doubles are distinct and the boundary must not
    // narrow silently. An integer reads as a number; a double never reads as
    // an integer.
    CHECK(Value{std::int64_t{7}}.as_number().has_value());
    CHECK(!Value{1.5}.as_int().has_value());
    CHECK(!Value{"7"}.as_int().has_value());
}

void objects() {
    Value object = Value::object();
    object.set("name", "hello");
    object.set("count", std::int64_t{3});
    object.set("count", std::int64_t{4});  // replaces, does not append

    CHECK_EQ(std::string{object.string_or("name", "")}, "hello");
    CHECK(object.int_or("count", 0) == 4);
    CHECK(object.as_object()->size() == 2);
    CHECK(object.find("absent") == nullptr);
    CHECK(object.bool_or("absent", true));

    // set() on a null promotes it, so result assembly needs no defensive
    // branching.
    Value fresh;
    fresh.set("k", "v");
    CHECK(fresh.is_object());
}

void json_roundtrip() {
    const std::string source = R"({"a":1,"b":[1,2,3],"c":{"d":"e"},"f":true,"g":null,"h":1.5})";
    auto parsed = support::json_parse(source);
    CHECK(parsed.has_value());
    CHECK(parsed->int_or("a", 0) == 1);
    CHECK(parsed->find("b")->as_array()->size() == 3);
    CHECK_EQ(std::string{parsed->find("c")->string_or("d", "")}, "e");
    CHECK(parsed->bool_or("f", false));

    // Object order is preserved, so writing the same value twice produces the
    // same bytes — §2.7.13 needs that, because metadata is a generated file.
    const std::string once = support::json_write(*parsed, false);
    const std::string twice = support::json_write(*support::json_parse(once), false);
    CHECK_EQ(twice, once);

    CHECK(!support::json_parse("{not json").has_value());
}

void escaping() {
    Value object = Value::object();
    object.set("text", std::string{"quote\" backslash\\ newline\n tab\t"});
    const std::string written = support::json_write(object, false);
    auto reparsed = support::json_parse(written);
    CHECK(reparsed.has_value());
    CHECK_EQ(std::string{reparsed->string_or("text", "")}, "quote\" backslash\\ newline\n tab\t");
}

}  // namespace

int main() {
    kinds();
    objects();
    json_roundtrip();
    escaping();
    return squared::pg::test::report("test_value");
}
