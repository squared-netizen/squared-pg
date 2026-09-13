# kit.{{project_name}} — build fragment.
#
# Included automatically: the workspace Makefile picks up mk/kit_*.mk by
# wildcard, so this fragment is live the moment the kit is installed. The name
# carries the kit's own name so two kits never collide.
#
# A kit ADDS to a workspace's build; it does not reorganise it
# (Generator Architecture 2.9). Two rules that came from real build failures:
#
#   - additional sysroots go in with -idirafter, never -I. -I prepends and
#     will displace the host's own C headers, at which point libc++ stops
#     finding them and the error appears far from its cause.
#   - name libraries by path rather than adding -L and using -l. An added -L
#     changes resolution for every -l in the link, including ones this kit
#     knows nothing about.
#
# The test: if your fragment only works when it comes first, it is
# reorganising the toolchain rather than adding to it.
#
# ## The include root (D-061)
#
# Headers go under sq_kit/include/{{project_name}}/ and the flag names the
# include ROOT, not the kit's directory inside it:
#
#     -Isq_kit/include            correct
#     -Isq_kit/{{project_name}}/include   wrong
#
# The distinction is not cosmetic. The flag must point at the root so the
# include reads <{{project_name}}/header.hpp>; a kit-name segment inside the
# root is what keeps two kits' headers from colliding with each other and
# with sq_app's. Point -I one level deeper and every kit's headers land in
# one flat search space, where a shared filename resolves silently by -I
# order with no diagnostic.
#
# Every kit adds this same root, so with several kits the flag appears more
# than once on the command line. Harmless, and it is the workspace's job to
# deduplicate, not this fragment's.

# Kits contribute through accumulators, never by touching the workspace's own
# variables. The template reads these after every mk/kit_*.mk has been
# included, so appending here is all a kit has to do:
#
#   SQ_KIT_CPPFLAGS   compile flags and include paths
#   SQ_KIT_LDFLAGS    link flags
#   SQ_KIT_LDLIBS     libraries, named by path
#   SQ_KIT_SRC        sources for the workspace to compile
#   SQ_KITS_PRESENT   this kit's identity, for diagnostics
#
# Appending to CXXFLAGS or LDLIBS directly would work by accident and break the
# moment the template reorders anything.

SQ_KIT_CPPFLAGS += -Isq_kit/include
SQ_KITS_PRESENT += kit.{{project_name}}

# --- an external dependency, if this kit wraps one -------------------------
#
# -idirafter, never -I: -I prepends, and an additional sysroot placed ahead of
# libc++ makes <cctype> find the wrong ctype.h, with the error appearing far
# from its cause.
#
# SQ_KIT_CPPFLAGS += -idirafter /usr/include/{{project_name}}
# SQ_KIT_LDLIBS   += /usr/lib/lib{{project_name}}.a

# --- sources this kit contributes, if any ----------------------------------
#
# A kit may ship .cpp files for the workspace to compile, rather than headers
# alone or a prebuilt archive. They are compiled with the workspace's own
# flags, including its warning set, so a library that does not build cleanly
# under -Wall -Wextra -Wpedantic will need attention here.
#
# SQ_KIT_SRC += sq_kit/include/{{project_name}}/{{project_name}}.cpp
