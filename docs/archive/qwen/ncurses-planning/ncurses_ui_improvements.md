# UI Improvements for ncurses Interface

## Current State

The ncurses interface in VfsBoot's qwen command provides a basic interactive terminal experience with the following features:
- Split-screen layout (input at bottom, output above)
- Tool approval prompts
- Support for special commands (/help, /exit, /detach, etc.)
- Streaming response display

## Suggested Improvements

### 1. Scrollable Message History

Currently, the message history fills the terminal window but lacks scrolling capabilities for viewing earlier messages when the conversation gets long.

**Implementation:**
- Add scrollable buffer using ncurses pad or scrolling window
- Implement arrow key navigation to scroll through history
- Add scroll indicators showing position in history

### 2. Visual Indicators and Formatting

**Message Differentiation:**
- Use different colors for different message types:
  - Green for user input
  - Cyan for AI responses
  - Yellow for tool requests
  - Blue for status messages
  - Red for errors

**Visual Enhancements:**
- Add message bubbles or boxed formatting
- Include timestamps for messages
- Add status bar showing current model, session info
- Visual separation between different conversation turns

### 3. Tool Approval Workflow

**Current:** Simple yes/no/detailed approval prompt
**Improved:** 
- Show tool execution preview
- Allow selective tool approval in groups
- Add "trust this session" option for repeated approvals

### 4. Input Enhancements

- Multi-line input support for complex prompts
- Input history with arrow keys
- Auto-completion for special commands
- Visual indication of streaming vs. complete responses

### 5. Layout Improvements

- Better use of screen real estate
- Configurable split ratios
- Collapsible side panels for tool history
- Status area for connection/processing status

### 6. Responsiveness

- Handle window resize events
- Dynamic layout adjustment based on terminal size
- Proper line wrapping for long messages

## Implementation Approach

The improvements should be added to the `run_ncurses_mode` function in `/common/active/sblo/Dev/VfsBoot/VfsShell/cmd_qwen.cpp`, specifically in the sections that handle:

1. Message display (the handlers for different message types)
2. Input handling (the main loop)
3. Window management (setup and resize handling)

## Sample Code Structure

```cpp
// In run_ncurses_mode function:

// Enhanced message display with colors
if (msg.role == Qwen::MessageRole::USER) {
    wattron(output_win, COLOR_PAIR(USER_COLOR));
    wprintw(output_win, "You: %s\n", msg.content.c_str());
    wattroff(output_win, COLOR_PAIR(USER_COLOR));
} else if (msg.role == Qwen::MessageRole::ASSISTANT) {
    wattron(output_win, COLOR_PAIR(AI_COLOR));
    // ... display AI message
    wattroff(output_win, COLOR_PAIR(AI_COLOR));
}

// Add scrolling support for large outputs
// Add status bar at bottom
// Improve tool approval UI
```

These improvements would significantly enhance the user experience of the ncurses interface while preserving its core functionality.