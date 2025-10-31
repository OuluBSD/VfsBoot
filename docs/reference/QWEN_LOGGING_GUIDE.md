# qwen-code Logging Guide

This document explains the fine-grained logging system for debugging qwen-code integration between VfsBoot (C++) and qwen-code (TypeScript).

## Overview

The logging system provides microsecond-precision timestamps with PIDs and descriptive tags, allowing logs from both sides to be merged and sorted to understand the exact sequence of events.

## Log Format

```
[YYYY-MM-DD HH:MM:SS.microseconds] [PID:12345] [LEVEL] [tag:id] message
```

Example logs:
```
[2025-10-23 14:23:45.123456] [PID:12345] [INFO ] [qwen-client:session-abc] QwenClient created, mode=stdin
[2025-10-23 14:23:45.234567] [PID:12346] [INFO ] [qwen-server:tcp-conn-1] Client connected from ::1:48338
[2025-10-23 14:23:45.345678] [PID:12345] [DEBUG] [qwen-client:session-abc] >>> Sending command: {"type":"user_input","content":"hello"}
[2025-10-23 14:23:45.456789] [PID:12346] [DEBUG] [qwen-server:tcp-conn-1] <<< Received: {"type":"user_input","content":"hello"}
```

## Environment Variables

### Both Client and Server

| Variable | Default | Description |
|----------|---------|-------------|
| `QWEN_LOG_FILE` | stderr | Path to log file for output |
| `QWEN_LOG_LEVEL` | INFO | Minimum log level: DEBUG, INFO, WARN, ERROR |

### Usage Examples

```bash
# Enable debug logging to file for both sides
export QWEN_LOG_FILE=/tmp/qwen_debug.log
export QWEN_LOG_LEVEL=DEBUG

# Terminal 1 - Start server with logging
./run_qwen_server.sh --openai

# Terminal 2 - Start client with logging
./run_qwen_client.sh

# Terminal 3 - Watch merged logs in real-time
tail -f /tmp/qwen_debug.log | sort -k1,2
```

## C++ Client Implementation (VfsBoot)

### Files

- `VfsShell/qwen_logger.h` - Logger class definition
- `VfsShell/qwen_client.cpp` - Logging integration

### Key Log Points

1. **Client Lifecycle**
   - `QwenClient created, mode=...` - Client instantiation
   - `start() called` - Starting client
   - `Subprocess started: PID=... stdin_fd=... stdout_fd=...` - Process forked
   - `stop() called, terminating subprocess PID=...` - Stopping client
   - `Client stopped` - Clean shutdown

2. **Sending Commands** (`>>>` prefix)
   - `>>> Sending command: {...}` - Command being sent (truncated if >200 chars)
   - `>>> Sent N bytes` - Write completed

3. **Receiving Messages** (`<<<` prefix)
   - `<<< Read N bytes from subprocess` - Data received
   - `<<< Received message: {...}` - Complete message parsed
   - `<<< Processed N message(s)` - Batch complete
   - `    Dispatching message type=...` - Handler invoked

4. **Errors**
   - `write() failed: ...` - Send error
   - `<<< read() failed: ...` - Receive error
   - `Failed to parse message: ...` - Protocol error
   - `<<< EOF: Subprocess closed stdout` - Unexpected disconnect

### Usage in C++

```cpp
#include "qwen_logger.h"

// Create logger with tag and optional ID
QwenLogger logger("qwen-client", "session-123");

// Log at different levels
logger.debug("Debug message with ", variable);
logger.info("Info message");
logger.warn("Warning message");
logger.error("Error message");

// Update ID when it becomes available
logger.set_id("session-abc123");
```

## TypeScript Server Implementation (qwen-code)

### Files

- `packages/cli/src/qwenLogger.ts` - Logger class (created)
- `packages/cli/src/structuredServerMode.ts` - Integration needed

### Integration Steps

1. **Import the logger** in `structuredServerMode.ts`:
```typescript
import { QwenLogger } from './qwenLogger.js';
```

2. **Add logger to server classes**:
```typescript
export class StdinStdoutServer extends BaseStructuredServer {
  private logger: QwenLogger;

  constructor(onCommand: (cmd: QwenCommand) => void) {
    super();
    this.logger = new QwenLogger("qwen-server", "stdin");
    this.logger.info("StdinStdoutServer created");
  }
}

export class TCPServer extends BaseStructuredServer {
  private logger: QwenLogger;
  private connectionId: number = 0;

  constructor(port: number, onCommand: (cmd: QwenCommand) => void) {
    super();
    this.logger = new QwenLogger("qwen-server", `tcp-port-${port}`);
    this.logger.info("TCPServer created, port=", port);
  }

  // Update ID when client connects
  private handleConnection(socket: net.Socket) {
    const connId = `tcp-conn-${++this.connectionId}`;
    this.logger.setId(connId);
    this.logger.info("Client connected from", socket.remoteAddress);
  }
}
```

3. **Add logging to key points**:

```typescript
// Sending messages (>>> prefix)
async sendMessage(msg: QwenStateMessage): Promise<void> {
  const json = JSON.stringify(msg);
  const truncated = json.length > 200 ? json.substr(0, 197) + "..." : json;
  this.logger.debug(">>> Sending message:", truncated);

  await this.writeToStream(json + '\n');

  this.logger.debug(">>> Sent", json.length, "bytes");
}

// Receiving commands (<<< prefix)
private async handleLine(line: string): Promise<void> {
  const truncated = line.length > 200 ? line.substr(0, 197) + "..." : line;
  this.logger.debug("<<< Received:", truncated);

  try {
    const cmd = JSON.parse(line) as QwenCommand;
    this.logger.debug("    Dispatching command type=", cmd.type);
    this.onCommand(cmd);
  } catch (err) {
    this.logger.error("Failed to parse command:", err.message);
  }
}

// Connection lifecycle
this.logger.info("Server started, listening for connections");
this.logger.warn("Client disconnected from", socket.remoteAddress);
this.logger.info("Server stopped");
```

## Debugging Workflow

### 1. Capture Logs from Both Sides

```bash
# Set up logging
export QWEN_LOG_FILE=/tmp/qwen_debug.log
export QWEN_LOG_LEVEL=DEBUG

# Clear old logs
> /tmp/qwen_debug.log

# Start server (Terminal 1)
./run_qwen_server.sh --openai

# Start client (Terminal 2)
./run_qwen_client.sh
```

### 2. Reproduce the Issue

Interact with the client to trigger the problem you're debugging.

### 3. Analyze the Merged Log

```bash
# Sort logs by timestamp to see exact sequence
sort /tmp/qwen_debug.log > /tmp/qwen_sorted.log
less /tmp/qwen_sorted.log
```

### 4. Look for Patterns

**Normal flow:**
```
[...] [PID:C] [INFO ] [qwen-client] start() called
[...] [PID:C] [INFO ] [qwen-client] Subprocess started: PID=S
[...] [PID:S] [INFO ] [qwen-server:stdin] Server started
[...] [PID:C] [DEBUG] [qwen-client] >>> Sending command: {"type":"user_input",...}
[...] [PID:S] [DEBUG] [qwen-server:stdin] <<< Received: {"type":"user_input",...}
[...] [PID:S] [DEBUG] [qwen-server:stdin] >>> Sending message: {"type":"conversation",...}
[...] [PID:C] [DEBUG] [qwen-client] <<< Received message: {"type":"conversation",...}
```

**Problem: Client connects then immediately disconnects:**
```
[...] [PID:S] [INFO ] [qwen-server:tcp] Client connected from ::1:48338
[...] [PID:S] [WARN ] [qwen-server:tcp] Client disconnected from ::1:48338
```
→ Look for: Missing messages between connect/disconnect, socket errors

**Problem: Command sent but no response:**
```
[...] [PID:C] [DEBUG] [qwen-client] >>> Sending command: {...}
[...] [PID:C] [DEBUG] [qwen-client] >>> Sent 123 bytes
[no server log showing receipt]
```
→ Look for: Server crash, pipe broken, parsing errors

**Problem: Response sent but not received:**
```
[...] [PID:S] [DEBUG] [qwen-server] >>> Sending message: {...}
[...] [PID:S] [DEBUG] [qwen-server] >>> Sent 456 bytes
[no client log showing receipt]
```
→ Look for: Buffer full, file descriptor closed, parsing errors

## Tips

1. **Use DEBUG level** for detailed I/O tracing:
   ```bash
   export QWEN_LOG_LEVEL=DEBUG
   ```

2. **Filter logs** by PID or tag:
   ```bash
   grep "PID:12345" /tmp/qwen_debug.log
   grep "qwen-client" /tmp/qwen_debug.log
   grep ">>>" /tmp/qwen_debug.log  # Only sends
   grep "<<<" /tmp/qwen_debug.log  # Only receives
   ```

3. **Compare timings** between operations:
   ```bash
   # Extract timestamps only
   awk '{print $1, $2}' /tmp/qwen_debug.log
   ```

4. **Real-time monitoring** during development:
   ```bash
   tail -f /tmp/qwen_debug.log | grep --line-buffered "ERROR\|WARN"
   ```

5. **Clean logs between runs**:
   ```bash
   echo "=== NEW TEST RUN ===" >> /tmp/qwen_debug.log
   ```

## Current Status

### ✅ Implemented
- C++ client logging (`qwen_logger.h`, `qwen_client.cpp`)
- TypeScript logger class (`qwenLogger.ts`)
- Environment variable support
- Log format standardization
- PID and timestamp tracking

### ⏳ TODO
- Integrate TypeScript logger into `structuredServerMode.ts`
- Add logging to TCP server connection handling
- Add logging to message routing in `gemini.tsx`
- Test merged log sorting and analysis
- Document common debugging patterns

## Example Debug Session

```bash
# 1. Set up logging
export QWEN_LOG_FILE=/tmp/qwen_debug.log
export QWEN_LOG_LEVEL=DEBUG
> /tmp/qwen_debug.log

# 2. Start server
./run_qwen_server.sh --openai &
SERVER_PID=$!

# 3. Start client
./run_qwen_client.sh &
CLIENT_PID=$!

# 4. Wait for issue to occur
sleep 5

# 5. Stop both
kill $CLIENT_PID $SERVER_PID

# 6. Analyze
echo "=== Server logs ==="
grep "qwen-server" /tmp/qwen_debug.log | tail -20

echo "=== Client logs ==="
grep "qwen-client" /tmp/qwen_debug.log | tail -20

echo "=== Merged timeline ==="
sort /tmp/qwen_debug.log | tail -40
```

---

**Last Updated**: 2025-10-23
**Status**: C++ logging complete, TypeScript integration needed
