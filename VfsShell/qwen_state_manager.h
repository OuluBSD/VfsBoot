#ifndef _VFSSHELL_QWEN_STATE_MANAGER_H_
#define _VFSSHELL_QWEN_STATE_MANAGER_H_

#include <string>
#include <vector>
#include <optional>
#include <chrono>

// Forward declare Vfs to avoid circular dependency
struct Vfs;

#include "qwen_protocol.h"

namespace Qwen {

// ============================================================================
// Session Information
// ============================================================================

struct SessionInfo {
    std::string session_id;            // Unique session ID (UUID or timestamp-based)
    std::string created_at;            // ISO8601 timestamp
    std::string last_modified;         // ISO8601 timestamp
    std::string model;                 // Model name used in this session
    int message_count;                 // Number of messages in conversation
    std::string workspace_root;        // Workspace root path
    std::vector<std::string> tags;     // User-defined tags for organization
};

// ============================================================================
// State Manager Configuration
// ============================================================================

struct StateManagerConfig {
    // VFS paths for storage
    std::string history_root = "/qwen/history";   // Conversation history storage
    std::string files_root = "/qwen/files";       // File storage for qwen context
    std::string sessions_root = "/qwen/sessions"; // Session metadata storage

    // Auto-save configuration
    bool auto_save = true;               // Automatically save after each message
    int save_interval_seconds = 30;      // Save interval for dirty sessions

    // History management
    int max_history_per_session = 1000;  // Max messages to keep per session
    int max_sessions = 100;              // Max sessions to keep (LRU eviction)

    // File management
    bool track_file_versions = true;     // Keep file version history
    int max_file_versions = 10;          // Max versions per file
};

// ============================================================================
// Qwen State Manager
// ============================================================================

class QwenStateManager {
public:
    explicit QwenStateManager(Vfs* vfs, const StateManagerConfig& config = StateManagerConfig{});
    ~QwenStateManager();

    // Prevent copying
    QwenStateManager(const QwenStateManager&) = delete;
    QwenStateManager& operator=(const QwenStateManager&) = delete;

    // ========================================================================
    // Session Management
    // ========================================================================

    // Create a new session
    // Returns session ID on success, empty string on failure
    std::string create_session(const std::string& model = "", const std::string& workspace_root = "");

    // Load an existing session (makes it the active session)
    // Returns true on success
    bool load_session(const std::string& session_id);

    // Save the current session
    // Returns true on success
    bool save_session();

    // Save a specific session
    bool save_session(const std::string& session_id);

    // Delete a session (removes all data)
    bool delete_session(const std::string& session_id);

    // List all available sessions
    std::vector<SessionInfo> list_sessions() const;

    // Get info about a specific session
    std::optional<SessionInfo> get_session_info(const std::string& session_id) const;

    // Get current active session ID
    std::string get_current_session() const;

    // Check if a session exists
    bool session_exists(const std::string& session_id) const;

    // Update model for current session
    bool set_session_model(const std::string& model);

    // ========================================================================
    // Conversation History Management
    // ========================================================================

    // Add a message to current session's history
    bool add_message(const ConversationMessage& msg);

    // Add multiple messages (bulk import)
    bool add_messages(const std::vector<ConversationMessage>& messages);

    // Get conversation history for current session
    std::vector<ConversationMessage> get_history() const;

    // Get conversation history for specific session
    std::vector<ConversationMessage> get_history(const std::string& session_id) const;

    // Get last N messages from current session
    std::vector<ConversationMessage> get_recent_messages(int count) const;

    // Clear history for current session
    bool clear_history();

    // Clear history for specific session
    bool clear_history(const std::string& session_id);

    // Get message count for current session
    int get_message_count() const;

    // ========================================================================
    // Tool Group Management
    // ========================================================================

    // Add a tool group to current session
    bool add_tool_group(const ToolGroup& group);

    // Get tool groups for current session
    std::vector<ToolGroup> get_tool_groups() const;

    // Get tool groups for specific session
    std::vector<ToolGroup> get_tool_groups(const std::string& session_id) const;

    // Get a specific tool group by ID
    std::optional<ToolGroup> get_tool_group(int group_id) const;

    // Update tool status (for tracking approvals/completions)
    bool update_tool_status(int group_id, const std::string& tool_id, ToolStatus status);

    // ========================================================================
    // File Storage Management
    // ========================================================================

    // Store a file in VFS under /qwen/files/<session_id>/
    // Returns VFS path on success
    std::string store_file(const std::string& filename, const std::string& content);

    // Store a file for specific session
    std::string store_file(const std::string& session_id, const std::string& filename,
                          const std::string& content);

    // Retrieve a file from storage
    std::optional<std::string> retrieve_file(const std::string& filename) const;

    // Retrieve a file from specific session
    std::optional<std::string> retrieve_file(const std::string& session_id,
                                            const std::string& filename) const;

    // List files for current session
    std::vector<std::string> list_files() const;

    // List files for specific session
    std::vector<std::string> list_files(const std::string& session_id) const;

    // Delete a file from storage
    bool delete_file(const std::string& filename);

    // Delete a file from specific session
    bool delete_file(const std::string& session_id, const std::string& filename);

    // ========================================================================
    // Session Metadata Management
    // ========================================================================

    // Set workspace root for current session
    bool set_workspace_root(const std::string& path);

    // Get workspace root for current session
    std::string get_workspace_root() const;

    // Set model for current session
    bool set_model(const std::string& model);

    // Get model for current session
    std::string get_model() const;

    // Add a tag to current session
    bool add_session_tag(const std::string& tag);

    // Remove a tag from current session
    bool remove_session_tag(const std::string& tag);

    // Get tags for current session
    std::vector<std::string> get_session_tags() const;

    // ========================================================================
    // Persistence & Maintenance
    // ========================================================================

    // Force immediate save of all dirty sessions
    bool flush_all();

    // Export session to JSON file
    bool export_session(const std::string& session_id, const std::string& output_path);

    // Import session from JSON file
    std::string import_session(const std::string& input_path);

    // Clean up old sessions (LRU eviction based on last_modified)
    int cleanup_old_sessions(int keep_count = 50);

    // Get statistics about storage usage
    struct StorageStats {
        int total_sessions;
        int total_messages;
        int total_files;
        size_t total_bytes;
    };
    StorageStats get_storage_stats() const;

    // ========================================================================
    // VFS Path Helpers
    // ========================================================================

    // Get VFS path for session directory
    std::string get_session_path(const std::string& session_id) const;

    // Get VFS path for history file
    std::string get_history_path(const std::string& session_id) const;

    // Get VFS path for session metadata file
    std::string get_metadata_path(const std::string& session_id) const;

    // Get VFS path for tool groups file
    std::string get_tool_groups_path(const std::string& session_id) const;

    // Get VFS path for file storage directory
    std::string get_files_path(const std::string& session_id) const;

private:
    // VFS instance (not owned)
    Vfs* vfs_;

    // Configuration
    StateManagerConfig config_;

    // Current active session ID
    std::string current_session_id_;

    // Dirty flag for auto-save
    bool session_dirty_;

    // Internal helpers
    std::string generate_session_id() const;
    std::string get_current_timestamp() const;
    bool ensure_directories();
    bool ensure_session_directories(const std::string& session_id);

    // Serialization helpers
    std::string serialize_message(const ConversationMessage& msg) const;
    std::optional<ConversationMessage> deserialize_message(const std::string& json) const;
    std::string serialize_tool_group(const ToolGroup& group) const;
    std::optional<ToolGroup> deserialize_tool_group(const std::string& json) const;
    std::string serialize_session_info(const SessionInfo& info) const;
    std::optional<SessionInfo> deserialize_session_info(const std::string& json) const;

    // File I/O helpers
    bool write_json_file(const std::string& vfs_path, const std::string& json);
    std::optional<std::string> read_json_file(const std::string& vfs_path) const;
    bool append_to_file(const std::string& vfs_path, const std::string& line);
    std::vector<std::string> read_lines(const std::string& vfs_path) const;
};

} // namespace Qwen

#endif // _VFSSHELL_QWEN_STATE_MANAGER_H_
