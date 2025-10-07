CXX ?= c++
CXXFLAGS ?= -std=gnu++17 -O2 -Wall -Wextra -pedantic
LDFLAGS ?=

VFSSHELL_SRC := VfsShell/codex.cpp VfsShell/snippet_catalog.cpp
VFSSHELL_HDR := VfsShell/codex.h VfsShell/snippet_catalog.h
VFSSHELL_BIN := codex

.PHONY: all clean debug release sample

all: $(VFSSHELL_BIN)

$(VFSSHELL_BIN): $(VFSSHELL_SRC) $(VFSSHELL_HDR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(VFSSHELL_SRC) -o $@

clean:
	rm -f $(VFSSHELL_BIN)
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
