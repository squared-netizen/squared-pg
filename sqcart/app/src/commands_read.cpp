// SPDX-License-Identifier: MIT
//
// Read-side commands. These are the jar equivalents: `list` is `jar t`,
// `extract` is `jar x`, `cat` has no jar analogue but is what makes the tool
// usable in a pipeline.

#include "commands.hpp"

#include <algorithm>
#include <iomanip>
#include <istream>
#include <ostream>
#include <sstream>
#include <string>

namespace sqcart::app {

std::filesystem::path resolve(const Environment& env, std::string_view p)
{
    std::filesystem::path path{p};
    if (path.is_absolute() || env.cwd.empty()) {
        return path;
    }
    return env.cwd / path;
}

SymlinkPolicy symlink_policy_from(const Invocation& inv)
{
    const auto v = inv.option("symlinks").value_or("skip");
    if (v == "follow" || v == "follow_internal") {
        return SymlinkPolicy::follow_internal;
    }
    if (v == "reject") {
        return SymlinkPolicy::reject;
    }
    return SymlinkPolicy::skip;
}

OpenOptions open_options_from(const Invocation& inv)
{
    OpenOptions o;
    o.conformance =
        inv.has_flag("lenient") ? Conformance::lenient : Conformance::strict;
    o.verify_integrity = inv.has_flag("verify-integrity");
    o.symlinks         = symlink_policy_from(inv);
    return o;
}

void report(Environment& env, const Error& e)
{
    env.err << "sqcart: " << to_string(e.code);
    if (e.entry) {
        env.err << " [" << *e.entry << "]";
    }
    env.err << ": " << e.message << "\n";
}

std::optional<Cartridge> open_positional(const Invocation& inv, Environment& env,
                                         std::string_view usage_hint)
{
    if (inv.positional.empty()) {
        env.err << "sqcart: " << inv.command << " requires a cartridge argument\n"
                << "usage: sqcart " << usage_hint << "\n";
        return std::nullopt;
    }
    auto c = Cartridge::open(resolve(env, inv.positional[0]), open_options_from(inv));
    if (!c) {
        report(env, c.error());
        return std::nullopt;
    }
    // Report omitted links on every open, not only on verify: a user listing
    // a cartridge should see that the source directory had more in it.
    for (const auto& link : c->skipped_symlinks()) {
        env.err << "sqcart: skipped symlink " << link << "\n";
    }
    return std::optional<Cartridge>(std::move(*c));
}

// ---------------------------------------------------------------------------
// list  (jar t)
// ---------------------------------------------------------------------------

ExitCode cmd_list(const Invocation& inv, Environment& env)
{
    auto c = open_positional(inv, env, "list <cartridge>");
    if (!c) {
        return inv.positional.empty() ? ExitCode::usage : ExitCode::failure;
    }

    // FR-CLI-3: stable, machine-parseable, tab-separated, one record per
    // line. Header suppressed unless asked for, so `sqcart list x.sq | cut`
    // works without a skip.
    if (inv.has_flag("verbose")) {
        env.err << "size\tcompressed\tmethod\tpath\n";
    }
    for (const auto& e : c->entries()) {
        env.out << e.uncompressed_size << '\t' << e.compressed_size << '\t'
                << (e.stored ? "stored" : "deflate") << '\t' << e.path << '\n';
    }
    return ExitCode::ok;
}

// ---------------------------------------------------------------------------
// show — raw manifest JSON, for piping into jq
// ---------------------------------------------------------------------------

ExitCode cmd_show(const Invocation& inv, Environment& env)
{
    auto c = open_positional(inv, env, "show <cartridge>");
    if (!c) {
        return inv.positional.empty() ? ExitCode::usage : ExitCode::failure;
    }
    env.out << c->manifest().raw_json();
    if (!c->manifest().raw_json().ends_with("\n")) {
        env.out << '\n';
    }
    return ExitCode::ok;
}

// ---------------------------------------------------------------------------
// info — human summary
// ---------------------------------------------------------------------------

ExitCode cmd_info(const Invocation& inv, Environment& env)
{
    auto c = open_positional(inv, env, "info <cartridge>");
    if (!c) {
        return inv.positional.empty() ? ExitCode::usage : ExitCode::failure;
    }
    const Manifest& m = c->manifest();

    auto field = [&](std::string_view label, std::string_view value) {
        if (!value.empty()) {
            env.out << std::left << std::setw(14) << label << value << '\n';
        }
    };

    field("id", m.id());
    field("version", m.version());
    field("kind", std::string{m.kind()});
    field("format", std::to_string(m.format_version()));
    if (auto t = m.title()) {
        field("title", *t);
    }
    if (auto l = m.license()) {
        field("license", *l);
    }
    field("tree", m.tree());
    field("form", c->exploded() ? "exploded" : "archive");
    field("entries", std::to_string(c->entries().size()));

    std::uint64_t total = 0;
    for (const auto& e : c->entries()) {
        total += e.uncompressed_size;
    }
    field("bytes", std::to_string(total));

    for (const auto& f : m.requires_features()) {
        field("requires", f);
    }

    // Consumer sections (§5.6). Names only.
    //
    // This is where format 1 printed kind-specific highlights -- a kit's
    // external dependency, a template's platforms, a cartridge's entry point.
    // Printing them required knowing those schemas, which this tool no longer
    // does. What it can honestly show is who the cartridge is addressed to;
    // `sqcart show` prints the manifest in full for anyone who needs more.
    for (const auto& name : m.consumers()) {
        field("consumer", std::string{name});
    }
    return ExitCode::ok;
}

// ---------------------------------------------------------------------------
// cat — one entry to stdout, bytes verbatim
// ---------------------------------------------------------------------------

ExitCode cmd_cat(const Invocation& inv, Environment& env)
{
    if (inv.positional.size() < 2) {
        env.err << "usage: sqcart cat <cartridge> <entry>\n";
        return ExitCode::usage;
    }
    auto c = open_positional(inv, env, "cat <cartridge> <entry>");
    if (!c) {
        return ExitCode::failure;
    }
    auto data = c->read(inv.positional[1]);
    if (!data) {
        report(env, data.error());
        return ExitCode::failure;
    }
    env.out.write(reinterpret_cast<const char*>(data->data()),
                  static_cast<std::streamsize>(data->size()));
    return ExitCode::ok;
}

// ---------------------------------------------------------------------------
// verify — full pipeline, exit 3 when non-conforming
// ---------------------------------------------------------------------------

ExitCode cmd_verify(const Invocation& inv, Environment& env)
{
    auto c = open_positional(inv, env, "verify <cartridge>");
    if (!c) {
        return inv.positional.empty() ? ExitCode::usage : ExitCode::failure;
    }

    const ValidationReport report_ = validate(*c);

    for (const auto& d : report_.diagnostics) {
        const char* sev = d.severity == Severity::error     ? "error"
                          : d.severity == Severity::warning ? "warning"
                                                            : "info";
        // FR-CLI-4: diagnostics to stderr; stdout stays reserved for future
        // machine-readable output.
        env.err << sev << ": " << d.message;
        if (d.entry) {
            env.err << " [" << *d.entry << "]";
        }
        env.err << '\n';
    }

    if (!report_.conforming) {
        env.err << "sqcart: " << c->manifest().id() << " is NOT conforming\n";
        return ExitCode::non_conforming;
    }
    if (inv.has_flag("verbose")) {
        env.err << "sqcart: " << c->manifest().id() << " conforms\n";
    }
    return ExitCode::ok;
}

// ---------------------------------------------------------------------------
// digest — content digest, spec 9.2
// ---------------------------------------------------------------------------

ExitCode cmd_digest(const Invocation& inv, Environment& env)
{
    auto c = open_positional(inv, env, "digest <cartridge>");
    if (!c) {
        return inv.positional.empty() ? ExitCode::usage : ExitCode::failure;
    }
    auto d = c->content_digest();
    if (!d) {
        report(env, d.error());
        return ExitCode::failure;
    }
    // sha256sum-compatible shape, so `sqcart digest x.sq` output can be
    // diffed against a recorded value directly.
    env.out << to_hex(*d) << "  " << inv.positional[0] << '\n';
    return ExitCode::ok;
}

// ---------------------------------------------------------------------------
// unpack  (jar x)
//
// Named to pair with `pack`. `extract` remains an accepted alias: it was the
// original spelling and there is no reason to punish muscle memory.
// ---------------------------------------------------------------------------

ExitCode cmd_unpack(const Invocation& inv, Environment& env)
{
    auto c = open_positional(inv, env, "unpack <cartridge> [-d DIR]");
    if (!c) {
        return inv.positional.empty() ? ExitCode::usage : ExitCode::failure;
    }

    const auto dest = resolve(env, inv.option("dest").value_or("."));
    auto       r    = c->extract_to(dest, inv.has_flag("overwrite"));
    if (!r) {
        report(env, r.error());
        // FR-EXT-4: extraction is not transactional, so say so rather than
        // let the user assume nothing was written.
        env.err << "sqcart: extraction stopped partway; files already written remain\n";
        return ExitCode::failure;
    }
    if (inv.has_flag("verbose")) {
        env.err << "sqcart: extracted " << c->entries().size() << " entries to "
                << dest.string() << '\n';
    }
    return ExitCode::ok;
}

}  // namespace sqcart::app
