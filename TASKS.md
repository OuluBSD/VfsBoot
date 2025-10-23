# Tasks Tracker

**Note**: sexp is for AI and shell script is for human user (+ ai called via sexp). This follows the classic codex behaviour.

**Context Location**: See [TASK_CONTEXT.md](TASK_CONTEXT.md) for detailed qwen-code integration context and status.

---

---

## Active Tasks (2025-10-23)

### qwen-code Integration - COMPLETE ✅

**Status**: Phase 5 COMPLETE - Full ncurses UI with 4,082 lines of C++ code

**Completed Features**:
- ✅ Full-screen ncurses interface (6-color support, status bar, scrollable history)
- ✅ Terminal capability detection with graceful stdio fallback
- ✅ `--simple` flag to force stdio mode
- ✅ Complete VFS session persistence
- ✅ Tool approval workflow
- ✅ TCP/stdio/pipe communication modes
- ✅ All tests passing (protocol, state, client, integration)
- ✅ Documentation updated and organized

**Optional Enhancements** (for qwen-code fork):
- See [docs/qwen-code-fork-enhancements.md](docs/qwen-code-fork-enhancements.md) for TCP startup instructions and --openai flag proposals

### Quick Links

- [QWEN.md](QWEN.md) - Integration guide, protocol spec, usage (UPDATED)
- [QWEN_STRUCTURED_PROTOCOL.md](QWEN_STRUCTURED_PROTOCOL.md) - Protocol reference
- [QWEN_LOGGING_GUIDE.md](QWEN_LOGGING_GUIDE.md) - Debugging guide
- [docs/qwen-code-fork-enhancements.md](docs/qwen-code-fork-enhancements.md) - Optional fork improvements
- [TASK_CONTEXT.md](TASK_CONTEXT.md) - Implementation status and detailed context
- [AGENTS.md](AGENTS.md) - VfsBoot architecture and agent documentation
- [README.md](README.md) - Build instructions and quick start

---

## Upcoming: Important (Ordered by Priority)

### 1. Build System: Makefile Timeout Issue (Low Priority)
**Status**: Minor annoyance, workaround exists
**Issue**: `make` command times out after 30+ seconds with no output
**Workaround**: Use `./build.sh` instead (works perfectly, completes in ~21 seconds)
**Impact**: Low - build.sh is preferred method anyway
**Investigation needed**:
- Check for circular dependencies in Makefile
- Verify pkg-config calls aren't hanging
- Review ncurses detection logic
**See**: [docs/COMMIT_REVIEW_FINDINGS.md](docs/COMMIT_REVIEW_FINDINGS.md) for details

### 2. Internal U++ Builder
- Stand up minimal in-process `umk` pipeline using `UppToolchain` metadata
- Emit per-translation-unit compile nodes and package-level link nodes
- Add regression coverage in `scripts/unittst/upp_internal_umk.md`
- See `docs/internal_umk_plan.md` for details

### 2. U++ Compatibility
- Support U++ assembly files (e.g. ~/.config/u++/theide/MyApps.var)
- Active workspace with primary package and dependencies
- Implement U++ build method + package build pipeline (umk subset)
- ✅ Introduced shared build graph + `upp.wksp.build` CLI

### 3. Web Browser GUI - Phase 3
**Phases 1 & 2 COMPLETE** ✅ (basic terminal + WebSocket command execution)

**Next**: Advanced GUI features
- Script Demo Gallery with Monaco editor
- Internal System Visualization (VfsNode graph, memory inspector, logic engine)
- WinBox.js floating windows + Cytoscape.js graph viz
- See "Completed" section for Phase 1 & 2 details

### 4. libclang AST Parsing
**Phases 1 & 1.5 COMPLETE** ✅ (parse, dump, regenerate C++ code)

**Next**: Phase 2 - Preprocessor Integration
- Collect preprocessor state: macros, includes, conditional compilation
- Hooks for CXCursor_MacroDefinition, CXCursor_MacroExpansion
- Store in `/ast/<file>/preprocessor/`

**Later Phases**:
- Phase 3: Complete CXCursor coverage (templates, C++ features)
- Phase 4: Multi-TU support, cross-references, call graphs
- Phase 5: Clang test suite import
- Phase 6: AI context optimization

---

## Upcoming: Less Important

### Shell UX Improvements
- History navigation in web browser CLI (arrows up/down loop)
- Ctrl+C to exit sub-CLI loops
- Customizable shell prompt with colors (like bash $PS1)
- Enhanced `ls` with colors and grid layout
- **Builtin minimal text editor** (nano/micro-like) ← IMPORTANT
- Command autocompletion improvements
- CLI editing: Ctrl+U, Ctrl+K, Home/End keys
- Fix non-ASCII input (ä character)
- Double-enter bug fixes (ai command)

### AI Discussion Enhancements
- User profiling for `discuss` sessions (learn preferences)
- Smart plan addition command (`ai.add.plan`)

### Web Server Enhancements
- Session management (attachable/detachable like tmux)
- Persist session state across devices
- Recover sessions after connection drops

### Shell Features
- Script sourcing (`source <script.sh>` or `. <script.sh>`)
- Commandline argument enhancements (--llama, --openai, --version, --help)
- sh compatibility mode, login shell support
- Turing-complete scripting (tcsh-like, DISCUSS FIRST)
- Path autocompletion for mounted paths

### Advanced Features
- Netcat-like networking (client/server, TTY server, file transfer)
- Internal logging to VFS (/log)
- AST resolver for multi-language support (Java, C#, TypeScript, JS, Python)
- Command aliases
- Multi-language sexp converters (JS, Python, PowerShell, Bash)

### Documentation
- File type documentation (.sexp, .cx, .cxpkg)
- Example solution with multiple packages
- Comprehensive examples in HOWTO_SCRIPTS.md

---

## Backlog / Ideas
- Harden string escaping in C++ AST dumper
- VfsNode memory optimization (POD-friendly, fast recycler)
- Node metadata storage (separate from VfsNode)
- byte-vm for sexp or cx script (maybe overkill?)

---

## Completed (Recent)

### qwen-code Integration - Phase 5 Option A (2025-10-23) ✅
**COMPLETE**: qwen-oauth server mode working end-to-end with real AI!

**What Works**:
- ✅ Config.refreshAuth() initializes contentGeneratorConfig + GeminiClient
- ✅ Server mode authentication (qwen-oauth, OpenAI)
- ✅ Bidirectional JSON protocol validated
- ✅ Full interactive session with streaming responses
- ✅ Tool approval flow works end-to-end
- ✅ C++ client implementation (3,700 lines)
- ✅ VFS session persistence
- ✅ TCP mode tested successfully

**Files**: See [QWEN.md](QWEN.md) for complete file list and architecture

**Fix Details** (commit `3ee2e276` in qwen-code):
- Problem: Config.getContentGeneratorConfig() undefined
- Solution: Call config.refreshAuth(authType) before runServerMode()
- Files: gemini.tsx:627-656, config.ts:595

### qwen TCP Mode Implementation (2025-10-23) ✅
**COMPLETE**: Full TCP client mode for connecting to qwen-code TCP servers

**What Was Implemented**:
- ✅ Added `mode`, `port`, `host` fields to QwenOptions struct
- ✅ Argument parsing for --mode, --port, --host in cmd_qwen.cpp
- ✅ TCP socket client implementation in qwen_client.cpp
- ✅ Updated help text with connection mode documentation
- ✅ Fixed Ctrl+C handling in run_qwen_server.sh
- ✅ Enhanced run_qwen_client.sh with --direct mode for ncurses
- ✅ Tested TCP connection and error handling

**Connection Modes**:
- `stdin` - Spawn qwen-code as subprocess (default)
- `tcp` - Connect to existing TCP server
- `pipe` - Named pipes (not yet implemented)

**Usage Examples**:
```bash
# Start TCP server
./run_qwen_server.sh                  # Port 7777 with qwen-oauth
./run_qwen_server.sh --openai         # Use OpenAI

# Connect with vfsh
./vfsh
qwen --mode tcp --port 7777           # Full ncurses support

# Or use client script
./run_qwen_client.sh                  # Auto-connect (line-based)
./run_qwen_client.sh --direct         # Manual connect (ncurses)
```

**Files Modified**:
- VfsShell/cmd_qwen.h:36-48 - Added mode/port/host fields
- VfsShell/cmd_qwen.cpp:78-86,123-159,875-884 - Parsing and config
- VfsShell/qwen_client.cpp:1-6,328-377 - TCP implementation
- run_qwen_server.sh:14-32,128-134 - Ctrl+C fix
- run_qwen_client.sh:22,35-61,94-129 - --direct mode

### qwen-code UI: Fixed Input/Output Separation in ncurses Mode (2025-10-24) ✅
**COMPLETE**: Proper chat interface with separated input/output and preserved input buffer

**Problem Solved**:
The cursor no longer moves around the screen based on output. The UI now maintains a proper chat interface where the cursor always stays in the input field while conversation history displays in a separate scrollable area above.

**What Was Implemented**:
1. ✅ **Proper Window Management**:
   - Output window: Scrollable area for conversation history (auto-scrolls to bottom)
   - Status bar: Shows session info, scroll position, and current state
   - Input window: Fixed 3-line area at bottom with box border and prompt

2. ✅ **Output Buffer System**:
   - All output stored in `std::vector<OutputLine>` with color information
   - Redraw functions that update only what's needed
   - Cursor always returns to input field after any output operation

3. ✅ **Non-blocking Input Handling**:
   - Character-by-character input processing with `wtimeout(50ms)`
   - Full line editing support: Left/Right arrows, Home/End, Backspace/Delete
   - Ctrl+A (Home), Ctrl+E (End) support
   - Input buffer preserved even when AI responses arrive

4. ✅ **Scrollable History**:
   - Page Up/Down or Ctrl+U/D to scroll through conversation history
   - Status bar shows scroll offset when not at bottom
   - Auto-scroll to bottom when new messages arrive
   - Scroll position tracked in `scroll_offset` variable

5. ✅ **Streaming Support**:
   - AI streaming responses update in-place on last line
   - No cursor jumping during streaming
   - Clean separation between streaming chunks and complete messages

6. ✅ **Input Preservation**:
   - Partial input never lost when new messages arrive
   - All message handlers call `redraw_input()` to restore cursor
   - Input buffer maintained across all UI updates

**Key Technical Details**:
- Helper lambdas: `add_output_line()`, `redraw_output()`, `redraw_status()`, `redraw_input()`
- Non-blocking event loop polls for both messages (0ms) and keyboard input (50ms)
- 10ms sleep in main loop to avoid CPU thrashing
- Color pairs maintained for all message types (User, AI, System, Error, Info, Tool)

**Files Modified**:
- VfsShell/cmd_qwen.cpp:319-758 - Complete ncurses mode rewrite

**Impact**:
Users can now type continuously while receiving AI responses, scroll through conversation history, and enjoy a professional chat interface. The cursor never leaves the input field, providing a clean and predictable UX.

### Web Server with Browser Terminal - Phases 1 & 2 (2025-10-10) ✅
**COMPLETE**: Full web terminal with command execution

**Phase 1**: HTTP/WebSocket server with xterm.js terminal
- libwebsockets-based server, embedded HTML, responsive UI
- Command: `./vfsh --web-server [--port 8080]`

**Phase 2**: WebSocket command execution
- Full integration with VfsShell command dispatcher
- Bidirectional terminal I/O, error handling with ANSI colors
- Support for command chains (&&, ||, |)
- Multiple concurrent sessions with isolated state

**Files**: VfsShell/web_server.cpp (~480 lines), Makefile updated

### libclang AST Parsing - Phases 1 & 1.5 (2025-10-08) ✅
**COMPLETE**: Parse C++ code, dump AST, regenerate source

**Phase 1**: AST structure and parsing
- Complete AST node structure (31 node types)
- Source location tracking with byte lengths
- Shell commands: `parse.file`, `parse.dump`

**Phase 1.5**: C++ code regeneration
- Source extraction using SourceLocation offset + length
- Shell command: `parse.generate <ast-path> <output-path>`
- Successfully tested: parse → generate → compile → run

**Files**: VfsShell/clang_parser.cpp (~1,350 lines), tests/hello.cpp

### Planner Core Engine (2025-10-08) ✅
**COMPLETE**: Full integration of planning, logic, context building, hypothesis testing

**Systems**:
- Tag system with enumerated registry
- Hierarchical plan nodes with shell commands
- Planning loop with AI discussion workflow
- Logic-based tag theorem proving
- Hypothesis testing (5 complexity levels)
- Advanced tree visualization
- Enhanced context builder

**Demo Scripts**:
- `scripts/examples/full-integration-hello-world.cx` - End-to-end workflow
- `scripts/examples/planner-logic-expert-demo.cx` - Logic integration
- `scripts/examples/tree-viz-context-demo.cx` - Visualization

### Bootstrap Build and Static Analysis (2025-10-10) ✅
**COMPLETE**: Dependency detection and code quality checks

**Features**:
- Automatic library detection before build
- Multi-distribution support (Gentoo, Debian, Fedora, Arch)
- clang-tidy integration with interactive prompts
- Command-line flags: `--static-analysis`, `--no-static-analysis`
- Logs saved to `.static-analysis-*.log`

**Files**: build.sh updated, .gitignore updated

### Minimal GNU Make Implementation (2025-10-09) ✅
**COMPLETE**: Internal make utility + standalone bootstrap

**Features**:
- Makefile parser with GNU make syntax
- Variable substitution: `$(VAR)`, `${VAR}`, `$(shell ...)`
- Automatic variables: `$@`, `$<`, `$^`
- .PHONY support, timestamp-based rebuilds
- Bootstrap build: `./build.sh -b`

**Files**: VfsShell/codex.{h,cpp}, make_main.cpp (472 lines)

### Scenario Harness for Planner Testing (2025-10-09) ✅
**COMPLETE**: Testing infrastructure for planner validation

**Components**:
- Scenario file parser with structured sections
- Five-phase execution engine
- Interactive runner (planner_demo) and batch generator (planner_train)

**Files**: harness/{scenario.h,scenario.cpp,runner.h,runner.cpp}

### Remote VFS Mounting (2025-10-08) ✅
**COMPLETE**: Network VFS access via TCP

**Features**:
- RemoteNode for transparent remote access
- Daemon mode: `--daemon <port>`
- EXEC protocol with line-based commands
- Shell command: `mount.remote <host> <port> <remote-path> <local-path>`

**Files**: VfsShell/codex.cpp, docs/REMOTE_VFS.md

### Filesystem and Library Mounting (2025-10-08) ✅
**COMPLETE**: Host filesystem and shared library access

**Features**:
- MountNode for transparent host filesystem access
- LibraryNode for .so/.dll loading via dlopen/dlsym
- Mount commands: mount, mount.lib, mount.list, unmount
- Mount control: mount.allow, mount.disallow

**Files**: VfsShell/codex.cpp, tests/libtest.so, scripts/examples/mount-demo.cx

### VFS Persistence with BLAKE3 Hashing (2025-10-07) ✅
**COMPLETE**: Overlay format v3 with hash verification

**Features**:
- BLAKE3 hash tracking for source files
- Hash verification on .vfs load
- Auto-load `<dirname>.vfs` on startup
- Timestamped backups in `.vfsh/` directory
- Autosave infrastructure for solution files

**Files**: Makefile (libblake3 linking)

### Test Harness C++ Compilation (2025-10-07) ✅
**COMPLETE**: Runtime output validation

**Features**:
- g++/c++ compilation with -std=c++17 -O2
- Linux sandbox execution using unshare
- Runtime output validation (contains/not-contains/equals)
- Compile timeout: 30s, execution timeout: 10s

**Files**: tools/test_harness.py, tests/*.sexp

### In-Binary Sample Runner (2025-10-09) ✅
**COMPLETE**: `sample.run` command for C++ demos

**Features**:
- Deterministic state reset
- AST construction via internal commands
- Compilation and execution with output capture
- VFS logging and status tracking
- Flags: `--keep`, `--trace`

**Files**: VfsShell/codex.cpp, scripts/examples/sample-run-demo.cx

---

## Notes

### File Types & Conventions
- `.sexp` – S-expression scripts for AI and automation (AI-first interface)
- `.cx` – Shell scripts for human users (can be called from sexp)
- `.cxpkg` – Solution package overlay files
- `.cxasm` – Assembly overlay files

### Environment Variables

**VfsBoot**:
- `OPENAI_API_KEY` - OpenAI (fallback: ~/openai-key.txt)
- `LLAMA_BASE_URL` / `LLAMA_SERVER` - Llama server
- `LLAMA_MODEL` - Llama model name
- `CODEX_AI_PROVIDER` - Force provider (openai, llama)
- `CODEX_ENGLISH_ONLY` - Force English responses

**qwen-code**:
- `OPENAI_API_KEY` - OpenAI authentication
- `GEMINI_API_KEY` - Google Gemini authentication
- `LLAMA_BASE_URL` - Llama server (not yet supported)

### Build Commands
```bash
make            # builds ./vfsh with gnu++17 + O2
make debug      # -O0 -g
make release    # -O3 -DNDEBUG
make sample     # runs hello-world pipeline test
```

### Test Commands
```bash
python tools/test_harness.py                    # run all tests
python tools/test_harness.py --target openai    # OpenAI only
python tools/test_harness.py --target llama -v  # Llama with verbose
```

---

**Last Updated**: 2025-10-23
**Total Lines**: < 500 (compacted from 1879)
**Context**: See [TASK_CONTEXT.md](TASK_CONTEXT.md)
