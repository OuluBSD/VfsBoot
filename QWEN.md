# qwen-code Integration Guide

**IMPORTANT**: All agents working on qwen integration MUST read this document first, then consult [TASK_CONTEXT.md](TASK_CONTEXT.md) for current status.

## Overview

VfsBoot integrates with [qwen-code](https://github.com/lvce-editor/qwen-code) - a TypeScript/Node.js AI coding assistant forked from Gemini Code Assist. The integration provides a native `qwen` command in VfsBoot that spawns qwen-code as a subprocess and communicates via a structured JSON protocol.

## Architecture

### Two-Process Design

```
┌─────────────────────────────────────────┐
│   VfsBoot (C++)                         │
│   ./vfsh                               │
│                                         │
│   ┌──────────────────────────────────┐ │
│   │  qwen command                    │ │
│   │  - Session management            │ │
│   │  - Interactive terminal          │ │
│   │  - Tool approval prompts         │ │
│   │  - VFS persistence               │ │
│   └────────┬─────────────────────────┘ │
│            │ JSON Protocol             │
│            │ (stdin/stdout or TCP)     │
└────────────┼──────────────────────────────┘
             │
             ▼
┌─────────────────────────────────────────┐
│   qwen-code (TypeScript)                │
│   node packages/cli/dist/index.js       │
│                                         │
│   ┌──────────────────────────────────┐ │
│   │  Server Mode                     │ │
│   │  - StdinStdoutServer (default)   │ │
│   │  - TCPServer (port 7777)         │ │
│   │  - NamedPipeServer               │ │
│   │  - GeminiClient / OpenAI client  │ │
│   │  - Tool scheduler                │ │
│   └──────────────────────────────────┘ │
└─────────────────────────────────────────┘
```

### Three Layers

1. **Protocol Layer** (`qwen_protocol.{h,cpp}`)
   - Type-safe message structs (C++ std::variant)
   - Lightweight JSON parser (no dependencies)
   - Commands: user_input, tool_approval, interrupt, model_switch
   - Messages: init, conversation, tool_group, status, info, error, completion_stats

2. **Communication Layer** (`qwen_client.{h,cpp}`)
   - Subprocess management (fork/exec)
   - Non-blocking I/O with poll()
   - Auto-restart on crash
   - Callback-based message handling

3. **Storage Layer** (`qwen_state_manager.{h,cpp}`)
   - VFS-backed session persistence
   - Conversation history (JSONL format)
   - File storage per session
   - Session metadata tracking

## Quick Start

### Current Interface Mode

**Note**: The current implementation uses **full-screen ncurses mode** by default with automatic terminal capability detection (xterm, screen, tmux, rxvt, st, putty, etc.). The interface provides:

- Split-screen layout with color-coded messages (user=green, AI=cyan, system=yellow, errors=red, info=blue, tools=magenta)
- Scrollable message history
- Status bar showing session ID, model, and connection info
- Interactive tool approval workflow
- Streaming response display

The system gracefully falls back to line-based stdio mode for unsupported terminals or when explicitly requested with `--simple` flag.

### Keyboard Controls in ncurses Mode

**Navigation & Editing:**
- Arrow keys: Move cursor in input field
- Home / Ctrl+A: Move to beginning of line
- End / Ctrl+E: Move to end of line
- Backspace / Delete: Remove characters
- Ctrl+K: Kill from cursor to end of line
- Ctrl+U: Kill from beginning to cursor (or scroll up if input empty)
- Page Up/Down: Scroll conversation history
- Ctrl+D: Scroll down
- Mouse wheel: Scroll (hold Shift to select text in most terminals)

**Permission Modes:**
- Shift+Tab: Cycle through permission modes (PLAN → NORMAL → AUTO-EDIT → YOLO)

**Tool Approval:**
- [y]: Approve and execute tools
- [n]: Reject tools
- [d]: Enter discuss mode to ask questions first

**Exit Behavior (Double Ctrl+C Pattern):**
- **First Ctrl+C**: Clear input buffer and show hint
- **Second Ctrl+C** (within 2 seconds): Detach from qwen, return to vfsh CLI
- **Third Ctrl+C** (in vfsh): Exit vfsh entirely

**RULE**: This double Ctrl+C pattern applies to ALL sub-programs in vfsh (qwen, text editor, etc.):
- First Ctrl+C: Cancel current operation / clear input
- Second Ctrl+C: Exit sub-program, return to vfsh
- Third Ctrl+C: Exit vfsh entirely

### Using qwen in VfsBoot

```bash
# Interactive session with default model
$ ./vfsh
vfsh> qwen

# Special commands within qwen session:
#   /exit   - Exit session
#   /detach - Detach (keeps session alive)
#   /help   - Show help

# Attach to existing session
vfsh> qwen --attach session-12345

# List sessions
vfsh> qwen --list-sessions

# Use specific model
vfsh> qwen --model gpt-4o-mini
```

### Running qwen-code as Standalone Server

qwen-code can run as a TCP server in a separate terminal, making it easy to connect from multiple VfsBoot instances or test with tools like `nc`.

**Terminal 1 - Start qwen-code TCP server:**
```bash
cd /common/active/sblo/Dev/qwen-code
npm install  # First time only
npm run build  # First time only

# Run with qwen-oauth (requires authentication setup)
node packages/cli/dist/index.js --server-mode tcp --tcp-port 7777

# Or run with OpenAI (requires OPENAI_API_KEY)
export OPENAI_API_KEY=sk-...
node packages/cli/dist/index.js --server-mode tcp --tcp-port 7777

# Or use wrapper script
/common/active/sblo/Dev/VfsBoot/qwen-code --server-mode tcp --tcp-port 7777
```

**Terminal 2 - Connect from VfsBoot:**
```bash
$ ./vfsh
codex> qwen --mode tcp --port 7777
```

**Terminal 3 - Test with netcat:**
```bash
$ echo '{"type":"user_input","content":"hello"}' | nc localhost 7777
{"type":"init","version":"0.0.14","workspaceRoot":"/common/active/sblo/Dev/VfsBoot","model":"gpt-4o-mini"}
{"type":"conversation","role":"user","content":"hello","id":1761158324665}
{"type":"status","content":"Processing..."}
...
```

## Server Modes

qwen-code supports three transport modes:

### 1. Stdin/Stdout (Default)
```bash
# VfsBoot spawns qwen-code automatically
qwen

# Or manually (for testing)
echo '{"type":"user_input","content":"test"}' | qwen-code --server-mode stdin
```

**Pros**: Simple, no network setup, process isolation
**Cons**: Cannot share across terminals

### 2. TCP Server (Recommended for Multi-Terminal)
```bash
# Start server
qwen-code --server-mode tcp --tcp-port 7777

# Connect from VfsBoot
qwen --mode tcp --port 7777

# Or test with nc/telnet
nc localhost 7777
```

**Pros**: Network-accessible, multiple clients, easy testing
**Cons**: Requires available port

### 3. Named Pipes (Unix Only)
```bash
# Start server with named pipes
qwen-code --server-mode pipe --pipe-path /tmp/qwen

# This creates:
# /tmp/qwen.in  (client writes here)
# /tmp/qwen.out (client reads here)
```

**Pros**: No network, filesystem-based IPC
**Cons**: Unix only, cleanup required

## Authentication Options

qwen-code supports multiple AI providers:

### Option 1: qwen-oauth (Google Gemini)
```bash
# Follow qwen-code authentication setup
qwen-code auth login

# Then run server mode
qwen-code --server-mode tcp --tcp-port 7777
```

### Option 2: OpenAI
```bash
# Set environment variable
export OPENAI_API_KEY=sk-...

# Run qwen-code
qwen-code --server-mode tcp --tcp-port 7777
```

### Option 3: Local Llama
```bash
# Set Llama server URL
export LLAMA_BASE_URL=http://192.168.1.169:8080

# Run qwen-code (requires code modification for Llama support)
# Currently qwen-code doesn't support Llama out-of-box
```

## VFS Session Storage

Sessions are stored in `/qwen/sessions/<session-id>/` with the following structure:

```
/qwen/sessions/session-1234567890-abc123/
├── metadata.json          # Session info (created_at, model, workspace, tags)
├── history.jsonl          # Conversation messages (one per line)
├── tool_groups.jsonl      # Tool execution tracking
└── files/                 # Session-specific files
    └── uploaded_image.png
```

### Session Commands

```bash
# Create new session
qwen

# List all sessions
qwen --list-sessions

# Attach to specific session
qwen --attach session-1234567890-abc123

# Resume last session
qwen --resume
```

## Protocol Specification

### Messages (qwen-code → VfsBoot)

#### init
```json
{
  "type": "init",
  "version": "0.0.14",
  "workspaceRoot": "/path/to/workspace",
  "model": "gpt-4o-mini"
}
```

#### conversation
```json
{
  "type": "conversation",
  "role": "user" | "assistant",
  "content": "message text",
  "id": 1234567890,
  "is_streaming": true | false
}
```

#### tool_group
```json
{
  "type": "tool_group",
  "id": "tool-group-123",
  "tools": [
    {
      "id": "tool-1",
      "name": "create_file",
      "arguments": {
        "path": "/test.txt",
        "content": "Hello"
      }
    }
  ],
  "confirmation_details": "Create /test.txt with content 'Hello'"
}
```

#### status
```json
{
  "type": "status",
  "content": "Processing your request..."
}
```

#### info
```json
{
  "type": "info",
  "content": "File created successfully"
}
```

#### error
```json
{
  "type": "error",
  "content": "Failed to create file: permission denied"
}
```

#### completion_stats
```json
{
  "type": "completion_stats",
  "prompt_tokens": 150,
  "completion_tokens": 75,
  "total_tokens": 225
}
```

### Commands (VfsBoot → qwen-code)

#### user_input
```json
{
  "type": "user_input",
  "content": "create a hello world program"
}
```

#### tool_approval
```json
{
  "type": "tool_approval",
  "tool_group_id": "tool-group-123",
  "approved": true | false,
  "approved_tools": ["tool-1", "tool-2"]  # Optional: partial approval
}
```

#### interrupt
```json
{
  "type": "interrupt"
}
```

#### model_switch
```json
{
  "type": "model_switch",
  "model": "gpt-4o"
}
```

## Implementation Files

### VfsBoot C++ Files

| File | Lines | Description |
|------|-------|-------------|
| `VfsShell/qwen_protocol.h` | 280 | Protocol message structs |
| `VfsShell/qwen_protocol.cpp` | 440 | JSON parser, serializer |
| `VfsShell/qwen_protocol_test.cpp` | 226 | 18/18 tests ✅ |
| `VfsShell/qwen_client.h` | 180 | Client API |
| `VfsShell/qwen_client.cpp` | 450 | Subprocess management |
| `VfsShell/qwen_client_test.cpp` | 140 | Integration test ✅ |
| `VfsShell/qwen_state_manager.h` | 280 | State management API |
| `VfsShell/qwen_state_manager.cpp` | 770 | VFS persistence |
| `VfsShell/qwen_state_manager_test.cpp` | 300 | 7/8 tests ✅ |
| `VfsShell/cmd_qwen.h` | 60 | Command interface |
| `VfsShell/cmd_qwen.cpp` | 508 | Interactive command |
| `VfsShell/qwen_echo_server.cpp` | 60 | Test echo server |

**Total**: ~3,700 lines of C++ code

### qwen-code TypeScript Files

| File | Description |
|------|-------------|
| `packages/cli/src/gemini.tsx` | Main entry point, runServerMode() |
| `packages/cli/src/structuredServerMode.ts` | Transport layer (stdin/tcp/pipe) |
| `packages/cli/src/qwenStateSerializer.ts` | State serialization |
| `packages/cli/src/config/config.ts` | Configuration system |

## Special Commands in qwen Session

While in an interactive qwen session, these special commands are available:

| Command | Description |
|---------|-------------|
| `/exit` | Exit qwen session |
| `/detach` | Detach from session (keeps it alive) |
| `/save` | Manually save session to VFS |
| `/help` | Show help |
| `/status` | Show session status |

## Environment Variables

### VfsBoot

| Variable | Default | Description |
|----------|---------|-------------|
| `QWEN_AUTO_APPROVE` | false | Auto-approve tool executions |
| `QWEN_MODEL` | gpt-4o-mini | Default model |
| `QWEN_WORKSPACE` | cwd | Workspace root path |

### qwen-code

| Variable | Default | Description |
|----------|---------|-------------|
| `OPENAI_API_KEY` | - | OpenAI authentication |
| `GEMINI_API_KEY` | - | Google Gemini authentication |
| `LLAMA_BASE_URL` | - | Llama server URL (not yet supported) |

## Troubleshooting

### qwen-code not found
```bash
# Check executable path
which qwen-code

# Or use full path in VfsBoot
export QWEN_CODE_PATH=/common/active/sblo/Dev/qwen-code/packages/cli/dist/index.js

# Or update cmd_qwen.cpp with correct path
```

### TCP port already in use
```bash
# Check what's using the port
lsof -i :7777

# Kill the process
pkill -f "qwen-code.*7777"

# Or use different port
qwen-code --server-mode tcp --tcp-port 7778
```

### Authentication errors
```bash
# For qwen-oauth
qwen-code auth login
qwen-code auth status

# For OpenAI
echo "sk-..." > ~/openai-key.txt
export OPENAI_API_KEY=sk-...

# Check config
cat ~/.config/qwen-code/config.json
```

### Subprocess fails to start
```bash
# Test qwen-code manually
echo '{"type":"user_input","content":"test"}' | qwen-code --server-mode stdin

# Check logs in VfsBoot
cat /logs/qwen_client.log  # If implemented
```

### Messages not received
```bash
# Enable debug logging in cmd_qwen.cpp
#define QWEN_DEBUG 1

# Check protocol messages
# Add std::cerr << "Received: " << message << std::endl;
```

## Demo Scripts

### Basic Usage
```bash
./vfsh scripts/examples/qwen-demo.cx
```

### Session Management
```bash
./vfsh scripts/examples/qwen-session-demo.cx
```

### TCP Mode Testing
```bash
# Terminal 1
qwen-code --server-mode tcp --tcp-port 7777

# Terminal 2
./vfsh
codex> qwen --mode tcp --port 7777
You> hello

# Terminal 3 (netcat test)
echo '{"type":"user_input","content":"test"}' | nc localhost 7777
```

## Future Enhancements

### Possible Improvements
- ⏳ Enhanced ncurses features
  - Scrollback buffer navigation with arrow keys
  - Multi-line input editor
  - Input history with up/down arrows
  - Window resize handling
  - Configurable color schemes
  - Side panel for tool history

- ⏳ Multi-session management (switch between sessions)
- ⏳ Session export/import
- ⏳ Llama backend support in qwen-code
- ⏳ WebSocket support for browser clients
- ⏳ Session sharing across machines
- ⏳ Tool approval policies (auto-approve safe tools)

### Experimental
- Voice input/output
- Image/screenshot sharing
- Collaborative sessions (multiple users)
- Integration with VfsBoot planner system

## Related Documentation

- [TASK_CONTEXT.md](TASK_CONTEXT.md) - Current status and context
- [TASKS.md](TASKS.md) - Active task tracking
- [AGENTS.md](AGENTS.md) - VfsBoot agent documentation
- [QWEN_STRUCTURED_PROTOCOL.md](QWEN_STRUCTURED_PROTOCOL.md) - Protocol details
- [QWEN_CLIENT_IMPLEMENTATION.md](QWEN_CLIENT_IMPLEMENTATION.md) - Phase 2 implementation notes

## Testing

### Automated Test Suite (Recommended)

The easiest way to run all qwen tests is using the test runner script:

```bash
# Run all tests in correct order
./run_qwen_tests.sh

# Run specific test layers
./run_qwen_tests.sh --protocol      # Layer 1: Protocol parsing
./run_qwen_tests.sh --state         # Layer 2: VFS persistence
./run_qwen_tests.sh --client        # Layer 3: Subprocess I/O
./run_qwen_tests.sh --integration   # Layer 4: End-to-end

# Verbose output for debugging
./run_qwen_tests.sh --protocol -v

# Show help
./run_qwen_tests.sh --help
```

The test runner executes tests in bottom-up order (protocol → state → client → integration) to isolate failures at each layer. Each test includes detailed explanations of what it tests and why.

### Manual Testing (Individual Components)

#### Protocol Tests
```bash
# Build and run protocol tests
make qwen-protocol-test
./qwen_protocol_test
# Expected: 18/18 tests PASS
```

Or use vfsh directly:
```bash
./vfsh --qwen-protocol-tests
```

#### State Manager Tests
```bash
# Build and run state manager tests
make qwen-state-manager-test
./qwen_state_manager_test
# Expected: 7/8 tests PASS (1 minor stats bug)
```

Or use vfsh directly:
```bash
./vfsh --qwen-state-tests
```

#### Client Tests
```bash
# Build and run client tests
make qwen-client-test
./qwen_client_test
# Expected: Integration test PASS
```

Or use vfsh directly:
```bash
./vfsh --qwen-client-test
```

#### Integration Test
```bash
./vfsh --qwen-integration-test
```

#### Echo Server (Manual Testing Utility)
```bash
# Start echo server for manual protocol testing
./vfsh --qwen-echo-server

# In another terminal, send test messages
echo '{"type":"user_input","content":"hello"}' | nc localhost 7777
```

### End-to-End Test
```bash
# Start qwen-code TCP server
qwen-code --server-mode tcp --tcp-port 7777

# Run qwen command
./vfsh
vfsh> qwen --mode tcp --port 7777
You> create a hello world program in C++
# Expected: AI generates code, asks for tool approval, creates file
```

## Contributing

When working on qwen integration:

1. **Read this document first** (QWEN.md)
2. **Check current status** in TASK_CONTEXT.md
3. **Update documentation** when making changes
4. **Write tests** for new features
5. **Test end-to-end** before committing

### Commit Convention
```bash
git commit -m "qwen: <description>"
git commit -m "qwen(protocol): add new message type"
git commit -m "qwen(client): fix subprocess restart"
git commit -m "qwen(docs): update QWEN.md"
```

### Pull Request Checklist
- [ ] All tests passing
- [ ] Documentation updated
- [ ] End-to-end tested
- [ ] No regressions
- [ ] Code follows project style

---

**Last Updated**: 2025-10-23
**Status**: Phase 5 Option A COMPLETE ✅
**Maintainer**: See AGENTS.md
