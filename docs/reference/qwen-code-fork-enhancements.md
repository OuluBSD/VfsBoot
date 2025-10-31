# Enhancement Requests for qwen-code Fork

**Repository**: `/common/active/sblo/Dev/qwen-code` (your local fork)
**Purpose**: Optional improvements to make your qwen-code fork work better with VfsBoot

These enhancements are **optional** - VfsBoot's C++ integration is fully functional without them. Implementing these would improve the user experience when running qwen-code as a standalone server.

---

## 1. TCP Startup Instructions

**Priority**: Medium
**Impact**: Better UX when starting qwen-code in TCP mode

### Problem
When users start qwen-code in TCP server mode, there's no indication of how to connect from VfsBoot or test the connection.

### Solution
Add helpful startup instructions when running in TCP mode.

### Implementation

**File**: `/common/active/sblo/Dev/qwen-code/packages/cli/src/gemini.tsx`

**Location**: After `await server.start();` (around line 650)

**Current Code**:
```typescript
await server.start();

const startupWarnings = [
  ...(await getStartupWarnings()),
  ...(await getUserStartupWarnings(workspaceRoot)),
];

return runServerMode(config, settings, startupWarnings, workspaceRoot, server);
```

**Proposed Addition**:
```typescript
await server.start();

// Print startup instructions for TCP mode
if (argv.serverMode === 'tcp' && argv.tcpPort) {
  console.error(`[ServerMode] TCP server listening on port ${argv.tcpPort}`);
  console.error(`[ServerMode] Connect from VfsBoot: qwen --mode tcp --port ${argv.tcpPort}`);
  console.error(`[ServerMode] Test with netcat: echo '{"type":"user_input","content":"hello"}' | nc localhost ${argv.tcpPort}`);
}

const startupWarnings = [
  ...(await getStartupWarnings()),
  ...(await getUserStartupWarnings(workspaceRoot)),
];

return runServerMode(config, settings, startupWarnings, workspaceRoot, server);
```

### Expected Output
```bash
$ qwen-code --server-mode tcp --tcp-port 7777
[ServerMode] TCP server listening on port 7777
[ServerMode] Connect from VfsBoot: qwen --mode tcp --port 7777
[ServerMode] Test with netcat: echo '{"type":"user_input","content":"hello"}' | nc localhost 7777
```

---

## 2. Simple --openai Flag

**Priority**: Medium
**Impact**: Easier OpenAI usage without explicit API key specification

### Problem
Users need to either:
- Specify API key explicitly: `--openai-api-key sk-...` (insecure, appears in process list)
- Set environment variable and remember authentication is auto-detected

A simple `--openai` flag would be more intuitive.

### Solution
Add a boolean `--openai` flag that:
1. Automatically detects `OPENAI_API_KEY` environment variable
2. Sets authentication type to OpenAI
3. Provides clear error if API key not found

### Implementation

#### Step 1: Add CLI Option

**File**: `/common/active/sblo/Dev/qwen-code/packages/cli/src/config/config.ts`

Add to yargs configuration:
```typescript
.option('openai', {
  type: 'boolean',
  description: 'Use OpenAI provider with API key from OPENAI_API_KEY environment variable',
  default: false,
})
```

#### Step 2: Update Auth Logic

**File**: `/common/active/sblo/Dev/qwen-code/packages/cli/src/gemini.tsx`

In server mode authentication section (around lines 605-620), before existing auth logic:

```typescript
// Check if --openai flag was provided
if (argv.openai) {
  console.error('[ServerMode] Using OpenAI provider as requested via --openai flag');

  // Verify OPENAI_API_KEY is set
  if (!process.env.OPENAI_API_KEY) {
    console.error('[ServerMode] ERROR: OPENAI_API_KEY environment variable not set');
    console.error('[ServerMode] Please set it before using --openai flag:');
    console.error('[ServerMode]   export OPENAI_API_KEY=sk-...');
    process.exit(1);
  }

  try {
    await config.refreshAuth(AuthType.USE_OPENAI);
    console.error('[ServerMode] OpenAI auth initialized successfully');
  } catch (error) {
    console.error('[ServerMode] Failed to initialize OpenAI auth:', error);
    process.exit(1);
  }

  return runServerMode(config, settings, startupWarnings, workspaceRoot, server);
}
```

### Usage Examples

**Before** (current):
```bash
export OPENAI_API_KEY=sk-...
qwen-code --server-mode tcp --tcp-port 7777 --openai-api-key $OPENAI_API_KEY
```

**After** (with new flag):
```bash
export OPENAI_API_KEY=sk-...
qwen-code --server-mode tcp --tcp-port 7777 --openai
```

**Interactive mode**:
```bash
export OPENAI_API_KEY=sk-...
qwen-code --openai
```

**Error handling**:
```bash
$ qwen-code --openai
[ServerMode] Using OpenAI provider as requested via --openai flag
[ServerMode] ERROR: OPENAI_API_KEY environment variable not set
[ServerMode] Please set it before using --openai flag:
[ServerMode]   export OPENAI_API_KEY=sk-...
```

---

## 3. Update Help Documentation

**Priority**: Low
**Impact**: Better discoverability of server modes

### Implementation

Update `--help` output to clearly document:
- All three server modes (stdin, tcp, pipe)
- When to use each mode
- Example commands for common scenarios

**File**: `/common/active/sblo/Dev/qwen-code/packages/cli/src/config/config.ts`

Enhance descriptions for existing options:
```typescript
.option('server-mode', {
  type: 'string',
  description: 'Server mode: stdin (default, single client), tcp (network, multiple clients), pipe (Unix only, filesystem IPC)',
  choices: ['stdin', 'tcp', 'pipe'],
})
.option('tcp-port', {
  type: 'number',
  description: 'Port for TCP server mode (use with --server-mode tcp)',
  default: 7777,
})
```

---

## Testing Checklist

After implementing these enhancements:

- [ ] TCP startup instructions appear when running `qwen-code --server-mode tcp --tcp-port 7777`
- [ ] `--openai` flag works with OPENAI_API_KEY set
- [ ] `--openai` flag shows clear error when OPENAI_API_KEY not set
- [ ] `--help` output shows updated documentation
- [ ] VfsBoot can connect successfully: `qwen --mode tcp --port 7777`
- [ ] netcat test works: `echo '{"type":"user_input","content":"test"}' | nc localhost 7777`

---

## Benefits Summary

1. **TCP Instructions**: Users immediately know how to connect after starting the server
2. **--openai Flag**: Simplified OpenAI usage without exposing API key in command line
3. **Better Help**: Improved discoverability of server mode features

**Total Implementation Time**: ~1-2 hours
**Total Code Changes**: ~30 lines across 2 files
