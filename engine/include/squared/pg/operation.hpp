// SPDX-License-Identifier: MIT
//
// squared/pg/operation.hpp — the operation registry and capability set.
//
// Specification: §2.4.6 (operation registry; D-007, D-009 fix a typed C++
// registry as canonical and synchronous execution as normative), §2.14.1
// (capability tokens; D-023), §2.6.3 (the binding layer is generated from the
// registry).
//
// The registry is what makes the control surface discoverable. Lua, the CLI,
// and documentation tooling all enumerate it rather than hardcoding a list of
// callable names — so the set of operations a workflow can reach cannot drift
// from the set the engine implements.

#ifndef SQUARED_PG_OPERATION_HPP
#define SQUARED_PG_OPERATION_HPP

#include "squared/pg/error.hpp"
#include "squared/pg/result.hpp"
#include "squared/pg/value.hpp"
#include "squared/pg/version.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace squared::pg {

/// A named, versioned engine feature a manifest may require (§2.14.1).
struct Capability {
    std::string token;    ///< e.g. "capability.template.substitute"
    Version     version;
    std::string summary;

    [[nodiscard]] Value to_value() const;
};

/// The declared type of an operation parameter. A closed vocabulary so that
/// validation happens in the engine rather than in every workflow (§2.8.7
/// applies the same reasoning to template parameters).
enum class ParameterType : std::uint8_t {
    string,
    identifier,   ///< a resource identity
    path,
    boolean,
    integer,
    object,
    array,
};

[[nodiscard]] std::string_view to_string(ParameterType type) noexcept;

struct ParameterSpec {
    std::string   name;
    ParameterType type{ParameterType::string};
    bool          required{false};
    std::string   summary;

    [[nodiscard]] Value to_value() const;
};

/// §2.4.6: identity, typed parameters, required services and capabilities,
/// transaction requirement, and the codes it may return.
struct OperationDescriptor {
    std::string                id;       ///< stable dotted name, e.g. "project.generate"
    std::string                summary;
    std::vector<ParameterSpec> parameters;
    std::vector<std::string>   required_services;
    std::vector<std::string>   required_capabilities;
    bool                       mutates_workspace{false};
    std::vector<std::string>   error_codes;

    [[nodiscard]] Value to_value() const;
};

/// Validate `params` against a descriptor's parameter list.
///
/// Returns one error naming *every* missing or ill-typed parameter rather
/// than the first: §2.7.2 requires a validation error that identifies each
/// missing input, because a workflow fixing them one round-trip at a time is
/// a workflow that asks the user five questions instead of one.
[[nodiscard]] Result<void> validate_parameters(const OperationDescriptor& descriptor, const Value& params);

}  // namespace squared::pg

#endif  // SQUARED_PG_OPERATION_HPP
