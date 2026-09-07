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

namespace {

/// Compression level for the writer, from `--compress` or `--store`.
///
/// Levels are miniz's deflate levels, exposed rather than wrapped in names
/// like "fast" and "best": zip tools have spoken 0-9 for thirty years, and a
/// private vocabulary would be one more thing to learn for no gain.
///
///   0    store, no compression. Fastest, largest.
///   1-9  deflate. 6 is the default and the usual sweet spot; 9 costs
///        noticeably more time for a few percent of size.
///
/// `--store` is kept as a synonym for `--compress=0`: it already shipped, it
/// reads better at a glance, and it names the case anyone actually wants it
/// for -- a cartridge of already-compressed payloads, where deflate spends
/// time to make the file marginally larger.
///
/// Compression never affects the content digest (format spec §9.2), which is
/// computed over uncompressed bytes in canonical order. Two cartridges packed
/// at different levels are the same cartridge by identity and by digest, and
/// differ only in size on disk. That is what makes exposing the knob safe to
/// hand a beginner: there is no wrong answer, only a size/time trade.
[[nodiscard]] std::optional<int> compression_from(const Invocation& inv, Environment& env)
{
    const bool store    = inv.has_flag("store");
    const auto declared = inv.option("compress");

    if (store && declared) {
        env.err << "sqcart: --store and --compress are mutually exclusive\n"
                << "        --store is a synonym for --compress=0\n";
        return std::nullopt;
    }
    if (store) return 0;
    if (!declared) return 6;

    // Parsed by hand rather than with stoi: a level is exactly one digit, and
    // accepting "6abc" or " 6" would let a typo silently select a level the
    // user did not ask for.
    if (declared->size() != 1 || declared->front() < '0' || declared->front() > '9') {
        env.err << "sqcart: --compress takes a single digit 0-9, not '" << *declared << "'\n"
                << "        0 stores without compressing; 1 is fastest; 9 is smallest\n";
        return std::nullopt;
    }
    return declared->front() - '0';
}

}  // namespace


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
    wo.symlinks     = symlink_policy_from(inv);

    auto level = compression_from(inv, env);
    if (!level) return ExitCode::usage;
    wo.compression = *level;

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
