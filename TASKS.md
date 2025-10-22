# Tasks Tracker
Note: sexp is for AI and shell script is for human user (+ ai called via sexp). This follows the classic codex behaviour.

---

## 🚀 CURRENT CONTEXT - qwen-code C++ Integration (Phase 2 COMPLETE)

**Last Updated**: 2025-10-22

### What We Just Completed (Phase 2)

✅ **C++ Client Implementation for qwen-code Integration** - 1,994 lines, all tests passing

**Protocol Layer:**
- `VfsShell/qwen_protocol.h` (280 lines) - Type-safe structs with std::variant
- `VfsShell/qwen_protocol.cpp` (440 lines) - Lightweight JSON parser (no dependencies)
- `VfsShell/qwen_protocol_test.cpp` (226 lines) - **18/18 tests PASS**

**Communication Layer:**
- `VfsShell/qwen_client.h` (180 lines) - Subprocess management API
- `VfsShell/qwen_client.cpp` (450 lines) - POSIX fork/exec, non-blocking I/O with poll()
- `VfsShell/qwen_client_test.cpp` (140 lines) - **Integration test PASS**

**Test Infrastructure:**
- `VfsShell/qwen_echo_server.cpp` (60 lines) - Test echo server
- `tests/100-qwen-protocol-parse.sexp` - Test plan documentation

**What Works:**
- Spawns qwen-code subprocess ✅
- Bidirectional JSON protocol via stdin/stdout ✅
- Callback-based message handling ✅
- Auto-restart on crash ✅
- Graceful shutdown ✅

**Committed**: `9dfa269 - Add qwen-code C++ client implementation (Phase 2)`

### Next Steps (Choose One)

**Option A: Phase 3 - VFS Integration** (Recommended)
- Create `qwen_state_manager.h/cpp` for VFS storage
- Store conversations in `/qwen/history/`
- Store files in `/qwen/files/`
- Persistence and session management

**Option B: Phase 4 - Shell Command Integration**
- Create `cmd_qwen.cpp` for `qwen` shell command
- Interactive mode and script mode
- Session attach/detach
- Configuration support

**Option C: qwen-code TypeScript Full App Integration**
- Complete `runServerMode()` in `gemini.tsx`
- Stream real state changes from qwen-code
- Handle tool approvals from C++
- End-to-end integration testing

### Related Files and Documentation

- **Plan**: `QWEN_STRUCTURED_PROTOCOL.md` (protocol spec)
- **Summary**: `QWEN_CLIENT_IMPLEMENTATION.md` (Phase 2 complete)
- **Old Plans**: `QWEN_CODE_INTEGRATION_PLAN.md`, `QWEN_VIRTUAL_TERMINAL_PLAN.md` (pre-restart, mostly obsolete)
- **qwen-code repo**: `/common/active/sblo/Dev/qwen-code/` (TypeScript implementation with structured protocol)
- **Test binaries**: `./qwen_protocol_test` (18/18), `./qwen_client_test` (PASS), `./qwen_echo_server`

### Architecture Summary

```
VfsBoot (C++)                     qwen-code (TypeScript)
┌─────────────────────┐           ┌──────────────────────┐
│ qwen_client         │◄─stdin───►│ structuredServerMode │
│ - fork/exec         │   stdout  │ - QwenStateSerializer│
│ - poll() I/O        │   JSON    │ - StdinStdoutServer  │
│ - callbacks         │   msgs    │ - runServerMode()    │
└─────────────────────┘           └──────────────────────┘
         │                                  │
         ▼                                  ▼
   qwen_protocol                   qwenStateSerializer
   - parse_message()               - serializeHistoryItem()
   - serialize_command()           - getIncrementalUpdates()
```

**Protocol Messages** (C++ → TS):
- init, conversation, tool_group, status, info, error, completion_stats

**Protocol Commands** (TS → C++):
- user_input, tool_approval, interrupt, model_switch

---

## TODO IMPORTANT: Qwen-Code Interactive Terminal Integration

**Goal**: Integrate qwen-code (TypeScript/React terminal AI assistant) as an internal `qwen` command in VfsBoot with full terminal UI, attachable/detachable sessions, and flexible communication protocols.

### Overview
Port qwen-code's React+Ink terminal frontend to C++ (ncurses), modify the TypeScript/Node.js backend to support stdin/stdout, named pipes, and TCP communication, and integrate as a builtin VfsBoot shell command with session management.

**Key Difference from Initial Plan**: qwen-code is a TypeScript/React-based terminal app (not Python/Aider). It uses React+Ink for UI rendering (3900+ lines in App.tsx), async generators for streaming, and complex state management with React hooks. This changes the porting strategy significantly.

### Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         VfsBoot (C++)                           │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │  qwen Command (Shell Builtin)                            │  │
│  │  - ncurses terminal UI (simplified from React+Ink)       │  │
│  │  - Session manager (attach/detach)                       │  │
│  │  - Protocol handler (stdin/pipe/tcp)                     │  │
│  │  - Event processor (handles streaming events)            │  │
│  └──────────────┬───────────────────────────────────────────┘  │
│                 │                                                │
└─────────────────┼────────────────────────────────────────────────┘
                  │ Communication Protocol (JSON events)
                  │ Event types: content, tool_call, error,
                  │              status, thought, finished
                  │ (everything except "detach" and "exit")
                  │
┌─────────────────┼────────────────────────────────────────────────┐
│                 ▼                                                │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │  qwen-code TypeScript/Node.js Backend                    │  │
│  │  ┌────────────────────────────────────────────────────┐  │  │
│  │  │ Communication Bottleneck (NEW)                     │  │  │
│  │  │ - StdinStdoutServer (default)                      │  │  │
│  │  │ - NamedPipeServer (/tmp/qwen-*.pipe)              │  │  │
│  │  │ - TCPServer (configurable port, e.g. 7777)        │  │  │
│  │  │ Serializes: ServerGeminiStreamEvent objects       │  │  │
│  │  └────────────────┬───────────────────────────────────┘  │  │
│  │                   │                                        │  │
│  │  ┌────────────────▼───────────────────────────────────┐  │  │
│  │  │ Qwen Code Core (Modified)                          │  │  │
│  │  │ - React+Ink frontend blocked/bypassed              │  │  │
│  │  │ - GeminiClient async generator sends events        │  │  │
│  │  │ - Tool scheduler executes tools on demand          │  │  │
│  │  │ - Events: content, tool_call, thought, finished    │  │  │
│  │  └────────────────────────────────────────────────────┘  │  │
│  └──────────────────────────────────────────────────────────┘  │
│                      qwen-code Repository                       │
│           /common/active/sblo/Dev/qwen-code/                   │
└─────────────────────────────────────────────────────────────────┘
```

### Phase 1: TypeScript Backend Communication Layer (Week 1-2)

**Create Communication Bottleneck in qwen-code**

1. **New File**: `packages/cli/src/server.ts` (~600 lines)
   - `BaseServer` abstract class
     - `send_message(type, data)` - serialize and send
     - `receive_message()` - deserialize and receive
     - `start()`, `stop()`, `is_alive()`
   - `StdinStdoutServer` - default mode
     - Read JSON from stdin, write JSON to stdout
     - Line-buffered protocol
   - `NamedPipeServer` - Unix named pipes
     - Create pipes in `/tmp/qwen-{pid}.in` and `/tmp/qwen-{pid}.out`
     - Support configurable path override via `--pipe-path`
   - `TCPServer` - TCP socket communication
     - Listen on configurable port (default 7777)
     - Support multiple connections (session per connection)
     - JSON message framing (length-prefix or newline-delimited)

2. **Message Protocol** (JSON-based, extensible to MessagePack)
   ```json
   {
     "type": "user_input" | "ai_response" | "screen_update" | "error" | "status",
     "session_id": "uuid-string",
     "timestamp": 1234567890,
     "data": {
       "content": "...",
       "metadata": {...}
     }
   }
   ```
   - Message types:
     - `user_input`: User typed text
     - `ai_response`: AI response chunk (streaming)
     - `screen_update`: Terminal screen changes (cursor pos, colors)
     - `error`: Error messages
     - `status`: Status updates (model loading, processing, etc.)
     - `command`: Internal commands (save session, load history)

3. **Modified Entry Point**: `qwencoder-eval/instruct/aider/main.py`
   - Add command-line flags:
     - `--server-mode <stdin|pipe|tcp>` (default: stdin)
     - `--pipe-path <path>` (for named pipes)
     - `--tcp-port <port>` (for TCP server)
   - Block original frontend initialization:
     - Skip `InputOutput` class initialization
     - Skip `prompt_toolkit` session creation
   - Wire backend to communication server:
     - Replace `io.get_input()` with `server.receive_message()`
     - Replace `io.tool_output()` with `server.send_message()`
     - Streaming responses sent as chunks through server

4. **Session Persistence**
   - Serialize conversation state to JSON
   - Store in `/tmp/qwen-session-{id}.json` or VFS
   - Include: chat history, file context, model config
   - Enable attach/detach by saving/loading state

5. **Testing**
   - Standalone Python test: `python main.py --server-mode stdin`
   - Echo client test: send JSON input, receive JSON output
   - Pipe test: `mkfifo /tmp/qwen-in /tmp/qwen-out`
   - TCP test: `nc localhost 7777`

**Deliverables**:
- [ ] `server.py` with 3 server implementations
- [ ] Modified `main.py` with `--server-mode` flag
- [ ] Message protocol documentation
- [ ] Standalone test scripts
- [ ] Session serialization/deserialization

### Phase 2: C++ Terminal UI (ncurses Frontend) (Week 2-4)

**Port Aider Terminal UI from Python to C++**

**Components to Port** (from `aider/io.py`, 586 lines):

1. **New File**: `VfsShell/qwen_terminal.h` (~300 lines)
   ```cpp
   class QwenTerminal {
   public:
       QwenTerminal();
       ~QwenTerminal();

       // Terminal control
       void initialize();
       void shutdown();
       void clear();
       void refresh();

       // Input/output
       std::string getInput(const std::string& prompt);
       void displayPrompt(const std::string& prompt);
       void displayUserInput(const std::string& text);
       void displayAIResponse(const std::string& text, bool streaming = false);
       void displayStatus(const std::string& status);
       void displayError(const std::string& error);

       // Screen management (interactive updates)
       void updateLine(int line, const std::string& content);
       void scrollToBottom();
       void setCursorPos(int row, int col);

       // Colors and formatting
       enum class Color { Default, Red, Green, Yellow, Blue, Magenta, Cyan, White };
       void setColor(Color fg, Color bg = Color::Default);
       void setBold(bool enabled);
       void setUnderline(bool enabled);

       // History
       void addToHistory(const std::string& input);
       std::vector<std::string> getHistory();
       void saveHistory(const std::string& path);
       void loadHistory(const std::string& path);

   private:
       WINDOW* main_win_;
       WINDOW* input_win_;
       WINDOW* status_win_;
       std::vector<std::string> history_;
       int history_index_;
       bool colors_enabled_;
   };
   ```

2. **New File**: `VfsShell/qwen_terminal.cpp` (~800 lines)
   - ncurses initialization with color support
   - Window management (main display, input area, status bar)
   - Keyboard input handling:
     - Arrow keys for history navigation
     - Ctrl+C to interrupt (not exit)
     - Ctrl+D to detach session
     - Tab for autocomplete (files/commands)
   - ANSI color code parsing and rendering
   - Real-time streaming display (character-by-character or line updates)
   - Input history with persistent storage

3. **Input Handling Patterns** (from `io.py` lines 277-365):
   - Multi-line input support (detect incomplete lines)
   - Command autocompletion (files, slash commands)
   - History search (Ctrl+R)
   - Edit mode (vi/emacs keybindings)

4. **Output Formatting** (from `io.py` lines 411-565):
   - Color-coded messages:
     - User input: default color
     - AI response: cyan/blue
     - Errors: red
     - Status: yellow
     - System: green
   - Markdown rendering (basic: bold, code blocks, lists)
   - Code syntax highlighting (optional, use Pygments-style patterns)
   - Progress indicators for long operations

5. **Screen Layout**
   ```
   ┌─────────────────────────────────────────────────────┐
   │ AI Response Area (scrollable, colored)             │
   │ - Streaming updates shown in real-time             │
   │ - Syntax highlighting for code blocks              │
   │ - Line numbers for code                            │
   │                                                     │
   │ [AI output continues here...]                      │
   └─────────────────────────────────────────────────────┘
   ┌─────────────────────────────────────────────────────┐
   │ Status: Connected to Qwen3-Coder | Model: qwen2.5  │
   └─────────────────────────────────────────────────────┘
   ┌─────────────────────────────────────────────────────┐
   │ You> [user input here with autocomplete]           │
   └─────────────────────────────────────────────────────┘
   ```

6. **Integration with VfsBoot**
   - Use VFS for history: `/qwen/history.txt`
   - Use VFS for sessions: `/qwen/sessions/{id}.json`
   - Use VFS for chat logs: `/qwen/chats/{timestamp}.md`

**Deliverables**:
- [ ] `qwen_terminal.h` with terminal UI interface
- [ ] `qwen_terminal.cpp` with ncurses implementation
- [ ] Input handling (history, autocomplete, keybindings)
- [ ] Output formatting (colors, markdown, streaming)
- [ ] VFS integration for persistence

### Phase 3: Communication Protocol Client (Week 3-4)

**C++ Client for Python Backend Communication**

1. **New File**: `VfsShell/qwen_client.h` (~200 lines)
   ```cpp
   enum class QwenCommMode { Stdin, Pipe, TCP };

   class QwenClient {
   public:
       QwenClient(QwenCommMode mode, const std::string& config);
       ~QwenClient();

       // Connection management
       bool connect();
       void disconnect();
       bool isConnected();

       // Message sending/receiving
       void sendUserInput(const std::string& text);
       void sendCommand(const std::string& cmd, const nlohmann::json& args);

       // Message handling (blocking or callback-based)
       void pollMessages(std::function<void(const Message&)> handler);
       std::optional<Message> receiveMessage(int timeout_ms = -1);

       // Session management
       void saveSession(const std::string& session_id);
       void loadSession(const std::string& session_id);
       void detachSession();
       void attachSession(const std::string& session_id);

   private:
       QwenCommMode mode_;
       std::unique_ptr<BaseCommChannel> channel_;
       std::string session_id_;
   };

   struct Message {
       std::string type;
       std::string session_id;
       uint64_t timestamp;
       nlohmann::json data;
   };
   ```

2. **New File**: `VfsShell/qwen_client.cpp` (~600 lines)
   - `StdinStdoutChannel`: subprocess communication
     - Fork/exec Python backend with `--server-mode stdin`
     - Pipe stdin/stdout, parse JSON messages
   - `NamedPipeChannel`: Unix named pipe communication
     - Create or open named pipes
     - Read/write JSON messages
   - `TCPChannel`: TCP socket communication
     - Connect to Python backend TCP server
     - JSON message framing

3. **Subprocess Management** (for stdin mode)
   - `fork()` + `exec()` to launch Python backend
   - Pass arguments: `--server-mode stdin --model qwen2.5-coder`
   - Pipe setup for bidirectional communication
   - Process lifecycle: start, monitor, restart on crash, stop

4. **Message Queue and Threading**
   - Background thread for message polling
   - Thread-safe message queue (producer/consumer)
   - Callbacks for different message types
   - Graceful shutdown handling

**Deliverables**:
- [ ] `qwen_client.h` with client interface
- [ ] `qwen_client.cpp` with 3 communication modes
- [ ] Subprocess management for stdin mode
- [ ] Message queuing and threading
- [ ] Connection error handling and retry logic

### Phase 4: VfsBoot Shell Command Integration (Week 4-5)

**Add `qwen` Command to VfsBoot**

1. **New File**: `VfsShell/cmd_qwen.cpp` (~400 lines)
   ```cpp
   // Shell command: qwen [options]
   // Options:
   //   --attach <session-id>  - attach to existing session
   //   --detach               - detach from current session
   //   --list-sessions        - list available sessions
   //   --mode <stdin|pipe|tcp> - communication mode
   //   --port <port>          - TCP port (for tcp mode)
   //   --pipe-path <path>     - named pipe path
   //   --model <name>         - model name (qwen2.5-coder, etc.)

   void cmd_qwen(const std::vector<std::string>& args,
                 Vfs& vfs,
                 std::map<std::string, std::string>& env) {
       // Parse arguments
       QwenOptions opts = parseQwenArgs(args);

       // Create client
       QwenClient client(opts.mode, opts.config);

       // Create terminal UI
       QwenTerminal terminal;
       terminal.initialize();

       // Handle session attach/detach
       if (opts.attach) {
           client.attachSession(opts.session_id);
       } else {
           client.connect();  // new session
       }

       // Main interaction loop
       while (true) {
           // Get user input
           std::string input = terminal.getInput("You> ");

           // Check for local commands
           if (input == "/detach") {
               client.detachSession();
               break;
           }
           if (input == "/exit") {
               client.disconnect();
               break;
           }

           // Send to backend
           client.sendUserInput(input);

           // Poll for responses (streaming)
           client.pollMessages([&](const Message& msg) {
               if (msg.type == "ai_response") {
                   terminal.displayAIResponse(msg.data["content"], true);
               } else if (msg.type == "error") {
                   terminal.displayError(msg.data["content"]);
               } else if (msg.type == "status") {
                   terminal.displayStatus(msg.data["content"]);
               }
           });
       }

       terminal.shutdown();
   }
   ```

2. **Wire Command into Shell** (modify `VfsShell/codex.cpp`)
   - Add to command registry (lines ~11000-11200)
   - Add to help text
   - Add to autocomplete

3. **Session Management in VFS**
   - Store sessions: `/qwen/sessions/{id}.json`
   - List sessions: `/qwen/sessions/list`
   - Session metadata: timestamp, model, conversation length
   - Cleanup old sessions (configurable retention)

4. **Configuration Support**
   - Config file: `~/.config/vfsboot/qwen.conf`
   - VFS config: `/env/qwen_config.json`
   - Options: default model, mode, port, pipe path, color scheme

**Deliverables**:
- [ ] `cmd_qwen.cpp` with main command logic
- [ ] Shell integration (command registry, help, autocomplete)
- [ ] Session management (create, list, attach, detach)
- [ ] Configuration file support
- [ ] VFS storage for sessions and history

### Phase 5: Testing and Documentation (Week 5-6)

1. **Unit Tests**
   - Python backend: test all 3 server modes
   - C++ client: test all 3 communication modes
   - Terminal UI: test input handling, display formatting
   - Session management: test attach/detach, persistence

2. **Integration Tests**
   - End-to-end: C++ qwen command → Python backend → AI response
   - Streaming: verify real-time display updates
   - Error handling: connection failures, backend crashes
   - Session persistence: detach/attach workflow

3. **Demo Scripts**
   - `scripts/examples/qwen-demo.cx` - basic usage
   - `scripts/examples/qwen-session-demo.cx` - session management
   - `scripts/examples/qwen-streaming-demo.cx` - streaming display

4. **Documentation**
   - `docs/QWEN_INTEGRATION.md` - architecture and design
   - Update `CLAUDE.md` - add `qwen` command reference
   - Update `README.md` - usage examples
   - Update `AGENTS.md` - implementation notes

5. **Performance Tuning**
   - Optimize JSON parsing (use simdjson or RapidJSON)
   - Optimize terminal rendering (reduce screen updates)
   - Profile subprocess overhead (stdin mode)
   - Benchmark TCP vs pipe vs stdin modes

**Deliverables**:
- [ ] Test suite (Python backend + C++ client)
- [ ] Integration tests (end-to-end workflows)
- [ ] Demo scripts
- [ ] Comprehensive documentation
- [ ] Performance benchmarks and tuning

### Technical Decisions

**Message Format**: JSON (human-readable, debuggable)
  - Alternative: MessagePack (more efficient, binary)
  - Decision: Start with JSON, optimize to MessagePack if needed

**Communication Mode Default**: stdin/stdout
  - Simplest: no file/network setup required
  - Subprocess lifecycle managed by C++
  - Fallback to pipes/TCP for remote scenarios

**Terminal Library**: ncurses
  - Portable, mature, well-documented
  - Alternative: termbox, FTXUI (C++ only)
  - Decision: ncurses for maximum compatibility

**Session Storage**: VFS JSON files
  - `/qwen/sessions/{uuid}.json`
  - Enables attach/detach across VfsBoot restarts
  - Supports overlays for project-specific sessions

**Threading Model**: Background polling thread
  - Main thread: ncurses UI
  - Background thread: message I/O
  - Thread-safe queue for message passing

### Dependencies

**Python (Qwen3-Coder Backend)**:
- No new dependencies (existing Aider requirements)
- Add: `--server-mode` flag to main.py

**C++ (VfsBoot)**:
- `ncurses` - terminal UI (already available on most systems)
- `nlohmann/json` - JSON parsing (header-only, add to vendor/)
- POSIX APIs: `fork()`, `exec()`, `pipe()`, `socket()`
- No additional external libraries required

**Build System**:
- Update `Makefile` to link ncurses: `-lncurses`
- Add `qwen_terminal.cpp`, `qwen_client.cpp`, `cmd_qwen.cpp` to build

### Risk Mitigation

| Risk | Impact | Mitigation |
|------|--------|-----------|
| ncurses complexity | High | Start with simple layout, iterate |
| Subprocess crashes | High | Auto-restart with exponential backoff |
| Message protocol mismatch | Medium | Version protocol, strict schema validation |
| Session corruption | Medium | Atomic writes, backup before overwrite |
| Terminal incompatibility | Low | Test on multiple terminals (xterm, alacritty, tmux) |
| Performance overhead | Low | Profile and optimize hot paths |

### Success Criteria

- [ ] `qwen` command launches interactive terminal
- [ ] User can type prompts and receive AI responses
- [ ] Streaming updates shown in real-time
- [ ] Session detach/attach works correctly
- [ ] All 3 communication modes functional (stdin, pipe, tcp)
- [ ] Terminal UI handles colors, formatting, scrolling
- [ ] Chat history persisted in VFS
- [ ] No data loss on disconnect/crash
- [ ] Documentation complete and accurate
- [ ] Demo scripts working

### Timeline Summary

| Phase | Duration | Focus |
|-------|----------|-------|
| Phase 1 | Week 1-2 | Python backend communication layer |
| Phase 2 | Week 2-4 | C++ ncurses terminal UI |
| Phase 3 | Week 3-4 | Communication protocol client |
| Phase 4 | Week 4-5 | Shell command integration |
| Phase 5 | Week 5-6 | Testing and documentation |

**Total Estimated Effort**: 5-6 weeks for full implementation

### Files to Create/Modify

**New Files** (estimated ~3,000 lines total):
- `qwencoder-eval/instruct/aider/server.py` (~500 lines)
- `VfsShell/qwen_terminal.h` (~300 lines)
- `VfsShell/qwen_terminal.cpp` (~800 lines)
- `VfsShell/qwen_client.h` (~200 lines)
- `VfsShell/qwen_client.cpp` (~600 lines)
- `VfsShell/cmd_qwen.cpp` (~400 lines)
- `docs/QWEN_INTEGRATION.md` (~200 lines)

**Modified Files**:
- `qwencoder-eval/instruct/aider/main.py` (add --server-mode flag)
- `VfsShell/codex.cpp` (register qwen command)
- `VfsShell/codex.h` (add qwen command prototype)
- `Makefile` (add ncurses, new source files)
- `CLAUDE.md` (document qwen command)
- `README.md` (usage examples)
- `AGENTS.md` (implementation notes)

### Next Actions

1. **Immediate**: Validate exploration documents are accurate
2. **Week 1**: Begin Phase 1 - Python backend server.py
3. **Week 2**: Continue Phase 1, start Phase 2 - Terminal UI
4. **Week 3**: Complete Phase 2, start Phase 3 - Client
5. **Week 4**: Complete Phase 3, start Phase 4 - Integration
6. **Week 5**: Complete Phase 4, start Phase 5 - Testing
7. **Week 6**: Complete Phase 5, polish and release

---

## Internal U++ Builder (see docs/internal_umk_plan.md)
- Stand up a minimal in-process `umk` pipeline leveraging `UppToolchain` metadata (compiler/linker, flags, includes). Reuse the shared `BuildGraph` with new command types for compile/link and ensure host paths come via `Vfs::mapToHostPath`.
- Emit per-translation-unit compile nodes and package-level link nodes when no builder `COMMAND` is provided; retain custom commands as override. Handle output directory selection and dependency ordering inside the workspace.
- Add regression coverage (scripts/unittst/upp_internal_umk.md) plus documentation describing usage and limitations; track follow-ups for BLITZ/caching, multi-language sources, and remote execution hooks.

## Immediate Next Moves
- Harden the C++ AST string escaping and scan dumps for malformed literals.
- Broaden the λ-syntax (multi-arg lambdas, let-binding sugar) to reduce ceremony in larger programs.
- Design Stage2 agent goals on top of the VfsShell substrate (richer codegen, persistence, higher-level scripting).

## U++ compatibility
- Support U++ assembly file: e.g. ~/.config/u++/theide/MyApps.var
	- have active workspace for assembly file
	- workspace has active primary package and it's dependency packages
	- have ncurses gui with: main code editor area, package list at top left and file list (of package) at bottom left. have menu bar too

- Implement U++ build method + package build pipeline (umk subset)
	- load package configuration (.upp) settings into builder context
	- honour toggles: use shared / only shared / only static
	- support debug / release toolchains and flag bundles
	- handle build flags (GUI, USEMALLOC, etc.) and verbosity switches
	- allow exporting makefiles and performing clean targets
	- document BLITZ / advanced optimizations as deferred follow-up
	- ✅ introduced shared build graph + new `upp.wksp.build` CLI for workspace/package compilation (dry-run + plan)

	
## Upcoming: web browser gui: important
- **Phase 1 COMPLETE** - Basic terminal working ✅
  - ✅ libwebsockets-based HTTP/WebSocket server
  - ✅ xterm.js terminal emulation in browser
  - ✅ Command-line flags: --web-server, --port
  - ✅ Embedded HTML (zero external file dependencies)
  - ✅ Multiple concurrent sessions supported
  - ✅ Responsive modern UI with status indicators
  - See "Completed" section for full details
- **Phase 2 COMPLETE** - WebSocket command execution ✅
  - ✅ Command callback registration system
  - ✅ Full integration with VfsShell command dispatcher
  - ✅ Bidirectional terminal I/O (commands → VFS → responses)
  - ✅ Error handling with ANSI color codes
  - ✅ Support for command chains (&&, ||, |)
  - ✅ All VFS commands work through web terminal
  - ✅ Multiple concurrent sessions with isolated state
  - See "Completed" section for technical details
- **Phase 3 PLANNED** - Advanced GUI features:
- Architecture:
	- Backend: C++ with Crow or Boost.Beast for HTTP server
		- Development: Standalone server on localhost:8080 (./codex --web-server --port 8080)
		- Production standalone: Same binary with --serve-static flag for bundled frontend
		- Production Apache: FastCGI binary (./codex-cgi.fcgi) compiled with -DCGI_MODE
		- Direct VFS access (no IPC overhead), single binary deployment
		- HTTP/WebSocket endpoints exposing VFS state and planner context
	- Frontend: WinBox.js (floating windows) + Golden Layout (dockable panels) + xterm.js (terminal) + Cytoscape.js (graph visualization)
		- Development: npm run dev with hot-reload, proxies API to C++ backend
		- Production: Bundled static files served by C++ backend or Apache
		- Windows-style desktop GUI feel with draggable/resizable windows, docking, and native-like controls
	- Serverless/WASM mode: Support web-llm https://github.com/mlc-ai/web-llm for client-side LLM inference
- Features:
	- Terminal: xterm.js running live codex sessions with ANSI color support
	- Script Demo Gallery: Interactive demos for all scripts in scripts/ directory
		- Category-based navigation (Logic System, Planner, VFS/Mount, AI Integration, Full Demos)
		- Progressive learning paths (EASIEST → INTERMEDIATE → ADVANCED → EXPERT)
		- Search and filter by difficulty, tags, prerequisites
		- Demo cards with title, description, difficulty badges, duration estimates
		- Interactive modes: Run in terminal, View code (Monaco editor), Step-by-step with explanations
		- Preset tours: "New to VfsBoot" (16min), "Advanced Developer", "AI Integration"
	- **Internal System Visualization** (debugging-level, NOT eye candy):
		- **VfsNode Object Graph**: Live object inspector showing memory addresses, pointers, vtables, std::map internals, object sizes
			- Click pointers to follow references, see container sizes, live updates on create/delete
		- **Shell Command Dispatcher**: Function call tracing with stack, this pointers, container sizes, return values, timing (μs)
		- **Overlay System**: Memory layout, hash map bucket distribution, overlay priority stack, path resolution trace
		- **AST Node Hierarchy**: Vtable pointers, string SSO state, vector capacity vs size, memory layout with padding
		- **Tag System / BitVector**: Actual bit patterns (binary/hex), bit-to-tag mapping, XOR operations animated, hash computation
		- **Logic Engine / Rule Application**: Rule evaluation order, which rules fired vs skipped, bit operations for formulas, inference chains
		- **Memory Allocations**: Heap visualization, object addresses, allocation/deallocation tracking, fragmentation, leak detection
		- **WebSocket Message Flow**: Backend/frontend message serialization, JSON structure, network latency tracking
		- **Context Builder**: Priority queue state (heap viz), token budget bar filling up, node selection (green=included, red=dropped)
		- **Planner State Machine**: FSM diagram with current state highlighted, transition triggers, state variables, invalid transitions
	- C++ Instrumentation API:
		- Classes expose toDebugJson() methods with ptr addresses, sizes, internal state
		- notifyGuiDebugger() function sends events via WebSocket (vfs_node_created, function_call, rule_fired, etc.)
		- Compile with -DCODEX_WEB_GUI to enable instrumentation hooks
	- Frontend Visualization Components:
		- Live object inspector (Chrome DevTools style)
		- Call stack viewer with timing
		- Memory heap viewer with allocation tracking
		- Inference graph for logic rules
		- Interactive object graph with clickable pointers
	- Network Topology: Visualize distributed VfsShell instances with Cytoscape.js (nodes=machines, edges=connections, mounted paths)
	- Real-time updates via WebSockets for live session monitoring
	- Image handling for AI vision tasks (upload/display/annotate)
	- Form-based interfaces for multi-step plan editing and hypothesis refinement

	
## Upcoming: important (in order)

### Phase 1: libclang Foundation - AST Structure, Locations, Type Links (COMPLETE)
- **Goal**: Parse hello world C++ program, dump AST, regenerate C++ code, compile and run
- **Purpose**: Fast codebase indexing for efficient AI context building

- **Status**: Core parsing implementation complete with code span tracking ✅
  - ✅ Makefile updated with libclang linking (-I/usr/lib/llvm/21/include -L/usr/lib/llvm/21/lib64 -lclang)
  - ✅ libclang verified working (test program compiled and ran successfully)
  - ✅ Complete AST node structure defined in codex.h (lines 966-1356):
    - ✅ SourceLocation struct for file:line:column tracking **with code span length in bytes**
    - ✅ ClangAstNode base class (inherits from AstNode → VfsNode)
    - ✅ Type nodes (5 types): ClangType, ClangBuiltinType, ClangPointerType, ClangReferenceType, ClangRecordType, ClangFunctionProtoType
    - ✅ Declaration nodes (9 types): ClangTranslationUnitDecl, ClangFunctionDecl, ClangVarDecl, ClangParmDecl, ClangFieldDecl, ClangClassDecl, ClangStructDecl, ClangEnumDecl, ClangNamespaceDecl, ClangTypedefDecl
    - ✅ Statement nodes (9 types): ClangCompoundStmt, ClangIfStmt, ClangForStmt, ClangWhileStmt, ClangReturnStmt, ClangDeclStmt, ClangExprStmt, ClangBreakStmt, ClangContinueStmt
    - ✅ Expression nodes (8 types): ClangBinaryOperator, ClangUnaryOperator, ClangCallExpr, ClangDeclRefExpr, ClangIntegerLiteral, ClangStringLiteral, ClangMemberRefExpr, ClangArraySubscriptExpr
    - ✅ ClangParser class structure defined
    - ✅ Shell command prototypes: cmd_parse_file(), cmd_parse_dump()

- **Implementation Complete (VfsShell/clang_parser.cpp, ~1200 lines)**:
  - ✅ Implemented SourceLocation::toString()
  - ✅ Implemented dump() methods for all node types (31 methods)
  - ✅ Implemented ClangParser::parseFile() - parse C++ file from disk
  - ✅ Implemented ClangParser::parseString() - parse C++ source from string
  - ✅ Implemented ClangParser::convertCursor() - main cursor-to-VfsNode converter
  - ✅ Implemented ClangParser::getLocation() - extract source location from CXCursor
  - ✅ Implemented ClangParser::getTypeString() - convert CXType to string
  - ✅ Implemented ClangParser::convertType() - convert CXType to ClangType node
  - ✅ Implemented ClangParser::visitChildren() - recursive AST traversal
  - ✅ Implemented ClangParser::handleDeclaration() - process declaration cursors
  - ✅ Implemented ClangParser::handleStatement() - process statement cursors
  - ✅ Implemented ClangParser::handleExpression() - process expression cursors
  - ✅ Implemented cmd_parse_file() - shell command for `parse.file <path>`
  - ✅ Implemented cmd_parse_dump() - shell command for `parse.dump [path]`
  - ✅ Wired up commands in shell dispatcher (codex.cpp lines 11213-11233)
  - ✅ Added commands to help text (lines 7921-7923)

- **Testing Complete**:
  - ✅ Created tests/hello.cpp with simple main() function
  - ✅ Parsed hello.cpp → verified AST created in /ast/translation_unit
  - ✅ Dumped AST → verified output shows correct structure with source locations **and byte lengths**
  - ✅ Successfully parsed main function at tests/hello.cpp:3:1 [75 bytes] with CompoundStmt [64 bytes] and ReturnStmt [8 bytes]
  - ✅ AST includes full details: function declarations, parameters, statements, expressions with file:line:column locations and code span lengths
  - ✅ Code span tracking enables precise source extraction for AI context building

### Phase 1.5: C++ Code Regeneration (COMPLETE) ✅
- **Goal**: Regenerate C++ source code from parsed AST
- **Implementation**: VfsShell/clang_parser.cpp (~150 lines added)
  - ✅ Source extraction using SourceLocation offset + length
  - ✅ File caching for efficiency (std::map<filepath, content>)
  - ✅ generateCppCode() function for AST-to-source conversion
  - ✅ Shell command: `parse.generate <ast-path> <output-path>`
  - ✅ VFS integration - generated code written to VFS
  - ✅ Successfully tested: parse → generate → compile → run workflow
- **Testing**:
  - ✅ Parsed tests/hello.cpp → generated identical source
  - ✅ Regenerated code is 97 bytes (exact match to original)
  - ✅ Verification: `parse.file tests/hello.cpp` → `parse.generate /ast/translation_unit /generated/hello.cpp`
  - ✅ Output compiles and runs successfully with c++ -std=c++17
- **Next Steps**: Phase 2 - Preprocessor Integration

- **Out of scope for Phase 1**:
  - Template instantiations (deferred to Phase 3)
  - Preprocessor hooks (deferred to Phase 2)
  - Complex expressions (cast operators, lambda, initializer lists - Phase 3)
  - Full CXCursor coverage (100+ types - Phases 2-4)

### Phase 2: Preprocessor Integration
- Collect preprocessor state: macros, includes, conditional compilation
- Hooks: CXCursor_MacroDefinition, CXCursor_MacroExpansion, CXCursor_InclusionDirective
- Store in `/ast/<file>/preprocessor/`
- Track macro expansion locations and include chains

### Phase 3: Complete CXCursor Coverage (C++ Specific)
- C++ features: CXCursor_Constructor, CXCursor_Destructor, CXCursor_CXXMethod, CXCursor_CXXAccessSpecifier
- Templates: CXCursor_ClassTemplate, CXCursor_FunctionTemplate, CXCursor_TemplateTypeParameter, CXCursor_TemplateRef
- Template instantiations: clang_getSpecializedCursorTemplate, clang_getTemplateCursorKind
- Advanced expressions: CXCursor_CXXStaticCastExpr, CXCursor_LambdaExpr, CXCursor_InitListExpr
- Operator overloading, conversion functions, friend declarations

### Phase 4: Advanced Features
- Multi-translation unit support with shared AST database
- Cross-reference tracking: find all usages of function/variable
- Call graph construction for context builder optimization
- Type hierarchy analysis for inheritance chains
- Symbol resolution across files

### Phase 5: Clang Test Suite Import
- Batch import Clang's test suite files
- Validation against known-good ASTs
- Performance benchmarking for large codebases
- Integration with context builder for token budget optimization

### Phase 6: AI Context Optimization
- Fast symbol lookup: "find all calls to function X" without full VFS scan
- Incremental parsing: only re-parse modified files
- Semantic search: "functions that return std::vector<int>"
- Dependency analysis: "what needs recompilation if I change this header?"

## Upcoming: less important

### Shell UX Improvements
- arrows up and down doesn seem to loop history in web browser CLI
- in sub-CLI loops, ctrl+c should exit the loop (like in bash etc)
	- in case of runnin scripts in "run <script>" ctrl+z should exit the script runner (just like in bash etc)
- **Customizable shell prompt prefix with colors**:
  - Current: plain text prompt prefix (e.g. "sblo@Gen2 ~/VfsBoot $")
  - Goal: Programmable prompt like bash $PS1 with ANSI colors
  - Support both bash-style `export VAR=value` and tcsh-style `setenv VAR value`
  - Read `.vfshrc` and `.vfshprofile` startup files on launch
  - Create reference example in `share/` directory with best practices
  - Sub-CLI modes (like `discuss`) should inherit custom prompt prefix
- **Enhanced `ls` command with colors and grid layout**:
  - Default: Grid view with colors by file type (like modern ls)
  - Add `--list` flag for traditional single-column output
  - Add `--long` / `-l` flag for detailed view (size, date, permissions)
  - Color scheme: directories=blue, executables=green, files=default, mounts=cyan
- **Builtin minimal text editor** (IMPORTANT):
  - Implement editor similar to nano/micro/FreeBSD ee
  - Needed for quick file editing without external dependencies
  - Essential for self-contained development workflow
  - Should integrate with VFS (edit both real and virtual files)
- **Command autocompletion improvements**:
  - Current: File/directory path completion only
  - Goal: Command name completion (TAB after partial command)
  - Store commands in VFS paths (like /bin/make) for unified completion
  - Support mounted path completion (recognize mount boundaries)
- **CLI editing improvements**:
  - Add Ctrl+U (clear line before cursor) and Ctrl+K (clear line after cursor)
  - Add Home/End key support for cursor movement
  - Fix non-ASCII input (e.g. 'ä' character does nothing currently)
- **Double-enter bug fixes**:
  - "ai some text" command requires pressing enter twice
  - "ai some message" script needs same fix

### AI Discussion Enhancements
- **User profiling for AI discussions**:
  - Automatically build user preference profile during `discuss` sessions
  - Track patterns: what user always asks separately, repeated concerns, common requests
  - Avoid repeating explanations user already knows
  - Store profile in VFS (e.g. `/env/user_profile.json`)
  - Add `ai.raw.profiling` command for raw discussion mode profiling
  - Use good-faith heuristics to improve AI responses over time
  - Profile persists across sessions, learns user's style and needs
- **Smart plan addition command** (`ai.add.plan`):
  - User provides raw text without needing to know current plan context
  - AI analyzes existing plan hierarchy and locates correct insertion point
  - Opens `discuss` mode if needed for clarification
  - Detects contradictions with existing plan items
  - On conflict: stops, warns user, requests confirmation before adding
  - Shows proposed location and asks for approval: "Add to /plan/subplan/goals? (y/n)"
  - Handles cases: append to existing section, create new section, merge similar items
  - Makes planning collaborative without requiring manual VFS navigation

### Web Server Enhancements (Phase 2)
- **WebSocket terminal functionality**:
  - Current: WebSocket layer exists but not wired to command execution
  - Goal: Full bidirectional terminal I/O (stdin/stdout redirection)
  - Question: Should WebSocket work for me? Needs verification
  - Add comprehensive tests for web browser interactions
  - Challenge: Testing web browser without actual browser (headless Chrome/Playwright?)
- **Session management for web terminal**:
  - Attachable/detachable sessions (like tmux/screen)
  - Persist session state when switching devices
  - Recover session after connection drops (critical for mobile/remote usage)
  - Store session snapshots in VFS for recovery
  - Multiple named sessions per user

### Shell Features
- **Script sourcing**:
  - CLI command: `source <some_script.sh>` (also alias: `. <some_script.sh>`)
- **Commandline argument enhancements**:
  - Add: --llama, --openai, --version/-v, --help/-h, etc.
- **Shell compatibility**:
  - sh compatibility mode
  - Login shell support (commandline -l flag)
  - Evaluate additional flags needed for POSIX compatibility
- **Turing-complete scripting** (DISCUSS FIRST):
  - Goal: Intuitive scripting more like tcsh than bash
  - Need to design syntax and grammar before implementing
  - Should support: conditionals, loops, functions, variables
  - Discuss with user before implementation
- **Path autocompletion**:
  - Recognize mounted paths and complete system paths
  - Handle VFS-to-filesystem boundary crossings

### Advanced Features
- **Netcat-like networking**:
  - Client and server like `nc` command
  - TTY server for CLI remote access
  - Leave room for minimal SSH implementation later
  - Remote command oneliner: single command for remote execution
  - Advanced remote shell integration
  - File transfer like scp (without encryption layers, like nc)
- **Internal logging to VFS**:
  - Some internal logs visible in /log (VFS)
  - Debug trace, command history, error logs
- **AST resolver for multi-language support**:
  - cpp ast node resolver
  - Keep codebase compatible with non-RAII, dynamic memory languages (Java, C#)
  - Add support for: Java, C#, TypeScript, JavaScript, Python
- **Command aliases**:
  - Alias for script functions (e.g. cpp.returni → "cri")
- **Multi-language sexp converters**:
  - sexp ↔ JavaScript converter
  - Also for: Python, PowerShell, Bash, etc.

### Documentation Improvements
- **File type documentation**:
  - Explain different purposes of .sexp, .cx, .cxpkg files
  - Discuss with user if unsure about conventions
  - Write comprehensive docs in README.md and AGENTS.md
  - Create example solution with multiple cxasm & cxpkg packages
  - Include multiple cpp/h files, compile and run successfully
	
## Upcoming: less important or skip altogether
- byte-vm (maybe overkill?) for sexp, or cx script

## Upcoming: important tests
- we should have small real-life examples, where we create "int main()". Then we need something new there couple of times: user -> system -> user -> system interaction; and the system searches and modifies the ast tree.
	- you should plan like 5 progressively more difficult interaction demos
- we should have actual programming project in a directory, with multiple files, which is mounted to the vfs as overlay. the code is kept both in persistent vfs-file and as cpp/h/Makefile files


## Completed
- **Bootstrap Build Dependency Detection and Static Analysis** (2025-10-10):
  - **COMPLETE**: Comprehensive library detection and static analysis integration
  - Dependency checking features:
    - Automatic detection of all required libraries before build
    - Check for: C++ compiler, pkg-config, BLAKE3, Subversion libs, libclang, libwebsockets, pthread
    - Helpful error messages with library names and Gentoo installation commands
    - Multi-distribution support (Gentoo, Debian/Ubuntu, Fedora/RHEL, Arch)
    - Integrated into both bootstrap (`./build.sh -b`) and regular make builds
    - Prevents cryptic linker errors by detecting missing dependencies early
  - Static analysis features:
    - clang-tidy integration for comprehensive code quality checks
    - Interactive prompt after successful build: "Would you like to run static analysis?"
    - Command-line flags: `--static-analysis` (auto-run), `--no-static-analysis` (skip)
    - Analyzes all VfsShell source files with extensive check categories
    - Logs saved to `.static-analysis-*.log` files (gitignored)
    - Checks for: bugs, modernization, performance, readability, best practices
  - Documentation updates:
    - CLAUDE.md: New "Static Analysis" section with usage examples and workflow
    - build.sh: Updated help text and usage examples
    - .gitignore: Added static analysis log files
  - Workflow integration:
    - Recommended to run after completing tasks
    - Suggested before git commits
    - CI/CD friendly with `--no-static-analysis` flag
  - **Status**: 100% complete, production ready

- **Minimal GNU Make Implementation with Bootstrap Build** (2025-10-09):
  - **COMPLETE**: Internal make utility for build automation within VfsBoot + standalone bootstrap executable
  - Implementation features:
    - Makefile parser supporting basic GNU make syntax
    - Variable substitution: `$(VAR)` and `${VAR}` expansion
    - Variable assignment operators: `=` (recursive), `:=` (immediate), `?=` (conditional with env check)
    - Shell function support: `$(shell command)` for inline command execution
    - Automatic variables: `$@` (target name), `$<` (first prerequisite), `$^` (all prerequisites)
    - .PHONY target support for always-rebuild targets
    - Timestamp-based dependency checking (VFS + filesystem)
    - Recursive dependency graph builder with cycle detection
    - Shell command execution via popen
    - Verbose mode for debugging with `-v` or `--verbose`
    - Environment variable integration for ?= assignments
  - Two implementations:
    - **Internal VFS make**: Integrated into codex VFS shell (`make [target] [-f makefile] [-v|--verbose]`)
    - **Standalone bootstrap make**: `make_main.cpp` (472 lines) compiled to `./codex-make` for bootstrapping
  - Bootstrap build system:
    - **Stage 1**: C++ compiler compiles `make_main.cpp` → `./codex-make` (71K executable)
    - **Stage 2**: `./codex-make` builds full codex binary from Makefile
    - Usage: `./build.sh -b` or `./build.sh --bootstrap`
    - Zero dependencies: only C++ stdlib + POSIX (no external make required)
    - Enables self-contained build on systems without GNU make
  - Command interface:
    - VFS make: `make [target] [-f makefile] [-v|--verbose]`
    - Standalone make: `./codex-make [target] [-f Makefile] [-v|--verbose] [-h|--help]`
    - Default Makefile: `/Makefile` (VFS) or `./Makefile` (standalone)
    - Default target: `all` (or first rule if `all` not defined)
  - Architecture:
    - `MakeRule` struct: target, dependencies, commands, is_phony flag
    - `MakeFile` class: parse(), expandVariables(), expandAutomaticVars(), build()
    - BuildResult struct with success/output/targets_built/errors tracking
    - getModTime() checks both VFS and filesystem for timestamp comparison
    - parseLine() handles variable assignments, rule definitions, .PHONY declarations
  - Features NOT implemented (minimal subset):
    - Pattern rules (`%.o: %.c`)
    - Built-in implicit rules
    - Conditional directives (ifdef/ifndef/else/endif)
    - Advanced functions ($(wildcard), $(patsubst), $(filter), etc.)
    - Multiple target support in single rule
    - Include directives
    - Special targets beyond .PHONY
    - Parallel job execution (-j)
  - Testing:
    - Successfully compiles with only warnings
    - Added to help text and autocomplete
    - Bootstrap build tested: `./build.sh -b` successfully builds codex (1.4M)
    - Standalone make compiles to 71K executable
    - Variable expansion verified with complex Makefiles ($(shell ...), :=, ?=)
  - Documentation:
    - Added to CLAUDE.md with comprehensive feature list
    - Command added to help output
    - Demo script created in scripts/examples/make-demo.cx
    - build.sh updated with bootstrap documentation
  - Files modified:
    - VfsShell/codex.h: Added MakeRule and MakeFile structures (lines 1818-1876)
    - VfsShell/codex.cpp: Implemented make functionality (~320 lines, 7929-8252) + shell command (10993-11064)
    - make_main.cpp: Standalone make executable (472 lines, NEW)
    - build.sh: Added bootstrap build support with -b/--bootstrap flag
    - CLAUDE.md: Added make utility documentation
  - **Status**: 100% complete for minimal subset with bootstrap support, ready for production use

- **In-Binary Sample Runner** (2025-10-09):
  - **COMPLETE**: Full implementation of `sample.run` command for in-binary C++ compilation and execution
  - Implementation features:
    - Command registered in shell dispatcher and added to help text
    - Deterministic state reset: clears `/astcpp/demo`, `/cpp/demo.cpp`, `/logs/sample.*`, `/env/sample.status`
    - AST construction using internal command execution: `cpp.tu`, `cpp.include`, `cpp.func`, `cpp.print`, `cpp.returni`, `cpp.dump`
    - Compiler detection: checks `/env/compiler`, `$CXX`, defaults to `c++`
    - Temporary file management: creates `/tmp/codex_sample_<pid>.cpp` and `/tmp/codex_sample_<pid>`
    - Compilation with `-std=c++17 -O2` flags
    - Execution with output capture
    - VFS logging: `/logs/sample.compile.out`, `/logs/sample.compile.err`, `/logs/sample.run.out`, `/logs/sample.run.err`
    - Status tracking: `/env/sample.status` with SUCCESS/FAILED, exit codes, timing
    - Flag support: `--keep` (preserve temp files), `--trace` (verbose diagnostics)
  - Documentation updates:
    - CLAUDE.md: Added Sample Runner section with command reference and flags
    - README.md: Updated Sample pipeline section with in-binary runner as recommended approach
    - Help text: Added `sample.run [--keep] [--trace]` with description
    - Command list: Added to autocomplete suggestions
  - Testing:
    - Successfully builds "Hello from codex-mini sample!" program
    - Compilation time: ~320ms on test system
    - All output captured correctly in VFS logs
    - Status file tracks success/failure and timing
    - Both flags (--keep, --trace) verified working
  - Integration:
    - Replaces external Makefile pipeline for simple demos
    - Can be invoked from scripts, tests, and interactive sessions
    - No external dependencies beyond C++ compiler
  - **Next steps**: Extend automated tests to use `sample.run`, add more complex demo programs
  - **Status**: 100% complete and ready for production use

- **Planner/Context System CLI Integration** (2025-10-09):
  - **COMPLETE**: Full integration of planner/context/feedback systems into CLI with comprehensive help documentation
  - Added feedback pipeline commands to help text:
    - `feedback.metrics.show [top_n]` - Display metrics history and top triggered/failed rules
    - `feedback.metrics.save [path]` - Save metrics to VFS file
    - `feedback.patches.list` - List pending rule patches
    - `feedback.patches.apply [index|all]` - Apply pending patches
    - `feedback.patches.reject [index|all]` - Reject pending patches
    - `feedback.patches.save [path]` - Save patches to VFS file
    - `feedback.cycle [--auto-apply] [--min-evidence=N]` - Run full feedback cycle
    - `feedback.review` - Interactive patch review
  - Updated CLAUDE.md with comprehensive command reference:
    - Planner System section with all plan.* commands
    - Context Builder section with context.build and filtering commands
    - Hypothesis Testing System with all 5 complexity levels
    - Feedback Pipeline section with metrics and patch management
    - Logic Engine section with inference and consistency checking
    - Advanced Visualization with tree.adv command
  - All systems previously implemented are now fully documented and discoverable:
    - Planner commands: plan.create, plan.goto, plan.discuss, plan.jobs.*, etc.
    - Context commands: context.build, context.build.adv, context.filter.*
    - Hypothesis commands: hypothesis.query, hypothesis.errorhandling, etc.
    - Feedback commands: feedback.metrics.*, feedback.patches.*, feedback.cycle
    - Logic commands: logic.init, logic.infer, logic.check, logic.explain
    - Visualization: tree.adv with multiple display options
  - Build status: Clean compilation with only warnings, all tests passing
  - **Status**: 100% complete, all core pieces stable and integrated

- **AI Planner Integration with Scenario Harness** (2025-10-09):
  - **COMPLETE**: Replaced stub plan generation with actual AI planner calls
  - Implementation changes:
    - Modified `ScenarioRunner::executePlanGeneration()` in `harness/runner.cpp` to call `call_ai()`
    - Constructs planning prompt similar to `discuss` command planning mode
    - Prompt includes: user intent, instructions to break down into structured plan, available commands
    - Added error handling for empty responses and exceptions
  - Language support enhancements:
    - Modified `system_prompt_text()` in `VfsShell/codex.cpp` to support English mode
    - Added `CODEX_ENGLISH_ONLY` environment variable to force English responses
    - Auto-detects language from `LANG` environment variable (Finnish/English)
    - Ensures AI responses match user's language preference
  - Testing results:
    - AI successfully generates plans for scenario user intents
    - Both OpenAI and Llama providers supported
    - Response caching works correctly
    - Example: "Create a text file with hello world content" → generates detailed plan with steps
  - Known limitations:
    - Plan verification in scenarios uses exact text matching, which fails with AI-generated plans
    - AI generates more detailed/verbose plans than simple expected plans in test scenarios
    - Future work: Implement semantic plan comparison instead of exact text matching
  - Build status:
    - Both `planner_demo` and `planner_train` compile and run successfully
    - Binary sizes: planner_demo (908K), planner_train (934K)
  - **Next steps**: Improve plan verification to use semantic matching, integrate with feedback pipeline for rule evolution
  - **Status**: 100% functional, AI planner fully operational in scenario harness

## Completed
- **Feedback Pipeline Integration with Scenario Harness** (2025-10-09):
  - **COMPLETE**: Full integration of feedback pipeline with scenario harness for real metrics collection
  - Integration points:
    - **ScenarioRunner**: Metrics collection at all phases (startRun, recordSuccess, recordIterations, recordPerformance, recordOutcome, finishRun)
    - **BreakdownLoop**: Iteration tracking for both success and failure paths
    - **planner_demo**: Displays metrics summary (average success rate, iterations, top triggered/failed rules, last run details)
    - **planner_train**: Aggregates metrics across scenarios, exports to JSON with full metrics data
  - Implementation details:
    - Added MetricsCollector pointer to ScenarioRunner and BreakdownLoop constructors
    - Execution timing tracked with std::chrono::steady_clock
    - Added <chrono> include to runner.h for timing support
    - Metrics recorded after all scenario phases (success or failure)
    - VFS node counting stubbed (TODO: implement proper counting)
  - planner_demo enhancements:
    - Shows average success rate and iterations
    - Displays top 5 triggered and failed rules
    - Detailed last run metrics (scenario name, success, iterations, execution time, VFS nodes, error)
  - planner_train enhancements:
    - Comprehensive JSON export with metrics: execution_time_ms, vfs_nodes_examined, rules_triggered, rules_failed
    - Aggregated metrics summary with top 10 triggered/failed rules
    - JSON escape function for safe string serialization
    - Enhanced TrainingData struct with all metrics fields
  - Test scenarios:
    - scenarios/basic/simple-file-creation.scenario: Text file creation test
    - scenarios/basic/multi-dir-creation.scenario: Nested directory creation test
  - Build status:
    - Both planner_demo (908K) and planner_train (926K) compile successfully
    - Only compiler warnings, no errors
    - Binary sizes reasonable for integration binaries
  - Documentation:
    - All metrics collection documented in code comments
    - Integration workflow: run scenarios → collect metrics → analyze patterns → generate patches → review → apply → improved rules
  - **Next steps**: Fix runtime segfault issue, integrate actual AI planner, add real rule triggering/failure detection
  - **Status**: 100% integrated and buildable, runtime execution needs debugging

- **Feedback Pipeline for Planner Rule Evolution** (2025-10-09):
  - **COMPLETE**: Automated metrics collection and rule evolution system
  - Implemented components:
    - **PlannerMetrics**: Tracks execution metrics (success/failure, iterations, rules triggered/failed, performance, outcomes)
    - **MetricsCollector**: Real-time metrics recording with history tracking and analysis
    - **RulePatch**: Rule modification proposals (Add, Modify, Remove, AdjustConfidence operations)
    - **RulePatchStaging**: Staged review system with pending/applied/rejected queues
    - **FeedbackLoop**: Orchestrator for pattern detection and patch generation
  - Pattern detection thresholds:
    - High-performing rules (>90% success, ≥5 triggers) → increase confidence
    - Low-performing rules (<50% success, ≥3 triggers) → decrease confidence
    - Failing rules (<20% success, ≥5 triggers) → propose removal
  - Shell commands:
    - `feedback.metrics.show [top_n]` - Display metrics summary
    - `feedback.metrics.save [path]` - Save metrics to VFS
    - `feedback.patches.list` - List pending patches
    - `feedback.patches.apply [index|all]` - Apply patches
    - `feedback.patches.reject [index|all]` - Reject patches
    - `feedback.patches.save [path]` - Save patches to VFS
    - `feedback.cycle [--auto-apply] [--min-evidence=N]` - Run complete cycle
    - `feedback.review` - Interactive patch review
  - Integration features:
    - Fully integrated with LogicEngine and ImplicationRule system
    - VFS persistence for metrics and patches
    - Ready for scenario harness integration (planner_demo/planner_train)
    - Optional AI assistance support (OpenAI/Llama)
  - Documentation:
    - docs/FEEDBACK_PIPELINE.md - Complete usage guide and architecture
    - scripts/examples/feedback-pipeline-demo.cx - Working demonstration
  - Build status:
    - All code compiles successfully
    - Global objects initialized in main()
    - No integration changes needed for existing code
  - **Next steps**: Integrate with scenario harness to collect real metrics, run feedback cycles on actual planner executions
  - **Status**: 100% complete, ready for production use

- **Scenario Harness for Planner Testing and Training** (2025-10-09):
  - **COMPLETE**: Full testing infrastructure for planner validation and training data generation
  - Implemented components:
    - **Scenario File Parser**: harness/scenario.{h,cpp} with structured [SETUP], [USER_INTENT], [EXPECTED_PLAN], [ACTIONS], [VERIFICATION] sections
    - **ScenarioRunner**: Five-phase execution engine (Setup, Plan Generation, Verification, Actions, Final Checks)
    - **BreakdownLoop**: Iterative refinement with VFS snapshot rollback for validation
    - **planner_demo**: Interactive single-scenario runner with verbose mode and iteration control
    - **planner_train**: Batch training data generator with JSON export
  - Build integration:
    - Successfully builds with make, cmake, and umk
    - Solution: Wrapped main() in codex.cpp with #ifndef CODEX_NO_MAIN / #endif
    - Compile flag -DCODEX_NO_MAIN excludes main() from planner binaries
  - Binary sizes:
    - codex: 1.3M (full shell with main())
    - planner_demo: 850K (scenario runner)
    - planner_train: 870K (training data generator)
  - Documentation:
    - harness/README.md: Complete usage guide, file format, integration notes
    - Example scenarios in scenarios/basic/hello-world.scenario
  - **Next steps**: Integrate actual AI planner (currently uses stub), execute actions through shell dispatcher
  - **Status**: 100% buildable, ready for integration with live planner

- **Scope Store System with Binary Diffs and Feature Masks** (2025-10-09):
  - **COMPLETE**: Core scope store infrastructure for deterministic context building and feature-gated development
  - Implemented components:
    - **BitVector**: 512-bit vector class with XOR hashing for O(1) operations
    - **FeatureMask**: Feature toggle system with 40+ enumerated features (VFS, AST, AI, Planner, Codegen, Experimental)
    - **BinaryDiff**: SVN delta-based binary diff compression (svn_txdelta algorithm)
    - **ScopeSnapshot**: VFS state snapshots with incremental binary diffs from parent
    - **ScopeStore**: Snapshot management, feature registry, persistence (save/load .scope files)
    - **DeterministicContextBuilder**: Reproducible context generation with stable sorting, sampling, metadata
  - Architecture features:
    - Incremental snapshots using binary deltas (efficient storage)
    - Feature masks enable/disable code regions at snapshot time
    - Deterministic ordering for reproducible AI context building
    - Context diffing between snapshots (added/removed/modified paths)
    - Replay system for context building across multiple snapshots
  - Implementation status:
    - Full structures and method signatures in VfsShell/codex.h
    - Core methods implemented in VfsShell/codex.cpp
    - VFS traversal uses stub implementations (TODO: use actual Vfs::listDir API)
    - Build system updated with libsvn_delta and libsvn_subr
    - Documentation in docs/SCOPE_STORE.md
  - **Next steps**: Implement VFS traversal, add scope.* shell commands, create demo scripts
  - **Note**: Foundation complete for scenario harness binaries and training data generation

- **Planner Core Engine for Breakdown/Context** (2025-10-08):
  - **COMPLETE**: Full integration of planning system, logic solver, action planner, C++ AST builder, and hypothesis testing
  - All major subsystems implemented and working together:
    - Tag system with enumerated registry (TagId = uint32_t)
    - Hierarchical plan nodes (Root, SubPlan, Goals, Ideas, Strategy, Jobs, Deps, Implemented, Research, Notes)
    - Shell commands for plan management (plan.create, plan.goto, plan.forward/backward, plan.context.*, plan.jobs.*, plan.save, plan.status)
    - Planning loop integration (discuss command, intent classification, auto-routing, session tracking)
    - Interactive AI discussion workflow (forward/backward modes, yes/no/explain questions)
    - Advanced tree visualization (tree.adv with tags, colors, sizes, depth limits, filtering)
    - Enhanced context builder (context.build.adv with deduplication, hierarchical output, adaptive budgets)
    - Logic-based tag system with theorem proving:
      - Tag mining workflow, implication engine, consistency checking, contradiction resolution
      - Knowledge representation in /plan/rules with persistence
      - Integration with planner (pre-planning verification, during-planning constraints, post-planning validation)
    - Hypothesis testing system (5 complexity levels: query, code modification, refactoring, feature addition, architecture)
    - Full system integration demo: `scripts/examples/full-integration-hello-world.cx`
      - Demonstrates complete workflow from user request to working C++ code
      - User: "ai create hello world program in c++"
      - Result: Compilable and executable C++ "Hello World" program
      - 10 phases: initialization, planning, breakdown, task planning, context building, code generation, hypothesis testing, visualization, validation, persistence
  - **Demo scripts** (progressively more complex):
    - `scripts/examples/planner-logic-integration-demo.cx` - BASIC
    - `scripts/examples/planner-logic-advanced-demo.cx` - ADVANCED
    - `scripts/examples/planner-logic-expert-demo.cx` - EXPERT
    - `scripts/examples/full-integration-hello-world.cx` - FULL INTEGRATION
  - **Note**: A* search and cost heuristics deferred (not needed for current workflow)
  - **Note**: Scope store with binary diffs, scenario harness binaries, and feedback pipeline remain as future enhancements
- **Advanced Tree Visualization and Context Builder Enhancements** (2025-10-08):
  - **Advanced tree visualization** with `tree.adv` / `tree.advanced` command
    - Box-drawing characters (├─, └─, │) for hierarchical display
    - ANSI color coding by node type (--colors): Dir=blue, File=default, Ast=magenta, Mount=cyan, Library=yellow
    - Inline tag display (--tags) showing all tags associated with nodes
    - Token/size estimates (--sizes) for capacity planning
    - Node kind indicators (--kind) showing type prefix (d/f/a/m/l)
    - Alphabetic sorting (--sort) for consistent output
    - Depth limiting (--depth=N) to control traversal depth
    - Path pattern filtering (--filter=pattern) for focused views
  - **Enhanced context builder** with `context.build.adv` / `context.build.advanced` command
    - Content deduplication (--dedup) using BLAKE3 hashing to eliminate redundant entries
    - Hierarchical output mode (--hierarchical) with overview + details sections
    - Adaptive token budgets (--adaptive) that expand for complex contexts (2x max_tokens)
    - Automatic summarization (--summary=N) with configurable threshold for large files
    - Dependency tracking (--deps) following LinkNodes and related content
    - Compound filter logic (AND/OR/NOT) for complex filtering scenarios
    - Smart context assembly with priority-based selection
    - Token budget management with overflow handling
  - **Context builder infrastructure**:
    - ContextBuilder::ContextOptions struct for configuration
    - buildWithOptions() method for advanced context assembly
    - getDependencies() for relationship tracking (extensible)
    - summarizeEntry() for intelligent content reduction (first/last 10 lines)
    - deduplicateEntries() using seen_content hash set
    - buildHierarchical() returning (overview, details) pair
    - addCompoundFilter() for logical filter composition
  - **Tree visualization infrastructure**:
    - Vfs::TreeOptions struct with 8 configurable display options
    - treeAdvanced() overloads for both node and path-based invocation
    - formatTreeNode() for customizable node formatting with tags/sizes/colors
    - Recursive traversal with proper is_last tracking for box chars
  - **Command surface**:
    - tree.adv [path] [--no-box] [--sizes] [--tags] [--colors] [--kind] [--sort] [--depth=N] [--filter=pattern]
    - context.build.adv [max_tokens] [--deps] [--dedup] [--summary=N] [--hierarchical] [--adaptive]
  - **Demo script**: scripts/examples/tree-viz-context-demo.cx with 10 progressive demonstrations
  - Added #include <unordered_set> to codex.h for seen_content deduplication
  - Fixed VfsNode::Kind enum references (Ast not AstHolder, removed non-existent Remote/Link)
  - All features compile cleanly and tested successfully
- **Hypothesis Testing System - 5 Progressive Complexity Levels** (2025-10-08):
  - Implemented comprehensive hypothesis testing framework for code analysis WITHOUT AI calls
  - **Level 1: Simple Query** (`hypothesis.query <target> [path]`)
    - VFS-wide pattern search using ContextBuilder and filters
    - Returns matched nodes with paths for further investigation
    - Example: Find all occurrences of a function name across codebase
  - **Level 2: Code Modification** (`hypothesis.errorhandling <function> [style]`)
    - Function definition detection with regex-based AST analysis
    - Return path identification for error handling insertion points
    - Multiple error handling strategies: try-catch, error-code, std::optional
    - Proposes specific actions for each identified insertion point
  - **Level 3: Refactoring** (`hypothesis.duplicates [path] [min_lines]`)
    - Duplicate code block detection using line-by-line similarity analysis
    - 80% similarity threshold with whitespace normalization
    - Proposes helper function extraction with parameter signature inference
    - Identifies all locations requiring refactoring updates
  - **Level 4: Feature Addition** (`hypothesis.logging [path]`)
    - Error path detection via pattern matching (return nullptr, -1, false, throw, error/fail keywords)
    - Logging instrumentation planning with context-aware placement
    - Proposes logger infrastructure design (class vs macros)
    - Tag-based tracking for instrumented code
  - **Level 5: Architecture** (`hypothesis.pattern <pattern> [path]`)
    - Design pattern applicability evaluation (visitor, factory, singleton)
    - Node hierarchy analysis for visitor pattern suitability
    - Implementation strategy proposals (double-dispatch vs std::variant vs CRTP)
    - Performance and migration considerations
  - Core components: Hypothesis, HypothesisResult, HypothesisTester, HypothesisTestSuite
  - Shell commands: test.hypothesis (run all 5), hypothesis.test (custom), hypothesis.{query,errorhandling,duplicates,logging,pattern}
  - Helper methods: findFunctionDefinitions, findReturnPaths, findDuplicateBlocks, findErrorPaths, contentSimilar
  - Demo scripts: scripts/examples/hypothesis-testing-demo.cx, hypothesis-demo-simple.cx
  - Integration with action planner's ContextBuilder for VFS traversal and filtering
  - All hypothesis testing executes locally - no AI API calls required
  - Enables hypothesis-driven development: validate code modification strategies before implementation
  - Foundation for advanced planning: SAT/SMT constraint solving, learned patterns, automated refactoring
- **Action Planner Test Suite - AI Context Builder** (2025-10-08):
  - Implemented ContextFilter with 9 filter types: TagAny, TagAll, TagNone, PathPrefix, PathPattern, ContentMatch, ContentRegex, NodeKind, Custom
  - Implemented ContextBuilder for building AI context with token budget management (default 4000 tokens)
  - Priority-based context selection: critical=200, important=150, default=100
  - Token estimation: ~4 chars per token (GPT-style tokenization)
  - Multi-overlay support: handles multiple VFS overlays transparently
  - Implemented ReplacementStrategy with 8 strategy types:
    - ReplaceAll, ReplaceRange, ReplaceFunction, InsertBefore, InsertAfter, DeleteMatching, CommentOut, ReplaceBlock (TODO)
  - Implemented ActionPlannerTestSuite with 6 comprehensive tests:
    - tag_filter_any, path_prefix, content_match, context_builder_tokens, replacement_all, replacement_insert_before
  - Added shell commands: context.build, context.filter.tag, context.filter.path, test.planner
  - Created comprehensive documentation: docs/ACTION_PLANNER.md
  - Created demonstration script: scripts/examples/action-planner-demo.cx
  - All tests passing (6/6 passed in test.planner)
  - This is the "AI context offloader" from TASKS.md - filters VFS nodes, builds context within token budgets, tests hypotheses before calling AI
  - Enables hypothesis-driven development: test code modification strategies without AI calls
  - Foundation for advanced features: tag-based theorem proving, SAT/SMT integration, learned patterns
- **Remote VFS mounting over network** (2025-10-08):
  - Implemented RemoteNode for transparent remote VFS access via TCP sockets
  - Added daemon mode: `--daemon <port>` to run codex as server accepting remote connections
  - EXEC protocol: line-based command execution (EXEC <cmd> → OK <output> | ERR <msg>)
  - mount.remote command: `mount.remote <host> <port> <remote-vfs-path> <local-vfs-path>`
  - Thread-safe socket communication with connection pooling and lazy connection
  - Integration with mount system: MountType::Remote, 'r' type marker in mount.list
  - Remote commands executed via shell (popen) allowing VFS and system command access
  - Use case: copy files between real filesystems on different hosts via VFS layer
  - Created demonstration scripts: remote-mount-demo.cx, remote-copy-demo.cx
  - Comprehensive documentation in docs/REMOTE_VFS.md
  - Updated README.md, AGENTS.md, HOWTO_SCRIPTS.md with remote mount examples
  - Note: libssh available but TCP-only implementation prioritized for simplicity
  - Future enhancements: SSH/SFTP transport, authentication, background I/O threads
- **Filesystem and library mounting** (2025-10-08):
  - Implemented MountNode for transparent host filesystem access (read/write to real files and directories)
  - Implemented LibraryNode for .so/.dll shared library loading via dlopen/dlsym
  - Added mount commands: mount, mount.lib, mount.list, mount.allow, mount.disallow, unmount
  - Mount nodes appear with type markers: m=filesystem mount, l=library
  - Created test shared library (tests/libtest.so) with 10 functions demonstrating various signatures
  - Added mount control system: mount.allow/mount.disallow gates new mounts without affecting existing ones
  - Mount tracking with MountInfo structure stores vfs_path, host_path, mount_node reference, and type
  - Updated documentation in AGENTS.md, README.md, and HOWTO_SCRIPTS.md
  - Created scripts/examples/mount-demo.cx demonstrating filesystem and library mounting
## Completed
- **VFS persistence with BLAKE3 hashing and autosave** (2025-10-07):
  - VFS overlay format upgraded to version 3 with BLAKE3 hash tracking for source files
  - Hash verification on .vfs load with mismatch warnings (compares stored hash with current file)
  - Auto-load `<dirname>.vfs` on startup when running in matching directory
  - Timestamped backups created in `.vfsh/` directory before overwriting (format: `file.vfs.YYYY-MM-DD-HHMMSS.bak`)
  - Autosave infrastructure for solution files (.cxpkg/.cxasm and their chained .vfs files):
    - Configurable delay (default 10 seconds after modification)
    - Only applies to solution packages, NOT standalone .vfs files
    - Background thread with dirty-flag tracking
  - Crash recovery snapshot framework (every 3 minutes to `.vfsh/recovery.vfs`)
  - Makefile updated to link libblake3
  - Note: Autosave thread startup and VFS write hooks still need integration in main()
## Completed
- **Test harness C++ compilation and execution** (2025-10-07):
  - test_harness.py now compiles generated C++ code using g++/c++ with -std=c++17 -O2
  - Added Linux sandbox execution using unshare (Gentoo-style) with timeout fallback for non-privileged environments
  - Implemented runtime output validation with `expected-runtime-output` field supporting contains/not-contains/equals assertions
  - All test .sexp files updated with runtime expectations for program output verification
  - Tests now verify actual program behavior (compilation + execution), not just AST structure
  - Sandbox provides network/PID namespace isolation on Linux, gracefully falls back to timeout-only when privileges unavailable
  - Compile timeout: 30s, execution timeout: 10s with automatic temp file cleanup
- Renamed Stage1 to VfsShell and updated all references (Makefile, header guards, documentation, test harness)
- Created VfsShell/AGENTS.md documenting implementation architecture (VFS design, S-expression language, C++ AST builder, AI bridge, code organization with line numbers, extension points)
- Linked VfsShell/AGENTS.md from root AGENTS.md under "Implementation details" section
- Created HOWTO_SCRIPTS.md with comprehensive examples for running all script files (.cx vs .sexp, running all scripts, creating custom scripts, common patterns, debugging)
- test_harness.py now uses AI response caching compatible with C++ cache format (cache/ai/{provider}/{hash}-in.txt and {hash}-out.txt)
- add shell commands ctrl+u and ctrl+k for clearing text
- AI bridge prompt & tests: added scripts/examples/ai-hello-world.sexp and tests/011-ai-bridge-hello.sexp to exercise cpp.* helpers via the ai command.
- `AGENTS.md` drafted from discussion notes to document VfsShell agent scope.
- Build tooling pipeline now operational:
  - Root `Makefile` builds the VfsShell binary and exposes debug/release toggles.
  - OpenAI integration loads the API key from the home directory fallback (`~/openai-key.txt`).
  - `make sample` exercises the C++ AST builder, exports generated code, compiles it, and checks the runtime output.
- VfsShell harness (`tools/test_harness.py`) runs `.sexp` specs end-to-end against configured LLM targets and validates results inside codex-mini.
- C++ AST shell surface now includes statements (`cpp.vardecl`, `cpp.expr`, `cpp.stmt`, `cpp.return`, `cpp.rangefor`) for structural codegen beyond canned print/return helpers.
- overlays: multiple persistent VFS overlays can now coexist without mixing nodes; the CLI exposes `overlay.*` commands and aggregate listings.

- **Web Server with Browser-Based Terminal** (2025-10-10):
  - **COMPLETE**: Phases 1 & 2 - Full web terminal with command execution
  - **Phase 1**: Basic HTTP/WebSocket server with terminal emulation
    - Built with **libwebsockets** for lightweight HTTP/WebSocket support
    - Frontend uses **xterm.js** v5.3.0 for full terminal emulation with ANSI colors
    - Single-file architecture: embedded HTML (no external file dependencies)
    - Command-line interface: `./codex --web-server [--port 8080]`
    - Default port: 8080, customizable with `--port` flag
    - Modern UI with header, status indicators (connected/disconnected)
    - Responsive design for various screen sizes
    - Multiple concurrent session support
  - **Phase 2**: WebSocket command execution (NEW)
    - ✅ Command callback registration system via `WebServer::set_command_callback()`
    - ✅ Full integration with VfsShell command dispatcher
    - ✅ Bidirectional terminal I/O: commands sent via WebSocket → executed in VfsShell → output returned
    - ✅ Error handling with ANSI color codes (red for errors)
    - ✅ Support for command chains (&&, ||) and pipelines (|)
    - ✅ All VFS commands work: ls, cd, mkdir, cat, ai, cpp.*, plan.*, etc.
    - ✅ Multiple concurrent sessions with isolated state (each session has own VFS context)
    - ✅ Automatic prompt display after command execution (`codex>`)
  - Technical architecture:
    - VfsShell/web_server.cpp (~480 lines): HTTP/WebSocket server with command routing
    - VfsShell/codex.h: WebServer namespace API (start, stop, is_running, send_output, set_command_callback)
    - VfsShell/codex.cpp: Integrated command execution lambda captures VFS, environment, working directory
    - Command execution flow: WebSocket recv → tokenize_command_line → parse_command_chain → run_pipeline → WebSocket send
    - Makefile: Updated with -lwebsockets -lpthread linking
  - Testing:
    - ✅ Successfully serves HTML on HTTP GET /
    - ✅ WebSocket endpoint functional at ws://localhost:port/ws
    - ✅ Terminal UI renders correctly with xterm.js
    - ✅ Status indicators update on connection state
    - ✅ Commands execute successfully: ls /, pwd, mkdir /test, etc.
    - ✅ Error handling works: invalid commands show red error messages
    - ✅ Clean compilation with only minor warnings
  - Documentation:
    - README.md: Full "Web Server (Browser-Based Terminal)" section
    - CLAUDE.md: Added to "Core Systems" with feature list
    - scripts/examples/web-server-demo.cx: Usage demonstration
    - TASKS.md: Updated with Phase 2 completion
  - Files modified:
    - VfsShell/web_server.cpp: Added command callback handling (~480 lines total)
    - VfsShell/codex.h: Added set_command_callback() to WebServer namespace
    - VfsShell/codex.cpp: Command execution callback registered in main() before server start
    - Makefile: web_server.cpp added to build
  - **Next steps** (Phase 3):
    - Add script demo gallery with Monaco editor
    - Implement internal system visualization (VfsNode graph, memory inspector)
    - Add floating window system (WinBox.js) and graph visualization (Cytoscape.js)
    - Session management: attachable/detachable sessions like tmux
  - **Status**: Phase 2 complete! Full interactive web terminal functional. Users can run any VfsShell command through their browser.
  - **Terminal Formatting Fix** (2025-10-10):
    - ✅ Fixed progressive indentation issue in web terminal output
    - ✅ Implemented automatic `\n` to `\r\n` conversion for xterm.js compatibility
    - ✅ Terminal cursor now properly returns to left margin after each line
    - ✅ Commands like `ls` and `tree` now display with correct formatting
    - Implementation: VfsShell/web_server.cpp lines 329-337, converts output before WebSocket transmission

## Backlog / Ideas
- Harden string escaping in the C++ AST dumper before expanding code generation.
- VfsNode memory optimization: keep VfsNode POD-friendly (trivially copyable), implement fast recycler for construction/destruction (hot code path)
- Node metadata storage: separate from VfsNode to maintain POD compatibility (VFS-owned map pattern)
