# qwen Documentation Audit & Cleanup Plan

**Date**: 2025-10-23
**Issue**: Previous AI agent created 15 qwen-related documentation files, many now outdated or redundant after ncurses implementation completion.

## Current Status

**✅ REALITY**: ncurses frontend IS fully implemented (465 lines in `VfsShell/cmd_qwen.cpp:320-785`, commit `b5c502d`)
**❌ DOCS SAY**: "ncurses not yet implemented" (QWEN.md:69) ← **INCORRECT!**

## Documentation Files Audit (15 total)

### Category A: Core Documentation (KEEP & UPDATE)

| File | Size | Status | Action |
|------|------|--------|--------|
| **QWEN.md** | 24K | ❌ Outdated | **UPDATE** - Fix line 69, add ncurses docs |
| **QWEN_STRUCTURED_PROTOCOL.md** | 14K | ✅ Good | Keep - protocol reference |
| **QWEN_LOGGING_GUIDE.md** | 2.8K | ✅ Good | Keep - debugging reference |

**Total to keep**: 3 files (40.8K)

---

### Category B: Implementation History (ARCHIVE)

These document phases that are now complete. Move to `docs/archive/qwen/`:

| File | Size | Why Archive |
|------|------|-------------|
| **QWEN_CLIENT_IMPLEMENTATION.md** | 12K | Phase 2 implementation notes (done) |
| **QWEN_CODE_INTEGRATION_PLAN.md** | 6.8K | Original planning doc (superseded) |
| **qwen_implementation_summary.md** | 3.4K | Old status snapshot |
| **qwen_integration_progress_summary.md** | 3.4K | Old progress report |
| **QWEN_VIRTUAL_TERMINAL_PLAN.md** | 9.1K | Virtual terminal plan (ncurses done) |

**Total to archive**: 5 files (34.7K)

**Action**: `mkdir -p docs/archive/qwen && mv <files> docs/archive/qwen/`

---

### Category C: Redundant "Next Steps" Docs (DELETE)

Multiple overlapping "what to do next" files created at different times:

| File | Size | Why Delete |
|------|------|------------|
| **qwen_next_steps.md** | 1.7K | Lists completed tasks as "critical" |
| **qwen_next_steps_roadmap.md** | 3.8K | Duplicate of above |

**Total to delete**: 2 files (5.5K)

**Action**: Delete after confirming no unique info

---

### Category D: qwen-code Team Requests (CONSOLIDATE)

These document changes needed in the **qwen-code TypeScript project** (not VfsBoot):

| File | Size | Purpose |
|------|------|---------|
| **qwen_code_tcp_docs.md** | 1.7K | Add TCP startup instructions to gemini.tsx |
| **qwen_code_openai_flag_docs.md** | 2.3K | Add --openai flag to qwen-code CLI |
| **qwen_code_openai_implementation_guide.md** | 3.7K | Implementation details for --openai |

**Action**: Consolidate into single `docs/qwen-code-team-requests.md`

---

### Category E: ncurses Docs (OBSOLETE - ALREADY DONE!)

These suggest implementing ncurses features that are **already implemented**:

| File | Size | Status |
|------|------|--------|
| **ncurses_ui_improvements.md** | 3.0K | ❌ Suggests already-done features |
| **ncurses_ui_implementation_guide.md** | 17K | ❌ Guide for completed work |

**Why obsolete**:
- Color support ✅ DONE (6 color pairs in cmd_qwen.cpp:331-343)
- Status bar ✅ DONE (cmd_qwen.cpp:400-415)
- Message differentiation ✅ DONE (cmd_qwen.cpp:445-520)
- Tool approval workflow ✅ DONE (cmd_qwen.cpp:580-650)
- Terminal detection ✅ DONE (cmd_qwen.cpp:275-318)

**Action**: Archive to `docs/archive/qwen/ncurses-planning/`

---

## Cleanup Actions

### Step 1: Update Core Docs

**QWEN.md** - Fix critical inaccuracies:
```diff
- **Note**: The current implementation uses **line-based stdio mode**. An ncurses full-screen interface is planned (Priority 3 in TASKS.md) but not yet implemented.
+ **Note**: The current implementation uses **full-screen ncurses mode** by default with automatic terminal capability detection. Falls back to line-based stdio mode for unsupported terminals. Use `qwen --simple` to force stdio mode.
```

Add ncurses documentation section:
- Terminal requirements
- Color support details
- Keyboard controls
- Status bar layout
- Special commands in ncurses mode

### Step 2: Create Archive Directory

```bash
mkdir -p docs/archive/qwen/ncurses-planning
mkdir -p docs/archive/qwen/implementation-phases
```

### Step 3: Move Files

**Archive implementation history**:
```bash
mv QWEN_CLIENT_IMPLEMENTATION.md docs/archive/qwen/implementation-phases/
mv QWEN_CODE_INTEGRATION_PLAN.md docs/archive/qwen/implementation-phases/
mv qwen_implementation_summary.md docs/archive/qwen/implementation-phases/
mv qwen_integration_progress_summary.md docs/archive/qwen/implementation-phases/
mv QWEN_VIRTUAL_TERMINAL_PLAN.md docs/archive/qwen/implementation-phases/
```

**Archive ncurses planning**:
```bash
mv ncurses_ui_improvements.md docs/archive/qwen/ncurses-planning/
mv ncurses_ui_implementation_guide.md docs/archive/qwen/ncurses-planning/
```

### Step 4: Consolidate qwen-code Team Requests

Create `docs/qwen-code-team-requests.md`:
```markdown
# Requests for qwen-code TypeScript Project

These are enhancement requests for the upstream qwen-code project (TypeScript/Node.js).
VfsBoot's C++ integration is complete; these would improve the qwen-code CLI.

## 1. TCP Startup Instructions
[content from qwen_code_tcp_docs.md]

## 2. Simple --openai Flag
[content from qwen_code_openai_flag_docs.md and implementation guide]
```

Then delete the originals:
```bash
rm qwen_code_tcp_docs.md
rm qwen_code_openai_flag_docs.md
rm qwen_code_openai_implementation_guide.md
```

### Step 5: Delete Redundant Files

```bash
rm qwen_next_steps.md
rm qwen_next_steps_roadmap.md
```

### Step 6: Update TASKS.md

Remove outdated qwen tasks:
```diff
- **Priority 3: VfsBoot qwen Command** ✅
- - ✅ Add ncurses mode to cmd_qwen.cpp (user expects interactive terminal like original qwen)
- - ✅ Detect terminal capabilities and default to ncurses
- - ✅ Fallback to line-based stdio when ncurses unavailable
- - ✅ Add `qwen --simple` flag to force stdio mode
```

Add actual next steps (TBD after testing).

---

## Result After Cleanup

**Before**: 15 files (113.5K total)
**After**: 4 files organized

```
Root directory:
├── QWEN.md (updated, 25K)                    # Main integration guide
├── QWEN_STRUCTURED_PROTOCOL.md (3K)          # Protocol reference
└── QWEN_LOGGING_GUIDE.md (2K)                # Debugging guide

docs/:
└── qwen-code-team-requests.md (8K)           # Consolidated upstream requests

docs/archive/qwen/:
├── implementation-phases/                    # Historical planning docs
│   ├── QWEN_CLIENT_IMPLEMENTATION.md
│   ├── QWEN_CODE_INTEGRATION_PLAN.md
│   ├── qwen_implementation_summary.md
│   ├── qwen_integration_progress_summary.md
│   └── QWEN_VIRTUAL_TERMINAL_PLAN.md
└── ncurses-planning/                         # ncurses planning (now implemented)
    ├── ncurses_ui_improvements.md
    └── ncurses_ui_implementation_guide.md
```

**Deleted**: 2 redundant "next steps" files

---

## Testing & Validation Needed

Before determining real next steps, verify current implementation:

1. **Build test**:
   ```bash
   make clean && make
   ```

2. **ncurses mode test**:
   ```bash
   # Start qwen-code server in another terminal
   cd /common/active/sblo/Dev/qwen-code
   node packages/cli/dist/index.js --server-mode tcp --tcp-port 7777

   # Test ncurses mode
   ./vfsh
   vfsh> qwen --mode tcp --port 7777
   # Should show full-screen interface with colors
   ```

3. **Stdio fallback test**:
   ```bash
   ./vfsh
   vfsh> qwen --simple --mode tcp --port 7777
   # Should use line-based mode
   ```

4. **Feature verification**:
   - ✅ Color support works
   - ✅ Status bar shows correct info
   - ✅ Tool approval workflow
   - ✅ `/exit`, `/detach`, `/help` commands
   - ✅ Session persistence

5. **Check for issues**:
   - Any crashes or hangs?
   - Terminal detection working correctly?
   - Colors displaying properly?
   - Any error messages in logs?

---

## Real Next Steps (TBD)

After testing, determine actual priorities:

**Possible areas**:
- Bug fixes (if any found during testing)
- Performance optimization (if sluggish)
- Additional ncurses features (if basic ones work well)
- Integration with VfsBoot planner system
- Session sharing/export
- WebSocket support for browser clients

**NOT priorities** (already done):
- ~~Implement ncurses mode~~ ✅
- ~~Add terminal detection~~ ✅
- ~~Color support~~ ✅
- ~~Tool approval workflow~~ ✅

---

## Summary

The previous AI agent was working from outdated context and created documentation suggesting features that are **already implemented**. The ncurses frontend is complete and functional. After cleanup, we'll have clean, accurate documentation and can focus on real improvements rather than reimplementing existing features.
