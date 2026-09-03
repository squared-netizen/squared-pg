// SPDX-License-Identifier: MIT
//
// squared/pg/result.hpp — the operation result.
//
// Specification: §2.4.7 (result handling), §2.6.11 (result representation),
// §2.15.5 (errors travel the same channel as successes).
//
// Every operation returns one of these, success or failure. There is no
// second channel: a workflow that has the result has everything the engine
// knows, and never needs to inspect the filesystem or read a log to find out
// what happened (§2.15.6).

#ifndef SQUARED_PG_RESULT_HPP
#define SQUARED_PG_RESULT_HPP

#include "squared/pg/error.hpp"
#include "squared/pg/value.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace squared::pg {

enum class Status : std::uint8_t { success, warning, failure, cancelled };

[[nodiscard]] std::string_view to_string(Status status) noexcept;

class OperationResult {
public:
    OperationResult() = default;

    [[nodiscard]] static OperationResult ok(Value data = {});
    [[nodiscard]] static OperationResult failed(EngineError error);

    [[nodiscard]] Status status() const noexcept { return status_; }
    [[nodiscard]] bool succeeded() const noexcept {
        return status_ == Status::success || status_ == Status::warning;
    }

    [[nodiscard]] const Value& data() const noexcept { return data_; }
    [[nodiscard]] Value& data() noexcept { return data_; }
    [[nodiscard]] const std::vector<Diagnostic>& diagnostics() const noexcept { return diagnostics_; }
    [[nodiscard]] const std::vector<Diagnostic>& warnings() const noexcept { return warnings_; }
    [[nodiscard]] const std::vector<EngineError>& errors() const noexcept { return errors_; }
    [[nodiscard]] const Value& metadata() const noexcept { return metadata_; }

    OperationResult& set_data(Value data);
    OperationResult& set_metadata(Value metadata);
    OperationResult& add_diagnostic(Diagnostic diagnostic);

    /// Adding a warning promotes a success to `warning` but never demotes a
    /// failure: §2.4.7 forbids hiding a failure behind partial success.
    OperationResult& add_warning(Diagnostic warning);

    /// Adding an error forces `failure`.
    OperationResult& add_error(EngineError error);

    OperationResult& set_operation(std::string_view operation);
    OperationResult& set_cancelled();

    /// Flatten into the representation the Lua binding and the CLI both
    /// consume. §2.6.11 fixes the field names; new fields may be added
    /// without breaking a workflow, existing ones may not change meaning.
    [[nodiscard]] Value to_value() const;

private:
    Status                  status_{Status::success};
    Value                   data_;
    Value                   metadata_;
    std::vector<Diagnostic> diagnostics_;
    std::vector<Diagnostic> warnings_;
    std::vector<EngineError> errors_;
};

}  // namespace squared::pg

#endif  // SQUARED_PG_RESULT_HPP
