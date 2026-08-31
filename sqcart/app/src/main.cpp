// SPDX-License-Identifier: MIT
//
// The only translation unit in the CLI that knows a terminal exists.
//
// Everything it does is bind the real streams into an Environment and hand
// off. If this file grows past a screen, logic has leaked out of the
// commands and into the shell boundary, which is exactly what the
// Environment seam exists to prevent.

#include "sqcart/app/cli.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#define SQCART_HAVE_ISATTY 1
#endif

int main(int argc, char** argv)
{
    std::vector<std::string> args;
    args.reserve(static_cast<std::size_t>(argc > 1 ? argc - 1 : 0));
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    std::error_code ec;
    sqcart::app::Environment env{std::cout, std::cerr, std::cin,
                                 std::filesystem::current_path(ec), false};
#ifdef SQCART_HAVE_ISATTY
    env.out_is_tty = ::isatty(STDOUT_FILENO) != 0;
#endif

    // `cat` writes raw bytes; stdout must not be line-buffered or translated.
    std::ios::sync_with_stdio(false);

    return sqcart::app::to_int(sqcart::app::run(args, env));
}
