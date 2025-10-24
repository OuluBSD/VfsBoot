#include "VfsShell.h"

namespace Qwen {

// ============================================================================
// JSON Serialization Helpers
// ============================================================================

// Escape JSON string
static std::string json_escape(const std::string& str) {
    std::ostringstream oss;
    for (char c : str) {
        switch (c) {
            case '\\': oss << "\\\\"; break;
            case '"': oss << "\\\""; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:
                if (c < 32) {
                    oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                } else {
                    oss << c;
                }
        }
    }
    return oss.str();
}

// Unescape JSON string
static std::string json_unescape(const std::string& str) {
    std::ostringstream oss;
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '\\' && i + 1 < str.size()) {
            switch (str[i + 1]) {
                case '\\': oss << '\\'; i++; break;
                case '"': oss << '"'; i++; break;
                case 'n': oss << '\n'; i++; break;
                case 'r': oss << '\r'; i++; break;
                case 't': oss << '\t'; i++; break;
                default: oss << str[i];
            }
        } else {
            oss << str[i];
        }
    }
    return oss.str();
}

// Simple JSON key-value extraction
static std::string extract_json_string(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";

    pos += search.length();
    size_t end = json.find("\"", pos);
    if (end == std::string::npos) return "";

    return json_unescape(json.substr(pos, end - pos));
}

static int extract_json_int(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return 0;

    pos += search.length();
    while (pos < json.size() && std::isspace(json[pos])) pos++;

    int result = 0;
    int sign = 1;
    if (json[pos] == '-') {
        sign = -1;
        pos++;
    }

    while (pos < json.size() && std::isdigit(json[pos])) {
        result = result * 10 + (json[pos] - '0');
        pos++;
    }

    return result * sign;
}

static std::vector<std::string> extract_json_array(const std::string& json, const std::string& key) {
    std::vector<std::string> result;
    std::string search = "\"" + key + "\":[";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return result;

    pos += search.length();
    size_t end = json.find("]", pos);
    if (end == std::string::npos) return result;

    std::string array_content = json.substr(pos, end - pos);
    size_t item_start = 0;

    while (true) {
        size_t quote_start = array_content.find("\"", item_start);
        if (quote_start == std::string::npos) break;

        size_t quote_end = array_content.find("\"", quote_start + 1);
        if (quote_end == std::string::npos) break;

        result.push_back(json_unescape(array_content.substr(quote_start + 1, quote_end - quote_start - 1)));
        item_start = quote_end + 1;
    }

    return result;
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

QwenStateManager::QwenStateManager(Vfs* vfs, const StateManagerConfig& config)
    : vfs_(vfs), config_(config), session_dirty_(false) {
    if (!vfs_) {
        throw std::runtime_error("QwenStateManager: VFS pointer is null");
    }
    ensure_directories();
}

QwenStateManager::~QwenStateManager() {
    if (session_dirty_) {
        save_session();
    }
}

// ============================================================================
// Session Management
// ============================================================================

std::string QwenStateManager::create_session(const std::string& model, const std::string& workspace_root) {
    std::string session_id = generate_session_id();

    // Create session directories
    if (!ensure_session_directories(session_id)) {
        return "";
    }

    // Create session metadata
    SessionInfo info;
    info.session_id = session_id;
    info.created_at = get_current_timestamp();
    info.last_modified = info.created_at;
    info.model = model.empty() ? "qwen2.5-coder-7b" : model;
    info.message_count = 0;
    info.workspace_root = workspace_root;

    // Write metadata
    std::string metadata_json = serialize_session_info(info);
    if (!write_json_file(get_metadata_path(session_id), metadata_json)) {
        return "";
    }

    // Set as current session
    current_session_id_ = session_id;
    session_dirty_ = false;

    return session_id;
}

bool QwenStateManager::load_session(const std::string& session_id) {
    if (!session_exists(session_id)) {
        return false;
    }

    // Save current session if dirty
    if (session_dirty_ && !current_session_id_.empty()) {
        save_session();
    }

    // Set new current session
    current_session_id_ = session_id;
    session_dirty_ = false;

    return true;
}

bool QwenStateManager::save_session() {
    if (current_session_id_.empty()) {
        return false;
    }
    return save_session(current_session_id_);
}

bool QwenStateManager::save_session(const std::string& session_id) {
    if (!session_exists(session_id)) {
        return false;
    }

    // Update last_modified timestamp in metadata
    auto info_opt = get_session_info(session_id);
    if (!info_opt) return false;

    SessionInfo info = *info_opt;
    info.last_modified = get_current_timestamp();

    std::string metadata_json = serialize_session_info(info);
    bool result = write_json_file(get_metadata_path(session_id), metadata_json);

    if (result && session_id == current_session_id_) {
        session_dirty_ = false;
    }

    return result;
}

bool QwenStateManager::delete_session(const std::string& session_id) {
    if (!session_exists(session_id)) {
        return false;
    }

    // Remove session directory from VFS
    std::string session_path = get_session_path(session_id);
    try {
        vfs_->rm(session_path, 0);

        // If this was the current session, clear it
        if (current_session_id_ == session_id) {
            current_session_id_.clear();
            session_dirty_ = false;
        }

        return true;
    } catch (...) {
        return false;
    }
}

std::vector<SessionInfo> QwenStateManager::list_sessions() const {
    std::vector<SessionInfo> sessions;

    try {
        auto sessions_node = vfs_->resolve(config_.sessions_root);
        if (!sessions_node || !sessions_node->isDir()) {
            return sessions;
        }

        auto& children = sessions_node->children();
        for (const auto& [name, node] : children) {
            if (node->isDir()) {
                auto info = get_session_info(name);
                if (info) {
                    sessions.push_back(*info);
                }
            }
        }
    } catch (...) {
        // Return empty vector on error
    }

    return sessions;
}

std::optional<SessionInfo> QwenStateManager::get_session_info(const std::string& session_id) const {
    if (!session_exists(session_id)) {
        return std::nullopt;
    }

    std::string metadata_path = get_metadata_path(session_id);
    auto json_opt = read_json_file(metadata_path);
    if (!json_opt) {
        return std::nullopt;
    }

    return deserialize_session_info(*json_opt);
}

std::string QwenStateManager::get_current_session() const {
    return current_session_id_;
}

bool QwenStateManager::session_exists(const std::string& session_id) const {
    std::string session_path = get_session_path(session_id);
    auto node = vfs_->resolve(session_path);
    return node && node->isDir();
}

bool QwenStateManager::set_session_model(const std::string& model) {
    if (current_session_id_.empty()) {
        return false;
    }

    // Load current session info
    auto info_opt = get_session_info(current_session_id_);
    if (!info_opt) {
        return false;
    }

    // Update model
    SessionInfo info = *info_opt;
    info.model = model;
    info.last_modified = get_current_timestamp();

    // Write updated metadata
    std::string metadata_json = serialize_session_info(info);
    return write_json_file(get_metadata_path(current_session_id_), metadata_json);
}

// ============================================================================
// Conversation History Management
// ============================================================================

bool QwenStateManager::add_message(const ConversationMessage& msg) {
    if (current_session_id_.empty()) {
        return false;
    }

    std::string history_path = get_history_path(current_session_id_);
    std::string msg_json = serialize_message(msg);

    bool result = append_to_file(history_path, msg_json);
    if (result) {
        session_dirty_ = true;

        // Update message count in metadata
        auto info_opt = get_session_info(current_session_id_);
        if (info_opt) {
            SessionInfo info = *info_opt;
            info.message_count++;
            write_json_file(get_metadata_path(current_session_id_), serialize_session_info(info));
        }
    }

    return result;
}

bool QwenStateManager::add_messages(const std::vector<ConversationMessage>& messages) {
    for (const auto& msg : messages) {
        if (!add_message(msg)) {
            return false;
        }
    }
    return true;
}

std::vector<ConversationMessage> QwenStateManager::get_history() const {
    if (current_session_id_.empty()) {
        return {};
    }
    return get_history(current_session_id_);
}

std::vector<ConversationMessage> QwenStateManager::get_history(const std::string& session_id) const {
    std::vector<ConversationMessage> messages;

    std::string history_path = get_history_path(session_id);
    auto lines = read_lines(history_path);

    for (const auto& line : lines) {
        auto msg_opt = deserialize_message(line);
        if (msg_opt) {
            messages.push_back(*msg_opt);
        }
    }

    return messages;
}

std::vector<ConversationMessage> QwenStateManager::get_recent_messages(int count) const {
    auto all_messages = get_history();

    if (all_messages.size() <= static_cast<size_t>(count)) {
        return all_messages;
    }

    return std::vector<ConversationMessage>(
        all_messages.end() - count,
        all_messages.end()
    );
}

bool QwenStateManager::clear_history() {
    if (current_session_id_.empty()) {
        return false;
    }
    return clear_history(current_session_id_);
}

bool QwenStateManager::clear_history(const std::string& session_id) {
    std::string history_path = get_history_path(session_id);
    return write_json_file(history_path, "");
}

int QwenStateManager::get_message_count() const {
    if (current_session_id_.empty()) {
        return 0;
    }

    auto info_opt = get_session_info(current_session_id_);
    return info_opt ? info_opt->message_count : 0;
}

// ============================================================================
// Tool Group Management
// ============================================================================

bool QwenStateManager::add_tool_group(const ToolGroup& group) {
    if (current_session_id_.empty()) {
        return false;
    }

    std::string tools_path = get_tool_groups_path(current_session_id_);
    std::string group_json = serialize_tool_group(group);

    bool result = append_to_file(tools_path, group_json);
    if (result) {
        session_dirty_ = true;
    }

    return result;
}

std::vector<ToolGroup> QwenStateManager::get_tool_groups() const {
    if (current_session_id_.empty()) {
        return {};
    }
    return get_tool_groups(current_session_id_);
}

std::vector<ToolGroup> QwenStateManager::get_tool_groups(const std::string& session_id) const {
    std::vector<ToolGroup> groups;

    std::string tools_path = get_tool_groups_path(session_id);
    auto lines = read_lines(tools_path);

    for (const auto& line : lines) {
        auto group_opt = deserialize_tool_group(line);
        if (group_opt) {
            groups.push_back(*group_opt);
        }
    }

    return groups;
}

std::optional<ToolGroup> QwenStateManager::get_tool_group(int group_id) const {
    auto groups = get_tool_groups();

    for (const auto& group : groups) {
        if (group.id == group_id) {
            return group;
        }
    }

    return std::nullopt;
}

bool QwenStateManager::update_tool_status(int group_id, const std::string& tool_id, ToolStatus status) {
    // Read all tool groups
    auto groups = get_tool_groups();

    // Find and update the specific tool
    bool found = false;
    for (auto& group : groups) {
        if (group.id == group_id) {
            for (auto& tool : group.tools) {
                if (tool.tool_id == tool_id) {
                    tool.status = status;
                    found = true;
                    break;
                }
            }
            if (found) break;
        }
    }

    if (!found) return false;

    // Rewrite all tool groups
    std::string tools_path = get_tool_groups_path(current_session_id_);
    write_json_file(tools_path, ""); // Clear file

    for (const auto& group : groups) {
        append_to_file(tools_path, serialize_tool_group(group));
    }

    session_dirty_ = true;
    return true;
}

// ============================================================================
// File Storage Management
// ============================================================================

std::string QwenStateManager::store_file(const std::string& filename, const std::string& content) {
    if (current_session_id_.empty()) {
        return "";
    }
    return store_file(current_session_id_, filename, content);
}

std::string QwenStateManager::store_file(const std::string& session_id, const std::string& filename,
                                        const std::string& content) {
    std::string files_dir = get_files_path(session_id);
    std::string file_path = files_dir + "/" + filename;

    try {
        vfs_->write(file_path, content, 0);
        session_dirty_ = true;
        return file_path;
    } catch (...) {
        return "";
    }
}

std::optional<std::string> QwenStateManager::retrieve_file(const std::string& filename) const {
    if (current_session_id_.empty()) {
        return std::nullopt;
    }
    return retrieve_file(current_session_id_, filename);
}

std::optional<std::string> QwenStateManager::retrieve_file(const std::string& session_id,
                                                           const std::string& filename) const {
    std::string file_path = get_files_path(session_id) + "/" + filename;

    try {
        std::string content = vfs_->read(file_path, std::nullopt);
        return content;
    } catch (...) {
        return std::nullopt;
    }
}

std::vector<std::string> QwenStateManager::list_files() const {
    if (current_session_id_.empty()) {
        return {};
    }
    return list_files(current_session_id_);
}

std::vector<std::string> QwenStateManager::list_files(const std::string& session_id) const {
    std::vector<std::string> files;

    try {
        std::string files_dir = get_files_path(session_id);
        auto files_node = vfs_->resolve(files_dir);
        if (!files_node || !files_node->isDir()) {
            return files;
        }

        auto& children = files_node->children();
        for (const auto& [name, node] : children) {
            if (!node->isDir()) {
                files.push_back(name);
            }
        }
    } catch (...) {
        // Return empty vector on error
    }

    return files;
}

bool QwenStateManager::delete_file(const std::string& filename) {
    if (current_session_id_.empty()) {
        return false;
    }
    return delete_file(current_session_id_, filename);
}

bool QwenStateManager::delete_file(const std::string& session_id, const std::string& filename) {
    std::string file_path = get_files_path(session_id) + "/" + filename;

    try {
        vfs_->rm(file_path, 0);
        session_dirty_ = true;
        return true;
    } catch (...) {
        return false;
    }
}

// ============================================================================
// Session Metadata Management
// ============================================================================

bool QwenStateManager::set_workspace_root(const std::string& path) {
    if (current_session_id_.empty()) {
        return false;
    }

    auto info_opt = get_session_info(current_session_id_);
    if (!info_opt) return false;

    SessionInfo info = *info_opt;
    info.workspace_root = path;
    info.last_modified = get_current_timestamp();

    session_dirty_ = true;
    return write_json_file(get_metadata_path(current_session_id_), serialize_session_info(info));
}

std::string QwenStateManager::get_workspace_root() const {
    if (current_session_id_.empty()) {
        return "";
    }

    auto info_opt = get_session_info(current_session_id_);
    return info_opt ? info_opt->workspace_root : "";
}

bool QwenStateManager::set_model(const std::string& model) {
    if (current_session_id_.empty()) {
        return false;
    }

    auto info_opt = get_session_info(current_session_id_);
    if (!info_opt) return false;

    SessionInfo info = *info_opt;
    info.model = model;
    info.last_modified = get_current_timestamp();

    session_dirty_ = true;
    return write_json_file(get_metadata_path(current_session_id_), serialize_session_info(info));
}

std::string QwenStateManager::get_model() const {
    if (current_session_id_.empty()) {
        return "";
    }

    auto info_opt = get_session_info(current_session_id_);
    return info_opt ? info_opt->model : "";
}

bool QwenStateManager::add_session_tag(const std::string& tag) {
    if (current_session_id_.empty()) {
        return false;
    }

    auto info_opt = get_session_info(current_session_id_);
    if (!info_opt) return false;

    SessionInfo info = *info_opt;

    // Check if tag already exists
    if (std::find(info.tags.begin(), info.tags.end(), tag) != info.tags.end()) {
        return true; // Already exists
    }

    info.tags.push_back(tag);
    info.last_modified = get_current_timestamp();

    session_dirty_ = true;
    return write_json_file(get_metadata_path(current_session_id_), serialize_session_info(info));
}

bool QwenStateManager::remove_session_tag(const std::string& tag) {
    if (current_session_id_.empty()) {
        return false;
    }

    auto info_opt = get_session_info(current_session_id_);
    if (!info_opt) return false;

    SessionInfo info = *info_opt;

    auto it = std::find(info.tags.begin(), info.tags.end(), tag);
    if (it == info.tags.end()) {
        return false; // Tag not found
    }

    info.tags.erase(it);
    info.last_modified = get_current_timestamp();

    session_dirty_ = true;
    return write_json_file(get_metadata_path(current_session_id_), serialize_session_info(info));
}

std::vector<std::string> QwenStateManager::get_session_tags() const {
    if (current_session_id_.empty()) {
        return {};
    }

    auto info_opt = get_session_info(current_session_id_);
    return info_opt ? info_opt->tags : std::vector<std::string>{};
}

// ============================================================================
// Persistence & Maintenance
// ============================================================================

bool QwenStateManager::flush_all() {
    if (!current_session_id_.empty() && session_dirty_) {
        return save_session();
    }
    return true;
}

bool QwenStateManager::export_session(const std::string& session_id, const std::string& output_path) {
    // Read all session data
    auto info_opt = get_session_info(session_id);
    if (!info_opt) return false;

    auto history = get_history(session_id);
    auto tool_groups = get_tool_groups(session_id);
    auto files = list_files(session_id);

    // Build JSON export
    std::ostringstream json;
    json << "{\n";
    json << "  \"session_info\": " << serialize_session_info(*info_opt) << ",\n";
    json << "  \"history\": [\n";
    for (size_t i = 0; i < history.size(); ++i) {
        json << "    " << serialize_message(history[i]);
        if (i < history.size() - 1) json << ",";
        json << "\n";
    }
    json << "  ],\n";
    json << "  \"tool_groups\": [\n";
    for (size_t i = 0; i < tool_groups.size(); ++i) {
        json << "    " << serialize_tool_group(tool_groups[i]);
        if (i < tool_groups.size() - 1) json << ",";
        json << "\n";
    }
    json << "  ],\n";
    json << "  \"files\": [\n";
    for (size_t i = 0; i < files.size(); ++i) {
        auto content_opt = retrieve_file(session_id, files[i]);
        json << "    {\"name\":\"" << json_escape(files[i]) << "\",\"content\":\"";
        json << json_escape(content_opt.value_or("")) << "\"}";
        if (i < files.size() - 1) json << ",";
        json << "\n";
    }
    json << "  ]\n";
    json << "}\n";

    return write_json_file(output_path, json.str());
}

std::string QwenStateManager::import_session(const std::string& input_path) {
    // Read JSON file
    auto json_opt = read_json_file(input_path);
    if (!json_opt) return "";

    // Extract session info
    std::string model = extract_json_string(*json_opt, "model");
    std::string workspace = extract_json_string(*json_opt, "workspace_root");

    // Create new session
    std::string session_id = create_session(model, workspace);
    if (session_id.empty()) return "";

    // TODO: Parse and import history, tool_groups, and files
    // This would require more complex JSON parsing

    return session_id;
}

int QwenStateManager::cleanup_old_sessions(int keep_count) {
    auto sessions = list_sessions();

    if (sessions.size() <= static_cast<size_t>(keep_count)) {
        return 0;
    }

    // Sort by last_modified (oldest first)
    std::sort(sessions.begin(), sessions.end(), [](const SessionInfo& a, const SessionInfo& b) {
        return a.last_modified < b.last_modified;
    });

    // Delete oldest sessions
    int deleted = 0;
    size_t to_delete = sessions.size() - keep_count;

    for (size_t i = 0; i < to_delete; ++i) {
        if (delete_session(sessions[i].session_id)) {
            deleted++;
        }
    }

    return deleted;
}

QwenStateManager::StorageStats QwenStateManager::get_storage_stats() const {
    StorageStats stats{0, 0, 0, 0};

    auto sessions = list_sessions();
    stats.total_sessions = static_cast<int>(sessions.size());

    for (const auto& session : sessions) {
        auto history = get_history(session.session_id);
        stats.total_messages += static_cast<int>(history.size());

        auto files = list_files(session.session_id);
        stats.total_files += static_cast<int>(files.size());

        // Estimate storage size
        for (const auto& msg : history) {
            stats.total_bytes += msg.content.size();
        }
        for (const auto& file : files) {
            auto content = retrieve_file(session.session_id, file);
            if (content) {
                stats.total_bytes += content->size();
            }
        }
    }

    return stats;
}

// ============================================================================
// VFS Path Helpers
// ============================================================================

std::string QwenStateManager::get_session_path(const std::string& session_id) const {
    return config_.sessions_root + "/" + session_id;
}

std::string QwenStateManager::get_history_path(const std::string& session_id) const {
    return get_session_path(session_id) + "/history.jsonl";
}

std::string QwenStateManager::get_metadata_path(const std::string& session_id) const {
    return get_session_path(session_id) + "/metadata.json";
}

std::string QwenStateManager::get_tool_groups_path(const std::string& session_id) const {
    return get_session_path(session_id) + "/tool_groups.jsonl";
}

std::string QwenStateManager::get_files_path(const std::string& session_id) const {
    return get_session_path(session_id) + "/files";
}

// ============================================================================
// Internal Helpers
// ============================================================================

std::string QwenStateManager::generate_session_id() const {
    // Generate UUID-like session ID using timestamp + random
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);

    std::ostringstream oss;
    oss << "session-" << timestamp << "-";
    for (int i = 0; i < 8; ++i) {
        oss << std::hex << dis(gen);
    }

    return oss.str();
}

std::string QwenStateManager::get_current_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);

    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t_now), "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

bool QwenStateManager::ensure_directories() {
    try {
        vfs_->mkdir(config_.history_root, 0);
        vfs_->mkdir(config_.files_root, 0);
        vfs_->mkdir(config_.sessions_root, 0);
        return true;
    } catch (...) {
        return false;
    }
}

bool QwenStateManager::ensure_session_directories(const std::string& session_id) {
    try {
        std::string session_path = get_session_path(session_id);
        vfs_->mkdir(session_path, 0);

        std::string files_path = get_files_path(session_id);
        vfs_->mkdir(files_path, 0);

        return true;
    } catch (...) {
        return false;
    }
}

// ============================================================================
// Serialization Helpers
// ============================================================================

std::string QwenStateManager::serialize_message(const ConversationMessage& msg) const {
    std::ostringstream json;
    json << "{";
    json << "\"role\":\"" << message_role_to_string(msg.role) << "\",";
    json << "\"content\":\"" << json_escape(msg.content) << "\",";
    json << "\"id\":" << msg.id;
    if (msg.timestamp) {
        json << ",\"timestamp\":" << *msg.timestamp;
    }
    json << "}";
    return json.str();
}

std::optional<ConversationMessage> QwenStateManager::deserialize_message(const std::string& json) const {
    ConversationMessage msg;

    msg.content = extract_json_string(json, "content");
    msg.id = extract_json_int(json, "id");

    std::string role_str = extract_json_string(json, "role");
    if (role_str == "user") msg.role = MessageRole::USER;
    else if (role_str == "assistant") msg.role = MessageRole::ASSISTANT;
    else if (role_str == "system") msg.role = MessageRole::SYSTEM;
    else return std::nullopt;

    // Timestamp is optional
    std::string ts_search = "\"timestamp\":";
    if (json.find(ts_search) != std::string::npos) {
        msg.timestamp = extract_json_int(json, "timestamp");
    }

    return msg;
}

std::string QwenStateManager::serialize_tool_group(const ToolGroup& group) const {
    std::ostringstream json;
    json << "{\"id\":" << group.id << ",\"tools\":[";

    for (size_t i = 0; i < group.tools.size(); ++i) {
        const auto& tool = group.tools[i];
        json << "{";
        json << "\"tool_id\":\"" << json_escape(tool.tool_id) << "\",";
        json << "\"tool_name\":\"" << json_escape(tool.tool_name) << "\",";
        json << "\"status\":\"" << tool_status_to_string(tool.status) << "\"";
        if (tool.result) {
            json << ",\"result\":\"" << json_escape(*tool.result) << "\"";
        }
        if (tool.error) {
            json << ",\"error\":\"" << json_escape(*tool.error) << "\"";
        }
        json << "}";
        if (i < group.tools.size() - 1) json << ",";
    }

    json << "]}";
    return json.str();
}

std::optional<ToolGroup> QwenStateManager::deserialize_tool_group(const std::string& json) const {
    ToolGroup group;
    group.id = extract_json_int(json, "id");

    // Simplified: just store the ID, full parsing would need more work
    return group;
}

std::string QwenStateManager::serialize_session_info(const SessionInfo& info) const {
    std::ostringstream json;
    json << "{\n";
    json << "  \"session_id\":\"" << json_escape(info.session_id) << "\",\n";
    json << "  \"created_at\":\"" << json_escape(info.created_at) << "\",\n";
    json << "  \"last_modified\":\"" << json_escape(info.last_modified) << "\",\n";
    json << "  \"model\":\"" << json_escape(info.model) << "\",\n";
    json << "  \"message_count\":" << info.message_count << ",\n";
    json << "  \"workspace_root\":\"" << json_escape(info.workspace_root) << "\",\n";
    json << "  \"tags\":[";
    for (size_t i = 0; i < info.tags.size(); ++i) {
        json << "\"" << json_escape(info.tags[i]) << "\"";
        if (i < info.tags.size() - 1) json << ",";
    }
    json << "]\n";
    json << "}";
    return json.str();
}

std::optional<SessionInfo> QwenStateManager::deserialize_session_info(const std::string& json) const {
    SessionInfo info;

    info.session_id = extract_json_string(json, "session_id");
    info.created_at = extract_json_string(json, "created_at");
    info.last_modified = extract_json_string(json, "last_modified");
    info.model = extract_json_string(json, "model");
    info.workspace_root = extract_json_string(json, "workspace_root");
    info.message_count = extract_json_int(json, "message_count");
    info.tags = extract_json_array(json, "tags");

    if (info.session_id.empty()) {
        return std::nullopt;
    }

    return info;
}

// ============================================================================
// File I/O Helpers
// ============================================================================

bool QwenStateManager::write_json_file(const std::string& vfs_path, const std::string& json) {
    try {
        vfs_->write(vfs_path, json, 0);
        return true;
    } catch (...) {
        return false;
    }
}

std::optional<std::string> QwenStateManager::read_json_file(const std::string& vfs_path) const {
    try {
        return vfs_->read(vfs_path, std::nullopt);
    } catch (...) {
        return std::nullopt;
    }
}

bool QwenStateManager::append_to_file(const std::string& vfs_path, const std::string& line) {
    try {
        // Read existing content
        std::string existing;
        try {
            existing = vfs_->read(vfs_path, std::nullopt);
        } catch (...) {
            existing = "";
        }

        // Append new line
        std::string new_content = existing;
        if (!new_content.empty() && new_content.back() != '\n') {
            new_content += '\n';
        }
        new_content += line;
        new_content += '\n';

        // Write back
        vfs_->write(vfs_path, new_content, 0);
        return true;
    } catch (...) {
        return false;
    }
}

std::vector<std::string> QwenStateManager::read_lines(const std::string& vfs_path) const {
    std::vector<std::string> lines;

    try {
        std::string content = vfs_->read(vfs_path, std::nullopt);
        std::istringstream iss(content);
        std::string line;

        while (std::getline(iss, line)) {
            if (!line.empty()) {
                lines.push_back(line);
            }
        }
    } catch (...) {
        // Return empty vector on error
    }

    return lines;
}

} // namespace Qwen
