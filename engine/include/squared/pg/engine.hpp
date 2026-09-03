// SPDX-License-Identifier: MIT
//
// squared/pg/engine.hpp — the engine instance and its control surface.
//
// Specification: §2.4 in full. Creation (§2.4.1), initialization (§2.4.2),
// service registration (§2.4.3), API exposure (§2.4.5), operation execution
// (§2.4.6), shutdown (§2.4.9), lifecycle states (§2.4.11), concurrency
// (§2.4.12).
//
// Patterns used here, documented in docs/developer/engine/patterns.md:
//   State           — LifecycleState with a central legality matrix
//   Service Locator — scoped to the instance, never global (§2.4.3)
//   Command         — the operation registry and the generation plan
//   Factory         — create() returns unique_ptr; there is no singleton
//   Facade          — Engine hides the services behind one narrow surface
//
// Threading (§2.4.12, D-015): an instance is not thread-safe and has affinity
// to its creating thread. Several instances may coexist provided no two
// target the same workspace.

#ifndef SQUARED_PG_ENGINE_HPP
#define SQUARED_PG_ENGINE_HPP

#include "squared/pg/error.hpp"
#include "squared/pg/operation.hpp"
#include "squared/pg/resource.hpp"
#include "squared/pg/result.hpp"
#include "squared/pg/value.hpp"
#include "squared/pg/version.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace squared::pg {

/// §2.4.11. Exactly one of these holds at all times.
enum class LifecycleState : std::uint8_t {
    created,
    initializing,
    ready,
    operating,
    failed,
    shutdown,
    destroyed,
};

[[nodiscard]] std::string_view to_string(LifecycleState state) noexcept;

/// Startup configuration. Every input is explicit: §2.7.2 forbids generation
/// depending on ambient environment or the current working directory, and the
/// same discipline applies to engine startup.
struct EngineConfig {
    /// Directories scanned for resources, in declared order. Scanning is
    /// deterministic (§2.8.4): roots in this order, entries within a root
    /// sorted, so duplicate-identity conflicts are reported consistently.
    std::vector<std::filesystem::path> resource_roots;

    /// Reported to manifests through `requires_engine` / manifest `engine`.
    Version engine_version{};

    /// Fail initialization when a resource root does not exist. Off by
    /// default so a fresh checkout with an empty resources/packages still
    /// starts; the absence is reported as a diagnostic either way.
    bool strict_roots{false};

    /// §2.15.11. `durable` fsyncs staged content before rename; `fast` fsyncs
    /// only journal state transitions. Recorded in every mutating result so
    /// the caller knows which was used.
    bool fast_durability{false};
};

/// The engine instance (§2.4.1).
class Engine {
public:
    /// Factory. Returns a fully constructed but uninitialized instance in
    /// `created`. Creation allocates and prepares; it reads no configuration
    /// files, resolves no resources and touches no filesystem (§2.4.1).
    [[nodiscard]] static std::unique_ptr<Engine> create(EngineConfig config);

    /// §2.4.2. Deterministic: the same configuration and resource set produce
    /// the same runtime state. On failure the instance enters `failed` and
    /// remains safely cleanable rather than being left undefined.
    [[nodiscard]] OperationResult initialize();

    [[nodiscard]] LifecycleState state() const noexcept;

    /// §2.4.6. Synchronous: the call does not return before its state changes
    /// are complete. Unknown operations, illegal lifecycle states and invalid
    /// parameters all fail before any partial modification.
    [[nodiscard]] OperationResult execute(std::string_view operation, const Value& parameters);

    /// The registry, enumerable per §2.4.6. The binding layer is built from
    /// this, which is how §2.6.3's no-drift requirement is met.
    [[nodiscard]] std::span<const OperationDescriptor> operations() const noexcept;

    /// §2.14.1. Compiled in, enumerable, checked against manifest
    /// `requires_capabilities` at resolution — never at materialization.
    [[nodiscard]] std::span<const Capability> capabilities() const noexcept;

    [[nodiscard]] const ResourceIndex& resources() const noexcept;

    /// The most recent result, readable in states where execution is not
    /// (§2.4.11 permits `inspect last result` from `failed` and `shutdown`).
    [[nodiscard]] const OperationResult& last_result() const noexcept;

    [[nodiscard]] Version version() const noexcept;

    /// §2.4.9. Safe from any valid state, including partial initialization.
    /// Releases services in reverse dependency order; the engine never relies
    /// on process termination to release anything.
    [[nodiscard]] OperationResult shutdown();

    ~Engine();

    Engine(const Engine&)            = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&)                 = delete;
    Engine& operator=(Engine&&)      = delete;

private:
    Engine();
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// The engine version this build reports.
[[nodiscard]] Version engine_version() noexcept;

}  // namespace squared::pg

#endif  // SQUARED_PG_ENGINE_HPP
