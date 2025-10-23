#ifndef _VFSSHELL_QWEN_CLIENT_H_
#define _VFSSHELL_QWEN_CLIENT_H_

#include "qwen_protocol.h"
#include <string>
#include <functional>
#include <memory>
#include <vector>

namespace Qwen {

// ============================================================================
// Message Handler Callbacks
// ============================================================================

// User-provided callback interface for handling incoming messages
struct MessageHandlers {
    std::function<void(const InitMessage&)> on_init;
    std::function<void(const ConversationMessage&)> on_conversation;
    std::function<void(const ToolGroup&)> on_tool_group;
    std::function<void(const StatusUpdate&)> on_status;
    std::function<void(const InfoMessage&)> on_info;
    std::function<void(const ErrorMessage&)> on_error;
    std::function<void(const CompletionStats&)> on_completion_stats;
};

// ============================================================================
// Communication Modes
// ============================================================================

enum class CommunicationMode {
    STDIN_STDOUT,   // Communicate via stdin/stdout with subprocess
    NAMED_PIPE,     // Communicate via named pipes
    TCP             // Communicate via TCP socket
};

// ============================================================================
// Qwen Client Configuration
// ============================================================================

struct QwenClientConfig {
    CommunicationMode mode = CommunicationMode::STDIN_STDOUT;

    // Path to qwen-code executable (for STDIN_STDOUT mode)
    std::string qwen_executable = "qwen";

    // Additional arguments to pass to qwen (e.g., "--workspace-root /path")
    std::vector<std::string> qwen_args;

    // For NAMED_PIPE mode
    std::string pipe_path;

    // For TCP mode
    std::string tcp_host = "localhost";
    int tcp_port = 8765;

    // Restart subprocess if it crashes
    bool auto_restart = true;

    // Maximum number of restart attempts
    int max_restarts = 3;

    // Enable verbose logging
    bool verbose = false;
    
    MessageHandlers handlers;
};

// ============================================================================
// Qwen Client
// ============================================================================

class QwenClient {
public:
    explicit QwenClient(const QwenClientConfig& config);
    ~QwenClient();

    // Prevent copying
    QwenClient(const QwenClient&) = delete;
    QwenClient& operator=(const QwenClient&) = delete;

    // ========================================================================
    // Lifecycle Management
    // ========================================================================

    // Start the qwen-code process and establish communication
    // Returns true on success, false on failure
    bool start();

    // Stop the qwen-code process gracefully
    void stop();

    // Check if the client is currently running
    bool is_running() const;

    // Restart the qwen-code process
    bool restart();

    // ========================================================================
    // Message Handling
    // ========================================================================

    // Set callbacks for handling incoming messages
    void set_handlers(const MessageHandlers& handlers);

    // Poll for incoming messages and dispatch to handlers
    // Returns number of messages processed, or -1 on error
    int poll_messages(int timeout_ms = 0);

    // ========================================================================
    // Command Sending
    // ========================================================================

    // Send user input to qwen
    bool send_user_input(const std::string& content);

    // Send tool approval/rejection
    bool send_tool_approval(const std::string& tool_id, bool approved);

    // Send interrupt signal
    bool send_interrupt();

    // Send model switch request
    bool send_model_switch(const std::string& model_id);

    // Send raw command (for advanced usage)
    bool send_command(const Command& cmd);

    // ========================================================================
    // Status & Diagnostics
    // ========================================================================

    // Get last error message
    std::string get_last_error() const;

    // Get number of restart attempts
    int get_restart_count() const;

    // Get process ID (for STDIN_STDOUT mode)
    int get_process_id() const;

private:
    // Implementation details hidden in .cpp
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace Qwen

#endif // _VFSSHELL_QWEN_CLIENT_H_
