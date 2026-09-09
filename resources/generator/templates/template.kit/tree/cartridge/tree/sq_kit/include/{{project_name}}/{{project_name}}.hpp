// SPDX-License-Identifier: MIT
//
// {{project_name}}/{{project_name}}.hpp — {{description}}
//
// This header is the kit's public surface. Application code reaches it as:
//
//     #include <{{project_name}}/{{project_name}}.hpp>
//
// The directory segment is the kit's own name (D-057). It sits INSIDE the
// include root, which is what keeps this header from colliding with a
// same-named header in sq_app/include or in another kit. Do not flatten it,
// and do not add a second level: kit identifiers are exactly two segments
// (D-058), so the mapping from kit.{{project_name}} to {{project_name}}/ is
// total and needs no rule.
//
// Everything under sq_kit/ is claimed by this kit's ownership table as
// sq_kit/** (D-061), so new headers added beside this one are covered
// without touching the manifest.

#pragma once

namespace squared::kit::{{project_name}} {

/// Replace with the kit's actual surface.
///
/// A bridge kit typically exposes: an initialise/shutdown pair if the
/// underlying library needs one, and thin types that adapt the library's
/// vocabulary to the framework's. Keep the surface narrow — a kit that
/// re-exports its dependency wholesale is a header, not a bridge.
[[nodiscard]] constexpr const char* name() noexcept {
    return "kit.{{project_name}}";
}

}  // namespace squared::kit::{{project_name}}
