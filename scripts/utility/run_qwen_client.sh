#!/bin/bash
# run_qwen_client.sh - Connect to qwen-code TCP server
#
# This script starts the VfsBoot shell and connects to the qwen-code
# TCP server for interactive AI assistance.
#
# Usage:
#   ./run_qwen_client.sh              # Connect to localhost:7777
#   ./run_qwen_client.sh --port 8080  # Connect to custom port
#   ./run_qwen_client.sh --host 192.168.1.100 --port 7777

set -e

# Trap Ctrl+C during startup
trap 'echo ""; echo "Cancelled."; exit 130' SIGINT SIGTERM

# Default configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VFSH_BIN="$SCRIPT_DIR/vfsh"
TCP_HOST="localhost"
TCP_PORT=7774
DIRECT_MODE=false

# Parse command line arguments
while [[ $# -gt 0 ]]; do
  case $1 in
    --host)
      TCP_HOST="$2"
      shift 2
      ;;
    --port)
      TCP_PORT="$2"
      shift 2
      ;;
    --direct)
      DIRECT_MODE=true
      shift
      ;;
    --help)
      echo "Usage: $0 [OPTIONS]"
      echo ""
      echo "Options:"
      echo "  --host HOST     Connect to specific host (default: localhost)"
      echo "  --port PORT     Connect to specific TCP port (default: 7774)"
      echo "  --direct        Launch vfsh with full ncurses support (manual command entry)"
      echo "  --help          Show this help message"
      echo ""
      echo "Examples:"
      echo "  $0                           # Connect to localhost:7774 (line-based mode)"
      echo "  $0 --port 8080               # Connect to localhost:8080"
      echo "  $0 --direct                  # Launch with full ncurses support"
      echo "  $0 --host 192.168.1.100      # Connect to remote host"
      exit 0
      ;;
    *)
      echo "Unknown option: $1"
      echo "Use --help for usage information"
      exit 1
      ;;
  esac
done

# Check if vfsh binary exists
if [ ! -f "$VFSH_BIN" ]; then
  echo "Error: vfsh binary not found at $VFSH_BIN"
  echo ""
  echo "Build it with:"
  echo "  make"
  exit 1
fi

# Check if server is reachable
if ! nc -z "$TCP_HOST" "$TCP_PORT" 2>/dev/null; then
  echo "Warning: Cannot connect to qwen-code server at $TCP_HOST:$TCP_PORT"
  echo ""
  echo "Start the server first with:"
  echo "  ./run_qwen_server.sh --port $TCP_PORT"
  echo ""
  read -p "Continue anyway? [y/N] " -n 1 -r
  echo
  if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    exit 1
  fi
fi

# Print welcome message
echo "═══════════════════════════════════════════════════════════"
echo "  qwen-code Client"
echo "═══════════════════════════════════════════════════════════"
echo ""
echo "  Connecting to: $TCP_HOST:$TCP_PORT"
echo ""

if [ "$DIRECT_MODE" = true ]; then
  echo "  Starting VfsBoot shell in DIRECT mode..."
  echo ""
  echo "  Full ncurses interface enabled!"
  echo "  Run this command after vfsh starts:"
  echo ""
  echo "    qwen --mode tcp --port $TCP_PORT --host $TCP_HOST"
  echo ""
  echo "  Commands while in qwen session:"
  echo "    /exit   - Exit qwen session"
  echo "    /detach - Detach from session (keeps it alive)"
  echo "    /help   - Show help"
  echo ""
  echo "═══════════════════════════════════════════════════════════"
  echo ""

  # Launch vfsh directly with full TTY access
  exec "$VFSH_BIN"
else
  echo "  Starting VfsBoot shell and launching qwen session..."
  echo ""
  echo "  NOTE: Using line-based stdio mode (ncurses requires direct TTY access)"
  echo "  For full ncurses interface, run: $0 --direct"
  echo ""
  echo "  Commands while in qwen session:"
  echo "    /exit   - Exit qwen session"
  echo "    /detach - Detach from session (keeps it alive)"
  echo "    /help   - Show help"
  echo ""
  echo "═══════════════════════════════════════════════════════════"
  echo ""

  # Start vfsh and automatically launch qwen session
  # Using printf + cat to send the qwen command then pass through stdin
  (printf "qwen --mode tcp --port %s --host %s\n" "$TCP_PORT" "$TCP_HOST"; cat) | "$VFSH_BIN"
fi
