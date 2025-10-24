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

// Structure to represent a Repository in ACCOUNTS.json
struct RepositoryConfig {
    std::string id;
    std::string url;
    std::string local_path;
    bool enabled = true;
    std::string worker_model = "qwen-auth";
    std::string manager_model = "qwen-openai";
};

// Structure to represent an Account in ACCOUNTS.json
struct AccountConfig {
    std::string id;
    std::string hostname;
    bool enabled = true;
    int max_concurrent_repos = 3;
    std::vector<RepositoryConfig> repositories;
};

// Session state for tracking workflow status
enum class SessionState {
    AUTOMATIC,    // Automatic mode (default)
    MANUAL,       // Manual override mode
    TESTING,      // Running tests
    BLOCKED,      // Blocked waiting for resolution
    IDLE          // Idle state
};

// Structure to represent a session snapshot
struct SessionSnapshot {
    std::string snapshot_id;
    std::string session_id;
    std::string name;
    std::string model;
    std::string repo_path;
    std::vector<std::pair<std::string, std::string>> conversation_history;  // role, content
    std::vector<std::string> tool_history;  // Tool execution history
    time_t created_at;
    time_t last_restored;
};

// Structure to represent a session group
struct SessionGroup {
    std::string group_id;
    std::string name;
    std::string description;
    std::vector<std::string> session_ids;  // List of session IDs in this group
    time_t created_at;
    time_t last_updated;
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
    std::string instructions;  // AI instructions for this session
    std::string account_id;    // Associated account ID (for ACCOUNT and REPO sessions)
    std::vector<std::string> group_ids;  // List of groups this session belongs to
    SessionState workflow_state = SessionState::AUTOMATIC;  // Current workflow state
    int failure_count = 0;      // Count of consecutive failures for escalation
    int commit_count = 0;       // Count of commits since last review
    int test_count = 0;         // Count of test runs
    time_t created_at;
    time_t last_activity;
    bool is_active;
    bool is_paused = false;     // Whether the session is paused
    time_t paused_at = 0;       // When the session was paused (0 if not paused)
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
    
    // File I/O
    std::string load_instructions_from_file(const std::string& filename);
    
    // ACCOUNTS.json management
    bool load_accounts_config();
    bool validate_accounts_config();
    void parse_accounts_json(const std::string& json_content);
    AccountConfig parse_account_object(const std::string& account_json);
    RepositoryConfig parse_repository_object(const std::string& repo_json);
    std::string extract_json_field(const std::string& json_str, const std::string& field_name);
    bool validate_account_config(const AccountConfig& account);
    bool validate_repository_config(const RepositoryConfig& repo);
    
    // ACCOUNT and repository management
    bool spawn_repo_sessions_for_account(const std::string& account_id);
    bool enforce_concurrent_repo_limit(const std::string& account_id);
    
    // Workflow and escalation management
    void track_worker_failure(const std::string& session_id);
    void reset_failure_count(const std::string& session_id);
    void increment_commit_count(const std::string& session_id);
    void update_session_state(const std::string& session_id, SessionState new_state);
    bool is_manual_override(const std::string& session_id);
    
    // Session management
    bool pause_session(const std::string& session_id);
    bool resume_session(const std::string& session_id);
    bool is_session_paused(const std::string& session_id) const;
    SessionInfo* find_session_by_repo(const std::string& account_id, const std::string& repo_id);
    
    // Session grouping
    std::string create_session_group(const std::string& name, const std::string& description);
    bool delete_session_group(const std::string& group_id);
    bool add_session_to_group(const std::string& session_id, const std::string& group_id);
    bool remove_session_from_group(const std::string& session_id, const std::string& group_id);
    std::vector<SessionGroup> list_session_groups() const;
    std::vector<SessionInfo*> get_sessions_in_group(const std::string& group_id);
    
    // Session snapshots
    bool save_session_snapshot(const std::string& session_id, const std::string& snapshot_name);
    bool restore_session_snapshot(const std::string& session_id, const std::string& snapshot_name);
    bool delete_session_snapshot(const std::string& session_id, const std::string& snapshot_name);
    std::vector<std::string> list_session_snapshots(const std::string& session_id);
    
    // JSON-to-prompt conversion
    std::string convert_json_to_prompt(const std::string& json_task_spec);
    
    // File watching
    void start_accounts_json_watcher();
    void stop_accounts_json_watcher();
    void accounts_json_watcher_thread();
    
    // Documentation generation
    void generate_vfsboot_doc();
    
    // TCP server management
    bool start_tcp_server();
    void stop_tcp_server();
    
    // Session registry
    std::vector<SessionInfo> sessions_;
    std::mutex sessions_mutex_;
    
    // Session groups
    std::vector<SessionGroup> session_groups_;
    std::mutex groups_mutex_;
    
    // Session snapshots
    std::map<std::string, std::vector<SessionSnapshot>> session_snapshots_;  // session_id -> list of snapshots
    std::mutex snapshots_mutex_;
    
    // Account configurations
    std::vector<AccountConfig> account_configs_;
    std::mutex account_configs_mutex_;
    
    // TCP server
    std::unique_ptr<QwenTCPServer> tcp_server_;
    std::string tcp_host_;
    int tcp_port_;
    
    // Manager state
    Vfs* vfs_;
    QwenManagerConfig config_;
    std::atomic<bool> running_{false};
    std::atomic<bool> accounts_watcher_running_{false};
    std::thread accounts_watcher_thread_;
    std::condition_variable stop_cv_;
    std::mutex stop_mutex_;
    std::mutex watcher_mutex_;
};

} // namespace Qwen

#endif // _VfsShell_qwen_manager_h_