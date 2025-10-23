# Implementation Guide: ncurses UI Improvements for VfsBoot qwen Command

## Objective
Enhance the ncurses interface for the qwen command with better visual feedback, usability improvements, and enhanced functionality.

## Files to Modify

### Primary File
File: `/common/active/sblo/Dev/VfsBoot/VfsShell/cmd_qwen.cpp`

## Specific Improvements

### 1. Color-Coded Message Display

Enhance the `run_ncurses_mode` function to use colors for different message types:

```cpp
// In the ncurses_handlers.on_conversation section
if (msg.role == Qwen::MessageRole::USER) {
    wattron(output_win, COLOR_PAIR(USER_MESSAGE_COLOR));
    wprintw(output_win, "You: %s\n", msg.content.c_str());
    wattroff(output_win, COLOR_PAIR(USER_MESSAGE_COLOR));
} else if (msg.role == Qwen::MessageRole::ASSISTANT) {
    wattron(output_win, COLOR_PAIR(AI_MESSAGE_COLOR));
    // Handle streaming vs non-streaming
    if (msg.is_streaming.value_or(false)) {
        // Streaming display
        wprintw(output_win, "AI: %s", msg.content.c_str());
    } else {
        wprintw(output_win, "AI: %s\n", msg.content.c_str());
    }
    wattroff(output_win, COLOR_PAIR(AI_MESSAGE_COLOR));
} else {
    wattron(output_win, COLOR_PAIR(SYSTEM_MESSAGE_COLOR));
    wprintw(output_win, "[system]: %s\n", msg.content.c_str());
    wattroff(output_win, COLOR_PAIR(SYSTEM_MESSAGE_COLOR));
}
```

### 2. Scrolling Message History

The current implementation doesn't support scrolling. To implement this, we'll need to:
- Use ncurses pads instead of windows for the output area
- Track message positions and implement arrow key navigation
- Add visual scroll indicators

### 3. Enhanced Tool Approval Interface

Improve the `ncurses_handlers.on_tool_group` section to:
- Show a preview of what tools will do
- Allow selective approval of tools in a group
- Add visual confirmation of approvals

### 4. Better Visual Layout

```cpp
// At the beginning of run_ncurses_mode, set up a more sophisticated UI:
int header_height = 3;
int output_height = max_y - 6;  // Leave room for header, input, and status
int footer_height = 3;

WINDOW* header_win = newwin(header_height, max_x, 0, 0);
WINDOW* output_win = newwin(output_height, max_x, header_height, 0);
WINDOW* status_win = newwin(footer_height, max_x, header_height + output_height, 0);
WINDOW* input_win = newwin(3, max_x, header_height + output_height + footer_height, 0);
```

### 5. Status Bar Information

Add a status bar showing:
- Current model
- Session ID
- Connection status
- Timestamps

```cpp
// In the status bar window
mvwprintw(status_win, 0, 0, "Model: %s | Session: %s | Status: %s", 
          state_mgr.get_model().c_str(),
          state_mgr.get_current_session().substr(0, 10).c_str(),
          client.is_running() ? "Connected" : "Disconnected");
```

### 6. Improved Input Area

Enhance the input handling to support:
- Multi-line input with special key combinations
- Command history with up/down arrows
- Auto-completion for special commands

## Implementation Priority

### High Priority
1. Color coding for different message types
2. Status bar with connection info

### Medium Priority  
3. Better visual separation of messages
4. Enhanced tool approval interface

### Low Priority
5. Scrolling message history (more complex)
6. Multi-line input support

## Code Structure Changes

The main changes will be in the `run_ncurses_mode` function, specifically:

1. Initialize color pairs in the setup section
2. Update the message handlers to use colors
3. Add status window and update logic
4. Enhance the tool approval process
5. Add window refresh management for the new layout

These improvements will make the ncurses interface more user-friendly and visually appealing while maintaining its core functionality.