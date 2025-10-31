# Task Context for qwen-code Integration

## Current State (2025-10-23)

### Phase 5 Option A: COMPLETE ✅

The qwen-oauth server mode is fully working end-to-end with real AI!

#### What Works
- ✅ Config.refreshAuth() properly initializes contentGeneratorConfig + GeminiClient
- ✅ Server mode authentication working (qwen-oauth)
- ✅ Bidirectional JSON protocol validated
- ✅ Protocol messages: init, conversation, tool_group, status, info, error, completion_stats
- ✅ Commands: user_input, tool_approval, interrupt, model_switch

#### Architecture Summary

```
VfsBoot (C++)                                    qwen-code (TypeScript)
┌──────────────────────────────────────────┐    ┌──────────────────────┐
│ [Phase 4] cmd_qwen (Shell Command)      │    │ structuredServerMode │
│ - Interactive terminal loop              │    │ - QwenStateSerializer│
│ - User input / display responses         │    │ - StdinStdoutServer  │
│ - Tool approval prompts                  │    │ - TCPServer          │
│ - Session management UI                  │    │ - NamedPipeServer    │
│ - /detach, /exit, /help commands         │    └──────────────────────┘
└──────────────┬───────────────────────────┘              ▲
               │                                           │
               ▼                                           │
┌──────────────────────────────────────────┐             │
│ [Phase 2] qwen_client                    │◄────stdin───┤
│ - fork/exec subprocess                   │     stdout  │
│ - poll() I/O (non-blocking)              │     JSON    │
│ - Message callbacks                      │     msgs    │
│ - Auto-restart on crash                  │             │
└──────────────┬───────────────────────────┘             │
               │                                           │
               ▼                                           │
┌──────────────────────────────────────────┐             │
│ [Phase 3] qwen_state_manager             │             │
│ - Session CRUD (create/load/save/delete) │             │
│ - Conversation history (/qwen/history/)  │             │
│ - File storage (/qwen/files/)            │             │
│ - Tool group tracking                    │             │
│ - VFS persistence (JSON/JSONL)           │             │
└──────────────┬───────────────────────────┘             │
               │                                           │
               ▼                                           ▼
┌──────────────────────────────────────────┐    qwenStateSerializer
│ [Phase 2] qwen_protocol                  │    - serializeHistoryItem()
│ - parse_message() (TS → C++)             │    - getIncrementalUpdates()
│ - serialize_command() (C++ → TS)         │
│ - Type-safe structs (std::variant)       │
└───────────────────────────────────────────┘

         VFS (/qwen/)
         ├── sessions/
         │   └── <session-id>/
         │       ├── metadata.json
         │       ├── history.jsonl
         │       ├── tool_groups.jsonl
         │       └── files/
         ├── history/  (future: shared history)
         └── files/    (future: shared files)
```

### VfsBoot Implementation Status (Phase 2-4 COMPLETE)

#### Phase 2: C++ Client Implementation ✅
- **qwen_protocol.{h,cpp}**: Type-safe protocol structs, lightweight JSON parser (no deps)
- **qwen_client.{h,cpp}**: Subprocess management, fork/exec, non-blocking I/O with poll()
- **qwen_echo_server.cpp**: Test echo server
- **Tests**: 18/18 protocol tests PASS, integration test PASS

#### Phase 3: VFS Integration ✅
- **qwen_state_manager.{h,cpp}**: Full session management API with VFS-backed persistence
- **Storage**: `/qwen/sessions/<id>/` with metadata.json, history.jsonl, tool_groups.jsonl, files/
- **API**: Session CRUD, history tracking, file storage, metadata, maintenance
- **Tests**: 7/8 tests PASS (minor bug in stats)

#### Phase 4: Shell Command Integration ✅
- **cmd_qwen.{h,cpp}**: Interactive command with session management
- **Features**: Session lifecycle, interactive loop, tool approval, special commands (/detach, /exit, etc.)
- **Demo scripts**: qwen-demo.cx, qwen-session-demo.cx

### qwen-code Implementation Status

#### Server Mode Infrastructure ✅
**File**: `packages/cli/src/structuredServerMode.ts` (~370 lines)

**Three transport modes**:
1. **StdinStdoutServer**: Line-buffered JSON via stdio (default)
2. **NamedPipeServer**: Unix named pipes
3. **TCPServer**: TCP socket server (port configurable)

**Current usage** (from TASKS.md context):
```bash
# TCP mode already works!
qwen-code --server-mode tcp --tcp-port 7777

# Test with netcat
echo '{"type":"user_input","content":"hello"}' | nc localhost 7777
```

**Note**: TCP mode is already implemented and tested successfully!

### Recent Fixes (2025-10-23)

**Problem**: Config.getContentGeneratorConfig() returned undefined in server mode

**Root Cause**:
- Config.initialize() doesn't create contentGeneratorConfig
- Normal CLI calls config.refreshAuth() which creates contentGeneratorConfig + GeminiClient
- Server mode skipped refreshAuth()

**Solution** (commit `3ee2e276` in qwen-code):
1. Added config.refreshAuth(authType) before runServerMode()
2. Simplified error handling (auth guaranteed before server mode)
3. Fixed TypeScript comparison error in config.ts:595

### What the User Wants

From the user's message:

1. **Documentation cleanup**:
   - TASKS.md is too messy (1879 lines!)
   - Extract context to TASK_CONTEXT.md (this file!)
   - Create QWEN.md with integration rules
   - Link QWEN.md from AGENTS.md (all agents must read it)

2. **qwen-code CLI improvements**:
   - Make TCP server mode easy to discover
   - Add instructions on how to run qwen-code as TCP server in another terminal
   - Add flag to easily use OpenAI for qwen (not just qwen-oauth)

3. **VfsBoot qwen command improvements**:
   - When running `qwen` in CLI, expect ncurses session (like original qwen experience)
   - Current stdio version is "messy" for user

### Key Files

**VfsBoot**:
- `VfsShell/qwen_protocol.{h,cpp}` - Protocol layer
- `VfsShell/qwen_client.{h,cpp}` - Communication layer
- `VfsShell/qwen_state_manager.{h,cpp}` - Storage layer
- `VfsShell/cmd_qwen.{h,cpp}` - Shell command
- `VfsShell/codex.cpp` - Main binary

**qwen-code**:
- `packages/cli/src/gemini.tsx` - Main entry point
- `packages/cli/src/structuredServerMode.ts` - Server mode infrastructure
- `packages/cli/src/config/config.ts` - Configuration system
- `packages/cli/src/qwenStateSerializer.ts` - State serialization

### Next Steps

#### Documentation (Priority 1)
1. ✅ Create TASK_CONTEXT.md (this file) with condensed context
2. Create QWEN.md with:
   - How to use qwen-code as standalone server
   - TCP server mode instructions
   - OpenAI authentication options
   - Integration with VfsBoot
   - Rules: All agents working on qwen integration must read QWEN.md first
3. Update AGENTS.md with link to QWEN.md
4. Compact TASKS.md (move most context here, keep only active task tracking)

#### qwen-code CLI Enhancements (Priority 2)
1. Add `--help` output documenting all server modes
2. Add startup instructions when running in TCP mode:
   ```
   qwen-code --server-mode tcp --tcp-port 7777
   > TCP server listening on port 7777
   > Connect from VfsBoot: qwen --mode tcp --port 7777
   > Test with netcat: echo '{"type":"user_input","content":"hello"}' | nc localhost 7777
   ```
3. Add `--openai` flag for easy OpenAI usage:
   ```
   qwen-code --server-mode tcp --tcp-port 7777 --openai
   qwen-code --openai  # Interactive mode with OpenAI
   ```

#### VfsBoot qwen Command (Priority 3)
1. Add ncurses mode to cmd_qwen.cpp
2. Detect terminal capabilities and default to ncurses when available
3. Fallback to line-based stdio mode when ncurses unavailable
4. Add `qwen --simple` flag to force stdio mode

### Protocol Reference

**Messages (TypeScript → C++)**:
- `init`: Initial handshake with version, workspace, model
- `conversation`: User/assistant message echo
- `tool_group`: Tool call requests with approval/rejection
- `status`: Processing status updates
- `info`: Informational messages
- `error`: Error messages
- `completion_stats`: Token counts and timing

**Commands (C++ → TypeScript)**:
- `user_input`: User typed input
- `tool_approval`: Approve/reject tool execution
- `interrupt`: Cancel current operation
- `model_switch`: Change AI model

### Environment Variables

**qwen-code**:
- `OPENAI_API_KEY` - OpenAI authentication
- `GEMINI_API_KEY` - Google Gemini authentication
- `CODEX_AI_PROVIDER` - Force provider (openai, llama, qwen-oauth)

**VfsBoot**:
- `OPENAI_API_KEY` - OpenAI (fallback: ~/openai-key.txt)
- `LLAMA_BASE_URL` / `LLAMA_SERVER` - Llama server URL
- `LLAMA_MODEL` - Llama model name
- `CODEX_AI_PROVIDER` - Force provider (openai, llama)
- `QWEN_AUTO_APPROVE` - Auto-approve tools in qwen command

### Testing Status

**VfsBoot C++ Tests**:
- Protocol parsing: 18/18 ✅
- Client integration: PASS ✅
- State manager: 7/8 (minor stats bug)
- Echo server: PASS ✅

**qwen-code TypeScript Tests**:
- TCP mode: PASS ✅ (tested with netcat)
- Stdin mode: PASS ✅ (subprocess tested)
- Named pipe mode: NOT TESTED
- Server mode auth: PASS ✅

### Known Issues

1. **stdin mode complexity**: VfsBoot REPL stdin conflicts with nested readline
2. **Named pipe mode**: Not tested end-to-end
3. **Stats bug**: get_storage_stats() needs better error handling for deleted sessions
4. **Ncurses missing**: qwen command doesn't have ncurses mode yet
5. **Documentation scattered**: Context spread across multiple QWEN_*.md files

### Related Documentation

- `QWEN_CODE_INTEGRATION_PLAN.md` - Original plan (mostly obsolete, superseded by completed phases)
- `QWEN_VIRTUAL_TERMINAL_PLAN.md` - Terminal UI plan (partially implemented)
- `QWEN_STRUCTURED_PROTOCOL.md` - Protocol specification
- `QWEN_CLIENT_IMPLEMENTATION.md` - Phase 2 summary (complete)
- `QWEN.md` - Currently contains Registry docs (WRONG CONTENT - needs replacement!)
- `scripts/examples/qwen-demo.cx` - Basic usage demo
- `scripts/examples/qwen-session-demo.cx` - Session management demo

### Success Criteria for Current Work

1. ✅ TASK_CONTEXT.md created with all context consolidated
2. ⏳ QWEN.md created with integration guide and rules
3. ⏳ AGENTS.md updated with QWEN.md link
4. ⏳ TASKS.md compacted (< 500 lines)
5. ⏳ qwen-code --help shows server modes
6. ⏳ TCP mode prints usage instructions on startup
7. ⏳ --openai flag added for easy OpenAI usage
8. ⏳ ncurses mode implemented for qwen command
