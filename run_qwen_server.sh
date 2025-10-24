#!/bin/bash
# run_qwen_server.sh - Start qwen-code as TCP server
#
# This script starts the qwen-code AI assistant in TCP server mode,
# making it accessible over the network on port 7777.
#
# Usage:
#   ./run_qwen_server.sh              # Use qwen-oauth (default)
#   ./run_qwen_server.sh --openai     # Use OpenAI (requires OPENAI_API_KEY)
#   ./run_qwen_server.sh --port 8080  # Use custom port

set -e

# Track server PID for cleanup
SERVER_PID=""

# Trap Ctrl+C and cleanup
cleanup() {
  echo ""
  echo "Shutting down qwen-code server..."

  # Kill the node process and its children if still running
  if [ -n "$SERVER_PID" ]; then
    # Check if process exists
    if ps -p "$SERVER_PID" > /dev/null 2>&1; then
      # Kill process group (includes children) with SIGTERM first
      kill -TERM -"$SERVER_PID" 2>/dev/null || kill -TERM "$SERVER_PID" 2>/dev/null || true

      # Wait up to 5 seconds for graceful shutdown
      local count=0
      while ps -p "$SERVER_PID" > /dev/null 2>&1 && [ $count -lt 50 ]; do
        sleep 0.1
        count=$((count + 1))
      done

      # If still running, force kill with SIGKILL
      if ps -p "$SERVER_PID" > /dev/null 2>&1; then
        echo "Process didn't respond to SIGTERM, forcing shutdown..."
        kill -KILL -"$SERVER_PID" 2>/dev/null || kill -KILL "$SERVER_PID" 2>/dev/null || true
        sleep 0.2
      fi
    fi
  fi

  echo "Server stopped."
  exit 0
}

trap cleanup SIGINT SIGTERM

# Default configuration
QWEN_CODE_DIR="/common/active/sblo/Dev/qwen-code"
TCP_PORT=7774
AUTH_MODE="qwen-oauth"

# Parse command line arguments
while [[ $# -gt 0 ]]; do
  case $1 in
    --openai)
      AUTH_MODE="openai"
      shift
      ;;
    --port)
      TCP_PORT="$2"
      shift 2
      ;;
    --help)
      echo "Usage: $0 [OPTIONS]"
      echo ""
      echo "Options:"
      echo "  --openai        Use OpenAI (requires OPENAI_API_KEY env var)"
      echo "  --port PORT     Use custom TCP port (default: 7774)"
      echo "  --help          Show this help message"
      echo ""
      echo "Examples:"
      echo "  $0                    # Start with qwen-oauth"
      echo "  $0 --openai           # Start with OpenAI"
      echo "  $0 --port 8080        # Start on port 8080"
      exit 0
      ;;
    *)
      echo "Unknown option: $1"
      echo "Use --help for usage information"
      exit 1
      ;;
  esac
done

# Check if qwen-code directory exists
if [ ! -d "$QWEN_CODE_DIR" ]; then
  echo "Error: qwen-code directory not found at $QWEN_CODE_DIR"
  echo "Please update QWEN_CODE_DIR in this script or install qwen-code"
  exit 1
fi

# Check if qwen-code is built
if [ ! -f "$QWEN_CODE_DIR/packages/cli/dist/index.js" ]; then
  echo "qwen-code is not built. Building now..."
  cd "$QWEN_CODE_DIR"
  npm install
  npm run build
  cd - > /dev/null
fi

# Check authentication setup
if [ "$AUTH_MODE" = "openai" ]; then
  if [ -z "$OPENAI_API_KEY" ] && [ ! -f ~/openai-key.txt ]; then
    echo "Error: OpenAI mode requires OPENAI_API_KEY environment variable"
    echo "or ~/openai-key.txt file"
    echo ""
    echo "Set it with:"
    echo "  export OPENAI_API_KEY=sk-..."
    echo "or:"
    echo "  echo 'sk-...' > ~/openai-key.txt"
    exit 1
  fi

  # Load from file if env var not set
  if [ -z "$OPENAI_API_KEY" ] && [ -f ~/openai-key.txt ]; then
    export OPENAI_API_KEY=$(cat ~/openai-key.txt)
  fi

  # Set QWEN_AUTH_TYPE to override settings (allows running both auth modes simultaneously)
  export QWEN_AUTH_TYPE="openai"
else
  # For qwen-oauth, explicitly set the auth type
  export QWEN_AUTH_TYPE="qwen-oauth"
fi

# Print startup message
echo "═══════════════════════════════════════════════════════════"
echo "  qwen-code TCP Server"
echo "═══════════════════════════════════════════════════════════"
echo ""
echo "  Authentication: $AUTH_MODE"
echo "  Port: $TCP_PORT"
echo ""
echo "  Connect from VfsBoot:"
echo "    ./vfsh"
echo "    codex> qwen --mode tcp --port $TCP_PORT"
echo ""
echo "  Or use the client script:"
echo "    ./run_qwen_client.sh --port $TCP_PORT"
echo ""
echo "  Test with netcat:"
echo "    echo '{\"type\":\"user_input\",\"content\":\"hello\"}' | nc localhost $TCP_PORT"
echo ""
echo "═══════════════════════════════════════════════════════════"
echo ""

# Start the server
cd "$QWEN_CODE_DIR"
node packages/cli/dist/index.js --server-mode tcp --tcp-port "$TCP_PORT" &
SERVER_PID=$!

# Wait for the server to exit
wait "$SERVER_PID"

# If we get here, server exited normally
echo ""
echo "qwen-code server stopped."
