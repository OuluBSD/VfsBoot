#ifndef _VfsShell_qwen_manager_h_
#define _VfsShell_qwen_manager_h_

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

// Forward declaration
struct Vfs;

namespace Qwen {

// Enum for different session types in manager mode
enum class SessionType {
    MANAGER_PROJECT,   // qwen-openai, ID=mgr-project
    MANAGER_TASK,      // qwen-auth, ID=mgr-task
    ACCOUNT,           // Remote account connection
    REPO_MANAGER,      // qwen-openai for repository
    REPO_WORKER        // qwen-auth for repository
};

// Structure to represent a session in the manager
struct SessionInfo {
    std::string session_id;
    SessionType type;
    std::string hostname;
    std::string repo_path;
    std::string status;
    std::string model;
    std::string connection_info;
    time_t created_at;
    time_t last_activity;
    bool is_active;
};

// Manager mode configuration
struct QwenManagerConfig {
    int tcp_port = 7778;              // Default port for incoming connections
    std::string tcp_host = "0.0.0.0"; // Listen on all interfaces
    bool auto_approve_tools = false;
    bool use_colors = true;
    std::string workspace_root;
    std::string management_repo_path;
};

// Forward declaration
class QwenTCPServer;

// QwenManager class - manages multiple qwen sessions and TCP connections
class QwenManager {
public:
    explicit QwenManager(Vfs* vfs);
    ~QwenManager();

    // Initialize manager mode
    bool initialize(const QwenManagerConfig& config);
    
    // Start manager mode with ncurses UI
    bool run_ncurses_mode();

    // Start manager mode in simple mode (stdio)
    bool run_simple_mode();

    // Check if manager is running
    bool is_running() const { return running_; }
    
    // Stop the manager
    void stop();

private:
    // Session management
    void generate_session_id(std::string& session_id);
    SessionInfo* find_session(const std::string& session_id);
    const SessionInfo* find_session(const std::string& session_id) const;
    
    // TCP server management
    bool start_tcp_server();
    void stop_tcp_server();
    
    // Session registry
    std::vector<SessionInfo> sessions_;
    std::mutex sessions_mutex_;
    
    // TCP server
    std::unique_ptr<QwenTCPServer> tcp_server_;
    std::string tcp_host_;
    int tcp_port_;
    
    // Manager state
    Vfs* vfs_;
    QwenManagerConfig config_;
    std::atomic<bool> running_{false};
    std::condition_variable stop_cv_;
    std::mutex stop_mutex_;
};

} // namespace Qwen

#endif // _VfsShell_qwen_manager_h_