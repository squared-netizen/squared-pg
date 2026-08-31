# Hand-written build, for environments without CMake (and for quick iteration
# on Termux). CMakeLists.txt is the supported build; this is a convenience
# that must stay behaviourally identical.
#
#   make            build library, CLI and tests
#   make check      build and run the tests
#   make clean

CXX      ?= g++
CC       ?= gcc
CXXSTD   ?= c++20
OPT      ?= -O1 -g
WARN     := -Wall -Wextra -Wpedantic
DEFS     := -DSQCART_ENABLE_WRITER=1 -DSQCART_TEST_SOURCE_DIR=\"$(CURDIR)\"

INC      := -Iinclude -Isrc -isystem third_party/miniz-3.1.2 \
            -isystem third_party/yyjson-0.12.0/src -isystem third_party/crypto-algorithms
APPINC   := -Iapp/include -Iapp/src

BUILD    := build
CXXFLAGS := -std=$(CXXSTD) $(OPT) $(WARN) $(DEFS) $(INC)
CFLAGS   := $(OPT) -w $(DEFS) $(INC)

LIB_SRC  := src/errors.cpp src/path.cpp src/glob.cpp src/manifest.cpp src/symlink.cpp \
            src/reader.cpp src/digest.cpp src/validate.cpp src/extract.cpp \
            src/sha256.cpp src/writer.cpp
APP_SRC  := app/src/cli.cpp app/src/commands_read.cpp app/src/commands_write.cpp \
            app/src/cmd_create.cpp app/src/seeds.cpp
C_SRC    := third_party/miniz-3.1.2/miniz.c third_party/miniz-3.1.2/miniz_tdef.c \
            third_party/miniz-3.1.2/miniz_tinfl.c third_party/miniz-3.1.2/miniz_zip.c \
            third_party/yyjson-0.12.0/src/yyjson.c \
            third_party/crypto-algorithms/sha256.c

LIB_OBJ  := $(patsubst %,$(BUILD)/%.o,$(LIB_SRC))
APP_OBJ  := $(patsubst %,$(BUILD)/%.o,$(APP_SRC))
C_OBJ    := $(patsubst %,$(BUILD)/%.o,$(C_SRC))

TESTS    := test_errors test_expected test_path test_manifest test_open test_write test_conformance test_seeds test_symlinks \
            test_digest test_validate test_extract test_exploded
APPTESTS := test_cli
TEST_BIN := $(patsubst %,$(BUILD)/%,$(TESTS)) $(patsubst %,$(BUILD)/%,$(APPTESTS))

.PHONY: all check clean
all: $(BUILD)/sqcart $(TEST_BIN)

$(BUILD)/%.cpp.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(APPINC) -c $< -o $@

$(BUILD)/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/libsqcart.a: $(LIB_OBJ) $(C_OBJ)
	ar rcs $@ $^

$(BUILD)/sqcart: app/src/main.cpp $(APP_OBJ) $(BUILD)/libsqcart.a
	$(CXX) $(CXXFLAGS) $(APPINC) $^ -o $@

$(BUILD)/test_cli: app/tests/test_cli.cpp $(APP_OBJ) $(BUILD)/libsqcart.a
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $(APPINC) -Itests $^ -o $@

$(BUILD)/test_seeds: tests/test_seeds.cpp $(BUILD)/app/src/seeds.cpp.o $(BUILD)/libsqcart.a
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $(APPINC) -Itests $^ -o $@

$(BUILD)/test_%: tests/test_%.cpp $(BUILD)/libsqcart.a
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -Itests $^ -o $@

check: all
	@fail=0; for t in $(TEST_BIN); do \
	  if ! ./$$t; then fail=1; fi; \
	done; \
	if [ $$fail -ne 0 ]; then echo "FAILURES"; exit 1; fi; \
	echo "all tests passed"

clean:
	rm -rf $(BUILD)
