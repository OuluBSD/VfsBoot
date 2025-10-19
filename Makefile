CXX ?= c++
LLVM_CONFIG ?= /usr/lib/llvm/21/bin/llvm-config
LIBCLANG_CFLAGS := -I/usr/lib/llvm/21/include
LIBCLANG_LDFLAGS := -L/usr/lib/llvm/21/lib64 -lclang
CXXFLAGS ?= -std=gnu++17 -O2 -Wall -Wextra -pedantic $(shell pkg-config --cflags libsvn_delta libsvn_subr) $(LIBCLANG_CFLAGS)
LDFLAGS ?= -lblake3 $(shell pkg-config --libs libsvn_delta libsvn_subr) $(LIBCLANG_LDFLAGS) -lwebsockets -lpthread

# Check for ncurses
NCURSES_CFLAGS := $(shell pkg-config --cflags ncurses 2>/dev/null || echo "")
NCURSES_LDFLAGS := $(shell pkg-config --libs ncurses 2>/dev/null || echo "-lncurses")

# Add ncurses flags if available
ifneq ($(NCURSES_CFLAGS),)
    CXXFLAGS += $(NCURSES_CFLAGS) -DCODEX_UI_NCURSES
    LDFLAGS += $(NCURSES_LDFLAGS)
endif

VFSSHELL_SRC := VfsShell/vfs_common.cpp VfsShell/tag_system.cpp VfsShell/logic_engine.cpp VfsShell/vfs_core.cpp VfsShell/vfs_mount.cpp VfsShell/sexp.cpp VfsShell/cpp_ast.cpp VfsShell/clang_parser.cpp VfsShell/planner.cpp VfsShell/ai_bridge.cpp VfsShell/context_builder.cpp VfsShell/make.cpp VfsShell/hypothesis.cpp VfsShell/scope_store.cpp VfsShell/feedback.cpp VfsShell/shell_commands.cpp VfsShell/repl.cpp VfsShell/main.cpp VfsShell/snippet_catalog.cpp VfsShell/utils.cpp VfsShell/web_server.cpp VfsShell/upp_assembly.cpp VfsShell/upp_builder.cpp VfsShell/command.cpp VfsShell/daemon.cpp VfsShell/registry.cpp
VFSSHELL_HDR := VfsShell/vfs_common.h VfsShell/tag_system.h VfsShell/logic_engine.h VfsShell/vfs_core.h VfsShell/vfs_mount.h VfsShell/sexp.h VfsShell/cpp_ast.h VfsShell/clang_parser.h VfsShell/planner.h VfsShell/ai_bridge.h VfsShell/context_builder.h VfsShell/make.h VfsShell/hypothesis.h VfsShell/scope_store.h VfsShell/feedback.h VfsShell/shell_commands.h VfsShell/repl.h VfsShell/snippet_catalog.h VfsShell/utils.h VfsShell/upp_assembly.h VfsShell/upp_builder.h VfsShell/registry.h
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
