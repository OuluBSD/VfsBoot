# Tasks Tracker

**Note**: sexp is for AI and shell script is for human user (+ ai called via sexp). This follows the classic codex behaviour.

**Context Location**: See [TASK_CONTEXT.md](TASK_CONTEXT.md) for detailed qwen-code integration context and status.

---

---

## Active Tasks (2025-10-23)

### qwen-code Integration - COMPLETE ✅

**Status**: Phase 5 COMPLETE - Full ncurses UI with 4,082 lines of C++ code

### Future Enhancements - PLANNED 🚀

**Priority**: MEDIUM - Enhancements to improve user experience and functionality

#### 1. Enhanced Session Management
**Goal**: More sophisticated session handling and resource management

**Files to Modify**:
- `VfsShell/qwen_manager.cpp` - Session management logic
- `VfsShell/qwen_manager.h` - Session data structures
- `VfsShell/cmd_qwen.cpp` - Session commands

**Tasks**:
1. ✅ Implement session pause/resume functionality
2. ✅ Add session snapshots for saving and restoring complex conversation states  
3. ✅ Create session grouping for organizing related repositories or tasks
4. ✅ Add configurable escalation policies (not just 3 failures, but based on task type)
5. ✅ Implement automated testing integration (trigger specific test suites based on changes)

#### 2. Performance & Resource Management
**Goal**: Better resource utilization and monitoring

**Files to Modify**:
- `VfsShell/qwen_manager.cpp` - Resource management logic
- `VfsShell/qwen_manager.h` - Resource management structures
- `VfsShell/cmd_qwen.cpp` - Monitoring display

**Tasks**:
1. ✅ Add CPU/memory usage monitoring for each session
2. ✅ Implement priority queues for tasks with different urgency levels
3. ✅ Add rate limiting for API calls to prevent quota exhaustion
4. ✅ Implement automatic context window management to prevent token overflow
5. ✅ Add save/load conversation contexts to disk

#### 3. Enhanced Communication & Security
**Goal**: More reliable and secure communication

**Files to Modify**:
- `VfsShell/qwen_tcp_server.cpp` - Communication protocol
- `VfsShell/qwen_protocol.cpp` - Message handling
- `VfsShell/qwen_manager.cpp` - Security features

**Tasks**:
1. ✅ Add encrypted communication between manager and accounts
2. ✅ Implement message queuing for reliable delivery during network interruptions
3. ✅ Add support for different AI providers beyond qwen (Anthropic, OpenAI, local models)
4. ✅ Implement configurable task templates for common development patterns

#### 4. Input & Output Enhancements
**Goal**: Better user interaction and visual feedback

**Files to Modify**:
- `VfsShell/cmd_qwen.cpp` - UI implementation
- `VfsShell/qwen_client.cpp` - Message handling
- `VfsShell/qwen_manager.cpp` - UI enhancements

**Tasks**:
1. ✅ Add multi-line input support with syntax highlighting for code snippets
2. ✅ Implement input history with search functionality (like bash history)
3. ✅ Add macro support for frequently used command sequences
4. ✅ Implement code block highlighting with language-specific syntax coloring
5. ✅ Add collapsible sections for long responses
6. ✅ Create inline diffs for file changes to make them more readable

#### 5. Visual Enhancements & Theming
**Goal**: Better visual aesthetics and customization options

**Files to Modify**:
- `VfsShell/cmd_qwen.cpp` - UI rendering
- `VfsShell/qwen_manager.cpp` - UI rendering for manager mode
- `VfsShell/qwen_logger.h` - Logging with visual indicators

**Tasks**:
1. ✅ Add customizable color themes (dark, light, high contrast, etc.)
2. ✅ Implement vim-style navigation keys (hjkl) for keyboard users
3. ✅ Add animated status indicators for active processing (spinner or progress bar)
4. ✅ Implement configurable panel sizes for different screen layouts
5. ✅ Add search within conversation history
6. ✅ Create dashboard view with summary statistics
7. ✅ Add hover effects for clickable UI elements
8. ✅ Implement visual feedback during file uploads/drag-and-drop

#### 6. Accessibility & Navigation Improvements
**Goal**: Better accessibility and navigation for all users

**Files to Modify**:
- `VfsShell/cmd_qwen.cpp` - Accessibility features
- `VfsShell/qwen_manager.cpp` - Enhanced navigation
- `VfsShell/qwen_client.cpp` - Input/output handling

**Tasks**:
1. ✅ Add screen reader support for visually impaired users
2. ✅ Implement keyboard navigation for all UI elements
3. ✅ Add high contrast mode and larger text options
4. ✅ Ensure proper color contrast ratios for readability
5. ✅ Implement configurable information density (compact vs detailed view)
6. ✅ Add notification options (visual/audio) for different events

#### 7. Visual Border & Box Drawing Improvements
**Goal**: Use rounded characters for visual boxes around AI messages and prompts

**Files to Modify**:
- `VfsShell/cmd_qwen.cpp` - Message rendering
- `VfsShell/qwen_manager.cpp` - Session rendering
- `VfsShell/qwen_logger.h` - Styled message output

**Tasks**:
1. ✅ Replace rectangular borders with rounded characters:
   - Use ╭─╮ for top borders
   - Use │ │ for sides
   - Use ╰─╯ for bottom borders
2. ✅ Add color coding for different message types:
   - Green box for user input
   - Cyan box for AI responses  
   - Yellow box for system messages
   - Red box for errors
   - Blue box for info
3. ✅ Implement consistent visual styling for:
   - Tool execution requests
   - File display
   - Status updates
   - Input prompts

**Implementation Example**:

```
┌─ User Input ──────────────────────────┐
│ This is an example of a user message │
│ that appears in a rounded box with   │
│ appropriate color coding.            │
└──────────────────────────────────────┘

┌─ AI Response ─────────────────────────┐
│ This is an example of an AI message  │
│ that appears in a differently        │
│ colored rounded box.                 │
└──────────────────────────────────────┘
```

#### 8. Enhanced Status Indicators
**Goal**: More informative and visually appealing status indicators

**Files to Modify**:
- `VfsShell/cmd_qwen.cpp` - Status bar enhancements
- `VfsShell/qwen_manager.cpp` - Session status display
- `VfsShell/qwen_client.cpp` - Connection status

**Tasks**:
1. ✅ Create animated status indicators for active processing
2. ✅ Implement visual queue status showing pending requests
3. ✅ Add connection status indicators with colors (green=connected, red=disconnected)
4. ✅ Add visual feedback when commands are processed
5. ✅ Implement subtle animations for new message arrivals

**Dependency**: None - builds on existing ncurses infrastructure

---

### qwen-code Integration - COMPLETE ✅

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

### 0. qwen Manager Mode - MAJOR NEW FEATURE 🎯

**Status**: Planning phase - Implementation plan ready
**Priority**: HIGH - Multi-repository AI-driven development automation
**Estimated Effort**: 6-8 weeks (2000+ lines of C++ code)

#### Overview

Manager mode (`qwen -m`) transforms qwen into a hierarchical multi-repository AI project manager with:
- **Vertical split UI**: Top 10 rows for session list, bottom for chat/command view
- **Session hierarchy**: MANAGERS → ACCOUNTS → REPOS (MANAGERS/WORKERS)
- **Flipped TCP architecture**: Manager acts as TCP server, qwen instances connect as clients
- **JSON-based communication**: ACCOUNTS.json for configuration, JSON protocols for inter-session communication
- **Automated workflows**: Escalation from WORKER to MANAGER, test-driven development cycles

#### Session Hierarchy

```
MANAGER (Management Repository)
├── PROJECT MANAGER (qwen-openai) - expensive, high quality
├── TASK MANAGER (qwen-auth) - cheaper, regular quality
│
├── ACCOUNT #1 (Computer A)
│   ├── REPO #1 (qwen-auth WORKER + qwen-openai MANAGER)
│   ├── REPO #2 (qwen-auth WORKER + qwen-openai MANAGER)
│   └── REPO #3 (qwen-auth WORKER + qwen-openai MANAGER)
│
└── ACCOUNT #2 (Computer B)
    ├── REPO #4 (qwen-auth WORKER + qwen-openai MANAGER)
    └── REPO #5 (qwen-auth WORKER + qwen-openai MANAGER)
```

---

#### Phase 1: Core Manager Infrastructure (Week 1-2)

**Goal**: Basic manager mode with split UI and TCP server

**Files to Create**:
- `VfsShell/qwen_manager.h` - Manager mode core API
- `VfsShell/qwen_manager.cpp` - Manager implementation
- `VfsShell/qwen_tcp_server.h` - TCP server for incoming connections
- `VfsShell/qwen_tcp_server.cpp` - TCP server implementation
- `scripts/account_client.sh` - Shell script for remote account connections

**Files to Modify**:
- `VfsShell/cmd_qwen.cpp` - Add `-m` flag parsing and manager mode entry point
- `VfsShell/cmd_qwen.h` - Add QwenManagerOptions struct

**Tasks**:
1. ✅ Implement `-m` / `--manager` CLI flag parsing
2. ✅ Create vertical split ncurses UI:
   - Top window: Session list (10 rows max)
   - Status bar: Separator between top and bottom
   - Bottom window: Chat/command view (remaining rows)
3. ✅ Implement TAB key navigation between list and input
4. ✅ Add TCP server infrastructure:
   - Bind to configurable port (default: 7778)
   - Accept multiple incoming connections
   - Non-blocking I/O with poll()
   - Connection lifecycle management
5. ✅ Create session registry:
   - Session ID generation and tracking
   - Session type enumeration (MANAGER, ACCOUNT, REPO_MANAGER, REPO_WORKER)
   - Basic metadata (type, id, hostname, repo_path, status)

**Testing**:
- Manual testing with `-m` flag
- Verify vertical split renders correctly
- TAB navigation works between list and input
- TCP server accepts connections from `nc localhost 7778`

**Dependencies**: None - builds on existing qwen_client infrastructure

---

#### Phase 2: Session Types & Special MANAGER Sessions (Week 2-3)

**Goal**: Implement PROJECT MANAGER and TASK MANAGER special sessions

**Files to Create**:
- `PROJECT_MANAGER.md` - AI instructions for PROJECT MANAGER (qwen-openai)
- `TASK_MANAGER.md` - AI instructions for TASK MANAGER (qwen-auth)

**Files to Modify**:
- `VfsShell/qwen_manager.cpp` - Add special session initialization

**Tasks**:
1. ✅ Define SessionType enum:
   ```cpp
   enum class SessionType {
       MANAGER_PROJECT,   // qwen-openai, ID=mgr-project
       MANAGER_TASK,      // qwen-auth, ID=mgr-task
       ACCOUNT,           // Remote account connection
       REPO_MANAGER,      // qwen-openai for repository
       REPO_WORKER        // qwen-auth for repository
   };
   ```
2. ✅ Auto-spawn PROJECT MANAGER and TASK MANAGER on startup:
   - Load PROJECT_MANAGER.md and TASK_MANAGER.md from management repository
   - Initialize both sessions with appropriate models
   - Pin to top of session list with custom IDs
3. ✅ Implement session list display:
   - Show: Type | ID | Computer | Repo Path | Status
   - Color coding: MANAGERS (cyan), ACCOUNTS (yellow), REPOS (green/magenta)
   - Brief status indicators (active, idle, error, waiting)
4. ✅ Implement chat view for MANAGER sessions:
   - Selecting PROJECT/TASK MANAGER shows normal chat interface
   - Input goes directly to selected MANAGER
   - Streaming responses displayed in bottom window

**Testing**:
- Verify PROJECT_MANAGER.md and TASK_MANAGER.md are loaded
- Check session list shows both managers at top
- Chat with PROJECT MANAGER and TASK MANAGER works
- Session switching preserves state

**Dependencies**: Phase 1 complete

---

#### Phase 3: ACCOUNTS.json Protocol & VFSBOOT.md (Week 3-4)

**Goal**: Define JSON communication protocols and auto-generated documentation

**Files to Create**:
- `docs/ACCOUNTS_JSON_SPEC.md` - ACCOUNTS.json schema documentation
- `docs/MANAGER_PROTOCOL.md` - Manager-Account-Repo communication protocol
- `VFSBOOT.md` (auto-generated in management repository)

**Files to Modify**:
- `VfsShell/qwen_manager.cpp` - ACCOUNTS.json parsing and monitoring

**Tasks**:
1. ✅ Define ACCOUNTS.json schema:
   ```json
   {
     "accounts": [
       {
         "id": "account-1",
         "hostname": "computer-a",
         "enabled": true,
         "max_concurrent_repos": 3,
         "repositories": [
           {
             "id": "repo-1",
             "url": "https://github.com/user/repo1.git",
             "local_path": "/path/to/local/clone",
             "enabled": true,
             "worker_model": "qwen-auth",
             "manager_model": "qwen-openai"
           }
         ]
       }
     ]
   }
   ```
2. ✅ Implement ACCOUNTS.json parsing and validation
3. ✅ Add file watching with 10-second startup delay:
   - Monitor ACCOUNTS.json for changes
   - Reload and update account connections on file change
   - User can stop auto-reload before 10-second countdown
4. ✅ Auto-generate VFSBOOT.md if missing:
   - Include ACCOUNTS.json rules and schema
   - Link to AGENTS.md, QWEN.md from management repo
   - Add PROJECT_MANAGER.md and TASK_MANAGER.md references
5. ✅ Prompt PROJECT MANAGER to link all AI files to VFSBOOT.md

**Testing**:
- Create test ACCOUNTS.json with 2 accounts
- Verify parsing and validation
- Test auto-reload on file change
- Check VFSBOOT.md auto-generation

**Dependencies**: Phase 2 complete

---

#### Phase 4: ACCOUNT Client & Repository Management (Week 4-5)

**Goal**: Implement ACCOUNT connections and REPO session spawning

**Files to Create**:
- `scripts/account_client.sh` - Connect ACCOUNT to manager from remote computer
- `VfsShell/qwen_account.h` - ACCOUNT session management
- `VfsShell/qwen_account.cpp` - ACCOUNT implementation

**Files to Modify**:
- `VfsShell/qwen_manager.cpp` - Handle ACCOUNT connections
- `VfsShell/cmd_qwen.cpp` - Add ACCOUNT view rendering

**Tasks**:
1. ✅ Implement `account_client.sh`:
   ```bash
   #!/bin/bash
   # Usage: ./account_client.sh --manager-host <host> --manager-port <port>
   # Connects to manager and announces ACCOUNT capabilities
   ```
2. ✅ ACCOUNT command view (bottom panel when ACCOUNT selected):
   - Show list of REPO sessions spawned by this ACCOUNT
   - Accept commands without "/" prefix (technical commands)
   - Commands: `list`, `enable <repo>`, `disable <repo>`, `status <repo>`
3. ✅ ACCOUNT session spawning:
   - Read repositories from ACCOUNTS.json for this account
   - Spawn REPO WORKER (qwen-auth) and REPO MANAGER (qwen-openai) on-demand
   - Enforce max_concurrent_repos limit (default: 3)
   - Keep sessions open while in use, close when idle
4. ✅ Repository URL to local path mapping:
   - ACCOUNT responsibility (not manager's job)
   - Check if local clone exists, pull if needed
   - Report clone status back to manager
5. ✅ JSON-to-prompt conversion:
   - ACCOUNT receives JSON task specifications from MANAGER
   - Convert to natural language prompts for REPO sessions
   - Send responses back as JSON

**Testing**:
- Run account_client.sh and connect to manager
- Verify ACCOUNT view shows repositories
- Test `enable`/`disable` commands
- Check concurrent repo limit enforcement

**Dependencies**: Phase 3 complete

---

#### Phase 5: Automation Workflows & Escalation (Week 5-6)

**Goal**: Implement WORKER→MANAGER escalation and test-driven workflows

**Files to Create**:
- `VfsShell/qwen_workflow.h` - Workflow automation engine
- `VfsShell/qwen_workflow.cpp` - Workflow implementation
- `docs/QWEN_WORKFLOWS.md` - Workflow documentation

**Files to Modify**:
- `VfsShell/qwen_account.cpp` - Add workflow tracking

**Tasks**:
1. ✅ REPO WORKER failure tracking:
   - Track test failures per REPO session
   - Increment counter on each failed test attempt
   - Escalate to REPO MANAGER after 3 failures
2. ✅ REPO MANAGER escalation:
   - Spawn REPO MANAGER (qwen-openai) for difficult tasks
   - Pass context from WORKER failures
   - MANAGER takes over until tests pass
3. ✅ Test interval triggers:
   - Count commits per REPO session
   - After 3 WORKER commits, trigger REPO MANAGER to review and test
   - MANAGER runs comprehensive tests and code review
4. ✅ Manual override mode:
   - User selects REPO from list
   - User can type in chat to override automatic prompts
   - Switch back to automatic mode with `/auto` command or shortcut key
5. ✅ Status tracking:
   - Session status: automatic, manual, testing, blocked, idle
   - Display in session list
   - Update in real-time

**Testing**:
- Simulate 3 WORKER failures, verify MANAGER escalation
- Make 3 commits, verify MANAGER review triggered
- Test manual override and `/auto` command
- Check status updates in list

**Dependencies**: Phase 4 complete

---

#### Phase 6: UI Polish & Integration (Week 6-7)

**Goal**: Finalize UI, add shortcuts, improve UX

**Files to Modify**:
- `VfsShell/cmd_qwen.cpp` - UI enhancements
- `VfsShell/qwen_manager.cpp` - Keyboard shortcuts

**Tasks**:
1. ✅ Keyboard shortcuts:
   - `TAB` - Switch between list and input
   - `Shift+TAB` - Cycle permission modes (if applicable)
   - `/auto` - Return REPO to automatic mode
   - `Ctrl+C` - Interrupt current operation
   - Arrow keys - Navigate session list
   - `Enter` - Select session from list
2. ✅ Session list improvements:
   - Highlight selected session
   - Show brief activity indicator (spinner for active, checkmark for idle)
   - Sort: MANAGERS first, then ACCOUNTS, then REPOS
   - Filter: Show/hide disabled sessions
3. ✅ Bottom panel context-aware rendering:
   - MANAGER selected → chat view
   - ACCOUNT selected → command view (list of repos, technical commands)
   - REPO selected → chat view with override controls
4. ✅ Status bar enhancements:
   - Show manager mode indicator
   - Active session count (e.g., "5/10 active")
   - Network status (connected/disconnected)
5. ✅ Color coding consistency:
   - MANAGER_PROJECT: Bright cyan
   - MANAGER_TASK: Cyan
   - ACCOUNT: Yellow
   - REPO_MANAGER: Bright green
   - REPO_WORKER: Green
   - Errors: Red
   - System messages: Gray

**Testing**:
- Test all keyboard shortcuts
- Verify session list sorting and filtering
- Check color coding is consistent
- Verify status bar shows correct information

**Dependencies**: Phase 5 complete

---

#### Phase 7: Testing & Documentation (Week 7-8)

**Goal**: Comprehensive testing and documentation

**Files to Create**:
- `tests/manager_mode_test.cpp` - Unit tests for manager mode
- `scripts/examples/manager-mode-demo.cx` - Demo script
- `docs/QWEN_MANAGER_MODE.md` - Complete manager mode documentation
- `docs/QWEN_MANAGER_TUTORIAL.md` - Step-by-step tutorial

**Files to Modify**:
- `QWEN.md` - Add manager mode section
- `README.md` - Update with manager mode quick start

**Tasks**:
1. ✅ Unit tests:
   - Session lifecycle (create, update, destroy)
   - ACCOUNTS.json parsing and validation
   - Workflow state machine (escalation, test intervals)
   - TCP server connection handling
2. ✅ Integration tests:
   - Full manager-account-repo workflow
   - Multi-computer simulation (local TCP)
   - Failure recovery and reconnection
3. ✅ Demo script (`manager-mode-demo.cx`):
   - Start manager mode
   - Connect 2 accounts
   - Enable 3 repositories
   - Simulate workflow automation
   - Show manual override
4. ✅ Documentation:
   - **QWEN_MANAGER_MODE.md**: Architecture, protocols, workflows
   - **QWEN_MANAGER_TUTORIAL.md**: Step-by-step setup guide
   - **PROJECT_MANAGER.md**: AI role and responsibilities
   - **TASK_MANAGER.md**: AI role and responsibilities
   - Update QWEN.md with manager mode overview
   - Update README.md with quick start

**Testing**:
- Run all unit tests
- Run integration tests
- Execute demo script end-to-end
- Verify documentation accuracy

**Dependencies**: Phase 6 complete

---

#### Implementation Notes

**Technical Considerations**:
1. **Concurrency**: Use non-blocking I/O throughout (poll/epoll)
2. **JSON Parsing**: Reuse existing lightweight JSON parser from qwen_protocol
3. **Session State**: Store in VFS under `/qwen/manager/sessions/`
4. **TCP Protocol**: Extend existing qwen JSON protocol with manager-specific messages
5. **Error Handling**: Graceful degradation, reconnection logic, timeout handling
6. **Security**: Validate all incoming JSON, rate limiting, authentication (future)

**Repository Structure** (Management Repository):
```
Management/
├── VFSBOOT.md (auto-generated, links all AI docs)
├── PROJECT_MANAGER.md (qwen-openai instructions)
├── TASK_MANAGER.md (qwen-auth instructions)
├── ACCOUNTS.json (account configuration)
├── .git/
└── (other management files)
```

**Key Files Summary**:
- **New Files**: ~12 files (headers, implementations, scripts, docs)
- **Modified Files**: ~4 files (cmd_qwen.cpp, cmd_qwen.h, QWEN.md, README.md)
- **Estimated LOC**: 2000-2500 lines of C++ code + 500 lines of documentation

**Risks & Mitigations**:
- **Complexity**: Break into small, testable phases
- **Testing**: Difficult to test multi-computer setup → Use local TCP simulation
- **JSON Schema**: Evolving schema → Version ACCOUNTS.json format
- **Performance**: Multiple connections → Profile and optimize poll() usage

---

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

### qwen-code UI: Enhanced ncurses Mode with Permission System (2025-10-24) ✅
**COMPLETE**: Full-featured chat interface with tool approval, permission modes, and interactive controls

**Problems Solved**:
1. ✅ Tool approval input wasn't being captured (blocking input race condition)
2. ✅ No discuss option for tools
3. ✅ Missing permission mode system
4. ✅ Status bar didn't show permission mode or context usage
5. ✅ Mouse scrolling not implemented
6. ✅ Cursor jumped around during output

**What Was Implemented**:

1. ✅ **UI State Machine**:
   - `UIState::Normal` - Regular chat mode
   - `UIState::ToolApproval` - Waiting for tool approval (no input race condition!)
   - `UIState::Discuss` - Discussion mode for questioning tools
   - State-based input handling prevents blocking issues

2. ✅ **Permission Mode System** (Shift+Tab to cycle):
   - **PLAN** - Plan before executing (future: shows plan first)
   - **NORMAL** - Ask for approval on every tool (default)
   - **AUTO-EDIT** - Auto-approve Edit/Write tools only
   - **YOLO** - Approve anything, no sandbox restrictions
   - Current mode always visible in status bar

3. ✅ **Fixed Tool Approval Workflow**:
   - **[y]es** - Approve and execute tools
   - **[n]o** - Reject tools
   - **[d]iscuss** - Enter discussion mode to ask questions about the tools
   - No more blocking input - uses state machine
   - Auto-approval based on permission mode

4. ✅ **Enhanced Status Bar**:
   - Left: Model name and session ID
   - Right: Permission mode | Context usage % | Scroll indicator
   - Dynamic spacing to fill terminal width
   - Reverse video for clear visibility

5. ✅ **Mouse Scrolling Support**:
   - Mouse wheel up = scroll up through history
   - Mouse wheel down = scroll down
   - 3 lines per scroll event
   - Full `mousemask()` integration

6. ✅ **Keyboard Scrolling**:
   - Page Up/Page Down - scroll history
   - Ctrl+U - scroll up
   - Ctrl+D - scroll down
   - Arrow keys for cursor movement in input
   - Home/End, Ctrl+A/E for line navigation

7. ✅ **Proper Window Management**:
   - Output window: Scrollable conversation history
   - Status bar: Shows mode, context, scroll position
   - Input window: Fixed 3-line area at bottom with box
   - Output buffer system with color-coded lines

8. ✅ **Non-blocking Input Handling**:
   - Character-by-character input with `wtimeout(50ms)`
   - Full line editing preserved during AI responses
   - Input buffer maintained across all state changes
   - No race conditions or blocking issues

9. ✅ **Streaming Support**:
   - AI responses update in-place on last line
   - No cursor jumping during streaming
   - Clean separation between chunks and complete messages

**Key Technical Details**:
- State machine prevents tool approval input bugs
- Permission modes enable flexible workflow control
- Mouse events: `BUTTON4_PRESSED` (scroll up), `BUTTON5_PRESSED` (scroll down)
- `KEY_BTAB` for Shift+Tab detection
- Helper lambdas: `add_output_line()`, `redraw_output()`, `redraw_status()`, `redraw_input()`
- Non-blocking event loop polls messages (0ms) and keyboard (50ms)
- Context usage tracking placeholder (0-100%)

**Files Modified**:
- VfsShell/cmd_qwen.cpp:319-935 - Complete ncurses rewrite with state machine

**Impact**:
Users can now:
- ✅ Approve/reject tools reliably (no more input bugs!)
- ✅ Use discuss mode to ask questions before approving
- ✅ Switch permission modes on the fly (Shift+Tab)
- ✅ Scroll with mouse wheel or keyboard
- ✅ See permission mode and context usage in status bar
- ✅ Type continuously while receiving responses
- ✅ Enjoy a professional, bug-free chat interface

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
