// SPDX-License-Identifier: MIT
//
// Argument parsing and command dispatch. No IO except through Environment.

#include "sqcart/app/cli.hpp"
#include "commands.hpp"

#include <algorithm>
#include <ostream>
#include <string>

namespace sqcart::app {

bool Invocation::has_flag(std::string_view name) const noexcept
{
    return std::find(flags.begin(), flags.end(), name) != flags.end();
}

std::optional<std::string> Invocation::option(std::string_view name) const
{
    for (const auto& [k, v] : options) {
        if (k == name) {
            return v;
        }
    }
    return std::nullopt;
}

void write_usage(std::ostream& stream)
{
    stream <<
        "sqcart - Squared Cartridge archive utility\n"
        "\n"
        "usage: sqcart <command> [options] [arguments]\n"
        "\n"
        "reading:\n"
        "  list <cartridge>            list entries: path, size, compressed, method\n"
        "  show <cartridge>            print the manifest JSON\n"
        "  info <cartridge>            print a human summary of the manifest\n"
        "  cat <cartridge> <entry>     write one entry's bytes to stdout\n"
        "  verify <cartridge>          run the full validation pipeline\n"
        "  digest <cartridge>          print the content digest (spec 9.2)\n"
        "\n"
        "writing:\n"
        "  create <dir> [-o <out>]     seed SQ-INF/manifest.json, then pack\n"
        "  pack <directory> -o <out>   build a cartridge from an exploded tree\n"
        "  unpack <cartridge> [-d D]   materialise the payload into a directory\n"
        "\n"
        "options:\n"
        "  -o, --output=PATH           output path (pack, create)\n"
        "      --kind=KIND             cartridge kind to seed (create)\n"
        "      --id=ID                 manifest id (create; derived otherwise)\n"
        "      --version=V             manifest version (create, default 0.1.0)\n"
        "      --title=T               manifest title (create)\n"
        "      --in-place              create: write the manifest, do not pack\n"
        "      --force                 create: overwrite an existing manifest\n"
        "  -d, --dest=PATH             destination directory (unpack, default '.')\n"
        "      --lenient               open at lenient conformance, not strict\n"
        "      --overwrite             allow unpack to overwrite existing files\n"
        "      --no-hashes             omit SQ-INF/hashes.json when packing\n"
        "  -c, --compress=0-9          deflate level when packing (default 6)\n"
        "                              0 stores; 1 is fastest; 9 is smallest\n"
        "      --store                 synonym for --compress=0\n"
        "      --verify-integrity      check declared digests while opening\n"
        "      --symlinks=MODE         skip (default) | follow | reject\n"
        "                              follow resolves links whose target stays\n"
        "                              inside the cartridge; links escaping it are\n"
        "                              never packed, whatever the mode\n"
        "  -v, --verbose               more detail on stderr\n"
        "  -h, --help                  this text\n"
        "\n"
        "exit codes:\n"
        "  0 success   1 operation failed   2 usage error   3 cartridge non-conforming\n"
        "\n"
        "stdout carries machine-readable results only; diagnostics go to stderr.\n";
}

namespace {

/// Expand short options to their long spellings so dispatch sees one form.
/// `-o out.sq` and `--output=out.sq` are the same invocation by the time a
/// command sees it.
std::optional<std::string> long_name_for_short(char c)
{
    switch (c) {
        case 'o': return "output";
        case 'c': return "compress";
        case 'd': return "dest";
        case 'v': return "verbose";
        case 'h': return "help";
        default:  return std::nullopt;
    }
}

bool option_takes_value(std::string_view name)
{
    return name == "output" || name == "dest" || name == "kind" || name == "id" ||
           name == "version" || name == "title" || name == "symlinks" ||
           name == "compress";
}

}  // namespace

/// Parse argv into an Invocation.
///
/// Deliberately small: this is a utility, not a framework. Anything a user
/// would reasonably type for a jar-like tool is accepted, and anything else
/// is a usage error rather than a silent reinterpretation.
Result<Invocation> parse(std::span<const std::string> args)
{
    Invocation inv;
    bool       positional_only = false;

    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];

        if (positional_only) {
            inv.positional.push_back(a);
            continue;
        }
        if (a == "--") {
            positional_only = true;
            continue;
        }

        if (a.starts_with("--")) {
            const std::string body = a.substr(2);
            const auto        eq   = body.find('=');
            if (eq != std::string::npos) {
                inv.options.emplace_back(body.substr(0, eq), body.substr(eq + 1));
            } else if (option_takes_value(body)) {
                if (i + 1 >= args.size()) {
                    return unexpected(std::string("option --") + body + " requires a value");
                }
                inv.options.emplace_back(body, args[++i]);
            } else {
                inv.flags.push_back(body);
            }
            continue;
        }

        if (a.size() >= 2 && a[0] == '-') {
            // Short options. Clustering ("-vh") is supported for flags; an
            // option taking a value must be last in its cluster.
            for (std::size_t k = 1; k < a.size(); ++k) {
                auto name = long_name_for_short(a[k]);
                if (!name) {
                    return unexpected(std::string("unknown option -") + a[k]);
                }
                if (option_takes_value(*name)) {
                    if (k + 1 < a.size()) {
                        inv.options.emplace_back(*name, a.substr(k + 1));
                    } else if (i + 1 < args.size()) {
                        inv.options.emplace_back(*name, args[++i]);
                    } else {
                        return unexpected("option -" + std::string(1, a[k]) +
                                          " requires a value");
                    }
                    break;
                }
                inv.flags.push_back(*name);
            }
            continue;
        }

        if (inv.command.empty()) {
            inv.command = a;
        } else {
            inv.positional.push_back(a);
        }
    }
    return inv;
}

ExitCode run(std::span<const std::string> args, Environment& env)
{
    auto parsed = parse(args);
    if (!parsed) {
        env.err << "sqcart: " << parsed.error() << "\n\n";
        write_usage(env.err);
        return ExitCode::usage;
    }
    Invocation& inv = *parsed;

    if (inv.has_flag("help") || (inv.command.empty() && args.empty())) {
        write_usage(inv.has_flag("help") ? env.out : env.err);
        return inv.has_flag("help") ? ExitCode::ok : ExitCode::usage;
    }
    if (inv.command.empty()) {
        env.err << "sqcart: no command given\n\n";
        write_usage(env.err);
        return ExitCode::usage;
    }

    static const struct {
        std::string_view name;
        ExitCode (*fn)(const Invocation&, Environment&);
    } kCommands[] = {
        {"list",    &cmd_list},
        {"show",    &cmd_show},
        {"info",    &cmd_info},
        {"cat",     &cmd_cat},
        {"verify",  &cmd_verify},
        {"digest",  &cmd_digest},
        {"unpack",  &cmd_unpack},
        {"extract", &cmd_unpack},   // accepted alias for the former spelling
        {"pack",    &cmd_pack},
        {"create",  &cmd_create},
    };

    for (const auto& c : kCommands) {
        if (c.name == inv.command) {
            return c.fn(inv, env);
        }
    }

    env.err << "sqcart: unknown command '" << inv.command << "'\n\n";
    write_usage(env.err);
    return ExitCode::usage;
}

}  // namespace sqcart::app
