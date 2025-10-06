CXX ?= c++
CXXFLAGS ?= -std=gnu++17 -O2 -Wall -Wextra -pedantic
LDFLAGS ?=

STAGE1_SRC := Stage1/codex.cpp
STAGE1_HDR := Stage1/codex.h
STAGE1_BIN := codex

.PHONY: all clean debug release

all: $(STAGE1_BIN)

$(STAGE1_BIN): $(STAGE1_SRC) $(STAGE1_HDR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $< -o $@

clean:
	rm -f $(STAGE1_BIN)

# Convenience configuration targets
release: CXXFLAGS += -DNDEBUG -O3
release: clean all

debug: CXXFLAGS += -O0 -g
debug: clean all
