CXX ?= c++
LLVM_CONFIG ?= /usr/lib/llvm/21/bin/llvm-config
LIBCLANG_CFLAGS := -I/usr/lib/llvm/21/include
LIBCLANG_LDFLAGS := -L/usr/lib/llvm/21/lib64 -lclang
CXXFLAGS ?= -std=gnu++17 -O2 -Wall -Wextra -pedantic $(shell pkg-config --cflags libsvn_delta libsvn_subr) $(LIBCLANG_CFLAGS)
LDFLAGS ?= -lblake3 $(shell pkg-config --libs libsvn_delta libsvn_subr) $(LIBCLANG_LDFLAGS) -lwebsockets -lpthread

VFSSHELL_SRC := VfsShell/codex.cpp VfsShell/snippet_catalog.cpp VfsShell/utils.cpp VfsShell/clang_parser.cpp VfsShell/web_server.cpp
VFSSHELL_HDR := VfsShell/codex.h VfsShell/snippet_catalog.h VfsShell/utils.h
VFSSHELL_BIN := codex

HARNESS_SRC := harness/scenario.cpp harness/runner.cpp
HARNESS_HDR := harness/scenario.h harness/runner.h
PLANNER_DEMO_BIN := planner_demo
PLANNER_TRAIN_BIN := planner_train

.PHONY: all clean debug release sample test-lib planner-demo planner-train

all: $(VFSSHELL_BIN)

planner-demo: $(PLANNER_DEMO_BIN)

planner-train: $(PLANNER_TRAIN_BIN)

$(VFSSHELL_BIN): $(VFSSHELL_SRC) $(VFSSHELL_HDR)
	$(CXX) $(CXXFLAGS) $(VFSSHELL_SRC) -o $@ $(LDFLAGS)

$(PLANNER_DEMO_BIN): harness/demo.cpp $(HARNESS_SRC) $(HARNESS_HDR) $(VFSSHELL_SRC) $(VFSSHELL_HDR)
	$(CXX) $(CXXFLAGS) -DCODEX_NO_MAIN harness/demo.cpp $(HARNESS_SRC) $(VFSSHELL_SRC) -o $@ $(LDFLAGS)

$(PLANNER_TRAIN_BIN): harness/train.cpp $(HARNESS_SRC) $(HARNESS_HDR) $(VFSSHELL_SRC) $(VFSSHELL_HDR)
	$(CXX) $(CXXFLAGS) -DCODEX_NO_MAIN harness/train.cpp $(HARNESS_SRC) $(VFSSHELL_SRC) -o $@ $(LDFLAGS)

clean:
	rm -f $(VFSSHELL_BIN) $(PLANNER_DEMO_BIN) $(PLANNER_TRAIN_BIN)
	rm -rf build

BUILD_DIR := build
SAMPLE_CPP := $(BUILD_DIR)/demo.cpp
SAMPLE_BIN := $(BUILD_DIR)/demo
SAMPLE_OUT := $(BUILD_DIR)/demo.out

sample: $(VFSSHELL_BIN)
	mkdir -p $(BUILD_DIR)
	printf '%s\n' \
		"cpp.tu /astcpp/demo" \
		"cpp.include /astcpp/demo iostream 1" \
		"cpp.func /astcpp/demo main int" \
		"cpp.print /astcpp/demo/main Hello from codex-mini sample!" \
		"cpp.returni /astcpp/demo/main 0" \
		"cpp.dump /astcpp/demo /cpp/demo.cpp" \
		"export /cpp/demo.cpp $(SAMPLE_CPP)" \
		"exit" \
	| ./$(VFSSHELL_BIN)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(SAMPLE_CPP) -o $(SAMPLE_BIN)
	./$(SAMPLE_BIN) > $(SAMPLE_OUT)
	@grep -q "Hello from codex-mini sample!" $(SAMPLE_OUT)

# Convenience configuration targets
release: CXXFLAGS += -DNDEBUG -O3
release: clean all

debug: CXXFLAGS += -O0 -g
debug: clean all

# Test library
TEST_LIB := tests/libtest.so
test-lib: $(TEST_LIB)

$(TEST_LIB): tests/test_lib.c
	$(CC) -shared -fPIC -o $@ $<
