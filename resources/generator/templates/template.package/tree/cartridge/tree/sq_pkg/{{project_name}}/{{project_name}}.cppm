// package.{{project_name}}: {{description}}
//
// The package's surface, a C++ module: a workspace made with the package
// imports it (`import {{project_name}};`). More modules beside it are its
// own; name them {{project_name}}.something, or make them partitions.

export module {{project_name}};

export namespace {{project_name}} {

/// Replace with the package's own.
[[nodiscard]] constexpr const char* name() noexcept {
    return "package.{{project_name}}";
}

}  // namespace {{project_name}}
