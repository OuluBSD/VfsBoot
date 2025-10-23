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
bool run_ncurses_mode(QwenStateManager& state_mgr, Qwen::QwenClient& client, const QwenConfig& config) {
    // Initialize ncurses
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    
    // Get screen dimensions
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    // Create windows for input and output
    int output_height = max_y - 3;  // Leave room for input
    WINDOW* output_win = newwin(output_height, max_x, 0, 0);
    WINDOW* input_win = newwin(3, max_x, output_height, 0);
    
    scrollok(output_win, TRUE);
    
    // Print initial info
    wprintw(output_win, "qwen - AI Assistant (NCurses Mode)\n");
    wprintw(output_win, "Active Session: %s\n", state_mgr.get_current_session().c_str());
    wprintw(output_win, "Model: %s\n", state_mgr.get_model().c_str());
    wprintw(output_win, "Type /help for commands, /exit to quit\n\n");
    wrefresh(output_win);
    
    // Message handlers for ncurses mode
    Qwen::MessageHandlers ncurses_handlers;
    
    ncurses_handlers.on_init = [&](const Qwen::InitMessage& msg) {
        wprintw(output_win, "[Connected to qwen-code]\n");
        if (!msg.version.empty()) {
            wprintw(output_win, "[Version: %s]\n", msg.version.c_str());
        }
        wrefresh(output_win);
    };
    
    ncurses_handlers.on_conversation = [&](const Qwen::ConversationMessage& msg) {
        if (msg.role == Qwen::MessageRole::USER) {
            wprintw(output_win, "You: %s\n", msg.content.c_str());
        } else if (msg.role == Qwen::MessageRole::ASSISTANT) {
            // Handle streaming messages
            if (msg.is_streaming.value_or(false)) {
                // For streaming, just append to the most recent line
                wprintw(output_win, "AI: %s", msg.content.c_str());
            } else {
                wprintw(output_win, "AI: %s\n", msg.content.c_str());
            }
        } else {
            wprintw(output_win, "[system]: %s\n", msg.content.c_str());
        }
        wrefresh(output_win);
        
        // Store in state manager
        state_mgr.add_message(msg);
    };
    
    ncurses_handlers.on_tool_group = [&](const Qwen::ToolGroup& group) {
        wprintw(output_win, "\n[Tool Execution Request:]\n");
        wprintw(output_win, "  Group ID: %d\n", group.id);
        wprintw(output_win, "  Tools to execute:\n");
        
        for (const auto& tool : group.tools) {
            wprintw(output_win, "    - %s (ID: %s)\n", tool.tool_name.c_str(), tool.tool_id.c_str());
            
            // Display confirmation details if present
            if (tool.confirmation_details.has_value()) {
                wprintw(output_win, "      Details: %s\n", tool.confirmation_details->message.c_str());
            }
            
            // Display arguments
            if (!tool.args.empty()) {
                wprintw(output_win, "      Arguments:\n");
                for (const auto& [key, value] : tool.args) {
                    wprintw(output_win, "        %s: %s\n", key.c_str(), value.c_str());
                }
            }
        }
        
        // Prompt for tool approval
        wprintw(output_win, "\nApprove tool execution? [y/n/d(details)]: ");
        wrefresh(output_win);
        
        int ch = wgetch(input_win);
        char response = (char)ch;
        
        bool approved = false;
        if (response == 'y' || response == 'Y') {
            approved = true;
        } else if (response == 'd' || response == 'D') {
            // Redraw tool group for details
            wprintw(output_win, "\nTool details shown above\n");
            wrefresh(output_win);
            // Prompt again
            wprintw(output_win, "Approve tool execution? [y/n]: ");
            wrefresh(output_win);
            ch = wgetch(input_win);
            response = (char)ch;
            approved = (response == 'y' || response == 'Y');
        }
        
        // Send approval for all tools in the group
        for (const auto& tool : group.tools) {
            client.send_tool_approval(tool.tool_id, approved);
            
            if (approved) {
                wprintw(output_win, "  ✓ Approved: %s\n", tool.tool_name.c_str());
            } else {
                wprintw(output_win, "  ✗ Rejected: %s\n", tool.tool_name.c_str());
            }
        }
        wrefresh(output_win);
        
        // Store in state manager
        state_mgr.add_tool_group(group);
    };
    
    ncurses_handlers.on_status = [&](const Qwen::StatusUpdate& msg) {
        wprintw(output_win, "[Status: %s]", Qwen::app_state_to_string(msg.state));
        if (msg.message.has_value()) {
            wprintw(output_win, " %s", msg.message.value().c_str());
        }
        wprintw(output_win, "\n");
        wrefresh(output_win);
    };
    
    ncurses_handlers.on_info = [&](const Qwen::InfoMessage& msg) {
        wprintw(output_win, "[Info: %s]\n", msg.message.c_str());
        wrefresh(output_win);
    };
    
    ncurses_handlers.on_error = [&](const Qwen::ErrorMessage& msg) {
        wprintw(output_win, "[Error: %s]\n", msg.message.c_str());
        wrefresh(output_win);
    };
    
    ncurses_handlers.on_completion_stats = [&](const Qwen::CompletionStats& stats) {
        wprintw(output_win, "[Stats");
        if (stats.prompt_tokens.has_value()) {
            wprintw(output_win, " - Prompt: %d", stats.prompt_tokens.value());
        }
        if (stats.completion_tokens.has_value()) {
            wprintw(output_win, ", Completion: %d", stats.completion_tokens.value());
        }
        if (!stats.duration.empty()) {
            wprintw(output_win, ", Duration: %s", stats.duration.c_str());
        }
        wprintw(output_win, "]\n");
        wrefresh(output_win);
    };
    
    // Update client handlers
    client.set_handlers(ncurses_handlers);
    
    bool should_exit = false;
    char input_buffer[1024];
    
    while (!should_exit && client.is_running()) {
        // Show input prompt
        mvwprintw(input_win, 0, 0, "> ");
        wclrtoeol(input_win);  // Clear to end of line
        box(input_win, 0, 0);
        wrefresh(input_win);
        
        // Get user input
        echo();  // Enable echoing for input
        int input_result = mvwgetnstr(input_win, 0, 2, input_buffer, sizeof(input_buffer) - 1);
        noecho(); // Disable echoing after input
        
        if (input_result == ERR) {
            // Error reading input
            continue;
        }
        
        std::string input_line(input_buffer);
        
        // Handle special commands
        if (input_line[0] == '/') {
            if (input_line == "/exit") {
                wprintw(output_win, "Exiting and closing session...\n");
                wrefresh(output_win);
                state_mgr.save_session();
                client.stop();
                should_exit = true;
                break;
            } else if (input_line == "/detach") {
                wprintw(output_win, "Detaching from session (saving state)...\n");
                wrefresh(output_win);
                state_mgr.save_session();
                wprintw(output_win, "Session saved. Use 'qwen --attach %s' to reconnect.\n", 
                       state_mgr.get_current_session().c_str());
                wrefresh(output_win);
                should_exit = true;
                break;
            } else if (input_line == "/save") {
                wprintw(output_win, "Saving session...\n");
                wrefresh(output_win);
                if (state_mgr.save_session()) {
                    wprintw(output_win, "Session saved successfully.\n");
                } else {
                    wprintw(output_win, "Failed to save session.\n");
                }
                wrefresh(output_win);
                continue;
            } else if (input_line == "/status") {
                wprintw(output_win, "Session Status:\n");
                wprintw(output_win, "  Session ID: %s\n", state_mgr.get_current_session().c_str());
                wprintw(output_win, "  Model: %s\n", state_mgr.get_model().c_str());
                wprintw(output_win, "  Message count: %d\n", state_mgr.get_message_count());
                wprintw(output_win, "  Workspace: %s\n", state_mgr.get_workspace_root().c_str());
                wprintw(output_win, "  Client running: %s\n", (client.is_running() ? "yes" : "no"));
                wrefresh(output_win);
                continue;
            } else if (input_line == "/help") {
                wprintw(output_win, "Interactive Commands:\n");
                wprintw(output_win, "  /detach   - Detach from session (keeps it running)\n");
                wprintw(output_win, "  /exit     - Exit and close session\n");
                wprintw(output_win, "  /save     - Save session immediately\n");
                wprintw(output_win, "  /status   - Show session status\n");
                wprintw(output_win, "  /help     - Show this help\n");
                wrefresh(output_win);
                continue;
            } else {
                wprintw(output_win, "Unknown command: %s\n", input_line.c_str());
                wprintw(output_win, "Type /help for available commands.\n");
                wrefresh(output_win);
                continue;
            }
        }
        
        // Skip empty lines
        if (input_line.empty()) {
            continue;
        }
        
        // Send user input to qwen
        if (!client.send_user_input(input_line)) {
            wprintw(output_win, "Failed to send message.\n");
            wrefresh(output_win);
            continue;
        }
        
        // Poll for responses with a timeout
        bool waiting_for_response = true;
        auto start_time = std::chrono::steady_clock::now();
        const int total_timeout_ms = 30000;  // 30 seconds total timeout
        
        while (waiting_for_response && client.is_running()) {
            // Poll for messages (100ms timeout per poll)
            int msg_count = client.poll_messages(100);
            
            if (msg_count < 0) {
                // Error occurred
                wprintw(output_win, "Error polling messages.\n");
                wrefresh(output_win);
                waiting_for_response = false;
                break;
            }
            
            if (msg_count > 0) {
                // Reset timer for next batch since we received messages
                start_time = std::chrono::steady_clock::now();
            }
            
            if (msg_count == 0) {
                // No messages yet, check timeout
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start_time
                ).count();
                
                if (elapsed > total_timeout_ms) {
                    wprintw(output_win, "\n[Response timeout]\n");
                    wrefresh(output_win);
                    waiting_for_response = false;
                    break;
                }
                
                // Brief sleep to avoid busy-waiting
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            
            // Continue polling for more messages (streaming)
            // We'll keep polling until we get a completion_stats or no messages for a bit
        }
    }
    
    // Cleanup ncurses
    delwin(output_win);
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

        std::cout << "\n";  // Add newline after response
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
