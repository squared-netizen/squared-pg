// {{"{{"}}project_name}}: what it says, a module of its own (main.cpp imports it).

export module greeting;

import std;

export namespace greeting {

[[nodiscard]] std::string hello(std::string_view who) {
    return std::format("hello, {}", who);
}

}  // namespace greeting
