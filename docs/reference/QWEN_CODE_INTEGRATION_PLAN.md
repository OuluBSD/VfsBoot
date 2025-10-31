# Qwen-Code Integration Plan (CORRECTED)

## Exploration Complete

**Repository**: `/common/active/sblo/Dev/qwen-code` ✅ (NOT `/common/active/sblo/Dev/Qwen3-Coder`)

## Summary

**qwen-code** is a TypeScript/React-based terminal AI assistant (not Python). Key differences from initial assumptions:

- **Language**: TypeScript/Node.js, not Python
- **UI Framework**: React + Ink (React for terminal), not prompt_toolkit/rich
- **Complexity**: 3,900 lines in App.tsx alone (vs ~600 in Python Aider)
- **Streaming**: Async generators, not simple threading
- **State Management**: React hooks (50+ custom hooks), not procedural

## Architecture Overview

### qwen-code Current Structure

```
packages/
├── cli/              # Frontend (Terminal UI with React+Ink)
│   └── src/
│       ├── gemini.tsx                 # Main entry (402 lines)
│       ├── ui/
│       │   ├── App.tsx                # Main React component (3902 lines!)
│       │   ├── components/            # 50+ UI components
│       │   │   └── InputPrompt.tsx    # Input handling (26K lines!)
│       │   └── hooks/
│       │       ├── useGeminiStream.ts # Streaming logic (1109 lines)
│       │       └── ... (30+ hooks)
└── core/             # Backend (AI Processing)
    └── src/
        ├── core/
        │   ├── client.ts              # GeminiClient (1137 lines)
        │   ├── contentGenerator.ts
        │   ├── geminiChat.ts
        │   └── turn.ts
        └── tools/                     # 40+ tool implementations
```

### Event-Driven Streaming

qwen-code uses **`ServerGeminiStreamEvent`** with types:
- `content` - AI text chunks
- `tool_call_request` - Function calls
- `tool_call_confirmation` - User approval
- `tool_call_response` - Tool results
- `error` - Errors
- `finished` - Completion
- `thought` - Extended thinking (Gemini-2.5)
- `chat_compressed` - Memory management
- `loop_detected` - Infinite loop detection

## Revised Integration Strategy

### Option 1: Keep TypeScript Backend, Build C++ Frontend (RECOMMENDED)

**Pros**:
- Less code to port (~1,500 lines C++ vs 5,000+ lines TypeScript)
- Reuse existing qwen-code features (tools, models, compression)
- Faster development (2-3 weeks vs 6-8 weeks)

**Cons**:
- Dependency on Node.js runtime
- IPC overhead (minimal with stdin/stdout)

**Architecture**:
```
C++ Frontend (ncurses)          TypeScript Backend (qwen-code)
     ↓                                    ↓
QwenTerminal                          Modified gemini.tsx
     ↓                                    ↓
QwenClient (stdin/pipe/tcp)    →    Server mode (new file)
     ↓                                    ↓
Event processor                      GeminiClient streams events
     ↓                                    ↓
Display updates                      JSON serialization via stdout
```

### Option 2: Full Port to C++ (COMPLEX, 8-12 weeks)

Port entire qwen-code to C++:
- 5,000+ lines of TypeScript logic
- Async generator pattern → C++20 coroutines
- React hooks → Custom state management
- Ink rendering → ncurses
- All 40+ tools → C++ implementations

**Recommendation**: Start with Option 1, evaluate Option 2 later.

## Phased Implementation (Option 1)

### Phase 1: TypeScript Backend Server Mode (Week 1)

**Goal**: Add communication layer to qwen-code

**New File**: `packages/cli/src/server.ts` (~600 lines)

```typescript
// Abstract server interface
abstract class BaseServer {
    abstract send(event: ServerGeminiStreamEvent): Promise<void>;
    abstract receive(): Promise<UserInput | null>;
    abstract start(): Promise<void>;
    abstract stop(): Promise<void>;
}

// Three implementations
class StdinStdoutServer extends BaseServer {
    // Read JSON from stdin, write to stdout
    // Default mode
}

class NamedPipeServer extends BaseServer {
    // Unix named pipes: /tmp/qwen-{pid}.in/out
}

class TCPServer extends BaseServer {
    // TCP socket: default port 7777
}
```

**Modified File**: `packages/cli/src/gemini.tsx`

Add server mode flags:
```typescript
// New CLI flags
--server-mode <stdin|pipe|tcp>
--pipe-path <path>
--tcp-port <port>
```

When `--server-mode` is set:
- Skip Ink rendering (`startInteractiveUI()`)
- Create server instance
- Read user input from server, not terminal
- Stream events to server, not Ink components

**Message Protocol**:
```json
{
  "type": "content" | "tool_call_request" | "error" | "finished" | ...,
  "session_id": "uuid",
  "timestamp": 1234567890,
  "value": { ... }
}
```

### Phase 2: C++ Terminal UI (Week 2-3)

**New Files**:
- `VfsShell/qwen_terminal.h` (~300 lines)
- `VfsShell/qwen_terminal.cpp` (~900 lines)

**QwenTerminal class** (simpler than React+Ink):
```cpp
class QwenTerminal {
    // Screen layout: header, content area, status, input
    WINDOW* header_win_;
    WINDOW* content_win_;  // scrollable AI responses
    WINDOW* status_win_;
    WINDOW* input_win_;

    // Core methods
    void displayEvent(const Event& event);
    void displayContent(const std::string& text, bool streaming);
    void displayToolCall(const ToolCallRequest& req);
    void displayError(const std::string& error);
    std::string getInput(const std::string& prompt);

    // History
    std::vector<std::string> history_;
    void saveHistory();
    void loadHistory();
};
```

**Features to implement**:
- ANSI color rendering
- Scrollable content area
- Input history (arrow keys)
- Status bar (model, tokens, status)
- Tool call approval dialogs
- Streaming text display

### Phase 3: C++ Protocol Client (Week 3)

**New Files**:
- `VfsShell/qwen_client.h` (~250 lines)
- `VfsShell/qwen_client.cpp` (~700 lines)

**QwenClient class**:
```cpp
class QwenClient {
    enum class Mode { Stdin, Pipe, TCP };

    // Connection
    bool connect(Mode mode, const std::string& config);
    void disconnect();

    // Send/receive
    void sendUserInput(const std::string& text);
    std::optional<Event> receiveEvent(int timeout_ms = -1);

    // Background polling
    void pollEvents(std::function<void(const Event&)> handler);

    // Session management
    void saveSession(const std::string& id);
    void loadSession(const std::string& id);
    void detach();
    void attach(const std::string& id);

private:
    std::unique_ptr<CommChannel> channel_;
    std::thread poll_thread_;
    std::queue<Event> event_queue_;
    std::mutex queue_mutex_;
};
```

**Communication channels**:
```cpp
// Subprocess: fork/exec qwen-code --server-mode stdin
class StdinChannel : public CommChannel {
    pid_t child_pid_;
    int stdin_fd_;
    int stdout_fd_;
};

// Named pipes
class PipeChannel : public CommChannel {
    int read_fd_;
    int write_fd_;
};

// TCP socket
class TCPChannel : public CommChannel {
    int socket_fd_;
};
```

### Phase 4: VfsBoot Integration (Week 4)

**New File**: `VfsShell/cmd_qwen.cpp` (~500 lines)

```cpp
void cmd_qwen(const std::vector<std::string>& args,
              Vfs& vfs,
              std::map<std::string, std::string>& env) {
    // Parse options
    QwenOptions opts = parseQwenArgs(args);

    // Create client and terminal
    QwenClient client;
    QwenTerminal terminal;

    terminal.initialize();

    // Connect to backend (fork/exec qwen-code)
    if (!client.connect(opts.mode, opts.config)) {
        terminal.displayError("Failed to start qwen-code backend");
        return;
    }

    // Main loop
    while (true) {
        // Get user input
        std::string input = terminal.getInput("You> ");

        // Handle local commands
        if (input == "/detach") {
            client.detach();
            break;
        }
        if (input == "/exit") {
            client.disconnect();
            break;
        }

        // Send to backend
        client.sendUserInput(input);

        // Process events
        client.pollEvents([&](const Event& event) {
            terminal.displayEvent(event);
        });
    }

    terminal.shutdown();
}
```

**Shell integration** (`VfsShell/codex.cpp`):
- Register `cmd_qwen` in command dispatcher
- Add to help text
- Add to autocomplete

**VFS integration**:
- Sessions: `/qwen/sessions/{id}.json`
- History: `/qwen/history.txt`
- Chat logs: `/qwen/chats/{timestamp}.md`

### Phase 5: Testing & Documentation (Week 5)

**Tests**:
1. TypeScript backend server modes (stdin/pipe/tcp)
2. C++ client communication
3. Terminal UI rendering
4. Session persistence
5. Tool execution workflow
6. Error handling

**Documentation**:
- `docs/QWEN_CODE_INTEGRATION.md` - full architecture
- Update `CLAUDE.md` - add `qwen` command
- Update `README.md` - usage examples
- Demo scripts in `scripts/examples/`

## Key Files to Modify (qwen-code)

| File | Changes | Complexity |
|------|---------|-----------|
| `packages/cli/src/gemini.tsx` | Add `--server-mode` flag | Low |
| `packages/cli/src/server.ts` | NEW: Server implementations | Medium |
| `packages/cli/package.json` | Add server mode script | Low |

## Key Files to Create (VfsBoot)

| File | Lines | Purpose |
|------|-------|---------|
| `VfsShell/qwen_terminal.h` | ~300 | Terminal UI interface |
| `VfsShell/qwen_terminal.cpp` | ~900 | ncurses implementation |
| `VfsShell/qwen_client.h` | ~250 | Client interface |
| `VfsShell/qwen_client.cpp` | ~700 | Protocol client |
| `VfsShell/cmd_qwen.cpp` | ~500 | Shell command |

**Total**: ~2,650 lines C++ + ~600 lines TypeScript

## Dependencies

**TypeScript (qwen-code)**:
- No new dependencies
- Existing: Node.js, React, Ink, undici

**C++ (VfsBoot)**:
- `ncurses` - terminal UI
- `nlohmann/json` - JSON parsing (header-only)
- POSIX APIs: fork, exec, pipe, socket

**Build**:
- Link ncurses: `-lncurses`
- Add new C++ files to Makefile

## Timeline

| Week | Focus | Deliverable |
|------|-------|-------------|
| 1 | TypeScript server mode | qwen-code runs in --server-mode |
| 2 | C++ terminal UI | Basic ncurses UI working |
| 3 | C++ client | Communication via stdin functional |
| 4 | VfsBoot integration | `qwen` command works end-to-end |
| 5 | Testing & docs | Production-ready release |

**Total**: 5 weeks for MVP

## Success Criteria

- [ ] `qwen` command launches
- [ ] User can type and receive AI responses
- [ ] Streaming works in real-time
- [ ] Tool calls show approval dialogs
- [ ] Session detach/attach works
- [ ] All 3 modes work (stdin/pipe/tcp)
- [ ] Chat history persists
- [ ] No data loss on crash
- [ ] Documentation complete

## Next Steps

1. ✅ Explore qwen-code repository (DONE)
2. ⏭️ Create `packages/cli/src/server.ts` (TypeScript)
3. ⏭️ Modify `gemini.tsx` for server mode
4. ⏭️ Test standalone: `qwen --server-mode stdin`
5. ⏭️ Build C++ terminal UI
6. ⏭️ Build C++ client
7. ⏭️ Integrate into VfsBoot
8. ⏭️ Test & document

## Questions for User

1. **Preferred approach**: Option 1 (keep TypeScript backend) or Option 2 (full C++ port)?
2. **Priority features**: Which features are most important from qwen-code?
   - Tool execution with approval?
   - Extended thinking (Gemini-2.5 feature)?
   - Session compression?
   - All tools (40+ implementations)?
3. **Timeline**: Is 5 weeks acceptable, or push for faster MVP (3 weeks, fewer features)?
4. **Dependencies**: OK to require Node.js for backend, or must be pure C++?

---

**This plan supersedes the incorrect plan in TASKS.md. The correct repository is `/common/active/sblo/Dev/qwen-code` (TypeScript/React), not `/common/active/sblo/Dev/Qwen3-Coder` (Python/Aider).**
