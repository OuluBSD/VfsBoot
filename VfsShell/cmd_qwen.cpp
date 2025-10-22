#include "VfsShell.h"
#include "cmd_qwen.h"
#include <iostream>
#include <cstdlib>
#include <cstring>

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

    // TODO: Implement interactive loop
    // For Phase 4 initial implementation, just show a message
    std::cout << Color::YELLOW << "\n[Phase 4 Integration - In Progress]\n" << Color::RESET;
    std::cout << "The qwen command structure is in place.\n";
    std::cout << "Session management is ready (create, load, save, list).\n";
    std::cout << "Interactive terminal loop needs to be implemented.\n\n";
    std::cout << "Available functionality:\n";
    std::cout << "  - qwen --list-sessions  (list all sessions)\n";
    std::cout << "  - qwen --help           (show help)\n\n";

    // Example: Create a test session
    if (opts.attach) {
        std::cout << "Loading session: " << opts.session_id << "\n";
        bool loaded = state_mgr.load_session(opts.session_id);
        if (loaded) {
            std::cout << Color::GREEN << "Session loaded successfully!" << Color::RESET << "\n";
        } else {
            std::cout << Color::RED << "Failed to load session." << Color::RESET << "\n";
        }
    } else {
        std::cout << "Creating new session with model: " << config.model << "\n";
        std::string session_id = state_mgr.create_session(config.model, config.workspace_root);
        if (!session_id.empty()) {
            std::cout << Color::GREEN << "Session created: " << session_id << Color::RESET << "\n";
            std::cout << "\nTo attach to this session later:\n";
            std::cout << "  qwen --attach " << session_id << "\n\n";
        } else {
            std::cout << Color::RED << "Failed to create session." << Color::RESET << "\n";
        }
    }

    std::cout << "\n" << Color::YELLOW << "Note:" << Color::RESET <<
        " Full interactive loop with qwen-code integration coming in next iteration.\n";
}

}  // namespace QwenCmd
