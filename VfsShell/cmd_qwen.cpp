#include "VfsShell.h"
#include "cmd_qwen.h"
#include "qwen_client.h"
#include "qwen_state_manager.h"
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <chrono>
#include <thread>

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
        else if (arg == "--help" || arg == "-h") {
            opts.help = true;
        }
    }

    return opts;
}

// Show help text
void show_help() {
    std::cout << "qwen - Interactive AI assistant powered by qwen-code\n\n";
    std::cout << "Usage:\n";
    std::cout << "  qwen [options]                 Start new interactive session\n";
    std::cout << "  qwen --attach <id>            Attach to existing session\n";
    std::cout << "  qwen --list-sessions          List all sessions\n";
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

// Display a conversation message with formatting
void display_conversation_message(const Qwen::ConversationMessage& msg) {
    if (msg.role == Qwen::MessageRole::USER) {
        std::cout << Color::GREEN << "You: " << Color::RESET << msg.content << "\n";
    } else if (msg.role == Qwen::MessageRole::ASSISTANT) {
        std::cout << Color::CYAN << "AI: " << Color::RESET << msg.content << "\n";
    } else {
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
    client_config.verbose = false;

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

    // Create client
    Qwen::QwenClient client(client_config);

    // Track pending tool approvals
    std::vector<Qwen::ToolGroup> pending_tools;

    // Setup message handlers
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

    // Start the client
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
