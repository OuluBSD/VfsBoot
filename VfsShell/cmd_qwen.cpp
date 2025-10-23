#include "VfsShell.h"
#include <termios.h>
#include <unistd.h>

#ifdef CODEX_UI_NCURSES
#include <ncurses.h>
#endif

namespace QwenCmd {

// ANSI color codes
namespace Color {
    const char* RESET = "\033[0m";
    const char* RED = "\033[31m";
    const char* GREEN = "\033[32m";
    const char* YELLOW = "\033[33m";
    const char* BLUE = "\033[34m";
    const char* MAGENTA = "\033[35m";
    const char* CYAN = "\033[36m";
    const char* GRAY = "\033[90m";
}

// Configuration implementation
void QwenConfig::load_from_env(const std::map<std::string, std::string>& /*env*/) {
    // Read from process environment
    if (const char* val = std::getenv("QWEN_MODEL"); val && *val) {
        model = val;
    }

    if (const char* val = std::getenv("QWEN_WORKSPACE"); val && *val) {
        workspace_root = val;
    }

    if (const char* val = std::getenv("QWEN_CODE_PATH"); val && *val) {
        qwen_code_path = val;
    }

    if (const char* val = std::getenv("QWEN_AUTO_APPROVE"); val && *val == '1') {
        auto_approve_tools = true;
    }
}

bool QwenConfig::load_from_file(const std::string& /*vfs_path*/, Vfs& /*vfs*/) {
    // TODO: Implement VFS config file loading
    // For now, return false (no config file)
    return false;
}

// Parse command-line arguments
QwenOptions parse_args(const std::vector<std::string>& args) {
    QwenOptions opts;

    for (size_t i = 0; i < args.size(); ++i) {
        const std::string& arg = args[i];

        if (arg == "--attach" && i + 1 < args.size()) {
            opts.attach = true;
            opts.session_id = args[++i];
        }
        else if (arg == "--list-sessions") {
            opts.list_sessions = true;
        }
        else if (arg == "--model" && i + 1 < args.size()) {
            opts.model = args[++i];
        }
        else if (arg == "--workspace" && i + 1 < args.size()) {
            opts.workspace_root = args[++i];
        }
        else if (arg == "--simple") {
            opts.simple_mode = true;
        }
        else if (arg == "--help" || arg == "-h") {
            opts.help = true;
        }
        else if (arg == "--openai") {
            opts.use_openai = true;
        }
    }

    return opts;
}

// Check if terminal supports ncurses
bool supports_ncurses() {
    // Check if we're running in a terminal
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        return false;
    }

    // Check TERM environment variable
    const char* term = std::getenv("TERM");
    if (!term || std::string(term).empty()) {
        return false;
    }

    // Common terminal types that support ncurses
    std::vector<std::string> supported_terms = {
        "xterm", "xterm-256color", "xterm-color", "linux",
        "screen", "screen-256color", "tmux", "tmux-256color",
        "rxvt", "rxvt-unicode", "rxvt-256color", "dtterm",
        "ansi", "cygwin", "putty", "st", "st-256color"
    };

    for (const auto& supported : supported_terms) {
        if (std::string(term).find(supported) != std::string::npos) {
            return true;
        }
    }

    return false;
}

// Show help text
void show_help() {
    std::cout << "qwen - Interactive AI assistant powered by qwen-code\n\n";
    std::cout << "Usage:\n";
    std::cout << "  qwen [options]                 Start new interactive session\n";
    std::cout << "  qwen --attach <id>            Attach to existing session\n";
    std::cout << "  qwen --list-sessions          List all sessions\n";
    std::cout << "  qwen --simple                 Force stdio mode instead of ncurses\n";
    std::cout << "  qwen --openai                 Use OpenAI provider instead of default\n";
    std::cout << "  qwen --help                   Show this help\n\n";
    std::cout << "Options:\n";
    std::cout << "  --model <name>                AI model to use (default: coder)\n";
    std::cout << "  --workspace <path>            Workspace root directory\n\n";
    std::cout << "Interactive Commands:\n";
    std::cout << "  /detach                       Detach from current session\n";
    std::cout << "  /exit                         Exit and close session\n";
    std::cout << "  /save                         Save session immediately\n";
    std::cout << "  /help                         Show help\n";
    std::cout << "  /status                       Show session status\n\n";
    std::cout << "Environment Variables:\n";
    std::cout << "  QWEN_MODEL                    Default model name\n";
    std::cout << "  QWEN_WORKSPACE                Default workspace path\n";
    std::cout << "  QWEN_CODE_PATH                Path to qwen-code executable\n";
    std::cout << "  QWEN_AUTO_APPROVE             Auto-approve tools (1=yes)\n\n";
    std::cout << "Configuration File:\n";
    std::cout << "  /env/qwen_config.json         VFS configuration file\n";
}

// List all sessions
void list_sessions(QwenStateManager& state_mgr) {
    std::vector<Qwen::SessionInfo> sessions = state_mgr.list_sessions();

    if (sessions.empty()) {
        std::cout << "No sessions found.\n";
        return;
    }

    std::cout << "Available sessions:\n\n";

    for (const auto& session : sessions) {
        std::cout << "  " << Color::CYAN << session.session_id << Color::RESET << "\n";
        std::cout << "    Created: " << session.created_at << "\n";
        std::cout << "    Model: " << session.model << "\n";

        if (!session.workspace_root.empty()) {
            std::cout << "    Workspace: " << session.workspace_root << "\n";
        }

        if (!session.tags.empty()) {
            std::cout << "    Tags: ";
            for (size_t i = 0; i < session.tags.size(); ++i) {
                std::cout << session.tags[i];
                if (i + 1 < session.tags.size()) std::cout << ", ";
            }
            std::cout << "\n";
        }

        std::cout << "    Messages: " << session.message_count << "\n\n";
    }
}

// Track if we're in the middle of streaming (to manage AI: prefix)
static bool streaming_in_progress = false;

// Display a conversation message with formatting
void display_conversation_message(const Qwen::ConversationMessage& msg) {
    if (msg.role == Qwen::MessageRole::USER) {
        std::cout << Color::GREEN << "You: " << Color::RESET << msg.content << "\n";
        return;
    }

    if (msg.role == Qwen::MessageRole::ASSISTANT) {
        // Handle streaming messages
        if (msg.is_streaming.value_or(false)) {
            // Streaming chunk
            if (!streaming_in_progress) {
                // First chunk - print prefix
                std::cout << Color::CYAN << "AI: " << Color::RESET;
                streaming_in_progress = true;
            }
            // Print content without newline
            std::cout << msg.content << std::flush;
        } else {
            // End of streaming or non-streaming message
            if (streaming_in_progress) {
                // End of streaming - just add newline
                std::cout << "\n";
                streaming_in_progress = false;
            } else if (!msg.content.empty()) {
                // Complete non-streaming message
                std::cout << Color::CYAN << "AI: " << Color::RESET << msg.content << "\n";
            }
        }
    } else {
        // System message
        std::cout << Color::GRAY << "[system]: " << Color::RESET << msg.content << "\n";
    }
}

// Display a tool group for approval
void display_tool_group(const Qwen::ToolGroup& group) {
    std::cout << "\n" << Color::YELLOW << "Tool Execution Request:" << Color::RESET << "\n";
    std::cout << "  Group ID: " << group.id << "\n";
    std::cout << "  Tools to execute:\n";

    for (const auto& tool : group.tools) {
        std::cout << "    - " << Color::MAGENTA << tool.tool_name << Color::RESET;
        std::cout << " (ID: " << tool.tool_id << ")\n";

        // Display confirmation details if present
        if (tool.confirmation_details.has_value()) {
            std::cout << "      Details: " << tool.confirmation_details->message << "\n";
        }

        // Display arguments
        if (!tool.args.empty()) {
            std::cout << "      Arguments:\n";
            for (const auto& [key, value] : tool.args) {
                std::cout << "        " << key << ": " << value << "\n";
            }
        }
    }
}

// Prompt user for tool approval
bool prompt_tool_approval(const Qwen::ToolGroup& group) {
    std::cout << "\n" << Color::YELLOW << "Approve tool execution? [y/n/d(details)]: " << Color::RESET;
    std::cout.flush();

    std::string response;
    std::getline(std::cin, response);

    if (response.empty() || response[0] == 'y' || response[0] == 'Y') {
        return true;
    } else if (response[0] == 'd' || response[0] == 'D') {
        // Display tool group again for details
        display_tool_group(group);
        return prompt_tool_approval(group);
    } else {
        return false;
    }
}

// Handle special commands (return true if command was handled and should exit loop)
bool handle_special_command(const std::string& input, QwenStateManager& state_mgr,
                            Qwen::QwenClient& client, bool& should_exit) {
    if (input.empty()) {
        return false;
    }

    if (input[0] != '/') {
        return false;  // Not a special command
    }

    if (input == "/exit") {
        std::cout << Color::YELLOW << "Exiting and closing session...\n" << Color::RESET;
        state_mgr.save_session();
        client.stop();
        should_exit = true;
        return true;
    }

    if (input == "/detach") {
        std::cout << Color::YELLOW << "Detaching from session (saving state)...\n" << Color::RESET;
        state_mgr.save_session();
        std::cout << Color::GREEN << "Session saved. Use 'qwen --attach "
                  << state_mgr.get_current_session() << "' to reconnect.\n" << Color::RESET;
        should_exit = true;
        return true;
    }

    if (input == "/save") {
        std::cout << Color::YELLOW << "Saving session...\n" << Color::RESET;
        if (state_mgr.save_session()) {
            std::cout << Color::GREEN << "Session saved successfully.\n" << Color::RESET;
        } else {
            std::cout << Color::RED << "Failed to save session.\n" << Color::RESET;
        }
        return true;
    }

    if (input == "/status") {
        std::cout << Color::CYAN << "Session Status:\n" << Color::RESET;
        std::cout << "  Session ID: " << state_mgr.get_current_session() << "\n";
        std::cout << "  Model: " << state_mgr.get_model() << "\n";
        std::cout << "  Message count: " << state_mgr.get_message_count() << "\n";
        std::cout << "  Workspace: " << state_mgr.get_workspace_root() << "\n";
        std::cout << "  Client running: " << (client.is_running() ? "yes" : "no") << "\n";
        return true;
    }

    if (input == "/help") {
        std::cout << Color::CYAN << "Interactive Commands:\n" << Color::RESET;
        std::cout << "  /detach   - Detach from session (keeps it running)\n";
        std::cout << "  /exit     - Exit and close session\n";
        std::cout << "  /save     - Save session immediately\n";
        std::cout << "  /status   - Show session status\n";
        std::cout << "  /help     - Show this help\n";
        return true;
    }

    std::cout << Color::RED << "Unknown command: " << input << Color::RESET << "\n";
    std::cout << "Type /help for available commands.\n";
    return true;
}

// NCurses UI implementation
#ifdef CODEX_UI_NCURSES

// Helper struct to store colored output lines
struct OutputLine {
    std::string text;
    int color_pair;

    OutputLine(const std::string& t, int cp = 0) : text(t), color_pair(cp) {}
};

// UI State for state machine
enum class UIState {
    Normal,         // Normal chat mode
    ToolApproval,   // Waiting for tool approval (y/n/d)
    Discuss         // Discuss mode (planning)
};

// Permission modes (cycle with Shift+Tab)
enum class PermissionMode {
    PlanMode,           // Plan before executing
    Normal,             // Ask for approval
    AutoAcceptEdits,    // Auto-accept file edits
    YOLO                // Approve anything, no sandbox
};

const char* permission_mode_to_string(PermissionMode mode) {
    switch (mode) {
        case PermissionMode::PlanMode: return "PLAN";
        case PermissionMode::Normal: return "NORMAL";
        case PermissionMode::AutoAcceptEdits: return "AUTO-EDIT";
        case PermissionMode::YOLO: return "YOLO";
    }
    return "UNKNOWN";
}

bool run_ncurses_mode(QwenStateManager& state_mgr, Qwen::QwenClient& client, const QwenConfig& config) {
    // Initialize ncurses
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    // Enable mouse support
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, nullptr);

    // Initialize colors if supported
    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_GREEN, COLOR_BLACK);   // User messages
        init_pair(2, COLOR_CYAN, COLOR_BLACK);    // AI messages
        init_pair(3, COLOR_YELLOW, COLOR_BLACK);  // System/status messages
        init_pair(4, COLOR_RED, COLOR_BLACK);     // Error messages
        init_pair(5, COLOR_BLUE, COLOR_BLACK);    // Info messages
        init_pair(6, COLOR_MAGENTA, COLOR_BLACK); // Tool messages
    }

    // Get screen dimensions
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    // Create windows for input and output
    int output_height = max_y - 4;  // Leave room for input, status bar, and separator
    WINDOW* output_win = newwin(output_height, max_x, 0, 0);
    WINDOW* status_win = newwin(1, max_x, output_height, 0);
    WINDOW* input_win = newwin(3, max_x, output_height + 1, 0);

    scrollok(output_win, TRUE);

    // Output buffer for scrollable history
    std::vector<OutputLine> output_buffer;
    int scroll_offset = 0;  // How many lines scrolled back from bottom

    // Input buffer and cursor position
    std::string input_buffer;
    size_t cursor_pos = 0;  // Position in input_buffer

    // UI state machine
    UIState ui_state = UIState::Normal;
    PermissionMode permission_mode = PermissionMode::Normal;
    Qwen::ToolGroup pending_tool_group;
    bool has_pending_tool_group = false;

    // Context usage tracking
    int context_usage_percent = 0;  // 0-100

    // Helper function to add a line to output buffer
    auto add_output_line = [&](const std::string& text, int color = 0) {
        output_buffer.emplace_back(text, color);
        scroll_offset = 0;  // Auto-scroll to bottom on new output
    };

    // Helper function to redraw output window
    auto redraw_output = [&]() {
        werase(output_win);

        // Calculate which lines to display
        int display_lines = output_height;
        int total_lines = output_buffer.size();
        int start_line = std::max(0, total_lines - display_lines - scroll_offset);
        int end_line = std::min(total_lines, start_line + display_lines);

        // Draw visible lines
        int y = 0;
        for (int i = start_line; i < end_line; ++i) {
            const auto& line = output_buffer[i];
            if (has_colors() && line.color_pair > 0) {
                wattron(output_win, COLOR_PAIR(line.color_pair));
                mvwprintw(output_win, y++, 0, "%s", line.text.c_str());
                wattroff(output_win, COLOR_PAIR(line.color_pair));
            } else {
                mvwprintw(output_win, y++, 0, "%s", line.text.c_str());
            }
        }

        wrefresh(output_win);
    };

    // Helper function to redraw status bar
    auto redraw_status = [&](const std::string& extra = "") {
        werase(status_win);
        wattron(status_win, A_REVERSE);

        // Left side: Model and session
        std::string left_text = "Model: " + state_mgr.get_model() +
                                " | Session: " + state_mgr.get_current_session().substr(0, 8);

        // Right side: Permission mode, context usage, scroll indicator
        std::string right_text = permission_mode_to_string(permission_mode);
        right_text += " | Ctx: " + std::to_string(context_usage_percent) + "%";

        if (scroll_offset > 0) {
            right_text += " | ↑" + std::to_string(scroll_offset);
        }

        if (!extra.empty()) {
            right_text = extra + " | " + right_text;
        }

        // Calculate spacing
        int total_len = left_text.length() + right_text.length();
        int spaces = max_x - total_len - 2;
        if (spaces < 1) spaces = 1;

        // Draw status bar
        mvwprintw(status_win, 0, 0, "%s", left_text.c_str());
        for (int i = 0; i < spaces; ++i) {
            waddch(status_win, ' ');
        }
        wprintw(status_win, "%s", right_text.c_str());

        // Pad to end
        int current_x = left_text.length() + spaces + right_text.length();
        for (int i = current_x; i < max_x - 1; ++i) {
            waddch(status_win, ' ');
        }

        wattroff(status_win, A_REVERSE);
        wrefresh(status_win);
    };

    // Helper function to redraw input window
    auto redraw_input = [&]() {
        werase(input_win);
        box(input_win, 0, 0);

        // Display input text (handle text longer than window width)
        int visible_width = max_x - 4;  // Account for box borders and "> " prompt
        int display_start = 0;

        if ((int)cursor_pos > visible_width - 1) {
            display_start = cursor_pos - visible_width + 1;
        }

        std::string visible_text = input_buffer.substr(display_start, visible_width);
        mvwprintw(input_win, 1, 2, "> %s", visible_text.c_str());

        // Position cursor
        int cursor_x = 4 + (cursor_pos - display_start);
        wmove(input_win, 1, cursor_x);

        wrefresh(input_win);
    };

    // Add initial info to output buffer
    add_output_line("qwen - AI Assistant (NCurses Mode)", has_colors() ? 2 : 0);
    add_output_line("Active Session: " + state_mgr.get_current_session(), has_colors() ? 5 : 0);
    add_output_line("Model: " + state_mgr.get_model(), has_colors() ? 5 : 0);
    add_output_line("Type /help for commands, /exit to quit, Shift+Tab to cycle permission modes", has_colors() ? 3 : 0);
    add_output_line("Use Page Up/Down or Ctrl+U/D to scroll | Mouse wheel to scroll", has_colors() ? 3 : 0);
    add_output_line("");

    // Load previous messages from session if any
    int msg_count = state_mgr.get_message_count();
    if (msg_count > 0) {
        add_output_line("=== Loading " + std::to_string(msg_count) + " previous message(s) ===", has_colors() ? 3 : 0);
        // Get messages from state manager (we'll need to add a method for this)
        // For now, just show a note that messages exist
        add_output_line("");
    }

    // Initial draw
    redraw_output();
    redraw_status();
    redraw_input();
    
    // Message handlers for ncurses mode
    Qwen::MessageHandlers ncurses_handlers;

    // Track streaming state
    static bool streaming_in_progress = false;
    static std::string streaming_buffer;

    ncurses_handlers.on_init = [&](const Qwen::InitMessage& msg) {
        add_output_line("[Connected to qwen-code]", has_colors() ? 5 : 0);
        if (!msg.version.empty()) {
            add_output_line("[Version: " + msg.version + "]", has_colors() ? 5 : 0);
        }
        redraw_output();
        redraw_input();  // Restore cursor to input
    };

    ncurses_handlers.on_conversation = [&](const Qwen::ConversationMessage& msg) {
        if (msg.role == Qwen::MessageRole::USER) {
            streaming_in_progress = false;
            streaming_buffer.clear();
            add_output_line("You: " + msg.content, has_colors() ? 1 : 0);
            redraw_output();
            redraw_input();
        } else if (msg.role == Qwen::MessageRole::ASSISTANT) {
            // Handle streaming messages
            if (msg.is_streaming.value_or(false)) {
                if (!streaming_in_progress) {
                    streaming_in_progress = true;
                    streaming_buffer = "AI: ";
                }
                streaming_buffer += msg.content;

                // Update the last line in the buffer (or add if empty)
                if (!output_buffer.empty() && output_buffer.back().text.substr(0, 4) == "AI: ") {
                    output_buffer.back().text = streaming_buffer;
                } else {
                    add_output_line(streaming_buffer, has_colors() ? 2 : 0);
                }
                redraw_output();
                redraw_input();
            } else {
                // End of streaming or non-streaming message
                if (streaming_in_progress) {
                    streaming_in_progress = false;
                    streaming_buffer.clear();
                } else if (!msg.content.empty()) {
                    add_output_line("AI: " + msg.content, has_colors() ? 2 : 0);
                    redraw_output();
                    redraw_input();
                }
            }
        } else {
            add_output_line("[system]: " + msg.content, has_colors() ? 3 : 0);
            redraw_output();
            redraw_input();
        }

        // Store in state manager
        state_mgr.add_message(msg);
    };
    
    ncurses_handlers.on_tool_group = [&](const Qwen::ToolGroup& group) {
        // Check permission mode for auto-approval
        bool auto_approve = false;
        if (permission_mode == PermissionMode::YOLO) {
            auto_approve = true;
        } else if (permission_mode == PermissionMode::AutoAcceptEdits) {
            // Check if all tools are edit-related
            bool all_edits = true;
            for (const auto& tool : group.tools) {
                if (tool.tool_name != "Edit" && tool.tool_name != "Write") {
                    all_edits = false;
                    break;
                }
            }
            auto_approve = all_edits;
        }

        // Display tool group
        add_output_line("", 0);
        add_output_line("[Tool Execution Request:]", has_colors() ? 6 : 0);
        add_output_line("  Group ID: " + std::to_string(group.id), has_colors() ? 6 : 0);
        add_output_line("  Tools to execute:", has_colors() ? 6 : 0);

        for (const auto& tool : group.tools) {
            add_output_line("    - " + tool.tool_name + " (ID: " + tool.tool_id + ")", has_colors() ? 6 : 0);

            if (tool.confirmation_details.has_value()) {
                add_output_line("      Details: " + tool.confirmation_details->message, has_colors() ? 5 : 0);
            }

            if (!tool.args.empty()) {
                add_output_line("      Arguments:", has_colors() ? 5 : 0);
                for (const auto& [key, value] : tool.args) {
                    std::string arg_line = "        " + key + ": " + value;
                    // Truncate very long argument values
                    if (arg_line.length() > 120) {
                        arg_line = arg_line.substr(0, 117) + "...";
                    }
                    add_output_line(arg_line, has_colors() ? 5 : 0);
                }
            }
        }

        redraw_output();

        if (auto_approve) {
            // Auto-approve based on permission mode
            add_output_line("  [Auto-approved by " + std::string(permission_mode_to_string(permission_mode)) + " mode]", has_colors() ? 3 : 0);
            redraw_output();

            for (const auto& tool : group.tools) {
                client.send_tool_approval(tool.tool_id, true);
                add_output_line("  ✓ Approved: " + tool.tool_name, has_colors() ? 1 : 0);
            }
            redraw_output();
            redraw_input();
        } else {
            // Request user approval - set state and store pending group
            add_output_line("", 0);
            add_output_line("Approve tools? [y]es / [n]o / [d]iscuss", has_colors() ? 3 : 0);
            redraw_output();
            redraw_status("Waiting for approval (y/n/d)");
            redraw_input();

            // Store pending tool group and change state
            pending_tool_group = group;
            has_pending_tool_group = true;
            ui_state = UIState::ToolApproval;
        }

        // Store in state manager
        state_mgr.add_tool_group(group);
    };
    
    ncurses_handlers.on_status = [&](const Qwen::StatusUpdate& msg) {
        std::string status_line = "[Status: " + std::string(Qwen::app_state_to_string(msg.state)) + "]";
        if (msg.message.has_value()) {
            status_line += " " + msg.message.value();
        }
        add_output_line(status_line, has_colors() ? 3 : 0);
        redraw_output();
        redraw_input();
    };

    ncurses_handlers.on_info = [&](const Qwen::InfoMessage& msg) {
        add_output_line("[Info: " + msg.message + "]", has_colors() ? 5 : 0);
        redraw_output();
        redraw_input();
    };

    ncurses_handlers.on_error = [&](const Qwen::ErrorMessage& msg) {
        add_output_line("[Error: " + msg.message + "]", has_colors() ? 4 : 0);
        redraw_output();
        redraw_input();
    };

    ncurses_handlers.on_completion_stats = [&](const Qwen::CompletionStats& stats) {
        std::string stats_line = "[Stats";
        if (stats.prompt_tokens.has_value()) {
            stats_line += " - Prompt: " + std::to_string(stats.prompt_tokens.value());
        }
        if (stats.completion_tokens.has_value()) {
            stats_line += ", Completion: " + std::to_string(stats.completion_tokens.value());
        }
        if (!stats.duration.empty()) {
            stats_line += ", Duration: " + stats.duration;
        }
        stats_line += "]";
        add_output_line(stats_line, has_colors() ? 3 : 0);
        redraw_output();
        redraw_input();
    };
    
    // Update client handlers
    client.set_handlers(ncurses_handlers);

    // Set non-blocking input mode with 50ms timeout
    wtimeout(input_win, 50);

    bool should_exit = false;

    // Helper to handle special commands
    auto handle_command = [&](const std::string& cmd) -> bool {
        if (cmd == "/exit") {
            add_output_line("Exiting and closing session...", has_colors() ? 3 : 0);
            redraw_output();
            state_mgr.save_session();
            client.stop();
            should_exit = true;
            return true;
        } else if (cmd == "/detach") {
            add_output_line("Detaching from session (saving state)...", has_colors() ? 3 : 0);
            redraw_output();
            state_mgr.save_session();
            add_output_line("Session saved. Use 'qwen --attach " + state_mgr.get_current_session() + "' to reconnect.", has_colors() ? 1 : 0);
            redraw_output();
            should_exit = true;
            return true;
        } else if (cmd == "/save") {
            add_output_line("Saving session...", has_colors() ? 3 : 0);
            redraw_output();
            if (state_mgr.save_session()) {
                add_output_line("Session saved successfully.", has_colors() ? 1 : 0);
            } else {
                add_output_line("Failed to save session.", has_colors() ? 4 : 0);
            }
            redraw_output();
            return true;
        } else if (cmd == "/status") {
            add_output_line("Session Status:", has_colors() ? 2 : 0);
            add_output_line("  Session ID: " + state_mgr.get_current_session(), 0);
            add_output_line("  Model: " + state_mgr.get_model(), 0);
            add_output_line("  Message count: " + std::to_string(state_mgr.get_message_count()), 0);
            add_output_line("  Workspace: " + state_mgr.get_workspace_root(), 0);
            add_output_line("  Client running: " + std::string(client.is_running() ? "yes" : "no"), 0);
            redraw_output();
            return true;
        } else if (cmd == "/help") {
            add_output_line("Interactive Commands:", has_colors() ? 2 : 0);
            add_output_line("  /detach   - Detach from session (keeps it running)", 0);
            add_output_line("  /exit     - Exit and close session", 0);
            add_output_line("  /save     - Save session immediately", 0);
            add_output_line("  /status   - Show session status", 0);
            add_output_line("  /help     - Show this help", 0);
            redraw_output();
            return true;
        } else {
            add_output_line("Unknown command: " + cmd, has_colors() ? 4 : 0);
            add_output_line("Type /help for available commands.", 0);
            redraw_output();
            return true;
        }
    };

    // Main event loop
    while (!should_exit && client.is_running()) {
        // Poll for incoming messages (non-blocking)
        client.poll_messages(0);

        // Get a character from input window (non-blocking, 50ms timeout)
        int ch = wgetch(input_win);

        if (ch != ERR) {
            // Handle mouse events
            if (ch == KEY_MOUSE) {
                MEVENT event;
                if (getmouse(&event) == OK) {
                    // Mouse wheel up = scroll up
                    if (event.bstate & BUTTON4_PRESSED) {
                        scroll_offset = std::min(scroll_offset + 3, (int)output_buffer.size() - output_height);
                        redraw_output();
                        redraw_status();
                        redraw_input();
                    }
                    // Mouse wheel down = scroll down
                    else if (event.bstate & BUTTON5_PRESSED) {
                        scroll_offset = std::max(scroll_offset - 3, 0);
                        redraw_output();
                        redraw_status();
                        redraw_input();
                    }
                }
                continue;
            }

            // Handle Shift+Tab for permission mode cycling
            if (ch == KEY_BTAB) {
                // Cycle through permission modes
                switch (permission_mode) {
                    case PermissionMode::PlanMode:
                        permission_mode = PermissionMode::Normal;
                        break;
                    case PermissionMode::Normal:
                        permission_mode = PermissionMode::AutoAcceptEdits;
                        break;
                    case PermissionMode::AutoAcceptEdits:
                        permission_mode = PermissionMode::YOLO;
                        break;
                    case PermissionMode::YOLO:
                        permission_mode = PermissionMode::PlanMode;
                        break;
                }
                add_output_line("Permission mode: " + std::string(permission_mode_to_string(permission_mode)), has_colors() ? 3 : 0);
                redraw_output();
                redraw_status();
                redraw_input();
                continue;
            }

            // Handle tool approval state
            if (ui_state == UIState::ToolApproval && has_pending_tool_group) {
                bool handled = false;
                bool approved = false;

                if (ch == 'y' || ch == 'Y') {
                    approved = true;
                    handled = true;
                } else if (ch == 'n' || ch == 'N') {
                    approved = false;
                    handled = true;
                } else if (ch == 'd' || ch == 'D') {
                    // Enter discuss mode
                    add_output_line("=== Entering Discuss Mode ===", has_colors() ? 3 : 0);
                    add_output_line("Explain your concerns or ask questions about the tools:", has_colors() ? 3 : 0);
                    add_output_line("(Type your message and press Enter, or 'y'/'n' to approve/reject)", has_colors() ? 3 : 0);
                    redraw_output();
                    ui_state = UIState::Discuss;
                    redraw_status("Discuss mode");
                    redraw_input();
                    continue;
                }

                if (handled) {
                    // Send approval/rejection for all tools
                    for (const auto& tool : pending_tool_group.tools) {
                        client.send_tool_approval(tool.tool_id, approved);

                        if (approved) {
                            add_output_line("  ✓ Approved: " + tool.tool_name, has_colors() ? 1 : 0);
                        } else {
                            add_output_line("  ✗ Rejected: " + tool.tool_name, has_colors() ? 4 : 0);
                        }
                    }

                    redraw_output();
                    has_pending_tool_group = false;
                    ui_state = UIState::Normal;
                    redraw_status();
                    redraw_input();
                    continue;
                }
            }

            // Handle discuss mode input
            if (ui_state == UIState::Discuss) {
                if (ch == 'y' || ch == 'Y') {
                    // Approve and exit discuss mode
                    if (has_pending_tool_group) {
                        for (const auto& tool : pending_tool_group.tools) {
                            client.send_tool_approval(tool.tool_id, true);
                            add_output_line("  ✓ Approved: " + tool.tool_name, has_colors() ? 1 : 0);
                        }
                        has_pending_tool_group = false;
                    }
                    ui_state = UIState::Normal;
                    redraw_output();
                    redraw_status();
                    redraw_input();
                    continue;
                } else if (ch == 'n' || ch == 'N') {
                    // Reject and exit discuss mode
                    if (has_pending_tool_group) {
                        for (const auto& tool : pending_tool_group.tools) {
                            client.send_tool_approval(tool.tool_id, false);
                            add_output_line("  ✗ Rejected: " + tool.tool_name, has_colors() ? 4 : 0);
                        }
                        has_pending_tool_group = false;
                    }
                    ui_state = UIState::Normal;
                    redraw_output();
                    redraw_status();
                    redraw_input();
                    continue;
                }
                // Otherwise fall through to normal input handling for discuss messages
            }

            // Normal input processing
            if (ch == '\n' || ch == KEY_ENTER || ch == 13) {
                // Enter key - submit input
                if (!input_buffer.empty()) {
                    // Handle special commands
                    if (input_buffer[0] == '/') {
                        handle_command(input_buffer);
                    } else {
                        // Send to AI
                        if (client.send_user_input(input_buffer)) {
                            // Input sent successfully (will appear via message handler)
                            if (ui_state == UIState::Discuss) {
                                add_output_line("(AI will respond to your question. Press 'y' to approve or 'n' to reject after.)", has_colors() ? 3 : 0);
                                redraw_output();
                            }
                        } else {
                            add_output_line("Failed to send message.", has_colors() ? 4 : 0);
                            redraw_output();
                        }
                    }

                    // Clear input buffer
                    input_buffer.clear();
                    cursor_pos = 0;
                    redraw_input();
                }
            } else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
                // Backspace
                if (cursor_pos > 0) {
                    input_buffer.erase(cursor_pos - 1, 1);
                    cursor_pos--;
                    redraw_input();
                }
            } else if (ch == KEY_DC) {
                // Delete key
                if (cursor_pos < input_buffer.length()) {
                    input_buffer.erase(cursor_pos, 1);
                    redraw_input();
                }
            } else if (ch == KEY_LEFT) {
                // Left arrow
                if (cursor_pos > 0) {
                    cursor_pos--;
                    redraw_input();
                }
            } else if (ch == KEY_RIGHT) {
                // Right arrow
                if (cursor_pos < input_buffer.length()) {
                    cursor_pos++;
                    redraw_input();
                }
            } else if (ch == KEY_HOME || ch == 1) {  // Ctrl+A
                // Home or Ctrl+A - move to beginning
                cursor_pos = 0;
                redraw_input();
            } else if (ch == KEY_END || ch == 5) {  // Ctrl+E
                // End or Ctrl+E - move to end
                cursor_pos = input_buffer.length();
                redraw_input();
            } else if (ch == KEY_PPAGE || ch == 21) {  // Page Up or Ctrl+U
                // Scroll up
                scroll_offset = std::min(scroll_offset + 5, (int)output_buffer.size() - output_height);
                redraw_output();
                redraw_status();
                redraw_input();
            } else if (ch == KEY_NPAGE || ch == 4) {  // Page Down or Ctrl+D
                // Scroll down
                scroll_offset = std::max(scroll_offset - 5, 0);
                redraw_output();
                redraw_status();
                redraw_input();
            } else if (ch >= 32 && ch < 127) {
                // Regular printable character
                input_buffer.insert(cursor_pos, 1, (char)ch);
                cursor_pos++;
                redraw_input();
            }
        }

        // Small delay to avoid busy-waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Cleanup ncurses
    delwin(output_win);
    delwin(status_win);
    delwin(input_win);
    endwin();
    
    return true;
}
#endif

// Main qwen command entry point
void cmd_qwen(const std::vector<std::string>& args,
              Vfs& vfs) {
    // Parse arguments
    QwenOptions opts = parse_args(args);

    if (opts.help) {
        show_help();
        return;
    }

    // Load configuration
    QwenConfig config;
    std::map<std::string, std::string> dummy_env;
    config.load_from_env(dummy_env);
    config.load_from_file("/env/qwen_config.json", vfs);

    // Override config with command-line options
    if (!opts.model.empty()) {
        config.model = opts.model;
    }
    if (!opts.workspace_root.empty()) {
        config.workspace_root = opts.workspace_root;
    }

    // Create state manager with VFS
    QwenStateManager state_mgr(&vfs);

    if (opts.list_sessions) {
        list_sessions(state_mgr);
        return;
    }

    // Create or load session
    std::string session_id;
    if (opts.attach) {
        std::cout << "Loading session: " << opts.session_id << "\n";
        bool loaded = state_mgr.load_session(opts.session_id);
        if (!loaded) {
            std::cout << Color::RED << "Failed to load session." << Color::RESET << "\n";
            return;
        }
        session_id = opts.session_id;
        std::cout << Color::GREEN << "Session loaded successfully!" << Color::RESET << "\n";
    } else {
        std::cout << "Creating new session with model: " << config.model << "\n";
        session_id = state_mgr.create_session(config.model, config.workspace_root);
        if (session_id.empty()) {
            std::cout << Color::RED << "Failed to create session." << Color::RESET << "\n";
            return;
        }
        std::cout << Color::GREEN << "Session created: " << session_id << Color::RESET << "\n";
    }

    // Display session info
    std::cout << "\n" << Color::CYAN << "Active Session: " << session_id << Color::RESET << "\n";
    std::cout << "Model: " << state_mgr.get_model() << "\n";
    std::cout << "Type /help for commands, /exit to quit\n\n";

    // Configure QwenClient
    Qwen::QwenClientConfig client_config;
    client_config.qwen_executable = config.qwen_code_path;
    client_config.auto_restart = true;
    client_config.verbose = false;  // Disable verbose logging for clean output

    // Note: --server-mode stdin is hardcoded in QwenClient, no need to add it here

    // Add model if specified
    if (!config.model.empty()) {
        client_config.qwen_args.push_back("--model");
        client_config.qwen_args.push_back(config.model);
    }

    // Add workspace argument if specified
    if (!config.workspace_root.empty()) {
        client_config.qwen_args.push_back("--workspace-root");
        client_config.qwen_args.push_back(config.workspace_root);
    }

    // Add OpenAI flag if specified
    if (opts.use_openai) {
        client_config.qwen_args.push_back("--openai");
    }

    // Create client
    Qwen::QwenClient client(client_config);

    // Track pending tool approvals
    std::vector<Qwen::ToolGroup> pending_tools;

    // Determine whether to use ncurses mode
    bool use_ncurses = !opts.simple_mode && supports_ncurses();
#ifdef CODEX_UI_NCURSES
    if (use_ncurses) {
        // Setup for ncurses mode - we'll set handlers inside the ncurses function
        // Start the client
        std::cout << Color::YELLOW << "Starting qwen-code...\n" << Color::RESET;
        if (!client.start()) {
            std::cout << Color::RED << "Failed to start qwen-code subprocess.\n" << Color::RESET;
            std::cout << "Error: " << client.get_last_error() << "\n";
            std::cout << "\nMake sure qwen-code is installed and accessible.\n";
            std::cout << "Set QWEN_CODE_PATH environment variable if needed.\n";
            std::cout << "Falling back to stdio mode.\n";
            use_ncurses = false;
        } else {
            std::cout << Color::GREEN << "Connected! Switching to ncurses mode...\n" << Color::RESET;
            run_ncurses_mode(state_mgr, client, config);
            // Save session before exiting
            std::cout << Color::YELLOW << "Saving session...\n" << Color::RESET;
            state_mgr.save_session();
            std::cout << Color::GREEN << "Session saved.\n" << Color::RESET;
            return;
        }
    }
#endif

    // Use stdio mode (fallback or forced with --simple)
    // Setup message handlers for stdio mode
    Qwen::MessageHandlers handlers;

    handlers.on_init = [&](const Qwen::InitMessage& msg) {
        std::cout << Color::GRAY << "[Connected to qwen-code]" << Color::RESET << "\n";
        if (!msg.version.empty()) {
            std::cout << Color::GRAY << "[Version: " << msg.version << "]" << Color::RESET << "\n";
        }
    };

    handlers.on_conversation = [&](const Qwen::ConversationMessage& msg) {
        // Display the message
        display_conversation_message(msg);

        // Store in state manager
        state_mgr.add_message(msg);
    };

    handlers.on_tool_group = [&](const Qwen::ToolGroup& group) {
        // Display tool group
        display_tool_group(group);

        // Store in state manager
        state_mgr.add_tool_group(group);

        // Check if auto-approve is enabled
        if (config.auto_approve_tools) {
            std::cout << Color::YELLOW << "[Auto-approving tools]\n" << Color::RESET;
            // Send approval for all tools
            for (const auto& tool : group.tools) {
                client.send_tool_approval(tool.tool_id, true);
            }
        } else {
            // Prompt user for approval
            bool approved = prompt_tool_approval(group);

            // Send approval/rejection for all tools in the group
            for (const auto& tool : group.tools) {
                client.send_tool_approval(tool.tool_id, approved);

                if (approved) {
                    std::cout << Color::GREEN << "  ✓ Approved: " << tool.tool_name << Color::RESET << "\n";
                } else {
                    std::cout << Color::RED << "  ✗ Rejected: " << tool.tool_name << Color::RESET << "\n";
                }
            }
        }
    };

    handlers.on_status = [&](const Qwen::StatusUpdate& msg) {
        std::cout << Color::GRAY << "[Status: " << Qwen::app_state_to_string(msg.state) << "]" << Color::RESET;
        if (msg.message.has_value()) {
            std::cout << " " << msg.message.value();
        }
        std::cout << "\n";
    };

    handlers.on_info = [&](const Qwen::InfoMessage& msg) {
        std::cout << Color::BLUE << "[Info: " << msg.message << "]" << Color::RESET << "\n";
    };

    handlers.on_error = [&](const Qwen::ErrorMessage& msg) {
        std::cout << Color::RED << "[Error: " << msg.message << "]" << Color::RESET << "\n";
    };

    handlers.on_completion_stats = [&](const Qwen::CompletionStats& stats) {
        std::cout << Color::GRAY << "[Stats";
        if (stats.prompt_tokens.has_value()) {
            std::cout << " - Prompt: " << stats.prompt_tokens.value();
        }
        if (stats.completion_tokens.has_value()) {
            std::cout << ", Completion: " << stats.completion_tokens.value();
        }
        if (!stats.duration.empty()) {
            std::cout << ", Duration: " << stats.duration;
        }
        std::cout << "]" << Color::RESET << "\n";
    };

    // Set handlers
    client.set_handlers(handlers);

    // Start the client in stdio mode
    std::cout << Color::YELLOW << "Starting qwen-code...\n" << Color::RESET;
    if (!client.start()) {
        std::cout << Color::RED << "Failed to start qwen-code subprocess.\n" << Color::RESET;
        std::cout << "Error: " << client.get_last_error() << "\n";
        std::cout << "\nMake sure qwen-code is installed and accessible.\n";
        std::cout << "Set QWEN_CODE_PATH environment variable if needed.\n";
        return;
    }

    std::cout << Color::GREEN << "Connected!\n" << Color::RESET << "\n";

    // Main interactive loop
    bool should_exit = false;
    std::string input_line;

    while (!should_exit && client.is_running()) {
        // Display prompt
        std::cout << Color::GREEN << "> " << Color::RESET;
        std::cout.flush();

        // Read user input
        if (!std::getline(std::cin, input_line)) {
            // EOF or error on stdin
            break;
        }

        // Handle special commands
        if (handle_special_command(input_line, state_mgr, client, should_exit)) {
            if (should_exit) {
                break;
            }
            continue;
        }

        // Skip empty lines
        if (input_line.empty()) {
            continue;
        }

        // Send user input to qwen
        if (!client.send_user_input(input_line)) {
            std::cout << Color::RED << "Failed to send message.\n" << Color::RESET;
            continue;
        }

        // Poll for responses with a timeout
        // We'll poll in a loop to handle streaming responses
        bool waiting_for_response = true;
        auto start_time = std::chrono::steady_clock::now();
        const int total_timeout_ms = 30000;  // 30 seconds total timeout

        while (waiting_for_response && client.is_running()) {
            // Poll for messages (100ms timeout per poll)
            int msg_count = client.poll_messages(100);

            if (msg_count < 0) {
                // Error occurred
                std::cout << Color::RED << "Error polling messages.\n" << Color::RESET;
                // Reset the streaming state if needed to ensure clean UI on error
                if (streaming_in_progress) {
                    std::cout << "\n";  // Complete the line if we were streaming
                    streaming_in_progress = false;
                }
                waiting_for_response = false;
                break;
            }

            if (msg_count == 0) {
                // No messages yet, check timeout
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start_time
                ).count();

                if (elapsed > total_timeout_ms) {
                    std::cout << Color::YELLOW << "\n[Response timeout]\n" << Color::RESET;
                    // Reset the streaming state if needed to ensure clean UI on timeout
                    if (streaming_in_progress) {
                        std::cout << "\n";  // Complete the line if we were streaming
                        streaming_in_progress = false;
                    }
                    waiting_for_response = false;
                    break;
                }

                // Brief sleep to avoid busy-waiting
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            // Messages received - reset timer for next batch
            start_time = std::chrono::steady_clock::now();

            // Continue polling for more messages (streaming)
            // We'll keep polling until we get a completion_stats or no messages for a bit
        }

        // Ensure we have a newline after response completion
        if (streaming_in_progress) {
            std::cout << "\n";
            streaming_in_progress = false;
        }
    }

    // Cleanup
    if (client.is_running()) {
        client.stop();
    }

    // Save session before exiting
    std::cout << Color::YELLOW << "Saving session...\n" << Color::RESET;
    state_mgr.save_session();
    std::cout << Color::GREEN << "Session saved.\n" << Color::RESET;
}

}  // namespace QwenCmd
