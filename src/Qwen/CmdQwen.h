#ifndef _Qwen_CmdQwen_h_
#define _Qwen_CmdQwen_h_

// All includes have been moved to Qwen.h - the main header
// This header is not self-contained as per U++ convention
// For reference: This header needs types from QwenClient.h and QwenStateManager.h
// #include "QwenClient.h"         // Commented for U++ convention - included in main header
// #include "QwenStateManager.h"   // Commented for U++ convention - included in main header

// Forward declaration
struct Vfs;

// Forward declarations from Qwen namespace
namespace Qwen {
    class QwenClient;
    class QwenStateManager;
}

namespace QwenCmd {

// Configuration for qwen command
struct QwenConfig {
    String model = "gpt-4o-mini";  // Default model
    String workspace_root;
    String qwen_code_path = "/common/active/sblo/Dev/VfsBoot/qwen-code";  // Path to qwen-code wrapper
    bool auto_approve_tools = false;
    bool use_colors = true;
    int max_retries = 3;

    // Load from VFS (/env/qwen_config.json) or environment
    void load_from_env(const Map<String, String>& env);

    // Load from VFS file
    bool load_from_file(const String& vfs_path, Vfs& vfs);
    typedef QwenConfig CLASSNAME;  // Required for THISBACK macros if used
};

// Options parsed from command-line arguments
struct QwenOptions {
    bool attach = false;
    bool list_sessions = false;
    bool help = false;
    bool simple_mode = false;  // Force stdio mode instead of ncurses
    bool use_openai = false;   // Use OpenAI instead of default provider
    bool manager_mode = false; // Enable manager mode
    String session_id;
    String model;
    String workspace_root;
    String mode = "stdin";  // Connection mode: stdin, tcp, pipe
    int port = 7777;             // TCP port (for mode=tcp)
    String host = "localhost";  // TCP host (for mode=tcp)
    typedef QwenOptions CLASSNAME;  // Required for THISBACK macros if used
};

// Parse command-line arguments
QwenOptions parse_args(const Vector<String>& args);

// Show help text
void show_help();

// Main qwen command entry point
void cmd_qwen(const Vector<String>& args,
              Vfs& vfs);

// List all sessions
void list_sessions(Qwen::QwenStateManager& state_mgr);

}  // namespace QwenCmd

#endif
