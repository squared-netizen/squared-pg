// SPDX-License-Identifier: MIT
//
// Write-side commands. Compiled only when the writer is available; when it is
// not, cmd_pack is a stub that says so rather than silently disappearing from
// the command table, because "unknown command" would be a lie.

#include "commands.hpp"

#include <fstream>
#include <ostream>
#include <system_error>

namespace sqcart::app {

#ifdef SQCART_ENABLE_WRITER

// ---------------------------------------------------------------------------
// pack  (jar c)
// ---------------------------------------------------------------------------

ExitCode cmd_pack(const Invocation& inv, Environment& env)
{
    if (inv.positional.empty()) {
        env.err << "usage: sqcart pack <directory> -o <out.sq>\n";
        return ExitCode::usage;
    }
    const auto out = inv.option("output");
    if (!out) {
        env.err << "sqcart: pack requires -o/--output\n";
        return ExitCode::usage;
    }

    const auto src      = resolve(env, inv.positional[0]);
    const auto out_path = resolve(env, *out);

    std::error_code ec;
    if (!std::filesystem::is_directory(src, ec)) {
        env.err << "sqcart: " << src.string() << " is not a directory\n";
        return ExitCode::failure;
    }

    // The source must itself be a valid exploded cartridge. Packing an
    // arbitrary directory would produce a .sq that fails to open, which is a
    // worse failure than refusing here.
    const auto manifest_path = src / kManifestPath;
    if (!std::filesystem::is_regular_file(manifest_path, ec)) {
        env.err << "sqcart: " << src.string() << " has no " << kManifestPath << "\n";
        return ExitCode::failure;
    }

    WriteOptions wo;
    wo.canonical    = true;   // FR-WRITE-6: determinism is not opt-in.
    wo.write_hashes = !inv.has_flag("no-hashes");
    wo.compression  = inv.has_flag("store") ? 0 : 6;
    wo.symlinks     = symlink_policy_from(inv);

    auto w = Writer::create(out_path, wo);
    if (!w) {
        report(env, w.error());
        return ExitCode::failure;
    }

    auto manifest_bytes = std::filesystem::file_size(manifest_path, ec);
    if (ec || manifest_bytes > wo.limits.max_manifest_bytes) {
        env.err << "sqcart: manifest missing or exceeds the size limit\n";
        return ExitCode::failure;
    }
    std::ifstream mf(manifest_path, std::ios::binary);
    std::string   manifest_json((std::istreambuf_iterator<char>(mf)),
                                std::istreambuf_iterator<char>());
    if (!mf) {
        env.err << "sqcart: cannot read " << manifest_path.string() << "\n";
        return ExitCode::failure;
    }

    if (auto r = w->set_manifest(manifest_json); !r) {
        report(env, r.error());
        return ExitCode::failure;
    }
    if (auto r = w->add_tree(src); !r) {
        report(env, r.error());
        return ExitCode::failure;
    }
    for (const auto& link : w->skipped_symlinks()) {
        env.err << "sqcart: skipped symlink " << link << "\n";
    }

    auto digest = std::move(*w).finish();
    if (!digest) {
        report(env, digest.error());
        return ExitCode::failure;
    }

    // FR-WRITE-7: the digest of what was just written, from the same
    // implementation the reader uses. Printed so a build can record it.
    env.out << to_hex(*digest) << "  " << out_path.string() << '\n';
    return ExitCode::ok;
}

#else   // !SQCART_ENABLE_WRITER

ExitCode cmd_pack(const Invocation&, Environment& env)
{
    env.err << "sqcart: this build has no writer; rebuild with "
               "-DSQCART_ENABLE_WRITER=ON\n";
    return ExitCode::failure;
}

#endif  // SQCART_ENABLE_WRITER

}  // namespace sqcart::app
