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

VFSSHELL_SRC := src/VfsShell/vfs_common.cpp src/VfsShell/tag_system.cpp src/VfsShell/logic_engine.cpp src/VfsShell/vfs_core.cpp src/VfsShell/vfs_mount.cpp src/VfsShell/sexp.cpp src/VfsShell/cpp_ast.cpp src/VfsShell/clang_parser.cpp src/VfsShell/planner.cpp src/VfsShell/ai_bridge.cpp src/VfsShell/context_builder.cpp src/VfsShell/build_graph.cpp src/VfsShell/make.cpp src/VfsShell/hypothesis.cpp src/VfsShell/scope_store.cpp src/VfsShell/feedback.cpp src/VfsShell/shell_commands.cpp src/VfsShell/repl.cpp src/VfsShell/main.cpp src/VfsShell/snippet_catalog.cpp src/VfsShell/utils.cpp src/VfsShell/web_server.cpp src/VfsShell/upp_assembly.cpp src/VfsShell/upp_builder.cpp src/VfsShell/upp_workspace_build.cpp src/VfsShell/command.cpp src/VfsShell/daemon.cpp src/VfsShell/registry.cpp src/VfsShell/qwen_protocol.cpp src/VfsShell/qwen_client.cpp src/VfsShell/qwen_state_manager.cpp src/VfsShell/qwen_manager.cpp src/VfsShell/qwen_tcp_server.cpp src/VfsShell/cmd_qwen.cpp
VFSSHELL_HDR := src/VfsShell/vfs_common.h src/VfsShell/tag_system.h src/VfsShell/logic_engine.h src/VfsShell/vfs_core.h src/VfsShell/vfs_mount.h src/VfsShell/sexp.h src/VfsShell/cpp_ast.h src/VfsShell/clang_parser.h src/VfsShell/planner.h src/VfsShell/ai_bridge.h src/VfsShell/context_builder.h src/VfsShell/build_graph.h src/VfsShell/make.h src/VfsShell/hypothesis.h src/VfsShell/scope_store.h src/VfsShell/feedback.h src/VfsShell/shell_commands.h src/VfsShell/repl.h src/VfsShell/snippet_catalog.h src/VfsShell/utils.h src/VfsShell/upp_assembly.h src/VfsShell/upp_builder.h src/VfsShell/upp_workspace_build.h src/VfsShell/registry.h src/VfsShell/qwen_protocol.h src/VfsShell/qwen_client.h src/VfsShell/qwen_state_manager.h src/VfsShell/qwen_manager.h src/VfsShell/qwen_tcp_server.h src/VfsShell/cmd_qwen.h
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
VFSSHELL_SRC_NO_MAIN := $(filter-out src/VfsShell/main.cpp, $(VFSSHELL_SRC))

$(QWEN_STATE_MANAGER_TEST_BIN): src/VfsShell/qwen_state_manager_test.cpp $(VFSSHELL_SRC) $(VFSSHELL_HDR)
	$(CXX) $(CXXFLAGS) -DCODEX_NO_MAIN -DQWEN_TESTS_ENABLED src/VfsShell/qwen_state_manager_test.cpp $(VFSSHELL_SRC_NO_MAIN) -o $@ $(LDFLAGS)

qwen-integration-test: src/VfsShell/qwen_integration_test.cpp $(VFSSHELL_SRC) $(VFSSHELL_HDR)
	$(CXX) $(CXXFLAGS) -DCODEX_NO_MAIN -DQWEN_TESTS_ENABLED src/VfsShell/qwen_integration_test.cpp $(VFSSHELL_SRC_NO_MAIN) -o $@ $(LDFLAGS)

# Add other qwen test binaries
$(QWEN_PROTOCOL_TEST_BIN): src/VfsShell/qwen_protocol_test.cpp $(VFSSHELL_SRC) $(VFSSHELL_HDR)
	$(CXX) $(CXXFLAGS) -DCODEX_NO_MAIN -DQWEN_TESTS_ENABLED src/VfsShell/qwen_protocol_test.cpp $(VFSSHELL_SRC_NO_MAIN) -o $@ $(LDFLAGS)

$(QWEN_CLIENT_TEST_BIN): src/VfsShell/qwen_client_test.cpp $(VFSSHELL_SRC) $(VFSSHELL_HDR)
	$(CXX) $(CXXFLAGS) -DCODEX_NO_MAIN -DQWEN_TESTS_ENABLED src/VfsShell/qwen_client_test.cpp $(VFSSHELL_SRC_NO_MAIN) -o $@ $(LDFLAGS)

$(QWEN_ECHO_SERVER_BIN): src/VfsShell/qwen_echo_server.cpp $(VFSSHELL_SRC) $(VFSSHELL_HDR)
	$(CXX) $(CXXFLAGS) -DCODEX_NO_MAIN -DQWEN_TESTS_ENABLED src/VfsShell/qwen_echo_server.cpp $(VFSSHELL_SRC_NO_MAIN) -o $@ $(LDFLAGS)

clean:
	rm -f $(VFSSHELL_BIN) $(PLANNER_DEMO_BIN) $(PLANNER_TRAIN_BIN) $(QWEN_STATE_MANAGER_TEST_BIN)
	rm -rf build-sample

BUILD_DIR := build-sample
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
release: all

debug: CXXFLAGS += -O0 -g
debug: all

# Test library
TEST_LIB := tests/libtest.so
test-lib: $(TEST_LIB)

$(TEST_LIB): tests/test_lib.c
	$(CC) -shared -fPIC -o $@ $<
