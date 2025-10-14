# VfsShell UI Backend Abstraction

This project implements a scalable UI backend abstraction layer that supports multiple UI libraries through preprocessor flags.

## Features

1. **Scalable Architecture**: Supports multiple UI libraries through a common abstraction layer
2. **Compile-time Selection**: Choose UI backend at compile time using preprocessor flags
3. **Default Implementation**: NCURSES is the default UI library when available
4. **Fallback Support**: Builtin terminal implementation for systems without ncurses
5. **Extensible Design**: Easy to add support for other UI libraries (Qt, GTK, etc.)

## Supported UI Libraries

- **NCURSES** (`-DCODEX_UI_NCURSES`): Full-featured terminal-based UI with colors and advanced controls
- **Builtin Terminal** (`-DCODEX_UI_BUILTIN`): Simple terminal-based UI using ANSI escape codes
- **Auto-detection**: Automatically selects NCURSES if available, otherwise falls back to builtin

## Usage

### Compilation

```bash
# Compile with NCURSES backend (default if available)
g++ -std=c++17 -O2 -DCODEX_UI_NCURSES your_program.cpp -lncurses -ltinfo

# Compile with builtin terminal backend
g++ -std=c++17 -O2 -DCODEX_UI_BUILTIN your_program.cpp

# Auto-detection (uses NCURSES if available, otherwise builtin)
g++ -std=c++17 -O2 your_program.cpp $(pkg-config --cflags --libs ncurses 2>/dev/null || echo "")
```

### Integration

Include the UI backend header in your project:

```cpp
#include "VfsShell/ui_backend.h"

int main() {
    // Initialize UI
    ui_init();
    
    // Clear screen
    ui_clear();
    
    // Move cursor and print text
    ui_move(5, 10);
    ui_print("Hello, World!");
    
    // Refresh screen
    ui_refresh();
    
    // Wait for keypress
    ui_getch();
    
    // Cleanup
    ui_end();
    
    return 0;
}
```

## API Reference

The UI backend provides a consistent API regardless of the underlying implementation:

- `void ui_init()` - Initialize UI system
- `void ui_end()` - Clean up UI system
- `void ui_clear()` - Clear screen
- `void ui_move(int y, int x)` - Move cursor to position
- `void ui_print(const char* text)` - Print text at current cursor position
- `void ui_refresh()` - Refresh/redraw screen
- `int ui_getch()` - Get character input
- `int ui_rows()` - Get terminal height
- `int ui_cols()` - Get terminal width

## Extensibility

To add support for additional UI libraries:

1. Add a new preprocessor flag (e.g., `-DCODEX_UI_QT`)
2. Implement the UI backend functions for the new library
3. Update the abstraction layer to include the new implementation
4. Update build scripts to detect and link the new library

## Dependencies

- **NCURSES**: Optional, for full-featured terminal UI
- **C++17**: Required for compilation

## License

This implementation is part of the VfsBoot project and follows the same licensing terms.