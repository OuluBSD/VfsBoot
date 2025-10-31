# Qwen-Code Structured Protocol Integration (REVISED)

## Core Concept: Semantic Data, Not Terminal Rendering

**Key Insight**: Instead of sending rendered terminal cells (thin client), we send structured semantic data about the application state. This allows VfsBoot to build a custom thick client UI that fits its design.

**Benefits**:
- **Semantic understanding**: C++ knows what's happening (tool execution, conversation, etc.)
- **Custom UI**: VfsBoot can render in its own style (tree views, split panes, VFS integration)
- **VFS integration**: Store conversation history in `/qwen/history`, files in `/qwen/files`
- **Minimal qwen-code changes**: Only ~600 lines of new TypeScript, no React/Ink modifications
- **Upstream mergeable**: Clean architecture that could be contributed back

## Architecture

```
┌────────────────────────────────────────────────────────────┐
│  qwen-code (TypeScript) - LOGIC ONLY                       │
│  ┌─────────────────────────────────────────────────────┐  │
│  │  App.tsx (React/Ink) - Unchanged                    │  │
│  │  - Conversation state (history, pending items)      │  │
│  │  - Streaming state (idle, responding, confirming)   │  │
│  │  - Tool executions                                  │  │
│  └──────────────┬──────────────────────────────────────┘  │
│                 │                                           │
│  ┌──────────────▼──────────────────────────────────────┐  │
│  │  QwenStateSerializer (NEW - 320 lines)              │  │
│  │  - Serializes HistoryItem → JSON                    │  │
│  │  - Tracks incremental changes                       │  │
│  │  - Maps StreamingState → status updates            │  │
│  └──────────────┬──────────────────────────────────────┘  │
│                 │                                           │
│  ┌──────────────▼──────────────────────────────────────┐  │
│  │  StructuredServerMode (NEW - 280 lines)             │  │
│  │  - stdin/stdout, pipes, TCP                         │  │
│  │  - Sends QwenStateMessage                           │  │
│  │  - Receives QwenCommand                             │  │
│  └──────────────┬──────────────────────────────────────┘  │
└─────────────────┼───────────────────────────────────────────┘
                  │
                  │ Protocol: JSON Lines (Semantic Data)
                  │
┌─────────────────▼───────────────────────────────────────────┐
│  VfsBoot C++ (Thick Client)                                │
│  ┌─────────────────────────────────────────────────────┐  │
│  │  QwenClient (stdin/pipe/tcp)                        │  │
│  │  - Receives semantic events                         │  │
│  │  - Sends user commands                              │  │
│  └──────────────┬──────────────────────────────────────┘  │
│                 │                                           │
│  ┌──────────────▼──────────────────────────────────────┐  │
│  │  QwenStateManager                                    │  │
│  │  - Stores conversation in /qwen/history             │  │
│  │  - Tracks tool executions                           │  │
│  │  - Manages application state                        │  │
│  └──────────────┬──────────────────────────────────────┘  │
│                 │                                           │
│  ┌──────────────▼──────────────────────────────────────┐  │
│  │  VfsBoot Custom UI                                   │  │
│  │  - Split pane layout                                │  │
│  │  - Tree view for files                              │  │
│  │  - Custom rendering of tools/diffs                  │  │
│  │  - VFS integration                                  │  │
│  └─────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

## Protocol: Structured Semantic Messages

### TypeScript → C++ (State Updates)

#### Init Message
```json
{
  "type": "init",
  "version": "0.0.14",
  "workspaceRoot": "/path/to/project",
  "model": "qwen2.5-coder-7b"
}
```

#### Conversation Messages
```json
{
  "type": "conversation",
  "role": "user",
  "content": "write a hello world program in C++",
  "id": 1,
  "timestamp": 1234567890
}

{
  "type": "conversation",
  "role": "assistant",
  "content": "I'll create a hello world program...",
  "id": 2
}
```

#### Tool Execution
```json
{
  "type": "tool_group",
  "id": 3,
  "tools": [
    {
      "type": "tool_call",
      "tool_id": "write_file_abc123",
      "tool_name": "write_file",
      "status": "pending",
      "args": {
        "path": "hello.cpp",
        "content": "#include <iostream>..."
      }
    }
  ]
}

// Later, status update
{
  "type": "tool_group",
  "id": 3,
  "tools": [
    {
      "tool_id": "write_file_abc123",
      "tool_name": "write_file",
      "status": "success",
      "args": {...},
      "result": "File written successfully"
    }
  ]
}
```

#### Status Updates
```json
{
  "type": "status",
  "state": "responding",
  "message": "Thinking...",
  "thought": "I need to create a C++ program..."
}

{
  "type": "status",
  "state": "waiting_for_confirmation",
  "message": "Waiting for approval"
}

{
  "type": "status",
  "state": "idle",
  "message": "Ready"
}
```

#### Info/Error Messages
```json
{
  "type": "info",
  "message": "Workspace context loaded from QWEN.md",
  "id": 4
}

{
  "type": "error",
  "message": "Failed to write file: Permission denied",
  "id": 5
}
```

### C++ → TypeScript (Commands)

#### User Input
```json
{
  "type": "user_input",
  "content": "add error handling"
}
```

#### Tool Approval
```json
{
  "type": "tool_approval",
  "tool_id": "write_file_abc123",
  "approved": true
}
```

#### Interrupt
```json
{
  "type": "interrupt"
}
```

#### Model Switch
```json
{
  "type": "model_switch",
  "model_id": "qwen2.5-coder-32b"
}
```

## Implementation

### Phase 1: TypeScript (qwen-code) - 600 lines total ✅ COMPLETED

**File**: `packages/cli/src/qwenStateSerializer.ts` (~320 lines)
- Interfaces for all protocol message types
- `serializeHistoryItem()` - Convert HistoryItem → JSON
- `serializeStreamingState()` - Convert state → status update
- `QwenStateSerializer` class - Tracks incremental changes

**File**: `packages/cli/src/structuredServerMode.ts` (~280 lines)
- `BaseStructuredServer` abstract class
- `StdinStdoutServer` - Line-buffered JSON over stdio
- `NamedPipeServer` - Bidirectional filesystem pipes
- `TCPServer` - Network communication
- Command parsing and message queue

**File**: `packages/cli/src/gemini.tsx` (modified)
- Import structured server modules
- `runServerMode()` function - Runs headless qwen-code
- Server mode check in `main()`

**File**: `packages/cli/src/config/config.ts` (modified)
- Added `--server-mode`, `--pipe-path`, `--tcp-port` flags

### Phase 2: C++ (VfsBoot) - ~2,000 lines

**File**: `VfsShell/qwen_protocol.h/cpp` (~400 lines)
- Protocol message structs matching TypeScript types
- JSON parsing/serialization using nlohmann::json
- Message validation

**File**: `VfsShell/qwen_client.h/cpp` (~600 lines)
- Communication layer (stdin/pipe/tcp)
- Message sending/receiving
- Connection management
- Auto-restart on crash

**File**: `VfsShell/qwen_state_manager.h/cpp` (~500 lines)
- Stores conversation history in VFS `/qwen/history`
- Tracks tool executions
- Application state machine
- Event callbacks for UI updates

**File**: `VfsShell/qwen_ui.h/cpp` (~500 lines)
- Custom VfsBoot UI rendering
- Split pane layout (conversation | files)
- Tree view for file operations
- Diff rendering for edits
- Tool execution visualization

**File**: `VfsShell/cmd_qwen.cpp` (~200 lines)
- Shell command integration: `qwen <query>`
- Interactive mode: `qwen --interactive`
- Session management

### Phase 3: VFS Integration (~300 lines)

**VFS Structure**:
```
/qwen
  /history         # Conversation messages
    /001-user.txt
    /002-assistant.txt
    /003-user.txt
  /files           # Files being edited
    /hello.cpp
    /README.md
  /tools           # Tool execution logs
    /write_file_abc123.json
  /state           # Current application state
    /status.json
    /model.txt
```

## Example Session

### 1. C++ starts qwen client
```bash
./codex
> qwen write a hello world program
```

### 2. VfsBoot forks qwen-code
```cpp
// Fork qwen-code process
QwenClient client(QwenClient::Mode::STDIN);
client.start("qwen", {"--server-mode", "stdin", "--model", "qwen2.5-coder-7b"});
```

### 3. TypeScript sends init
```json
{"type":"init","version":"0.0.14","workspaceRoot":"/home/user/project","model":"qwen2.5-coder-7b"}
```

### 4. C++ sends user input
```json
{"type":"user_input","content":"write a hello world program"}
```

### 5. TypeScript sends conversation + tools
```json
{"type":"conversation","role":"user","content":"write a hello world program","id":1}
{"type":"status","state":"responding","message":"Thinking..."}
{"type":"conversation","role":"assistant","content":"I'll create hello.cpp...","id":2}
{"type":"tool_group","id":3,"tools":[{"tool_id":"write_abc","tool_name":"write_file","status":"confirming",...}]}
```

### 6. C++ renders UI + waits for approval
```
┌─ Qwen Assistant ──────────────────────┐
│ You> write a hello world program     │
│                                       │
│ Assistant> I'll create hello.cpp with│
│ a simple hello world program.        │
│                                       │
│ 🔧 Tool: write_file                  │
│    Path: hello.cpp                   │
│    Action: Create new file           │
│                                       │
│ [A]pprove [R]eject [V]iew           │
└───────────────────────────────────────┘
```

### 7. User approves, C++ sends approval
```json
{"type":"tool_approval","tool_id":"write_abc","approved":true}
```

### 8. TypeScript executes tool, sends result
```json
{"type":"tool_group","id":3,"tools":[{"tool_id":"write_abc","status":"success","result":"File created"}]}
{"type":"status","state":"idle","message":"Ready"}
```

### 9. VfsBoot stores in VFS
```cpp
// Store conversation
vfs.writeFile("/qwen/history/001-user.txt", "write a hello world program");
vfs.writeFile("/qwen/history/002-assistant.txt", "I'll create hello.cpp...");

// Store created file
vfs.writeFile("/qwen/files/hello.cpp", file_content);
```

## Benefits of This Approach

1. **Full semantic understanding**: C++ knows exactly what's happening
2. **Custom UI**: Render in VfsBoot style, not terminal emulation
3. **VFS integration**: Seamless with existing VfsBoot architecture
4. **Stateful**: Can resume sessions, replay history
5. **Extensible**: Easy to add new message types
6. **Debuggable**: JSON protocol is human-readable
7. **Clean separation**: TypeScript = AI logic, C++ = UI
8. **Minimal changes**: qwen-code remains mostly unchanged

## Testing Strategy

### 1. Protocol Testing (TypeScript)
```bash
echo '{"type":"user_input","content":"hello"}' | qwen --server-mode stdin
```

### 2. Echo Server (C++)
Simple test client that echoes received messages.

### 3. Full Integration
Run end-to-end with actual conversations.

## Timeline

| Week | Focus | Deliverable |
|------|-------|-------------|
| 1 | TypeScript protocol implementation | ✅ COMPLETED |
| 2 | C++ protocol + client | JSON parsing, message handling working |
| 3 | C++ state manager + VFS | Conversation stored in VFS |
| 4 | C++ custom UI | Full interactive interface |
| 5 | Polish + testing | Production ready |

**Total**: 5 weeks

## Next Steps

1. ✅ Design structured protocol
2. ✅ Create TypeScript serializer
3. ✅ Create TypeScript server modes
4. ✅ Update gemini.tsx entry point
5. ⏭️ Implement C++ protocol parser
6. ⏭️ Build C++ client (stdin mode first)
7. ⏭️ Test basic echo/response
8. ⏭️ Implement state manager + VFS
9. ⏭️ Build custom UI
10. ⏭️ End-to-end testing

---

**Ready to start Phase 2: C++ Implementation**
