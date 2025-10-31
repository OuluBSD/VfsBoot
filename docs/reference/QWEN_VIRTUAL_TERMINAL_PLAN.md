# Qwen-Code Virtual Terminal Integration Plan (REVISED)

## Core Concept: Virtual Terminal Context Serialization

**Key Insight**: Ink (React for terminals) already maintains a virtual terminal buffer with text rectangles, colors, and cursor positions. We serialize this terminal state and send incremental updates to C++, which renders using ncurses.

**Benefits**:
- Minimal modifications to qwen-code (upstream-mergeable)
- Efficient: only send terminal state changes (diffs)
- Clean separation: TypeScript handles logic, C++ handles rendering
- Reuses Ink's layout engine (no need to reimplement)

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  qwen-code (TypeScript/React+Ink) - MOSTLY UNCHANGED       │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  App.tsx (React components)                          │  │
│  │  - InputPrompt, HistoryDisplay, Footer, etc.        │  │
│  │  - State management (hooks)                         │  │
│  └──────────────┬───────────────────────────────────────┘  │
│                 │                                            │
│  ┌──────────────▼───────────────────────────────────────┐  │
│  │  Ink Renderer (built-in)                             │  │
│  │  - Renders to virtual terminal buffer                │  │
│  │  - Maintains: text grid, cursor, colors, rectangles │  │
│  └──────────────┬───────────────────────────────────────┘  │
│                 │                                            │
│  ┌──────────────▼───────────────────────────────────────┐  │
│  │  Virtual Terminal Adapter (NEW - 200 lines)          │  │
│  │  - Hooks into Ink's render output                    │  │
│  │  - Computes diffs (changed cells only)               │  │
│  │  - Serializes to JSON                                │  │
│  └──────────────┬───────────────────────────────────────┘  │
│                 │                                            │
│  ┌──────────────▼───────────────────────────────────────┐  │
│  │  Server Mode (NEW - 300 lines)                       │  │
│  │  - stdin/stdout, named pipes, TCP                    │  │
│  │  - Sends terminal updates + receives input           │  │
│  └──────────────┬───────────────────────────────────────┘  │
└─────────────────┼────────────────────────────────────────────┘
                  │
                  │ Protocol: VirtualTerminalUpdate (JSON)
                  │ { type: "render", cells: [...], cursor: {...} }
                  │ { type: "input_request" }
                  │
┌─────────────────▼────────────────────────────────────────────┐
│  VfsBoot C++ (ncurses frontend)                             │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  QwenClient (stdin/pipe/tcp)                         │  │
│  │  - Receives terminal updates                         │  │
│  │  - Sends keyboard input                              │  │
│  └──────────────┬───────────────────────────────────────┘  │
│                 │                                            │
│  ┌──────────────▼───────────────────────────────────────┐  │
│  │  VirtualTerminalContext (C++)                        │  │
│  │  - Stores terminal grid (char + color per cell)     │  │
│  │  - Applies incremental updates                       │  │
│  └──────────────┬───────────────────────────────────────┘  │
│                 │                                            │
│  ┌──────────────▼───────────────────────────────────────┐  │
│  │  NcursesRenderer                                      │  │
│  │  - Renders terminal grid to screen                   │  │
│  │  - Handles keyboard input                            │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

## Protocol: VirtualTerminalUpdate

### Terminal State Structure (Ink's internal representation)

Ink maintains a 2D grid of cells:
```typescript
interface Cell {
  char: string;          // UTF-8 character
  fg: Color;             // foreground color (ANSI 256)
  bg: Color;             // background color
  bold: boolean;
  italic: boolean;
  underline: boolean;
  strikethrough: boolean;
}

interface TerminalState {
  width: number;         // terminal columns
  height: number;        // terminal rows
  cells: Cell[][];       // 2D grid [row][col]
  cursor: {
    x: number;
    y: number;
    visible: boolean;
  };
}
```

### Message Types (TypeScript → C++)

```json
// Initial state (sent once)
{
  "type": "init",
  "width": 80,
  "height": 24
}

// Incremental update (only changed cells)
{
  "type": "render",
  "updates": [
    {
      "row": 5,
      "col": 10,
      "cells": [
        { "char": "H", "fg": 15, "bg": 0, "bold": true },
        { "char": "e", "fg": 15, "bg": 0 },
        { "char": "l", "fg": 15, "bg": 0 },
        { "char": "l", "fg": 15, "bg": 0 },
        { "char": "o", "fg": 15, "bg": 0 }
      ]
    }
  ],
  "cursor": { "x": 15, "y": 5, "visible": true }
}

// Request user input
{
  "type": "input_request",
  "prompt": "You> "
}

// Status update
{
  "type": "status",
  "message": "Connected to Qwen 2.5 Coder"
}
```

### Message Types (C++ → TypeScript)

```json
// User keyboard input
{
  "type": "input",
  "text": "write a hello world program",
  "key": null  // or special keys: "enter", "tab", "ctrl-c"
}

// Special keys
{
  "type": "keypress",
  "key": "ctrl-c"  // interrupt
}

// Terminal resize
{
  "type": "resize",
  "width": 100,
  "height": 30
}
```

## Implementation Plan

### Phase 1: qwen-code Modifications (Week 1)

#### 1.1 Virtual Terminal Adapter (NEW FILE)

**File**: `packages/cli/src/virtualTerminalAdapter.ts` (~200 lines)

```typescript
import { RenderOutput } from 'ink';

export interface VirtualCell {
  char: string;
  fg: number;
  bg: number;
  bold?: boolean;
  italic?: boolean;
  underline?: boolean;
}

export interface VirtualTerminalUpdate {
  type: 'init' | 'render' | 'input_request' | 'status';
  width?: number;
  height?: number;
  updates?: Array<{
    row: number;
    col: number;
    cells: VirtualCell[];
  }>;
  cursor?: { x: number; y: number; visible: boolean };
  message?: string;
}

export class VirtualTerminalAdapter {
  private lastFrame: VirtualCell[][] | null = null;

  constructor(private width: number, private height: number) {}

  // Hook into Ink's render output
  processRenderOutput(output: RenderOutput): VirtualTerminalUpdate {
    const currentFrame = this.extractFrameFromInk(output);
    const diff = this.computeDiff(this.lastFrame, currentFrame);
    this.lastFrame = currentFrame;
    return diff;
  }

  private extractFrameFromInk(output: RenderOutput): VirtualCell[][] {
    // Convert Ink's output (ANSI string) to 2D cell grid
    // Parse ANSI escape codes for colors/styles
    // Return grid
  }

  private computeDiff(
    oldFrame: VirtualCell[][] | null,
    newFrame: VirtualCell[][]
  ): VirtualTerminalUpdate {
    if (!oldFrame) {
      // First render: send everything
      return this.createFullUpdate(newFrame);
    }

    // Find changed rows
    const updates: Array<{ row: number; col: number; cells: VirtualCell[] }> = [];

    for (let row = 0; row < newFrame.length; row++) {
      const changes = this.findRowChanges(oldFrame[row], newFrame[row], row);
      updates.push(...changes);
    }

    return {
      type: 'render',
      updates,
      cursor: this.extractCursor(newFrame)
    };
  }

  private findRowChanges(
    oldRow: VirtualCell[],
    newRow: VirtualCell[],
    rowNum: number
  ): Array<{ row: number; col: number; cells: VirtualCell[] }> {
    const changes: Array<{ row: number; col: number; cells: VirtualCell[] }> = [];
    let changeStart: number | null = null;
    let changeCells: VirtualCell[] = [];

    for (let col = 0; col < newRow.length; col++) {
      if (!this.cellsEqual(oldRow?.[col], newRow[col])) {
        if (changeStart === null) changeStart = col;
        changeCells.push(newRow[col]);
      } else {
        if (changeStart !== null) {
          changes.push({ row: rowNum, col: changeStart, cells: changeCells });
          changeStart = null;
          changeCells = [];
        }
      }
    }

    if (changeStart !== null) {
      changes.push({ row: rowNum, col: changeStart, cells: changeCells });
    }

    return changes;
  }

  private cellsEqual(a: VirtualCell | undefined, b: VirtualCell): boolean {
    if (!a) return false;
    return a.char === b.char &&
           a.fg === b.fg &&
           a.bg === b.bg &&
           a.bold === b.bold;
  }
}
```

#### 1.2 Server Mode (NEW FILE)

**File**: `packages/cli/src/serverMode.ts` (~300 lines)

```typescript
import { VirtualTerminalAdapter, VirtualTerminalUpdate } from './virtualTerminalAdapter';
import { render } from 'ink';
import readline from 'readline';

export type ServerModeType = 'stdin' | 'pipe' | 'tcp';

export interface ServerModeConfig {
  mode: ServerModeType;
  pipePath?: string;
  tcpPort?: number;
}

abstract class BaseServer {
  protected adapter: VirtualTerminalAdapter;

  constructor(protected width: number, protected height: number) {
    this.adapter = new VirtualTerminalAdapter(width, height);
  }

  abstract send(update: VirtualTerminalUpdate): Promise<void>;
  abstract receive(): Promise<string | null>;
  abstract start(): Promise<void>;
  abstract stop(): Promise<void>;
}

class StdinStdoutServer extends BaseServer {
  private rl: readline.Interface;
  private running: boolean = false;

  async start() {
    this.running = true;
    this.rl = readline.createInterface({
      input: process.stdin,
      output: process.stderr, // Use stderr for logs, stdout for protocol
      terminal: false
    });

    // Send init message
    await this.send({
      type: 'init',
      width: this.width,
      height: this.height
    });
  }

  async send(update: VirtualTerminalUpdate) {
    // Write to stdout (line-buffered JSON)
    process.stdout.write(JSON.stringify(update) + '\n');
  }

  async receive(): Promise<string | null> {
    return new Promise((resolve) => {
      this.rl.once('line', (line) => {
        try {
          const msg = JSON.parse(line);
          resolve(msg.text || null);
        } catch {
          resolve(null);
        }
      });
    });
  }

  async stop() {
    this.running = false;
    this.rl.close();
  }
}

class NamedPipeServer extends BaseServer {
  // Similar to StdinStdout but uses named pipes
  // Implementation: fs.createWriteStream / fs.createReadStream
}

class TCPServer extends BaseServer {
  // Similar but uses net.createServer
}

export function createServer(config: ServerModeConfig): BaseServer {
  const { mode, pipePath, tcpPort } = config;
  const width = process.stdout.columns || 80;
  const height = process.stdout.rows || 24;

  switch (mode) {
    case 'stdin':
      return new StdinStdoutServer(width, height);
    case 'pipe':
      return new NamedPipeServer(width, height, pipePath!);
    case 'tcp':
      return new TCPServer(width, height, tcpPort!);
  }
}
```

#### 1.3 Modified Entry Point (MINIMAL CHANGE)

**File**: `packages/cli/src/gemini.tsx` (add ~20 lines)

```typescript
// Add to imports
import { createServer } from './serverMode';

// Add CLI flags (in yargs config)
.option('server-mode', {
  type: 'string',
  choices: ['stdin', 'pipe', 'tcp'],
  description: 'Run in server mode for external frontend'
})
.option('pipe-path', {
  type: 'string',
  description: 'Named pipe path (for pipe mode)'
})
.option('tcp-port', {
  type: 'number',
  default: 7777,
  description: 'TCP port (for tcp mode)'
})

// In main() function, add before startInteractiveUI():
if (argv.serverMode) {
  const server = createServer({
    mode: argv.serverMode,
    pipePath: argv.pipePath,
    tcpPort: argv.tcpPort
  });

  await runServerMode(server, argv);
  return;
}

// New function: runServerMode()
async function runServerMode(server: BaseServer, argv: any) {
  await server.start();

  // Create Ink app but DON'T render to terminal
  const { rerender, unmount } = render(<App {...argv} />, {
    stdout: {
      write: () => {}, // Suppress stdout
      columns: server.width,
      rows: server.height
    } as any,
    stdin: process.stdin,
    exitOnCtrlC: false
  });

  // Hook into Ink's render cycle
  // (This requires accessing Ink internals or using a custom renderer)
  // For now, use periodic polling + forcing re-renders

  while (true) {
    // Wait for render
    await sleep(100);

    // Get render output from Ink (HACK: may need Ink API access)
    const update = server.adapter.processRenderOutput(/* ink output */);
    await server.send(update);

    // Check for input
    const input = await server.receive();
    if (input) {
      // Inject input into App via props/state
      // This may require App refactoring to accept external input source
    }
  }
}
```

**Note**: Accessing Ink's render output cleanly may require:
- Using Ink's experimental `measureElement()` API
- Creating a custom Ink renderer
- Forking Ink to expose render output (not ideal for upstream merging)

**Alternative**: Use Ink's `stdout` option with a custom writable stream that captures output.

### Phase 2: C++ Virtual Terminal Client (Week 2-3)

#### 2.1 Virtual Terminal Context (NEW)

**File**: `VfsShell/qwen_virtual_terminal.h` (~200 lines)

```cpp
#pragma once
#include <vector>
#include <string>
#include <nlohmann/json.hpp>

namespace QwenVT {

struct Cell {
    char32_t character;
    uint8_t fg_color;
    uint8_t bg_color;
    bool bold;
    bool italic;
    bool underline;
    bool strikethrough;
};

struct CursorPos {
    int x;
    int y;
    bool visible;
};

class VirtualTerminalContext {
public:
    VirtualTerminalContext(int width, int height);

    // Apply update from JSON message
    void applyUpdate(const nlohmann::json& update);

    // Getters
    const std::vector<std::vector<Cell>>& getGrid() const { return grid_; }
    const CursorPos& getCursor() const { return cursor_; }
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }

    // For rendering
    const Cell& getCell(int row, int col) const;
    bool isDirty() const { return dirty_; }
    void clearDirty() { dirty_ = false; }

private:
    int width_;
    int height_;
    std::vector<std::vector<Cell>> grid_;
    CursorPos cursor_;
    bool dirty_;

    void resizeGrid(int width, int height);
    void applyRenderUpdate(const nlohmann::json& update);
    void applyCellUpdates(const nlohmann::json& updates);
};

} // namespace QwenVT
```

#### 2.2 Ncurses Renderer (NEW)

**File**: `VfsShell/qwen_ncurses_renderer.h` (~150 lines)

```cpp
#pragma once
#include "qwen_virtual_terminal.h"
#include <ncurses.h>

namespace QwenVT {

class NcursesRenderer {
public:
    NcursesRenderer();
    ~NcursesRenderer();

    // Initialize ncurses
    void initialize();
    void shutdown();

    // Render virtual terminal to screen
    void render(const VirtualTerminalContext& vt);

    // Input handling
    std::string getInput();  // blocking
    char getChar();          // non-blocking

private:
    WINDOW* main_win_;
    bool initialized_;

    void setupColors();
    void renderCell(int row, int col, const Cell& cell);
    void setCursor(const CursorPos& cursor);
};

} // namespace QwenVT
```

**File**: `VfsShell/qwen_ncurses_renderer.cpp` (~300 lines)

```cpp
#include "qwen_ncurses_renderer.h"
#include <locale.h>

namespace QwenVT {

NcursesRenderer::NcursesRenderer() : initialized_(false) {}

void NcursesRenderer::initialize() {
    // Set locale for UTF-8 support
    setlocale(LC_ALL, "");

    // Initialize ncurses
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);  // non-blocking input

    // Colors
    if (has_colors()) {
        start_color();
        use_default_colors();
        setupColors();
    }

    main_win_ = stdscr;
    initialized_ = true;
}

void NcursesRenderer::setupColors() {
    // Initialize 256 ANSI color pairs
    for (int i = 0; i < 256; i++) {
        init_pair(i, i, -1);  // fg=i, bg=default
    }
}

void NcursesRenderer::render(const VirtualTerminalContext& vt) {
    const auto& grid = vt.getGrid();

    // Render all cells
    for (int row = 0; row < vt.getHeight(); row++) {
        for (int col = 0; col < vt.getWidth(); col++) {
            renderCell(row, col, vt.getCell(row, col));
        }
    }

    // Set cursor
    setCursor(vt.getCursor());

    refresh();
}

void NcursesRenderer::renderCell(int row, int col, const Cell& cell) {
    attr_t attrs = 0;
    if (cell.bold) attrs |= A_BOLD;
    if (cell.italic) attrs |= A_ITALIC;
    if (cell.underline) attrs |= A_UNDERLINE;

    attron(COLOR_PAIR(cell.fg_color) | attrs);

    // Convert char32_t to UTF-8 for ncurses
    char utf8[5];
    int len = wctomb(utf8, cell.character);
    if (len > 0) {
        mvaddnstr(row, col, utf8, len);
    }

    attroff(COLOR_PAIR(cell.fg_color) | attrs);
}

void NcursesRenderer::setCursor(const CursorPos& cursor) {
    if (cursor.visible) {
        curs_set(1);
        move(cursor.y, cursor.x);
    } else {
        curs_set(0);
    }
}

std::string NcursesRenderer::getInput() {
    std::string result;
    int ch;

    while ((ch = getch()) != '\n' && ch != ERR) {
        if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
            if (!result.empty()) result.pop_back();
        } else {
            result += static_cast<char>(ch);
        }
    }

    return result;
}

} // namespace QwenVT
```

#### 2.3 Client (communication layer)

**File**: `VfsShell/qwen_client.cpp` (integrate with existing from plan)

Modified to:
- Send `{ "type": "input", "text": "..." }` for user input
- Receive `VirtualTerminalUpdate` messages
- Pass updates to `VirtualTerminalContext::applyUpdate()`

### Phase 3: VfsBoot Integration (Week 3-4)

**File**: `VfsShell/cmd_qwen.cpp` (~400 lines)

```cpp
#include "qwen_client.h"
#include "qwen_virtual_terminal.h"
#include "qwen_ncurses_renderer.h"
#include <thread>

using namespace QwenVT;

void cmd_qwen(const std::vector<std::string>& args,
              Vfs& vfs,
              std::map<std::string, std::string>& env) {
    // Parse args
    QwenOptions opts = parseQwenArgs(args);

    // Create components
    VirtualTerminalContext vt(80, 24);
    NcursesRenderer renderer;
    QwenClient client(opts);

    // Initialize
    renderer.initialize();

    // Connect to backend (fork/exec qwen-code --server-mode stdin)
    if (!client.connect()) {
        renderer.shutdown();
        throw std::runtime_error("Failed to start qwen-code");
    }

    // Background thread for message polling
    std::atomic<bool> running(true);
    std::thread msg_thread([&]() {
        while (running) {
            auto msg = client.receiveMessage(100);  // 100ms timeout
            if (msg) {
                vt.applyUpdate(*msg);
                if (vt.isDirty()) {
                    renderer.render(vt);
                    vt.clearDirty();
                }
            }
        }
    });

    // Main input loop
    while (running) {
        char ch = renderer.getChar();

        if (ch == 3) {  // Ctrl+C
            // Send interrupt signal
            client.sendKeypress("ctrl-c");
        } else if (ch == 4) {  // Ctrl+D (detach)
            break;
        } else if (ch != ERR) {
            // Send character
            std::string input(1, ch);
            client.sendInput(input);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Cleanup
    running = false;
    msg_thread.join();
    client.disconnect();
    renderer.shutdown();
}
```

## Summary

### Files to Create/Modify

**qwen-code (TypeScript)** - MINIMAL changes:
- `packages/cli/src/virtualTerminalAdapter.ts` (NEW - 200 lines)
- `packages/cli/src/serverMode.ts` (NEW - 300 lines)
- `packages/cli/src/gemini.tsx` (MODIFY - add ~20 lines)

**VfsBoot (C++)**:
- `VfsShell/qwen_virtual_terminal.h` (NEW - 200 lines)
- `VfsShell/qwen_virtual_terminal.cpp` (NEW - 300 lines)
- `VfsShell/qwen_ncurses_renderer.h` (NEW - 150 lines)
- `VfsShell/qwen_ncurses_renderer.cpp` (NEW - 300 lines)
- `VfsShell/qwen_client.h` (NEW - 250 lines, from previous plan)
- `VfsShell/qwen_client.cpp` (NEW - 700 lines, from previous plan)
- `VfsShell/cmd_qwen.cpp` (NEW - 400 lines)

**Total**: ~520 lines TypeScript (minimal mods), ~2,300 lines C++

### Timeline

| Week | Focus | Deliverable |
|------|-------|-------------|
| 1 | Virtual terminal adapter + server mode | qwen --server-mode stdin works |
| 2 | C++ virtual terminal context + renderer | Ncurses renders terminal state |
| 3 | Client + integration | Full end-to-end working |
| 4 | Polish + session management | Production ready |

**Total**: 4 weeks (vs 5 weeks in previous plan)

### Advantages of This Approach

1. **Minimal upstream changes**: Only add new files + tiny entry point change
2. **Clean separation**: TypeScript = logic, C++ = rendering
3. **Efficient**: Only send diffs, not full frames
4. **Reusable**: Virtual terminal abstraction can work with other backends
5. **Maintainable**: Easy to merge upstream changes
6. **Extensible**: Can add more features (colors, styles, rectangles) later

### Next Steps

1. ✅ Explore qwen-code (DONE)
2. ⏭️ Create `virtualTerminalAdapter.ts`
3. ⏭️ Create `serverMode.ts`
4. ⏭️ Test standalone: `qwen --server-mode stdin`
5. ⏭️ Build C++ components
6. ⏭️ Integrate and test end-to-end

Ready to proceed with Phase 1?
