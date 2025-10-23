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

VFSSHELL_SRC := VfsShell/vfs_common.cpp VfsShell/tag_system.cpp VfsShell/logic_engine.cpp VfsShell/vfs_core.cpp VfsShell/vfs_mount.cpp VfsShell/sexp.cpp VfsShell/cpp_ast.cpp VfsShell/clang_parser.cpp VfsShell/planner.cpp VfsShell/ai_bridge.cpp VfsShell/context_builder.cpp VfsShell/build_graph.cpp VfsShell/make.cpp VfsShell/hypothesis.cpp VfsShell/scope_store.cpp VfsShell/feedback.cpp VfsShell/shell_commands.cpp VfsShell/repl.cpp VfsShell/main.cpp VfsShell/snippet_catalog.cpp VfsShell/utils.cpp VfsShell/web_server.cpp VfsShell/upp_assembly.cpp VfsShell/upp_builder.cpp VfsShell/upp_workspace_build.cpp VfsShell/command.cpp VfsShell/daemon.cpp VfsShell/registry.cpp VfsShell/qwen_protocol.cpp VfsShell/qwen_client.cpp VfsShell/qwen_state_manager.cpp VfsShell/cmd_qwen.cpp
VFSSHELL_HDR := VfsShell/vfs_common.h VfsShell/tag_system.h VfsShell/logic_engine.h VfsShell/vfs_core.h VfsShell/vfs_mount.h VfsShell/sexp.h VfsShell/cpp_ast.h VfsShell/clang_parser.h VfsShell/planner.h VfsShell/ai_bridge.h VfsShell/context_builder.h VfsShell/build_graph.h VfsShell/make.h VfsShell/hypothesis.h VfsShell/scope_store.h VfsShell/feedback.h VfsShell/shell_commands.h VfsShell/repl.h VfsShell/snippet_catalog.h VfsShell/utils.h VfsShell/upp_assembly.h VfsShell/upp_builder.h VfsShell/upp_workspace_build.h VfsShell/registry.h VfsShell/qwen_protocol.h VfsShell/qwen_client.h VfsShell/qwen_state_manager.h VfsShell/cmd_qwen.h
VFSSHELL_BIN := vfsh

HARNESS_SRC := harness/scenario.cpp harness/runner.cpp
HARNESS_HDR := harness/scenario.h harness/runner.h
PLANNER_DEMO_BIN := planner_demo
PLANNER_TRAIN_BIN := planner_train

# Qwen test binaries
QWEN_PROTOCOL_TEST_BIN := qwen_protocol_test
QWEN_CLIENT_TEST_BIN := qwen_client_test
QWEN_STATE_MANAGER_TEST_BIN := qwen_state_manager_test
QWEN_ECHO_SERVER_BIN := qwen_echo_server

.PHONY: all clean debug release sample test-lib planner-demo planner-train qwen-tests qwen-protocol-test qwen-client-test qwen-state-manager-test qwen-echo-server

all: $(VFSSHELL_BIN)

planner-demo: $(PLANNER_DEMO_BIN)

planner-train: $(PLANNER_TRAIN_BIN)

$(VFSSHELL_BIN): $(VFSSHELL_SRC) $(VFSSHELL_HDR)
	$(CXX) $(CXXFLAGS) $(VFSSHELL_SRC) -o $@ $(LDFLAGS)

$(PLANNER_DEMO_BIN): harness/demo.cpp $(HARNESS_SRC) $(HARNESS_HDR) $(VFSSHELL_SRC) $(VFSSHELL_HDR)
	$(CXX) $(CXXFLAGS) -DCODEX_NO_MAIN harness/demo.cpp $(HARNESS_SRC) $(VFSSHELL_SRC) -o $@ $(LDFLAGS)

$(PLANNER_TRAIN_BIN): harness/train.cpp $(HARNESS_SRC) $(HARNESS_HDR) $(VFSSHELL_SRC) $(VFSSHELL_HDR)
	$(CXX) $(CXXFLAGS) -DCODEX_NO_MAIN harness/train.cpp $(HARNESS_SRC) $(VFSSHELL_SRC) -o $@ $(LDFLAGS)

# Qwen test binaries
qwen-tests: $(QWEN_STATE_MANAGER_TEST_BIN)

qwen-state-manager-test: $(QWEN_STATE_MANAGER_TEST_BIN)

# Filter out main.cpp from VFSSHELL_SRC for test binaries
VFSSHELL_SRC_NO_MAIN := $(filter-out VfsShell/main.cpp, $(VFSSHELL_SRC))

$(QWEN_STATE_MANAGER_TEST_BIN): VfsShell/qwen_state_manager_test.cpp $(VFSSHELL_SRC) $(VFSSHELL_HDR)
	$(CXX) $(CXXFLAGS) -DCODEX_NO_MAIN -DQWEN_TESTS_ENABLED VfsShell/qwen_state_manager_test.cpp $(VFSSHELL_SRC_NO_MAIN) -o $@ $(LDFLAGS)

qwen-integration-test: VfsShell/qwen_integration_test.cpp $(VFSSHELL_SRC) $(VFSSHELL_HDR)
	$(CXX) $(CXXFLAGS) -DCODEX_NO_MAIN -DQWEN_TESTS_ENABLED VfsShell/qwen_integration_test.cpp $(VFSSHELL_SRC_NO_MAIN) -o $@ $(LDFLAGS)

# Add other qwen test binaries
$(QWEN_PROTOCOL_TEST_BIN): VfsShell/qwen_protocol_test.cpp $(VFSSHELL_SRC) $(VFSSHELL_HDR)
	$(CXX) $(CXXFLAGS) -DCODEX_NO_MAIN -DQWEN_TESTS_ENABLED VfsShell/qwen_protocol_test.cpp $(VFSSHELL_SRC_NO_MAIN) -o $@ $(LDFLAGS)

$(QWEN_CLIENT_TEST_BIN): VfsShell/qwen_client_test.cpp $(VFSSHELL_SRC) $(VFSSHELL_HDR)
	$(CXX) $(CXXFLAGS) -DCODEX_NO_MAIN -DQWEN_TESTS_ENABLED VfsShell/qwen_client_test.cpp $(VFSSHELL_SRC_NO_MAIN) -o $@ $(LDFLAGS)

$(QWEN_ECHO_SERVER_BIN): VfsShell/qwen_echo_server.cpp $(VFSSHELL_SRC) $(VFSSHELL_HDR)
	$(CXX) $(CXXFLAGS) -DCODEX_NO_MAIN -DQWEN_TESTS_ENABLED VfsShell/qwen_echo_server.cpp $(VFSSHELL_SRC_NO_MAIN) -o $@ $(LDFLAGS)

clean:
	rm -f $(VFSSHELL_BIN) $(PLANNER_DEMO_BIN) $(PLANNER_TRAIN_BIN) $(QWEN_STATE_MANAGER_TEST_BIN)
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
		"cpp.print /astcpp/demo/main Hello from VfsBoot sample!" \
		"cpp.returni /astcpp/demo/main 0" \
		"cpp.dump /astcpp/demo /cpp/demo.cpp" \
		"export /cpp/demo.cpp $(SAMPLE_CPP)" \
		"exit" \
	| ./$(VFSSHELL_BIN)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(SAMPLE_CPP) -o $(SAMPLE_BIN)
	./$(SAMPLE_BIN) > $(SAMPLE_OUT)
	@grep -q "Hello from VfsBoot sample!" $(SAMPLE_OUT)

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
