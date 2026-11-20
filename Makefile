# The build, for every host: Termux, Linux, macOS (with Homebrew's LLVM).
#
#   make            build the engine, the CLI and the tests
#   make check      build and run the tests
#   make smoke      generate a project and build it, end to end
#   make clean
#
# clang and libc++ new enough for `import std;` (19 or later; 23 is what
# sqterm and CI use), C++23: the engine reads cartridges through sq.cart
# (sqcart 3.0), whose C++23 modules sqcart/v3's own Makefile builds, with
# libc++'s std module. Only engine/src/services/cartridge.cpp imports them.
#
#   make CXX=/path/to/clang++
#
# Offline throughout (§1.13): everything it compiles is already in the tree.

ifeq ($(origin CXX),default)
CXX      := clang++
endif
ifeq ($(origin CC),default)
CC       := $(patsubst %clang++,%clang,$(CXX))
endif
CXXSTD   ?= c++23
STDLIB   := -stdlib=libc++
OPT      ?= -O1 -g
WARN     := -Wall -Wextra -Wpedantic

BUILD    := build

THIRD    := third-party
YYJSON   := $(THIRD)/yyjson-0.12.0/src
SHA256   := $(THIRD)/crypto-algorithms
LUA      := $(THIRD)/lua-5.4.8/src

# sq.cart: sqcart/v3, built in its own build folder by its own Makefile.
SQCART   := sqcart/v3
SQB      := $(SQCART)/build
SQCART_LIBS := $(SQB)/libsqcart.a $(SQB)/libsqcart-unzstd.a
SQCART_MODS := -fprebuilt-module-path=$(SQB)/pcm -fmodule-file=std=$(SQB)/pcm/std.pcm

SQ_VERSION := $(shell cat VERSION 2>/dev/null || echo 0.0.0)

DEFS     := -DSQUARED_PG_VERSION=\"$(SQ_VERSION)\"
INC      := -Iengine/include -Iengine/src -Iengine/lua/include \
            -isystem $(YYJSON) -isystem $(SHA256) -isystem $(LUA)

# -MMD -MP: header dependency tracking. Without it, editing a header does not
# rebuild the objects that include it, and you debug a stale binary while
# reading the corrected source. The generated project's Makefile had this from
# the start; this one did not, and it cost an afternoon.
DEPFLAGS := -MMD -MP

CXXFLAGS := -std=$(CXXSTD) $(STDLIB) $(OPT) $(WARN) $(DEFS) $(INC) $(DEPFLAGS)
CFLAGS   := $(OPT) -w $(INC) $(DEPFLAGS)
LDFLAGS  := $(STDLIB) -lpthread

# --- sources ---------------------------------------------------------------

ENGINE_SRC := engine/src/value.cpp engine/src/version.cpp engine/src/identity.cpp \
              engine/src/error.cpp engine/src/engine.cpp engine/src/support/support.cpp \
              engine/src/services/filesystem_service.cpp \
              engine/src/services/resource_service.cpp \
              engine/src/services/resolve_services.cpp \
              engine/src/services/validation_service.cpp \
              engine/src/services/project_service.cpp \
              engine/src/services/transaction_service.cpp \
              engine/src/services/cartridge.cpp

BINDING_SRC := engine/lua/src/bindings.cpp

# yyjson's code is in libsqcart.a (sq.data's, the same 0.12.0): one copy.
C_SRC    := $(SHA256)/sha256.c

# Everything except the standalone interpreter and compiler drivers.
LUA_SRC  := $(filter-out $(LUA)/lua.c $(LUA)/luac.c,$(wildcard $(LUA)/*.c))

ENGINE_OBJ  := $(patsubst %,$(BUILD)/%.o,$(ENGINE_SRC))
BINDING_OBJ := $(patsubst %,$(BUILD)/%.o,$(BINDING_SRC))
C_OBJ       := $(patsubst %,$(BUILD)/%.o,$(C_SRC))
LUA_OBJ     := $(patsubst %,$(BUILD)/%.o,$(LUA_SRC))

TESTS    := test_version test_identity test_value test_support test_lifecycle test_generate \
            test_android test_cli
TEST_BIN := $(patsubst %,$(BUILD)/%,$(TESTS))

# Components assembled by tools/bootstrap.sh.
#
# Checked before anything is compiled. Without this the first symptom of an
# un-assembled tree is a compiler error about a missing sq.cart module, which
# names the wrong problem and sends the reader into the include paths.
ifeq ($(wildcard $(SQCART)/src/sq.cart.cppm),)
$(error sqcart/ is not present. Run ./tools/bootstrap.sh to assemble this tree)
endif

.PHONY: all check smoke clean sqcart components
all: $(BUILD)/sqpg $(TEST_BIN) sqcart

# What is assembled, and what it is pinned to.
components:
	@./tools/bootstrap.sh --check

# sqcart's CLI travels with sqpg (D-055: a fresh clone must be able to
# author a cartridge without $SQCART set by hand). sqcart/v3 is a project
# with its own build; a sub-make keeps its link lines in one place.
# sq.cart's libraries, its modules and sqcart3, by sqcart/v3's own Makefile.
# Always asked (it knows what is up to date); the engine waits for it.
sqcart:
	@$(MAKE) --no-print-directory -C $(SQCART) CXX='$(CXX)' $(patsubst $(SQCART)/%,%,$(SQCART_LIBS)) build/sqcart3

$(SQCART_LIBS): sqcart

# The one source that imports sq.cart.
$(BUILD)/engine/src/services/cartridge.cpp.o: engine/src/services/cartridge.cpp $(SQCART_LIBS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(SQCART_MODS) -c $< -o $@

$(BUILD)/%.cpp.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -DLUA_USE_POSIX -c $< -o $@

$(BUILD)/liblua.a: $(LUA_OBJ)
	ar rcs $@ $^

$(BUILD)/libsquaredpg.a: $(ENGINE_OBJ) $(C_OBJ)
	ar rcs $@ $^

# Inputs are named explicitly rather than with $^.
#
# $^ is every prerequisite, and once the .d files below are included, each
# link target gains its headers as prerequisites -- so $^ would expand to
# include .hpp files and clang refuses with "cannot specify -o when generating
# multiple output files". It only appears on an *incremental* build, because a
# clean tree has no .d files when the makefile is parsed, which is exactly the
# kind of bug that passes locally and fails on the next machine.
$(BUILD)/sqpg: app/src/main.cpp $(BINDING_OBJ) $(BUILD)/libsquaredpg.a $(BUILD)/liblua.a
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) app/src/main.cpp $(BINDING_OBJ) \
	    $(BUILD)/libsquaredpg.a $(BUILD)/liblua.a $(SQCART_LIBS) $(LDFLAGS) -o $@

# Tests reach the real resources/ tree rather than a fixture copy: the point of
# these tests is that the shipped template and kits generate correctly.
$(BUILD)/test_cli: $(BUILD)/sqpg

$(BUILD)/test_%: engine/tests/test_%.cpp $(BUILD)/libsquaredpg.a
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -DSQUARED_PG_TEST_SOURCE_DIR=\"$(CURDIR)\" -Iengine/tests \
	    $< $(BUILD)/libsquaredpg.a $(SQCART_LIBS) $(LDFLAGS) -o $@

check: all
	@fail=0; for t in $(TEST_BIN); do \
	  if ! ./$$t; then fail=1; fi; \
	done; \
	if [ $$fail -ne 0 ]; then echo "FAILURES"; exit 1; fi; \
	echo "all engine tests passed"

smoke: all
	@sh tools/smoke.sh

clean:
	rm -rf $(BUILD)
	@$(MAKE) --no-print-directory -C $(SQCART) clean

DEPS := $(shell find $(BUILD) -name '*.d' 2>/dev/null)
-include $(DEPS)
