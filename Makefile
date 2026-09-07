# Hand-written build, for hosts without CMake — Termux, chiefly — and for quick
# iteration. CMakeLists.txt is the supported build; this must stay
# behaviourally identical to it.
#
#   make            build the engine, the CLI and the tests
#   make check      build and run the tests
#   make smoke      generate a project and build it, end to end
#   make clean
#
# Offline throughout (§1.13): everything it compiles is already in the tree.

CXX      ?= g++
CC       ?= gcc
CXXSTD   ?= c++20
OPT      ?= -O1 -g
WARN     := -Wall -Wextra -Wpedantic

BUILD    := build
THIRD    := third-party

MINIZ    := $(THIRD)/miniz-3.1.2
YYJSON   := $(THIRD)/yyjson-0.12.0/src
SHA256   := $(THIRD)/crypto-algorithms
LUA      := $(THIRD)/lua-5.4.8/src

SQ_VERSION := $(shell cat VERSION 2>/dev/null || echo 0.0.0)

DEFS     := -DSQCART_ENABLE_WRITER=1 -DSQUARED_PG_VERSION=\"$(SQ_VERSION)\"
INC      := -Iengine/include -Iengine/src -Iengine/lua/include -Isqcart/include -Isqcart/src \
            -isystem $(MINIZ) -isystem $(YYJSON) -isystem $(SHA256) -isystem $(LUA)

# -MMD -MP: header dependency tracking. Without it, editing a header does not
# rebuild the objects that include it, and you debug a stale binary while
# reading the corrected source. The generated project's Makefile had this from
# the start; this one did not, and it cost an afternoon.
DEPFLAGS := -MMD -MP

CXXFLAGS := -std=$(CXXSTD) $(OPT) $(WARN) $(DEFS) $(INC) $(DEPFLAGS)
CFLAGS   := $(OPT) -w $(INC) $(DEPFLAGS)

# --- sources ---------------------------------------------------------------

ENGINE_SRC := engine/src/value.cpp engine/src/version.cpp engine/src/identity.cpp \
              engine/src/error.cpp engine/src/engine.cpp engine/src/support/support.cpp \
              engine/src/services/filesystem_service.cpp \
              engine/src/services/resource_service.cpp \
              engine/src/services/resolve_services.cpp \
              engine/src/services/validation_service.cpp \
              engine/src/services/project_service.cpp \
              engine/src/services/transaction_service.cpp

BINDING_SRC := engine/lua/src/bindings.cpp

SQCART_SRC := sqcart/src/errors.cpp sqcart/src/path.cpp \
              sqcart/src/manifest.cpp sqcart/src/symlink.cpp sqcart/src/reader.cpp \
              sqcart/src/digest.cpp sqcart/src/validate.cpp sqcart/src/extract.cpp \
              sqcart/src/sha256.cpp sqcart/src/writer.cpp

# One copy of miniz and yyjson for the whole build, shared with sqcart, exactly
# as the CMake build arranges through SQCART_USE_EXTERNAL_* (§1.6).
C_SRC    := $(MINIZ)/miniz.c $(MINIZ)/miniz_tdef.c $(MINIZ)/miniz_tinfl.c $(MINIZ)/miniz_zip.c \
            $(YYJSON)/yyjson.c $(SHA256)/sha256.c

# Everything except the standalone interpreter and compiler drivers.
LUA_SRC  := $(filter-out $(LUA)/lua.c $(LUA)/luac.c,$(wildcard $(LUA)/*.c))

ENGINE_OBJ  := $(patsubst %,$(BUILD)/%.o,$(ENGINE_SRC))
BINDING_OBJ := $(patsubst %,$(BUILD)/%.o,$(BINDING_SRC))
SQCART_OBJ  := $(patsubst %,$(BUILD)/%.o,$(SQCART_SRC))
C_OBJ       := $(patsubst %,$(BUILD)/%.o,$(C_SRC))
LUA_OBJ     := $(patsubst %,$(BUILD)/%.o,$(LUA_SRC))

TESTS    := test_version test_identity test_value test_support test_lifecycle test_generate \
            test_android
TEST_BIN := $(patsubst %,$(BUILD)/%,$(TESTS))

.PHONY: all check smoke clean
all: $(BUILD)/sqpg $(TEST_BIN)

$(BUILD)/%.cpp.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -DLUA_USE_POSIX -c $< -o $@

$(BUILD)/liblua.a: $(LUA_OBJ)
	ar rcs $@ $^

$(BUILD)/libsquaredpg.a: $(ENGINE_OBJ) $(SQCART_OBJ) $(C_OBJ)
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
	    $(BUILD)/libsquaredpg.a $(BUILD)/liblua.a -o $@

# Tests reach the real resources/ tree rather than a fixture copy: the point of
# these tests is that the shipped template and kits generate correctly.
$(BUILD)/test_%: engine/tests/test_%.cpp $(BUILD)/libsquaredpg.a
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -DSQUARED_PG_TEST_SOURCE_DIR=\"$(CURDIR)\" -Iengine/tests \
	    $< $(BUILD)/libsquaredpg.a -o $@

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

DEPS := $(shell find $(BUILD) -name '*.d' 2>/dev/null)
-include $(DEPS)
