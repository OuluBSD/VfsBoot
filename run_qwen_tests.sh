#!/bin/bash
# run_qwen_tests.sh - qwen-code Integration Test Suite
#
# This script runs the complete test suite for the qwen-code integration
# in VfsBoot. The tests are organized in layers from lowest to highest level.
#
# Test Architecture:
#
#   ┌─────────────────────────────────────────┐
#   │ Integration Test (--qwen-integration-test)│  ← Full end-to-end workflow
#   └──────────────┬──────────────────────────┘
#                  │
#   ┌──────────────▼──────────────────────────┐
#   │ Client Test (--qwen-client-test)        │  ← Subprocess + I/O
#   └──────────────┬──────────────────────────┘
#                  │
#   ┌──────────────▼──────────────────────────┐
#   │ State Tests (--qwen-state-tests)        │  ← VFS persistence
#   └──────────────┬──────────────────────────┘
#                  │
#   ┌──────────────▼──────────────────────────┐
#   │ Protocol Tests (--qwen-protocol-tests)  │  ← Message parsing
#   └─────────────────────────────────────────┘
#
# Each layer builds on the previous, so we test bottom-up to isolate failures.

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Trap Ctrl+C and cleanup
cleanup() {
  echo ""
  echo -e "${YELLOW}Test run interrupted by user.${NC}"
  exit 130
}

trap cleanup SIGINT SIGTERM

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VFSH_BIN="$SCRIPT_DIR/vfsh"

# Check if vfsh binary exists
if [ ! -f "$VFSH_BIN" ]; then
  echo -e "${RED}Error: vfsh binary not found at $VFSH_BIN${NC}"
  echo "Build it with: make"
  exit 1
fi

# Parse command line arguments
RUN_ALL=true
RUN_PROTOCOL=false
RUN_STATE=false
RUN_CLIENT=false
RUN_INTEGRATION=false
VERBOSE=false

while [[ $# -gt 0 ]]; do
  case $1 in
    --protocol)
      RUN_ALL=false
      RUN_PROTOCOL=true
      shift
      ;;
    --state)
      RUN_ALL=false
      RUN_STATE=true
      shift
      ;;
    --client)
      RUN_ALL=false
      RUN_CLIENT=true
      shift
      ;;
    --integration)
      RUN_ALL=false
      RUN_INTEGRATION=true
      shift
      ;;
    -v|--verbose)
      VERBOSE=true
      shift
      ;;
    --help)
      echo "Usage: $0 [OPTIONS]"
      echo ""
      echo "Run qwen-code integration test suite"
      echo ""
      echo "Options:"
      echo "  --protocol      Run only protocol tests"
      echo "  --state         Run only state manager tests"
      echo "  --client        Run only client tests"
      echo "  --integration   Run only integration test"
      echo "  -v, --verbose   Verbose output"
      echo "  --help          Show this help message"
      echo ""
      echo "Examples:"
      echo "  $0                    # Run all tests"
      echo "  $0 --protocol         # Run only protocol tests"
      echo "  $0 --client -v        # Run client tests with verbose output"
      exit 0
      ;;
    *)
      echo "Unknown option: $1"
      echo "Use --help for usage information"
      exit 1
      ;;
  esac
done

# Set flags if running all
if [ "$RUN_ALL" = true ]; then
  RUN_PROTOCOL=true
  RUN_STATE=true
  RUN_CLIENT=true
  RUN_INTEGRATION=true
fi

echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}  qwen-code Integration Test Suite${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
echo ""

TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# Helper function to run a test
run_test() {
  local test_name="$1"
  local test_flag="$2"
  local description="$3"

  TOTAL_TESTS=$((TOTAL_TESTS + 1))

  echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
  echo -e "${YELLOW}Test $TOTAL_TESTS: $test_name${NC}"
  echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
  echo ""
  echo -e "${BLUE}Description:${NC}"
  echo "$description"
  echo ""
  echo -e "${BLUE}Running:${NC} $VFSH_BIN $test_flag"
  echo ""

  if [ "$VERBOSE" = true ]; then
    if "$VFSH_BIN" "$test_flag"; then
      echo ""
      echo -e "${GREEN}✓ $test_name PASSED${NC}"
      PASSED_TESTS=$((PASSED_TESTS + 1))
      return 0
    else
      echo ""
      echo -e "${RED}✗ $test_name FAILED${NC}"
      FAILED_TESTS=$((FAILED_TESTS + 1))
      return 1
    fi
  else
    local output
    if output=$("$VFSH_BIN" "$test_flag" 2>&1); then
      echo "$output" | tail -n 5
      echo ""
      echo -e "${GREEN}✓ $test_name PASSED${NC}"
      PASSED_TESTS=$((PASSED_TESTS + 1))
      return 0
    else
      echo "$output" | tail -n 10
      echo ""
      echo -e "${RED}✗ $test_name FAILED${NC}"
      FAILED_TESTS=$((FAILED_TESTS + 1))
      return 1
    fi
  fi
}

# Test 1: Protocol Tests (Layer 1 - Foundation)
if [ "$RUN_PROTOCOL" = true ]; then
  run_test \
    "Protocol Tests" \
    "--qwen-protocol-tests" \
    "Tests the JSON protocol message parsing and serialization layer.
This is the foundation of qwen-code integration - all communication
between VfsBoot (C++) and qwen-code (TypeScript) uses this protocol.

Tests include:
  - Message parsing (init, conversation, tool_group, status, etc.)
  - Command serialization (user_input, tool_approval, interrupt, etc.)
  - JSON edge cases (escaping, nested objects, unicode)
  - Error handling for malformed messages

Expected result: 18/18 tests PASS

Why run this first?
  If protocol tests fail, nothing else will work. This validates
  the lowest-level communication layer." || true

  echo ""
fi

# Test 2: State Manager Tests (Layer 2 - Persistence)
if [ "$RUN_STATE" = true ]; then
  run_test \
    "State Manager Tests" \
    "--qwen-state-tests" \
    "Tests the VFS-backed session persistence layer.
This layer manages:
  - Session creation, loading, saving, deletion
  - Conversation history storage (JSONL format)
  - Tool group tracking
  - Session metadata (created_at, model, workspace, tags)
  - File storage per session (/qwen/sessions/<id>/files/)

Storage structure:
  /qwen/sessions/<session-id>/
  ├── metadata.json          # Session info
  ├── history.jsonl          # Conversation messages
  ├── tool_groups.jsonl      # Tool execution tracking
  └── files/                 # Session-specific files

Expected result: 7/8 tests PASS (1 known minor stats bug)

Why run this second?
  State management is independent of network I/O but depends on
  the protocol for serialization. This tests data persistence
  before we introduce subprocess complexity." || true

  echo ""
fi

# Test 3: Client Tests (Layer 3 - Communication)
if [ "$RUN_CLIENT" = true ]; then
  run_test \
    "Client Tests" \
    "--qwen-client-test" \
    "Tests the subprocess management and I/O layer.
This layer handles:
  - fork/exec to spawn qwen-code subprocess
  - Non-blocking I/O using poll()
  - Bidirectional JSON message passing
  - Auto-restart on crash
  - Message callback routing

Tests verify:
  - Subprocess starts successfully
  - Messages are sent and received correctly
  - Callbacks fire for different message types
  - Error handling for subprocess failures

Expected result: Integration test PASS

Why run this third?
  Client tests require both protocol and state layers to work.
  This validates subprocess communication before full integration." || true

  echo ""
fi

# Test 4: Integration Test (Layer 4 - End-to-End)
if [ "$RUN_INTEGRATION" = true ]; then
  run_test \
    "Integration Test" \
    "--qwen-integration-test" \
    "Full end-to-end integration test of the qwen-code system.
This test validates the complete workflow:
  1. Create a new qwen session
  2. Send user input to qwen-code
  3. Receive AI response (streaming)
  4. Handle tool approval workflow
  5. Execute approved tools
  6. Save session to VFS
  7. Clean up resources

Components tested:
  - cmd_qwen.cpp (interactive command)
  - qwen_client.cpp (subprocess management)
  - qwen_state_manager.cpp (VFS persistence)
  - qwen_protocol.cpp (message parsing)
  - qwen-code server (TypeScript subprocess)

Expected result: Full workflow completes successfully

Why run this last?
  Integration tests exercise all layers together. If this fails
  but previous tests passed, the issue is in the integration
  between components, not the components themselves." || true

  echo ""
fi

# Summary
echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}  Test Summary${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
echo ""
echo -e "Total tests:  $TOTAL_TESTS"
echo -e "${GREEN}Passed:       $PASSED_TESTS${NC}"

if [ $FAILED_TESTS -gt 0 ]; then
  echo -e "${RED}Failed:       $FAILED_TESTS${NC}"
else
  echo -e "Failed:       $FAILED_TESTS"
fi

echo ""

if [ $FAILED_TESTS -eq 0 ]; then
  echo -e "${GREEN}✓ All tests passed!${NC}"
  echo ""
  echo "Next steps:"
  echo "  1. Start qwen server: ./run_qwen_server.sh"
  echo "  2. Connect client:    ./run_qwen_client.sh"
  echo "  3. Or use directly:   ./vfsh"
  echo "                        vfsh> qwen"
  exit 0
else
  echo -e "${RED}✗ Some tests failed${NC}"
  echo ""
  echo "Debugging tips:"
  echo "  - Run failed tests individually with --verbose flag"
  echo "  - Check test output above for error messages"
  echo "  - Protocol failures: check qwen_protocol.cpp"
  echo "  - State failures: check qwen_state_manager.cpp"
  echo "  - Client failures: check qwen_client.cpp, ensure qwen-code is built"
  echo "  - Integration failures: check all layers + qwen-code server"
  echo ""
  echo "For more details:"
  echo "  - See QWEN.md for integration guide"
  echo "  - See TASK_CONTEXT.md for implementation status"
  exit 1
fi
