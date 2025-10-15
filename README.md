# VfsShell Text Editor

A full-featured ncurses-based text editor with UI backend abstraction.

## Features

- Syntax highlighting for C++ code
- Search functionality (`:/text` command)
- File navigation with integrated file browser
- Standard editor commands (save with `:w`, quit with `:q`, help with `:help`)
- Page navigation with Page Up/Down keys
- Color-coded UI elements
- Support for multiple UI backends (ncurses, builtin terminal, fallback)

## Building

```bash
# Create build directory
mkdir build
cd build

# Configure with cmake
cmake ..

# Compile
make
```

## Running

```bash
# Set terminal type and run the editor
TERM=xterm ./text_editor_demo
```

## Editor Commands

- `:w` - Save file
- `:q` - Quit editor
- `:wq` or `:x` - Save and quit
- `:help` - Show help
- `:/text` - Search for text
- `:q!` - Quit without saving

## Navigation

- Arrow keys: Move cursor
- Page Up/Page Down: Scroll page
- ESC: Enter command mode

## Terminal Compatibility

If you experience issues with terminal initialization, make sure to set the TERM environment variable to a recognized value like `xterm` or `xterm-256color`.

## Architecture

The editor uses a UI backend abstraction layer that supports multiple UI libraries:
- NCURSES for full-featured terminal UI
- Builtin terminal implementation
- Fallback implementation