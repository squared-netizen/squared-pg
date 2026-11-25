// kit.{{project_name}}: {{description}}
//
// The kit's surface, a C++ module: a workspace made with the kit imports it
// (`import {{project_name}};`). Keep it narrow: a bridge adapts what it wraps
// to squared's ways; it does not re-export it whole.

export module {{project_name}};

export namespace {{project_name}} {

/// Replace with the kit's own.
[[nodiscard]] constexpr const char* name() noexcept {
    return "kit.{{project_name}}";
}

}  // namespace {{project_name}}
