#include "VfsShell.h"

namespace Qwen {

// Constructor
QwenManager::QwenManager(Vfs* vfs) : vfs_(vfs) {
}

// Destructor
QwenManager::~QwenManager() {
    stop();
}

// Initialize manager mode
bool QwenManager::initialize(const QwenManagerConfig& config) {
    config_ = config;
    
    // Generate special sessions for PROJECT MANAGER and TASK MANAGER
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        
        // Create PROJECT MANAGER session
        ManagerSessionInfo project_manager;
        project_manager.session_id = "mgr-project";
        project_manager.type = SessionType::MANAGER_PROJECT;
        project_manager.hostname = "local";
        project_manager.repo_path = config_.management_repo_path;
        project_manager.status = "active";
        project_manager.model = "qwen-openai";
        project_manager.created_at = time(nullptr);
        project_manager.last_activity = project_manager.created_at;
        project_manager.is_active = true;
        
        // Load PROJECT_MANAGER.md instructions if available
        project_manager.instructions = load_instructions_from_file("PROJECT_MANAGER.md");
        sessions_.push_back(project_manager);
        
        // Create TASK MANAGER session
        ManagerSessionInfo task_manager;
        task_manager.session_id = "mgr-task";
        task_manager.type = SessionType::MANAGER_TASK;
        task_manager.hostname = "local";
        task_manager.repo_path = config_.management_repo_path;
        task_manager.status = "active";
        task_manager.model = "qwen-auth";
        task_manager.created_at = time(nullptr);
        task_manager.last_activity = task_manager.created_at;
        task_manager.is_active = true;
        
        // Load TASK_MANAGER.md instructions if available
        task_manager.instructions = load_instructions_from_file("TASK_MANAGER.md");
        sessions_.push_back(task_manager);
    }
    
    // Load account configurations from ACCOUNTS.json
    if (!load_accounts_config()) {
        std::cout << "[QwenManager] Warning: Could not load ACCOUNTS.json, continuing with empty config\n";
    }
    
    // Start TCP server
    if (!start_tcp_server()) {
        return false;
    }
    
    // Start accounts.json watcher with a delay
    start_accounts_json_watcher();
    
    // Generate VFSBOOT.md documentation
    generate_vfsboot_doc();
    
    running_ = true;
    return true;
}

// Start TCP server
bool QwenManager::start_tcp_server() {
    tcp_host_ = config_.tcp_host;
    tcp_port_ = config_.tcp_port;
    
    // Initialize TCP server
    tcp_server_ = std::make_unique<QwenTCPServer>();
    
    // Set up callbacks
    tcp_server_->set_on_connect([this](int client_fd, const std::string& client_addr) {
        // Add to session registry
        {
            std::lock_guard<std::mutex> lock(sessions_mutex_);
            
            ManagerSessionInfo account_session;
            account_session.session_id = "acc-" + std::to_string(client_fd);
            account_session.type = SessionType::ACCOUNT;
            account_session.hostname = client_addr;
            account_session.repo_path = "";
            account_session.status = "connected";
            account_session.model = "unknown";
            account_session.connection_info = "tcp";
            account_session.created_at = time(nullptr);
            account_session.last_activity = account_session.created_at;
            account_session.is_active = true;
            
            sessions_.push_back(account_session);
        }
        
        std::cout << "[QwenManager] New account connection from " << client_addr << "\n";
    });
    
    tcp_server_->set_on_message([this](int client_fd, const std::string& message) {
        // Update last activity for this session
        {
            std::lock_guard<std::mutex> lock(sessions_mutex_);
            for (auto& session : sessions_) {
                if (session.connection_info == std::to_string(client_fd)) {
                    session.last_activity = time(nullptr);
                    break;
                }
            }
        }
        
        // For now, just log the received message
        std::cout << "[QwenManager] Received message from client " << client_fd << ": " 
                 << message << std::endl;
    });
    
    // Start the TCP server
    bool success = tcp_server_->start(tcp_host_, tcp_port_);
    
    if (success) {
        std::cout << "[QwenManager] TCP server listening on " << tcp_host_ << ":" << tcp_port_ << "\n";
    }
    
    return success;
}

// Stop the manager
void QwenManager::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    // Stop accounts json watcher
    stop_accounts_json_watcher();
    
    // Stop TCP server
    stop_tcp_server();
}

// Stop TCP server
void QwenManager::stop_tcp_server() {
    if (tcp_server_) {
        tcp_server_->stop();
    }
}

// Auto-generate VFSBOOT.md documentation
void QwenManager::generate_vfsboot_doc() {
    std::string content = R"(# VFSBOOT - qwen Manager Documentation

This document provides an overview of the qwen Manager Mode and its components.

## Overview

The qwen Manager Mode enables hierarchical multi-repository AI project management with the following components:

- **PROJECT MANAGER**: Expensive, high-quality AI for strategic decisions (qwen-openai)
- **TASK MANAGER**: Regular quality AI for task coordination (qwen-auth) 
- **ACCOUNTS**: Remote computers that connect to the manager
- **REPOSITORIES**: Individual project repositories managed by worker/manager pairs

## Configuration

The system is configured using `ACCOUNTS.json` which defines accounts, repositories, and their properties.

### ACCOUNTS.json Schema

The configuration file follows this schema:

```json
{
  "accounts": [
    {
      "id": "unique-account-identifier",
      "hostname": "computer-hostname-or-ip",
      "enabled": true,
      "max_concurrent_repos": 3,
      "repositories": [
        {
          "id": "unique-repo-identifier", 
          "url": "git-repository-url",
          "local_path": "/local/path/to/clone",
          "enabled": true,
          "worker_model": "qwen-auth",
          "manager_model": "qwen-openai"
        }
      ]
    }
  ]
}
```

For more details about the schema, see [docs/ACCOUNTS_JSON_SPEC.md](docs/ACCOUNTS_JSON_SPEC.md).

## Communication Protocol

The manager, accounts, and repositories communicate using a JSON-based protocol. For the detailed specification, see [docs/MANAGER_PROTOCOL.md](docs/MANAGER_PROTOCOL.md).

## AI Role Definitions

This system uses specialized AI roles with specific responsibilities:

- [PROJECT_MANAGER.md](PROJECT_MANAGER.md) - Instructions for PROJECT MANAGER AI (qwen-openai)
- [TASK_MANAGER.md](TASK_MANAGER.md) - Instructions for TASK MANAGER AI (qwen-auth)

## File Locations

- `ACCOUNTS.json` - Main configuration file for defining accounts and repositories
- `PROJECT_MANAGER.md` - AI instructions for project-level management
- `TASK_MANAGER.md` - AI instructions for task-level coordination  
- `docs/ACCOUNTS_JSON_SPEC.md` - Schema specification for configuration
- `docs/MANAGER_PROTOCOL.md` - Communication protocol specification

## Usage

To start the manager mode:

```bash
qwen --manager
# or
qwen -m
```

This will initialize the manager, load the account configurations, start the TCP server for account connections, and provide the UI for managing the multi-repository setup.
)";

    // Write the content to VFSBOOT.md in the VFS
    if (vfs_) {
        // Check if VFSBOOT.md already exists
        bool should_write = true;
        try {
            std::string existing = vfs_->read("VFSBOOT.md");
            if (existing == content) {
                should_write = false;
            }
        } catch (const std::runtime_error&) {
            // File doesn't exist, will create it
        }

        if (should_write) {
            vfs_->write("VFSBOOT.md", content);
            std::cout << "[QwenManager] VFSBOOT.md generated successfully\n";
        }
    }
    
    // Also try to write to the local filesystem as a fallback
    std::ofstream file("VFSBOOT.md");
    if (file.is_open()) {
        file << content;
        file.close();
        std::cout << "[QwenManager] VFSBOOT.md written to local filesystem\n";
    }
}

// Generate unique session ID
void QwenManager::generate_session_id(std::string& session_id) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    ss << "session-";
    
    for (int i = 0; i < 16; ++i) {
        ss << std::hex << dis(gen);
        if (i == 7 || i == 11) {
            ss << '-';
        }
    }
    
    session_id = ss.str();
}

// Find session by ID (non-const)
ManagerSessionInfo* QwenManager::find_session(const std::string& session_id) {
    for (auto& session : sessions_) {
        if (session.session_id == session_id) {
            return &session;
        }
    }
    return nullptr;
}

// Find session by ID (const)
const ManagerSessionInfo* QwenManager::find_session(const std::string& session_id) const {
    for (const auto& session : sessions_) {
        if (session.session_id == session_id) {
            return &session;
        }
    }
    return nullptr;
}

// Load instructions from file
std::string QwenManager::load_instructions_from_file(const std::string& filename) {
    if (!vfs_) {
        return "";  // Can't load from VFS without vfs pointer
    }

    // Try to load the file from VFS
    try {
        return vfs_->read(filename);
    } catch (const std::runtime_error&) {
        // File not found in VFS, will try filesystem
    }

    // If not in VFS, try to load from the local filesystem
    std::ifstream file(filename);
    if (file.is_open()) {
        std::string content;
        std::string line;
        while (std::getline(file, line)) {
            content += line + "\n";
        }
        file.close();
        return content;
    }

    // File not found
    std::cout << "[QwenManager] Warning: Could not load instructions file: " << filename << std::endl;
    return "";
}

// Spawn REPO sessions for an account
bool QwenManager::spawn_repo_sessions_for_account(const std::string& account_id) {
    std::lock_guard<std::mutex> lock(account_configs_mutex_);
    
    // Find the account configuration
    auto account_it = std::find_if(account_configs_.begin(), account_configs_.end(),
        [&account_id](const AccountConfig& acc) { return acc.id == account_id; });
    
    if (account_it == account_configs_.end()) {
        std::cout << "[QwenManager] Account not found: " << account_id << std::endl;
        return false;
    }
    
    const AccountConfig& account = *account_it;
    
    // Check concurrent repo limit
    int active_repo_count = 0;
    {
        std::lock_guard<std::mutex> lock_sessions(sessions_mutex_);
        for (const auto& session : sessions_) {
            if (session.account_id == account_id && 
                (session.type == SessionType::REPO_MANAGER || session.type == SessionType::REPO_WORKER)) {
                active_repo_count++;
            }
        }
    }
    
    if (active_repo_count >= account.max_concurrent_repos) {
        std::cout << "[QwenManager] Max concurrent repos limit reached for account " << account_id 
                  << " (" << active_repo_count << "/" << account.max_concurrent_repos << ")" << std::endl;
        return false;
    }
    
    // Spawn sessions for each repository in the account
    for (const auto& repo : account.repositories) {
        if (!repo.enabled) continue; // Skip disabled repositories
        
        // Spawn REPO_WORKER session
        {
            std::lock_guard<std::mutex> lock_sessions(sessions_mutex_);
            
            ManagerSessionInfo repo_worker;
            repo_worker.session_id = "wrk-" + repo.id + "-" + std::to_string(time(nullptr));
            repo_worker.type = SessionType::REPO_WORKER;
            repo_worker.hostname = account.hostname;
            repo_worker.repo_path = repo.local_path;
            repo_worker.status = "idle";
            repo_worker.model = repo.worker_model;
            repo_worker.account_id = account.id;
            repo_worker.created_at = time(nullptr);
            repo_worker.last_activity = repo_worker.created_at;
            repo_worker.is_active = true;
            
            sessions_.push_back(repo_worker);
        }
        
        // Spawn REPO_MANAGER session
        {
            std::lock_guard<std::mutex> lock_sessions(sessions_mutex_);
            
            ManagerSessionInfo repo_manager;
            repo_manager.session_id = "mgr-" + repo.id + "-" + std::to_string(time(nullptr));
            repo_manager.type = SessionType::REPO_MANAGER;
            repo_manager.hostname = account.hostname;
            repo_manager.repo_path = repo.local_path;
            repo_manager.status = "idle";
            repo_manager.model = repo.manager_model;
            repo_manager.account_id = account.id;
            repo_manager.created_at = time(nullptr);
            repo_manager.last_activity = repo_manager.created_at;
            repo_manager.is_active = true;
            
            sessions_.push_back(repo_manager);
        }
        
        std::cout << "[QwenManager] Spawned sessions for repo: " << repo.id 
                  << " (worker: wrk-" << repo.id << ", manager: mgr-" << repo.id << ")" << std::endl;
    }
    
    return true;
}

// Enforce concurrent repository limit
bool QwenManager::enforce_concurrent_repo_limit(const std::string& account_id) {
    std::lock_guard<std::mutex> lock(account_configs_mutex_);
    
    // Find the account configuration
    auto account_it = std::find_if(account_configs_.begin(), account_configs_.end(),
        [&account_id](const AccountConfig& acc) { return acc.id == account_id; });
    
    if (account_it == account_configs_.end()) {
        return false; // Account not found
    }
    
    const AccountConfig& account = *account_it;
    
    // Count active repo sessions for this account
    int active_repo_count = 0;
    {
        std::lock_guard<std::mutex> lock_sessions(sessions_mutex_);
        for (const auto& session : sessions_) {
            if (session.account_id == account_id && 
                (session.type == SessionType::REPO_MANAGER || session.type == SessionType::REPO_WORKER)) {
                active_repo_count++;
            }
        }
    }
    
    return active_repo_count < account.max_concurrent_repos;
}

// Track failure for a REPO WORKER session
void QwenManager::track_worker_failure(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    for (auto& session : sessions_) {
        if (session.session_id == session_id && 
            (session.type == SessionType::REPO_WORKER || session.type == SessionType::REPO_MANAGER)) {
            session.failure_count++;
            
            std::cout << "[QwenManager] Failure tracked for session " << session_id 
                      << ", current count: " << session.failure_count << std::endl;
            
            // Check if we need to escalate after 3 failures
            if (session.failure_count >= 3 && session.type == SessionType::REPO_WORKER) {
                // Find the corresponding REPO MANAGER for this repository
                std::string repo_path = session.repo_path;
                for (auto& mgr_session : sessions_) {
                    if (mgr_session.repo_path == repo_path && 
                        mgr_session.type == SessionType::REPO_MANAGER &&
                        mgr_session.account_id == session.account_id) {
                        std::cout << "[QwenManager] Escalating from WORKER " << session_id 
                                  << " to MANAGER " << mgr_session.session_id << std::endl;
                        
                        // Mark the manager session as needing escalation
                        mgr_session.status = "escalated";
                        mgr_session.failure_count = session.failure_count; // Pass on the failure count
                        session.status = "escalated"; // Mark worker as escalated
                        
                        // In a real implementation, we would now send the task to the manager
                        break;
                    }
                }
            }
            break;
        }
    }
}

// Reset failure count after successful operation
void QwenManager::reset_failure_count(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    for (auto& session : sessions_) {
        if (session.session_id == session_id) {
            session.failure_count = 0;
            session.status = "active";
            break;
        }
    }
}

// Increment commit count for test interval triggers
void QwenManager::increment_commit_count(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    for (auto& session : sessions_) {
        if (session.session_id == session_id && 
            (session.type == SessionType::REPO_WORKER || session.type == SessionType::REPO_MANAGER)) {
            session.commit_count++;
            
            std::cout << "[QwenManager] Commit count for session " << session_id 
                      << " is now: " << session.commit_count << std::endl;
            
            // Check if we need to trigger a review after 3 commits
            if (session.commit_count >= 3) {
                // Find the corresponding REPO MANAGER for this repository to trigger review
                std::string repo_path = session.repo_path;
                for (auto& mgr_session : sessions_) {
                    if (mgr_session.repo_path == repo_path && 
                        mgr_session.type == SessionType::REPO_MANAGER &&
                        mgr_session.account_id == session.account_id) {
                        std::cout << "[QwenManager] Triggering review for repo " << repo_path 
                                  << " after " << session.commit_count << " commits by " << session_id << std::endl;
                        
                        // Mark the manager session to perform a review
                        mgr_session.status = "review_pending";
                        session.commit_count = 0; // Reset commit count after triggering review
                        break;
                    }
                }
            }
            break;
        }
    }
}

// Update session workflow state
void QwenManager::update_session_state(const std::string& session_id, SessionState new_state) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    for (auto& session : sessions_) {
        if (session.session_id == session_id) {
            session.workflow_state = new_state;
            
            // Update status string based on workflow state
            switch (new_state) {
                case SessionState::AUTOMATIC:
                    session.status = "automatic";
                    break;
                case SessionState::MANUAL:
                    session.status = "manual";
                    break;
                case SessionState::TESTING:
                    session.status = "testing";
                    break;
                case SessionState::BLOCKED:
                    session.status = "blocked";
                    break;
                case SessionState::IDLE:
                    session.status = "idle";
                    break;
            }
            break;
        }
    }
}

// Check if manual override is enabled for a session
bool QwenManager::is_manual_override(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    for (const auto& session : sessions_) {
        if (session.session_id == session_id) {
            return session.workflow_state == SessionState::MANUAL;
        }
    }
    
    return false; // Default to automatic if session not found
}

// Find session by account ID and repository ID
ManagerSessionInfo* QwenManager::find_session_by_repo(const std::string& account_id, const std::string& repo_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    // Look for REPO sessions (both WORKER and MANAGER) that match the account and repo
    for (auto& session : sessions_) {
        if ((session.type == SessionType::REPO_WORKER || session.type == SessionType::REPO_MANAGER) &&
            session.account_id == account_id) {
            // Based on how we create session IDs in spawn_repo_sessions_for_account
            // Session IDs follow the pattern: "wrk-{repo_id}-{timestamp}" or "mgr-{repo_id}-{timestamp}"
            // So we look for session IDs that start with the appropriate prefix
            std::string worker_prefix = "wrk-" + repo_id + "-";
            std::string manager_prefix = "mgr-" + repo_id + "-";
            
            if (session.session_id.substr(0, worker_prefix.length()) == worker_prefix ||
                session.session_id.substr(0, manager_prefix.length()) == manager_prefix) {
                return &session;
            }
        }
    }
    
    return nullptr; // Not found
}

// Save a session snapshot
bool QwenManager::save_session_snapshot(const std::string& session_id, const std::string& snapshot_name) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    std::lock_guard<std::mutex> snap_lock(snapshots_mutex_);
    
    // Find the session
    ManagerSessionInfo* session = find_session(session_id);
    if (!session) {
        std::cout << "[QwenManager] Cannot save snapshot: Session not found: " << session_id << std::endl;
        return false;
    }
    
    // Create a new snapshot
    SessionSnapshot snapshot;
    snapshot.snapshot_id = session_id + "-" + std::to_string(time(nullptr));
    snapshot.session_id = session_id;
    snapshot.name = snapshot_name.empty() ? "snapshot-" + std::to_string(time(nullptr)) : snapshot_name;
    snapshot.model = session->model;
    snapshot.repo_path = session->repo_path;
    snapshot.created_at = time(nullptr);
    snapshot.last_restored = 0;
    
    // In a real implementation, we would capture the conversation history and tool history
    // from the actual qwen session. For now, we'll create a placeholder.
    snapshot.conversation_history.push_back({"system", "Snapshot of session " + session_id});
    snapshot.conversation_history.push_back({"assistant", "Session state saved at " + std::to_string(snapshot.created_at)});
    
    // Store the snapshot
    session_snapshots_[session_id].push_back(snapshot);
    
    std::cout << "[QwenManager] Snapshot saved for session " << session_id 
              << " with name: " << snapshot.name << std::endl;
    
    return true;
}

// Restore a session snapshot
bool QwenManager::restore_session_snapshot(const std::string& session_id, const std::string& snapshot_name) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    std::lock_guard<std::mutex> snap_lock(snapshots_mutex_);
    
    // Find the session
    ManagerSessionInfo* session = find_session(session_id);
    if (!session) {
        std::cout << "[QwenManager] Cannot restore snapshot: Session not found: " << session_id << std::endl;
        return false;
    }
    
    // Find the snapshot
    auto it = session_snapshots_.find(session_id);
    if (it == session_snapshots_.end()) {
        std::cout << "[QwenManager] Cannot restore snapshot: No snapshots found for session: " << session_id << std::endl;
        return false;
    }
    
    // Find the specific snapshot by name
    auto& snapshots = it->second;
    auto snap_it = std::find_if(snapshots.begin(), snapshots.end(),
        [&snapshot_name](const SessionSnapshot& snap) { return snap.name == snapshot_name; });
    
    if (snap_it == snapshots.end()) {
        std::cout << "[QwenManager] Cannot restore snapshot: Snapshot not found: " << snapshot_name << std::endl;
        return false;
    }
    
    // Restore the session state
    session->model = snap_it->model;
    session->repo_path = snap_it->repo_path;
    
    // Update the last restored timestamp
    const_cast<SessionSnapshot&>(*snap_it).last_restored = time(nullptr);
    
    std::cout << "[QwenManager] Snapshot restored for session " << session_id 
              << " from: " << snapshot_name << std::endl;
    
    return true;
}

// Delete a session snapshot
bool QwenManager::delete_session_snapshot(const std::string& session_id, const std::string& snapshot_name) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    std::lock_guard<std::mutex> snap_lock(snapshots_mutex_);
    
    // Find the session snapshots
    auto it = session_snapshots_.find(session_id);
    if (it == session_snapshots_.end()) {
        std::cout << "[QwenManager] Cannot delete snapshot: No snapshots found for session: " << session_id << std::endl;
        return false;
    }
    
    // Find and remove the specific snapshot by name
    auto& snapshots = it->second;
    auto snap_it = std::find_if(snapshots.begin(), snapshots.end(),
        [&snapshot_name](const SessionSnapshot& snap) { return snap.name == snapshot_name; });
    
    if (snap_it == snapshots.end()) {
        std::cout << "[QwenManager] Cannot delete snapshot: Snapshot not found: " << snapshot_name << std::endl;
        return false;
    }
    
    snapshots.erase(snap_it);
    
    std::cout << "[QwenManager] Snapshot deleted for session " << session_id 
              << ": " << snapshot_name << std::endl;
    
    return true;
}

// List session snapshots
std::vector<std::string> QwenManager::list_session_snapshots(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    std::lock_guard<std::mutex> snap_lock(snapshots_mutex_);
    
    std::vector<std::string> snapshot_names;
    
    // Find the session snapshots
    auto it = session_snapshots_.find(session_id);
    if (it != session_snapshots_.end()) {
        for (const auto& snapshot : it->second) {
            snapshot_names.push_back(snapshot.name);
        }
    }
    
    return snapshot_names;
}

// Create a new session group
std::string QwenManager::create_session_group(const std::string& name, const std::string& description) {
    std::lock_guard<std::mutex> lock(groups_mutex_);
    
    // Generate a unique group ID
    std::string group_id = "group-" + std::to_string(time(nullptr)) + "-" + 
                          std::to_string(session_groups_.size());
    
    // Create the group
    SessionGroup group;
    group.group_id = group_id;
    group.name = name;
    group.description = description;
    group.created_at = time(nullptr);
    group.last_updated = group.created_at;
    
    session_groups_.push_back(group);
    
    std::cout << "[QwenManager] Created session group: " << name << " (" << group_id << ")" << std::endl;
    
    return group_id;
}

// Delete a session group
bool QwenManager::delete_session_group(const std::string& group_id) {
    std::lock_guard<std::mutex> lock(groups_mutex_);
    
    // Find and remove the group
    auto it = std::find_if(session_groups_.begin(), session_groups_.end(),
        [&group_id](const SessionGroup& group) { return group.group_id == group_id; });
    
    if (it == session_groups_.end()) {
        std::cout << "[QwenManager] Cannot delete group: Group not found: " << group_id << std::endl;
        return false;
    }
    
    // Remove the group from all sessions that reference it
    {
        std::lock_guard<std::mutex> session_lock(sessions_mutex_);
        for (auto& session : sessions_) {
            auto group_it = std::find(session.group_ids.begin(), session.group_ids.end(), group_id);
            if (group_it != session.group_ids.end()) {
                session.group_ids.erase(group_it);
            }
        }
    }
    
    session_groups_.erase(it);
    
    std::cout << "[QwenManager] Deleted session group: " << group_id << std::endl;
    
    return true;
}

// Add a session to a group
bool QwenManager::add_session_to_group(const std::string& session_id, const std::string& group_id) {
    std::lock_guard<std::mutex> group_lock(groups_mutex_);
    std::lock_guard<std::mutex> session_lock(sessions_mutex_);
    
    // Verify the group exists
    auto group_it = std::find_if(session_groups_.begin(), session_groups_.end(),
        [&group_id](const SessionGroup& group) { return group.group_id == group_id; });
    
    if (group_it == session_groups_.end()) {
        std::cout << "[QwenManager] Cannot add session to group: Group not found: " << group_id << std::endl;
        return false;
    }
    
    // Find the session
    ManagerSessionInfo* session = find_session(session_id);
    if (!session) {
        std::cout << "[QwenManager] Cannot add session to group: Session not found: " << session_id << std::endl;
        return false;
    }
    
    // Check if session is already in the group
    if (std::find(session->group_ids.begin(), session->group_ids.end(), group_id) != session->group_ids.end()) {
        std::cout << "[QwenManager] Session " << session_id << " is already in group " << group_id << std::endl;
        return true; // Already in group, technically successful
    }
    
    // Add the group to the session
    session->group_ids.push_back(group_id);
    
    // Add the session to the group's session list
    group_it->session_ids.push_back(session_id);
    group_it->last_updated = time(nullptr);
    
    std::cout << "[QwenManager] Added session " << session_id << " to group " << group_id << std::endl;
    
    return true;
}

// Remove a session from a group
bool QwenManager::remove_session_from_group(const std::string& session_id, const std::string& group_id) {
    std::lock_guard<std::mutex> group_lock(groups_mutex_);
    std::lock_guard<std::mutex> session_lock(sessions_mutex_);
    
    // Verify the group exists
    auto group_it = std::find_if(session_groups_.begin(), session_groups_.end(),
        [&group_id](const SessionGroup& group) { return group.group_id == group_id; });
    
    if (group_it == session_groups_.end()) {
        std::cout << "[QwenManager] Cannot remove session from group: Group not found: " << group_id << std::endl;
        return false;
    }
    
    // Find the session
    ManagerSessionInfo* session = find_session(session_id);
    if (!session) {
        std::cout << "[QwenManager] Cannot remove session from group: Session not found: " << session_id << std::endl;
        return false;
    }
    
    // Remove the group from the session
    auto session_group_it = std::find(session->group_ids.begin(), session->group_ids.end(), group_id);
    if (session_group_it != session->group_ids.end()) {
        session->group_ids.erase(session_group_it);
    } else {
        std::cout << "[QwenManager] Session " << session_id << " is not in group " << group_id << std::endl;
        return true; // Not in group, technically successful
    }
    
    // Remove the session from the group's session list
    auto group_session_it = std::find(group_it->session_ids.begin(), group_it->session_ids.end(), session_id);
    if (group_session_it != group_it->session_ids.end()) {
        group_it->session_ids.erase(group_session_it);
        group_it->last_updated = time(nullptr);
    }
    
    std::cout << "[QwenManager] Removed session " << session_id << " from group " << group_id << std::endl;
    
    return true;
}

// List all session groups
std::vector<SessionGroup> QwenManager::list_session_groups() const {
    std::lock_guard<std::mutex> lock(groups_mutex_);
    return session_groups_;
}

// Get all sessions in a group
std::vector<ManagerSessionInfo*> QwenManager::get_sessions_in_group(const std::string& group_id) {
    std::lock_guard<std::mutex> group_lock(groups_mutex_);
    std::lock_guard<std::mutex> session_lock(sessions_mutex_);
    
    std::vector<ManagerSessionInfo*> group_sessions;
    
    // Find the group
    auto group_it = std::find_if(session_groups_.begin(), session_groups_.end(),
        [&group_id](const SessionGroup& group) { return group.group_id == group_id; });
    
    if (group_it == session_groups_.end()) {
        std::cout << "[QwenManager] Cannot get sessions in group: Group not found: " << group_id << std::endl;
        return group_sessions;
    }
    
    // Find all sessions that belong to this group
    for (auto& session : sessions_) {
        if (std::find(session.group_ids.begin(), session.group_ids.end(), group_id) != session.group_ids.end()) {
            group_sessions.push_back(&session);
        }
    }
    
    return group_sessions;
}

// Pause a session
bool QwenManager::pause_session(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    for (auto& session : sessions_) {
        if (session.session_id == session_id) {
            if (session.is_paused) {
                std::cout << "[QwenManager] Session " << session_id << " is already paused" << std::endl;
                return true; // Already paused, so technically successful
            }
            
            session.is_paused = true;
            session.paused_at = time(nullptr);
            session.status = "paused";
            
            std::cout << "[QwenManager] Session " << session_id << " paused" << std::endl;
            return true;
        }
    }
    
    std::cout << "[QwenManager] Session not found: " << session_id << std::endl;
    return false; // Session not found
}

// Resume a session
bool QwenManager::resume_session(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    for (auto& session : sessions_) {
        if (session.session_id == session_id) {
            if (!session.is_paused) {
                std::cout << "[QwenManager] Session " << session_id << " is not paused" << std::endl;
                return true; // Not paused, so technically successful
            }
            
            session.is_paused = false;
            session.paused_at = 0;
            
            // Restore the previous status or set to active
            if (session.status == "paused") {
                session.status = "active";
            }
            
            std::cout << "[QwenManager] Session " << session_id << " resumed" << std::endl;
            return true;
        }
    }
    
    std::cout << "[QwenManager] Session not found: " << session_id << std::endl;
    return false; // Session not found
}

// Check if a session is paused
bool QwenManager::is_session_paused(const std::string& session_id) const {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    for (const auto& session : sessions_) {
        if (session.session_id == session_id) {
            return session.is_paused;
        }
    }
    
    return false; // Session not found or not paused
}

// Convert JSON task specification to natural language prompt
std::string QwenManager::convert_json_to_prompt(const std::string& json_task_spec) {
    // This is a simplified implementation - in a real system you'd have more sophisticated parsing
    // Parse the JSON task specification and convert it to a natural language prompt
    
    // Extract key fields from the JSON
    std::string title = extract_json_field(json_task_spec, "title");
    std::string description = extract_json_field(json_task_spec, "description");
    std::string repo_id = extract_json_field(json_task_spec, "repository_id");
    
    if (title.empty() && description.empty()) {
        return "Perform the requested task on the repository.";
    }
    
    std::string prompt = "Task: " + (title.empty() ? "Unspecified task" : title) + "\n";
    
    if (!description.empty()) {
        prompt += "Description: " + description + "\n";
    }
    
    if (!repo_id.empty()) {
        prompt += "Repository: " + repo_id + "\n";
    }
    
    // Extract other fields if present
    std::string requirements_field = extract_json_field(json_task_spec, "requirements");
    if (!requirements_field.empty() && requirements_field != "null") {
        prompt += "Requirements: " + requirements_field + "\n";
    }
    
    std::string deadline_field = extract_json_field(json_task_spec, "deadline");
    if (!deadline_field.empty() && deadline_field != "null") {
        prompt += "Deadline: " + deadline_field + "\n";
    }
    
    prompt += "\nPlease implement this task in the specified repository, following best practices and ensuring code quality.";
    
    return prompt;
}

// Load accounts configuration from ACCOUNTS.json
bool QwenManager::load_accounts_config() {
    std::string json_content = load_instructions_from_file("ACCOUNTS.json");
    if (json_content.empty()) {
        std::cout << "[QwenManager] ACCOUNTS.json not found, will create default configuration\n";
        // Optionally create a default ACCOUNTS.json structure
        return true;
    }
    
    parse_accounts_json(json_content);
    return validate_accounts_config();
}

// Parse ACCOUNTS.json content
void QwenManager::parse_accounts_json(const std::string& json_content) {
    // Reset existing configurations
    std::lock_guard<std::mutex> lock(account_configs_mutex_);
    account_configs_.clear();
    
    if (json_content.empty()) {
        return;
    }
    
    // Find the start of the accounts array
    size_t accounts_start = json_content.find("\"accounts\"");
    if (accounts_start == std::string::npos) {
        std::cout << "[QwenManager] Warning: No 'accounts' field found in ACCOUNTS.json\n";
        return;
    }
    
    // Find the beginning of the array after "accounts":
    size_t array_start = json_content.find('[', accounts_start);
    if (array_start == std::string::npos) {
        std::cout << "[QwenManager] Warning: No accounts array found in ACCOUNTS.json\n";
        return;
    }
    
    // Simple JSON parsing - look for account objects within the array
    size_t pos = array_start + 1; // After the opening bracket
    
    while (pos < json_content.length()) {
        // Find the start of an account object
        size_t obj_start = json_content.find('{', pos);
        if (obj_start == std::string::npos) break;
        
        // Find the matching closing brace for this account object
        int brace_count = 1;
        size_t obj_end = obj_start + 1;
        while (obj_end < json_content.length() && brace_count > 0) {
            if (json_content[obj_end] == '{') {
                brace_count++;
            } else if (json_content[obj_end] == '}') {
                brace_count--;
            } else if (json_content[obj_end] == '"' && json_content[obj_end-1] != '\\') {
                // Skip content inside quotes
                obj_end++;
                while (obj_end < json_content.length() && json_content[obj_end] != '"') {
                    if (json_content[obj_end] == '\\' && obj_end + 1 < json_content.length()) {
                        obj_end += 2; // Skip escaped character
                        continue;
                    }
                    obj_end++;
                }
            }
            obj_end++;
        }
        
        if (brace_count != 0) {
            std::cout << "[QwenManager] Warning: Mismatched braces in ACCOUNTS.json\n";
            break;
        }
        
        obj_end--; // Move back to the closing brace
        
        // Extract the account object JSON string
        std::string account_json = json_content.substr(obj_start, obj_end - obj_start + 1);
        
        // Parse this account object
        AccountConfig account = parse_account_object(account_json);
        if (!account.id.empty()) {  // Only add if parsing was successful
            account_configs_.push_back(account);
        }
        
        // Look for the next object after a comma
        pos = json_content.find(',', obj_end);
        if (pos != std::string::npos) {
            pos++; // Move past comma
        } else {
            break; // No more objects
        }
    }
}

// Parse a single account object from JSON
AccountConfig QwenManager::parse_account_object(const std::string& account_json) {
    AccountConfig account;
    
    // Extract id
    std::string id_val = extract_json_field(account_json, "id");
    if (!id_val.empty()) {
        account.id = id_val;
    }
    
    // Extract hostname
    std::string hostname_val = extract_json_field(account_json, "hostname");
    if (!hostname_val.empty()) {
        account.hostname = hostname_val;
    }
    
    // Extract enabled status
    std::string enabled_val = extract_json_field(account_json, "enabled");
    if (!enabled_val.empty()) {
        account.enabled = (enabled_val == "true");
    }
    
    // Extract max_concurrent_repos
    std::string max_concurrent_val = extract_json_field(account_json, "max_concurrent_repos");
    if (!max_concurrent_val.empty()) {
        try {
            account.max_concurrent_repos = std::stoi(max_concurrent_val);
        } catch (...) {
            account.max_concurrent_repos = 3; // default
        }
    }
    
    // Extract repositories array
    size_t repos_start = account_json.find("\"repositories\"");
    if (repos_start != std::string::npos) {
        size_t array_start = account_json.find('[', repos_start);
        if (array_start != std::string::npos) {
            size_t pos = array_start + 1;
            
            while (pos < account_json.length()) {
                size_t obj_start = account_json.find('{', pos);
                if (obj_start == std::string::npos) break;
                
                // Find the matching closing brace for this repo object
                int brace_count = 1;
                size_t obj_end = obj_start + 1;
                while (obj_end < account_json.length() && brace_count > 0) {
                    if (account_json[obj_end] == '{') {
                        brace_count++;
                    } else if (account_json[obj_end] == '}') {
                        brace_count--;
                    } else if (account_json[obj_end] == '"' && 
                              (obj_end == 0 || account_json[obj_end-1] != '\\')) {
                        // Skip content inside quotes
                        obj_end++;
                        while (obj_end < account_json.length() && account_json[obj_end] != '"') {
                            if (account_json[obj_end] == '\\' && obj_end + 1 < account_json.length()) {
                                obj_end += 2; // Skip escaped character
                                continue;
                            }
                            obj_end++;
                        }
                    }
                    obj_end++;
                }
                
                if (brace_count != 0) {
                    break;
                }
                
                obj_end--; // Move back to the closing brace
                
                // Extract the repo object JSON string
                std::string repo_json = account_json.substr(obj_start, obj_end - obj_start + 1);
                
                // Parse this repo object
                RepositoryConfig repo = parse_repository_object(repo_json);
                if (!repo.id.empty()) {  // Only add if parsing was successful
                    account.repositories.push_back(repo);
                }
                
                // Look for the next object after a comma
                pos = account_json.find(',', obj_end);
                if (pos != std::string::npos) {
                    pos++; // Move past comma
                } else {
                    break; // No more objects
                }
            }
        }
    }
    
    return account;
}

// Parse a single repository object from JSON
RepositoryConfig QwenManager::parse_repository_object(const std::string& repo_json) {
    RepositoryConfig repo;
    
    // Extract id
    std::string id_val = extract_json_field(repo_json, "id");
    if (!id_val.empty()) {
        repo.id = id_val;
    }
    
    // Extract url
    std::string url_val = extract_json_field(repo_json, "url");
    if (!url_val.empty()) {
        repo.url = url_val;
    }
    
    // Extract local_path
    std::string path_val = extract_json_field(repo_json, "local_path");
    if (!path_val.empty()) {
        repo.local_path = path_val;
    }
    
    // Extract enabled status
    std::string enabled_val = extract_json_field(repo_json, "enabled");
    if (!enabled_val.empty()) {
        repo.enabled = (enabled_val == "true");
    }
    
    // Extract worker_model
    std::string worker_model_val = extract_json_field(repo_json, "worker_model");
    if (!worker_model_val.empty()) {
        repo.worker_model = worker_model_val;
    }
    
    // Extract manager_model
    std::string manager_model_val = extract_json_field(repo_json, "manager_model");
    if (!manager_model_val.empty()) {
        repo.manager_model = manager_model_val;
    }
    
    return repo;
}

// Helper to extract a field value from JSON string
std::string QwenManager::extract_json_field(const std::string& json_str, const std::string& field_name) {
    std::string search = "\"" + field_name + "\"";
    size_t pos = json_str.find(search);
    if (pos == std::string::npos) {
        return "";
    }
    
    pos += search.length();
    // Skip colon and whitespace
    while (pos < json_str.length() && (json_str[pos] == ':' || std::isspace(json_str[pos]))) {
        pos++;
    }
    
    if (pos >= json_str.length()) {
        return "";
    }
    
    char start_char = json_str[pos];
    if (start_char == '"') {
        // String value
        pos++;
        size_t end_pos = pos;
        while (end_pos < json_str.length() && json_str[end_pos] != '"') {
            if (json_str[end_pos] == '\\' && end_pos + 1 < json_str.length()) {
                end_pos += 2; // Skip escaped character
                continue;
            }
            end_pos++;
        }
        
        if (end_pos < json_str.length()) {
            return json_str.substr(pos, end_pos - pos);
        }
    } else if (start_char == 't' || start_char == 'f') {
        // Boolean value
        if (json_str.substr(pos, 4) == "true") {
            return "true";
        } else if (json_str.substr(pos, 5) == "false") {
            return "false";
        }
    } else if (std::isdigit(start_char) || start_char == '-') {
        // Numeric value
        size_t end_pos = pos;
        while (end_pos < json_str.length() && 
               (std::isdigit(json_str[end_pos]) || json_str[end_pos] == '.' || 
                json_str[end_pos] == '-' || json_str[end_pos] == '+' || 
                json_str[end_pos] == 'e' || json_str[end_pos] == 'E')) {
            end_pos++;
        }
        return json_str.substr(pos, end_pos - pos);
    } else if (start_char == '[' || start_char == '{') {
        // Array or object - return the entire structure
        int brace_count = 1;
        size_t end_pos = pos + 1;
        char match_char = (start_char == '[') ? ']' : '}';
        
        while (end_pos < json_str.length() && brace_count > 0) {
            if (json_str[end_pos] == '{' || json_str[end_pos] == '[') {
                brace_count++;
            } else if (json_str[end_pos] == '}' || json_str[end_pos] == ']') {
                brace_count--;
            } else if (json_str[end_pos] == '"' && json_str[end_pos-1] != '\\') {
                // Skip content inside quotes
                end_pos++;
                while (end_pos < json_str.length() && json_str[end_pos] != '"') {
                    if (json_str[end_pos] == '\\' && end_pos + 1 < json_str.length()) {
                        end_pos += 2; // Skip escaped character
                        continue;
                    }
                    end_pos++;
                }
            }
            if (brace_count > 0) end_pos++;
        }
        
        if (brace_count == 0) {
            return json_str.substr(pos, end_pos - pos + 1);
        }
    }
    
    return "";
}

// Validate accounts configuration
bool QwenManager::validate_accounts_config() {
    std::lock_guard<std::mutex> lock(account_configs_mutex_);
    
    for (const auto& account : account_configs_) {
        if (!validate_account_config(account)) {
            std::cout << "[QwenManager] Invalid account configuration: " << account.id << std::endl;
            return false;
        }
        
        for (const auto& repo : account.repositories) {
            if (!validate_repository_config(repo)) {
                std::cout << "[QwenManager] Invalid repository configuration: " << repo.id << std::endl;
                return false;
            }
        }
    }
    
    std::cout << "[QwenManager] Account configurations validated successfully (" 
              << account_configs_.size() << " accounts)" << std::endl;
    return true;
}

// Validate account configuration
bool QwenManager::validate_account_config(const AccountConfig& account) {
    if (account.id.empty()) {
        std::cout << "[QwenManager] Account ID cannot be empty" << std::endl;
        return false;
    }
    
    if (account.hostname.empty()) {
        std::cout << "[QwenManager] Account hostname cannot be empty" << std::endl;
        return false;
    }
    
    if (account.max_concurrent_repos <= 0) {
        std::cout << "[QwenManager] max_concurrent_repos must be positive" << std::endl;
        return false;
    }
    
    return true;
}

// Validate repository configuration
bool QwenManager::validate_repository_config(const RepositoryConfig& repo) {
    if (repo.id.empty()) {
        std::cout << "[QwenManager] Repository ID cannot be empty" << std::endl;
        return false;
    }
    
    if (repo.url.empty()) {
        std::cout << "[QwenManager] Repository URL cannot be empty" << std::endl;
        return false;
    }
    
    if (repo.local_path.empty()) {
        std::cout << "[QwenManager] Repository local path cannot be empty" << std::endl;
        return false;
    }
    
    return true;
}

// Start the ACCOUNTS.json file watcher
void QwenManager::start_accounts_json_watcher() {
    accounts_watcher_running_ = true;
    accounts_watcher_thread_ = std::thread(&QwenManager::accounts_json_watcher_thread, this);
}

// Stop the ACCOUNTS.json file watcher
void QwenManager::stop_accounts_json_watcher() {
    if (accounts_watcher_running_) {
        accounts_watcher_running_ = false;
        
        // Notify the watcher thread to stop
        std::unique_lock<std::mutex> lock(watcher_mutex_);
        stop_cv_.notify_all();
    }
    
    if (accounts_watcher_thread_.joinable()) {
        accounts_watcher_thread_.join();
    }
}

// Thread function for watching ACCOUNTS.json file changes
void QwenManager::accounts_json_watcher_thread() {
    // Initial delay of 10 seconds as specified
    std::this_thread::sleep_for(std::chrono::seconds(10));
    
    std::string last_content;

    if (vfs_) {
        try {
            last_content = vfs_->read("ACCOUNTS.json");
        } catch (const std::runtime_error&) {
            // File doesn't exist yet
        }
    }
    
    std::cout << "[QwenManager] ACCOUNTS.json watcher started\n";
    
    while (accounts_watcher_running_) {
        // Check if the file has been modified
        std::string current_content;
        if (vfs_) {
            try {
                current_content = vfs_->read("ACCOUNTS.json");
            } catch (const std::runtime_error&) {
                // File doesn't exist yet
            }
        }
        
        if (current_content != last_content) {
            std::cout << "[QwenManager] ACCOUNTS.json has been modified, reloading configuration\n";
            last_content = current_content;
            
            // Parse and validate the new configuration
            if (!current_content.empty()) {
                parse_accounts_json(current_content);
                if (validate_accounts_config()) {
                    std::cout << "[QwenManager] New ACCOUNTS.json configuration loaded successfully\n";
                    // TODO: Update any active account connections based on new config
                } else {
                    std::cout << "[QwenManager] New ACCOUNTS.json configuration failed validation\n";
                }
            }
        }
        
        // Check every 5 seconds
        std::unique_lock<std::mutex> lock(watcher_mutex_);
        if (stop_cv_.wait_for(lock, std::chrono::seconds(5), [this] { return !accounts_watcher_running_; })) {
            // Stop requested
            break;
        }
    }
    
    std::cout << "[QwenManager] ACCOUNTS.json watcher stopped\n";
}

// Helper struct to store colored output lines
struct OutputLine {
    std::string text;
    int color_pair;

    OutputLine(const std::string& t, int cp = 0) : text(t), color_pair(cp) {}
};

// Run manager mode with ncurses UI
bool QwenManager::run_ncurses_mode() {
#ifdef CODEX_UI_NCURSES
    // Initialize ncurses
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    
    // Enable mouse support for scrolling
    mousemask(BUTTON4_PRESSED | BUTTON5_PRESSED, nullptr);
    
    // Initialize colors if supported
    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_CYAN, COLOR_BLACK);     // MANAGER sessions
        init_pair(2, COLOR_YELLOW, COLOR_BLACK);   // ACCOUNT sessions
        init_pair(3, COLOR_GREEN, COLOR_BLACK);    // REPO sessions
        init_pair(4, COLOR_RED, COLOR_BLACK);      // Error messages
        init_pair(5, COLOR_BLUE, COLOR_BLACK);     // Info messages
        init_pair(6, COLOR_MAGENTA, COLOR_BLACK);  // System messages
        init_pair(7, COLOR_WHITE, COLOR_BLACK);    // Default text
    }

    // Get screen dimensions
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    // Create windows for session list and chat/command view
    int list_height = std::min(10, max_y - 5);  // Max 10 rows for list, leave room for status bar and input
    WINDOW* list_win = newwin(list_height, max_x, 0, 0);
    WINDOW* status_separator_win = newwin(1, max_x, list_height, 0);
    WINDOW* chat_win = newwin(max_y - list_height - 4, max_x, list_height + 1, 0);
    WINDOW* input_win = newwin(3, max_x, max_y - 3, 0);

    scrollok(list_win, TRUE);
    scrollok(chat_win, TRUE);

    // Enable keypad for input window
    keypad(input_win, TRUE);

    // Session list buffer (for scrolling)
    std::vector<OutputLine> session_list_buffer;
    int list_scroll_offset = 0;
    
    // Chat buffer (for scrolling)
    std::vector<OutputLine> chat_buffer;
    int chat_scroll_offset = 0;

    // Input buffer and cursor position
    std::string input_buffer;
    size_t cursor_pos = 0;
    
    // Currently selected session (for list navigation)
    int selected_session_idx = 0;
    bool list_focused = false;  // Whether the session list currently has focus
    
    // Current active session for chat view
    std::string active_session_id = "";
    std::map<std::string, std::vector<OutputLine>> session_chat_buffers; // Chat history per session

    // Helper function to update session list
    auto update_session_list = [&]() {
        session_list_buffer.clear();
        
        // Add header
        session_list_buffer.emplace_back("Type      | ID          | Computer   | Repo Path                    | Status", has_colors() ? 7 : 0);
        session_list_buffer.emplace_back("----------|-------------|------------|------------------------------|-------", has_colors() ? 7 : 0);
        
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        for (size_t i = 0; i < sessions_.size(); ++i) {
            const auto& session = sessions_[i];
            std::string type_str;
            int color_pair = 0;
            std::string status_icon = session.is_active ? "●" : "○";  // Active/inactive indicators
            
            switch (session.type) {
                case SessionType::MANAGER_PROJECT:
                    type_str = "MGR-PROJ ";
                    color_pair = has_colors() ? 1 : 0;  // Bright cyan for project manager
                    break;
                case SessionType::MANAGER_TASK:
                    type_str = "MGR-TASK ";
                    color_pair = has_colors() ? 6 : 0;  // Magenta for task manager
                    break;
                case SessionType::ACCOUNT:
                    type_str = "ACCOUNT  ";
                    color_pair = has_colors() ? 2 : 0;  // Yellow for accounts
                    break;
                case SessionType::REPO_MANAGER:
                    type_str = "REPO-MGR ";
                    color_pair = has_colors() ? 3 : 0;  // Green for repo managers
                    break;
                case SessionType::REPO_WORKER:
                    type_str = "REPO-WRK ";
                    color_pair = has_colors() ? 5 : 0;  // Blue for repo workers
                    break;
                default:
                    type_str = "UNKNOWN  ";
                    color_pair = has_colors() ? 6 : 0;  // Magenta for unknown
                    break;
            }
            
            std::string status_str = session.is_active ? "active" : "inactive";
            
            std::string line = status_icon + " " + type_str + " | " +
                              session.session_id.substr(0, 11) + " | " +
                              session.hostname.substr(0, 10) + " | " +
                              session.repo_path.substr(0, 26) + " | " +
                              status_str;
            
            // Highlight selected session
            if (i == selected_session_idx) {
                line = "> " + line;  // Add selection indicator
                session_list_buffer.emplace_back(line, has_colors() ? 7 : 0);  // White for selected
            } else {
                session_list_buffer.emplace_back("  " + line, color_pair);
            }
        }
    };

    // Helper function to redraw session list window
    auto redraw_session_list = [&]() {
        werase(list_win);
        
        // Calculate which lines to display
        int display_lines = list_height;
        int total_lines = session_list_buffer.size();
        int start_line = std::max(0, total_lines - display_lines - list_scroll_offset);
        int end_line = std::min(total_lines, start_line + display_lines);

        // Draw visible lines
        int y = 0;
        for (int i = start_line; i < end_line; ++i) {
            const auto& line = session_list_buffer[i];
            if (has_colors() && line.color_pair > 0) {
                wattron(list_win, COLOR_PAIR(line.color_pair));
                mvwprintw(list_win, y++, 0, "%s", line.text.c_str());
                wattroff(list_win, COLOR_PAIR(line.color_pair));
            } else {
                mvwprintw(list_win, y++, 0, "%s", line.text.c_str());
            }
        }

        box(list_win, 0, 0);
        wrefresh(list_win);
    };

    // Helper function to redraw status separator
    auto redraw_status_separator = [&]() {
        werase(status_separator_win);
        wattron(status_separator_win, A_REVERSE);
        
        std::string separator = "─";
        for (int i = 1; i < max_x; ++i) {
            separator += "─";
        }
        mvwprintw(status_separator_win, 0, 0, "%s", separator.c_str());
        
        wattroff(status_separator_win, A_REVERSE);
        wrefresh(status_separator_win);
    };

    // Helper function to redraw chat window
    auto redraw_chat_window = [&]() {
        werase(chat_win);

        // Get the active session's chat buffer
        std::vector<OutputLine>* active_buffer = nullptr;
        if (!active_session_id.empty()) {
            auto it = session_chat_buffers.find(active_session_id);
            if (it != session_chat_buffers.end()) {
                active_buffer = &(it->second);
            }
        }
        
        // If there's no active session buffer, use the default one
        std::vector<OutputLine> default_buffer;
        if (!active_buffer) {
            // Create a default message about the session view
            default_buffer.emplace_back("Select a session from the list above to view its chat history", has_colors() ? 5 : 0);
            default_buffer.emplace_back("", has_colors() ? 7 : 0);
            default_buffer.emplace_back("MANAGER sessions will show strategic planning and coordination", has_colors() ? 1 : 0);
            default_buffer.emplace_back("ACCOUNT sessions will show connection status and commands", has_colors() ? 2 : 0);
            default_buffer.emplace_back("REPO sessions will show development activity and progress", has_colors() ? 3 : 0);
            active_buffer = &default_buffer;
        }

        // Calculate which lines to display
        int display_lines = max_y - list_height - 4;
        int total_lines = active_buffer->size();
        int start_line = std::max(0, total_lines - display_lines - chat_scroll_offset);
        int end_line = std::min(total_lines, start_line + display_lines);

        // Draw visible lines
        int y = 0;
        for (int i = start_line; i < end_line; ++i) {
            const auto& line = (*active_buffer)[i];
            if (has_colors() && line.color_pair > 0) {
                wattron(chat_win, COLOR_PAIR(line.color_pair));
                mvwprintw(chat_win, y++, 0, "%s", line.text.c_str());
                wattroff(chat_win, COLOR_PAIR(line.color_pair));
            } else {
                mvwprintw(chat_win, y++, 0, "%s", line.text.c_str());
            }
        }

        wrefresh(chat_win);
    };

    // Helper function to draw input window
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

    // Helper function to redraw status bar
    auto redraw_status = [&]() {
        werase(status_separator_win);
        wattron(status_separator_win, A_REVERSE);

        // Count different types of sessions
        int project_mgr_count = 0, task_mgr_count = 0, account_count = 0, repo_mgr_count = 0, repo_worker_count = 0;
        for (const auto& session : sessions_) {
            switch (session.type) {
                case SessionType::MANAGER_PROJECT: project_mgr_count++; break;
                case SessionType::MANAGER_TASK: task_mgr_count++; break;
                case SessionType::ACCOUNT: account_count++; break;
                case SessionType::REPO_MANAGER: repo_mgr_count++; break;
                case SessionType::REPO_WORKER: repo_worker_count++; break;
                default: break;
            }
        }

        // Left side: Mode indicator and connection info
        std::string left_text = "MANAGER MODE | " + tcp_host_ + ":" + std::to_string(tcp_port_);

        // Right side: Session info with type breakdown
        std::string right_text = "Sessions: " + std::to_string(sessions_.size()) + 
                                " | MGR:" + std::to_string(project_mgr_count + task_mgr_count) +
                                " ACC:" + std::to_string(account_count) +
                                " REPO:" + std::to_string(repo_mgr_count + repo_worker_count) +
                                " | " + (list_focused ? "LIST" : "INPUT");

        // Calculate spacing
        int total_len = left_text.length() + right_text.length();
        int spaces = max_x - total_len - 2;
        if (spaces < 1) spaces = 1;

        // Draw status bar
        mvwprintw(status_separator_win, 0, 0, "%s", left_text.c_str());
        for (int i = 0; i < spaces; ++i) {
            waddch(status_separator_win, ' ');
        }
        wprintw(status_separator_win, "%s", right_text.c_str());

        // Pad to end
        int current_x = left_text.length() + spaces + right_text.length();
        for (int i = current_x; i < max_x - 1; ++i) {
            waddch(status_separator_win, ' ');
        }

        wattroff(status_separator_win, A_REVERSE);
        wrefresh(status_separator_win);
    };

    // Initial population of session list
    update_session_list();

    // Add initial info to chat buffer
    chat_buffer.emplace_back("qwen Manager Mode", has_colors() ? 5 : 0);
    chat_buffer.emplace_back("TCP Server: " + tcp_host_ + ":" + std::to_string(tcp_port_), has_colors() ? 5 : 0);
    chat_buffer.emplace_back("Active Sessions: " + std::to_string(sessions_.size()), has_colors() ? 5 : 0);
    chat_buffer.emplace_back("Use TAB to switch between session list and input/output areas", has_colors() ? 3 : 0);
    chat_buffer.emplace_back("Use UP/DOWN arrows to navigate session list", has_colors() ? 3 : 0);
    chat_buffer.emplace_back("Use Ctrl+C twice to exit", has_colors() ? 3 : 0);
    chat_buffer.emplace_back("");

    // Initial draw
    redraw_session_list();
    redraw_status_separator();
    redraw_chat_window();
    redraw_input();

    // Set non-blocking input mode with 50ms timeout
    wtimeout(input_win, 50);

    bool should_exit = false;
    
    // Ctrl+C handling (double Ctrl+C to exit)
    auto last_ctrl_c_time = std::chrono::steady_clock::now();
    bool ctrl_c_pressed_recently = false;

    // Main event loop
    while (!should_exit && running_) {
        // Poll for connection events, etc. (non-blocking)
        // For now, just update session list periodically
        
        // Repaint session list periodically to show status changes
        update_session_list();
        redraw_session_list();
        redraw_status();

        // Get a character from the appropriate window based on focus
        int ch = ERR;
        if (list_focused) {
            // Check for input on the session list window
            wtimeout(list_win, 50);
            ch = wgetch(list_win);
            // Restore timeout for input window
            wtimeout(input_win, 50);
        } else {
            // Check for input on the input window (default)
            ch = wgetch(input_win);
        }

        if (ch != ERR) {
            // Handle special cases that work regardless of focus
            if (ch == 9) {  // TAB key - switch focus
                list_focused = !list_focused;
                continue;
            } else if (ch == 27) {  // ESC key
                // Escape might be start of arrow key sequence - check for that
                wtimeout(list_focused ? list_win : input_win, 10);
                int next_ch = wgetch(list_focused ? list_win : input_win);
                wtimeout(list_focused ? list_win : input_win, 50);  // Restore normal timeout
                
                if (next_ch != ERR) {
                    // This was an escape sequence (like arrow keys)
                    if (list_focused && next_ch == 65) {  // Up arrow
                        if (selected_session_idx > 1) {  // Skip header lines
                            selected_session_idx--;
                        }
                    } else if (list_focused && next_ch == 66) {  // Down arrow
                        if (selected_session_idx < (int)sessions_.size() - 1) {
                            selected_session_idx++;
                        }
                    }
                    continue;
                }
                // If it was a standalone ESC, ignore it
                continue;
            }

            // Handle mouse events
            if (ch == KEY_MOUSE) {
                MEVENT event;
                if (getmouse(&event) == OK) {
                    // Mouse wheel up = scroll session list or chat up
                    if (event.bstate & BUTTON4_PRESSED) {
                        if (list_focused) {
                            list_scroll_offset = std::min(list_scroll_offset + 3, 
                                                         (int)session_list_buffer.size() - list_height);
                        } else {
                            chat_scroll_offset = std::min(chat_scroll_offset + 3, 
                                                         (int)chat_buffer.size() - (max_y - list_height - 4));
                        }
                        redraw_session_list();
                        redraw_chat_window();
                        continue;
                    }
                    // Mouse wheel down = scroll session list or chat down
                    else if (event.bstate & BUTTON5_PRESSED) {
                        if (list_focused) {
                            list_scroll_offset = std::max(list_scroll_offset - 3, 0);
                        } else {
                            chat_scroll_offset = std::max(chat_scroll_offset - 3, 0);
                        }
                        redraw_session_list();
                        redraw_chat_window();
                        continue;
                    }
                }
                continue;
            }

            // Handle focus-specific input
            if (list_focused) {
                // Session list has focus
                if (ch == KEY_UP || ch == 65) {  // Up arrow
                    if (selected_session_idx > 1) {  // Skip header lines
                        selected_session_idx--;
                        update_session_list();  // Redraw with new selection
                        redraw_session_list();
                    }
                } else if (ch == KEY_DOWN || ch == 66) {  // Down arrow
                    if (selected_session_idx < (int)sessions_.size() + 1) {  // +1 to account for headers
                        selected_session_idx++;
                        update_session_list();  // Redraw with new selection
                        redraw_session_list();
                    }
                } else if (ch == '\n' || ch == KEY_ENTER || ch == 13) {
                    // Enter on a selected session to switch to that session's chat view
                    if (selected_session_idx >= 2 && selected_session_idx < (int)sessions_.size() + 2) {  // Account for header
                        int session_index = selected_session_idx - 2;  // Adjust for headers
                        if (session_index < sessions_.size()) {
                            const auto& session = sessions_[session_index];
                            active_session_id = session.session_id;
                            
                            // Initialize session's chat buffer if it doesn't exist
                            if (session_chat_buffers.find(active_session_id) == session_chat_buffers.end()) {
                                session_chat_buffers[active_session_id] = std::vector<OutputLine>();
                                
                                // Add session-specific initial messages based on type
                                std::string session_type_str;
                                int session_color = has_colors() ? 7 : 0;
                                
                                switch (session.type) {
                                    case SessionType::MANAGER_PROJECT:
                                        session_type_str = "PROJECT MANAGER";
                                        session_color = has_colors() ? 1 : 0; // Cyan
                                        session_chat_buffers[active_session_id].emplace_back("PROJECT MANAGER Session: " + session.session_id, session_color);
                                        session_chat_buffers[active_session_id].emplace_back("Model: " + session.model, has_colors() ? 7 : 0);
                                        session_chat_buffers[active_session_id].emplace_back(std::string("Instructions: ") + (session.instructions.empty() ? "No instructions loaded" : "Loaded"), has_colors() ? 6 : 0);
                                        session_chat_buffers[active_session_id].emplace_back("", has_colors() ? 7 : 0);
                                        session_chat_buffers[active_session_id].emplace_back("Use this session for high-level project planning and architectural decisions", has_colors() ? 5 : 0);
                                        break;
                                    case SessionType::MANAGER_TASK:
                                        session_type_str = "TASK MANAGER";
                                        session_color = has_colors() ? 6 : 0; // Magenta
                                        session_chat_buffers[active_session_id].emplace_back("TASK MANAGER Session: " + session.session_id, session_color);
                                        session_chat_buffers[active_session_id].emplace_back("Model: " + session.model, has_colors() ? 7 : 0);
                                        session_chat_buffers[active_session_id].emplace_back(std::string("Instructions: ") + (session.instructions.empty() ? "No instructions loaded" : "Loaded"), has_colors() ? 6 : 0);
                                        session_chat_buffers[active_session_id].emplace_back("", has_colors() ? 7 : 0);
                                        session_chat_buffers[active_session_id].emplace_back("Use this session for task coordination and issue resolution", has_colors() ? 5 : 0);
                                        break;
                                    case SessionType::ACCOUNT:
                                        session_type_str = "ACCOUNT";
                                        session_color = has_colors() ? 2 : 0; // Yellow
                                        session_chat_buffers[active_session_id].emplace_back("ACCOUNT Session: " + session.session_id, session_color);
                                        session_chat_buffers[active_session_id].emplace_back("Host: " + session.hostname, has_colors() ? 7 : 0);
                                        session_chat_buffers[active_session_id].emplace_back("Status: " + session.status, has_colors() ? 7 : 0);
                                        session_chat_buffers[active_session_id].emplace_back("", has_colors() ? 7 : 0);
                                        
                                        // Look up account configuration to show associated repositories
                                        {
                                            std::lock_guard<std::mutex> lock(account_configs_mutex_);
                                            auto account_it = std::find_if(account_configs_.begin(), account_configs_.end(),
                                                [&session](const AccountConfig& acc) { return acc.id == session.account_id; });
                                            
                                            if (account_it != account_configs_.end()) {
                                                session_chat_buffers[active_session_id].emplace_back("Configured Repositories:", has_colors() ? 2 : 0);
                                                for (const auto& repo : account_it->repositories) {
                                                    session_chat_buffers[active_session_id].emplace_back("  - " + repo.id + " (" + (repo.enabled ? "enabled" : "disabled") + ")", has_colors() ? 7 : 0);
                                                }
                                                session_chat_buffers[active_session_id].emplace_back("", has_colors() ? 7 : 0);
                                                session_chat_buffers[active_session_id].emplace_back("Available Commands:", has_colors() ? 2 : 0);
                                                session_chat_buffers[active_session_id].emplace_back("  - list              : Show all repositories", has_colors() ? 7 : 0);
                                                session_chat_buffers[active_session_id].emplace_back("  - enable <repo_id>  : Enable a repository", has_colors() ? 7 : 0);
                                                session_chat_buffers[active_session_id].emplace_back("  - disable <repo_id> : Disable a repository", has_colors() ? 7 : 0);
                                                session_chat_buffers[active_session_id].emplace_back("  - status <repo_id>  : Check repository status", has_colors() ? 7 : 0);
                                            } else {
                                                session_chat_buffers[active_session_id].emplace_back("No configuration found for this account", has_colors() ? 4 : 0);
                                            }
                                        }
                                        break;
                                    case SessionType::REPO_MANAGER:
                                    case SessionType::REPO_WORKER:
                                        session_type_str = session.type == SessionType::REPO_MANAGER ? "REPO MANAGER" : "REPO WORKER";
                                        session_color = has_colors() ? 3 : 0; // Green
                                        session_chat_buffers[active_session_id].emplace_back(session_type_str + " Session: " + session.session_id, session_color);
                                        session_chat_buffers[active_session_id].emplace_back("Model: " + session.model, has_colors() ? 7 : 0);
                                        session_chat_buffers[active_session_id].emplace_back("Repo: " + session.repo_path, has_colors() ? 7 : 0);
                                        session_chat_buffers[active_session_id].emplace_back("", has_colors() ? 7 : 0);
                                        session_chat_buffers[active_session_id].emplace_back("Use this session for repository development work", has_colors() ? 5 : 0);
                                        break;
                                    default:
                                        session_type_str = "UNKNOWN";
                                        session_color = has_colors() ? 6 : 0; // Magenta
                                        session_chat_buffers[active_session_id].emplace_back("UNKNOWN Session: " + session.session_id, session_color);
                                        break;
                                }
                                
                                session_chat_buffers[active_session_id].emplace_back("", has_colors() ? 7 : 0);
                                session_chat_buffers[active_session_id].emplace_back("--- Session started ---", has_colors() ? 5 : 0);
                            }
                            
                            // Update chat window to show the selected session's content
                            redraw_chat_window();
                        }
                    }
                } else if (ch == KEY_PPAGE) {  // Page Up
                    list_scroll_offset = std::min(list_scroll_offset + list_height - 1, 
                                                 (int)session_list_buffer.size() - list_height);
                    redraw_session_list();
                } else if (ch == KEY_NPAGE) {  // Page Down
                    list_scroll_offset = std::max(list_scroll_offset - list_height + 1, 0);
                    redraw_session_list();
                } else if (ch == 3) {  // Ctrl+C
                    // Double Ctrl+C pattern: first press shows message, second press exits
                    auto now = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_ctrl_c_time).count();

                    if (ctrl_c_pressed_recently && elapsed < 2000) {
                        // Second Ctrl+C within 2 seconds - exit
                        chat_buffer.emplace_back("^C (exiting manager mode)", has_colors() ? 4 : 0);
                        redraw_chat_window();
                        should_exit = true;
                    } else {
                        // First Ctrl+C - show message
                        chat_buffer.emplace_back("^C (press Ctrl+C again to exit)", has_colors() ? 3 : 0);
                        redraw_chat_window();
                        ctrl_c_pressed_recently = true;
                        last_ctrl_c_time = now;
                    }
                }
            } else {
                // Input window has focus
                if (ch == '\n' || ch == KEY_ENTER || ch == 13) {
                    if (!input_buffer.empty()) {
                        // Determine which session buffer to add the message to
                        std::vector<OutputLine>* target_buffer = &chat_buffer;
                        
                        if (!active_session_id.empty()) {
                            // Add to active session's specific buffer
                            if (session_chat_buffers.find(active_session_id) != session_chat_buffers.end()) {
                                target_buffer = &session_chat_buffers[active_session_id];
                            }
                        }
                        
                        // Process input command
                        if (input_buffer == "/exit" || input_buffer == "/quit") {
                            should_exit = true;
                        } else if (input_buffer == "/list") {
                            update_session_list();  // Refresh the list
                            target_buffer->emplace_back("Session list refreshed", has_colors() ? 5 : 0);
                            redraw_chat_window();
                        } else if (input_buffer == "/status") {
                            target_buffer->emplace_back("Manager status:", has_colors() ? 5 : 0);
                            target_buffer->emplace_back("  - Running: " + std::string(running_ ? "Yes" : "No"), has_colors() ? 5 : 0);
                            target_buffer->emplace_back("  - TCP Server: " + std::string(tcp_server_ && tcp_server_->is_running() ? "Active" : "Inactive"), has_colors() ? 5 : 0);
                            target_buffer->emplace_back("  - Active Sessions: " + std::to_string(sessions_.size()), has_colors() ? 5 : 0);
                            redraw_chat_window();
                        } else if (input_buffer == "/clear") {
                            target_buffer->clear();
                            target_buffer->emplace_back("Session buffer cleared", has_colors() ? 5 : 0);
                            redraw_chat_window();
                        } else if (input_buffer == "/auto") {
                            // Switch back to automatic mode
                            if (!active_session_id.empty()) {
                                update_session_state(active_session_id, SessionState::AUTOMATIC);
                                target_buffer->emplace_back("You: /auto", has_colors() ? 7 : 0);
                                target_buffer->emplace_back("Session returned to automatic mode", has_colors() ? 5 : 0);
                                
                                // Update the session's chat buffer header to reflect the state change
                                ManagerSessionInfo* session = find_session(active_session_id);
                                if (session) {
                                    std::string session_type_str;
                                    int session_color = has_colors() ? 7 : 0;
                                    
                                    switch (session->type) {
                                        case SessionType::REPO_WORKER:
                                        case SessionType::REPO_MANAGER:
                                            session_type_str = (session->type == SessionType::REPO_MANAGER ? "REPO MANAGER" : "REPO WORKER");
                                            session_color = has_colors() ? 3 : 0;
                                            // Update the status in the session list to show automatic mode
                                            break;
                                        default:
                                            break;
                                    }
                                }
                            } else {
                                target_buffer->emplace_back("You: /auto", has_colors() ? 7 : 0);
                                target_buffer->emplace_back("No active session to switch to automatic mode", has_colors() ? 4 : 0);
                            }
                            redraw_chat_window();
                        } else if (input_buffer == "/pause") {
                            // Pause the active session
                            if (!active_session_id.empty()) {
                                if (pause_session(active_session_id)) {
                                    target_buffer->emplace_back("You: /pause", has_colors() ? 7 : 0);
                                    target_buffer->emplace_back("Session paused successfully", has_colors() ? 5 : 0);
                                } else {
                                    target_buffer->emplace_back("You: /pause", has_colors() ? 7 : 0);
                                    target_buffer->emplace_back("Failed to pause session", has_colors() ? 4 : 0);
                                }
                            } else {
                                target_buffer->emplace_back("You: /pause", has_colors() ? 7 : 0);
                                target_buffer->emplace_back("No active session to pause", has_colors() ? 4 : 0);
                            }
                            redraw_chat_window();
                        } else if (input_buffer == "/resume") {
                            // Resume the active session
                            if (!active_session_id.empty()) {
                                if (resume_session(active_session_id)) {
                                    target_buffer->emplace_back("You: /resume", has_colors() ? 7 : 0);
                                    target_buffer->emplace_back("Session resumed successfully", has_colors() ? 5 : 0);
                                } else {
                                    target_buffer->emplace_back("You: /resume", has_colors() ? 7 : 0);
                                    target_buffer->emplace_back("Failed to resume session", has_colors() ? 4 : 0);
                                }
                            } else {
                                target_buffer->emplace_back("You: /resume", has_colors() ? 7 : 0);
                                target_buffer->emplace_back("No active session to resume", has_colors() ? 4 : 0);
                            }
                            redraw_chat_window();
                        } else if (input_buffer.substr(0, 6) == "/save ") {
                            // Save a session snapshot
                            if (!active_session_id.empty()) {
                                std::string snapshot_name = input_buffer.substr(6);
                                if (save_session_snapshot(active_session_id, snapshot_name)) {
                                    target_buffer->emplace_back("You: " + input_buffer, has_colors() ? 7 : 0);
                                    target_buffer->emplace_back("Session snapshot saved: " + snapshot_name, has_colors() ? 5 : 0);
                                } else {
                                    target_buffer->emplace_back("You: " + input_buffer, has_colors() ? 7 : 0);
                                    target_buffer->emplace_back("Failed to save session snapshot: " + snapshot_name, has_colors() ? 4 : 0);
                                }
                            } else {
                                target_buffer->emplace_back("You: " + input_buffer, has_colors() ? 7 : 0);
                                target_buffer->emplace_back("No active session to save snapshot", has_colors() ? 4 : 0);
                            }
                            redraw_chat_window();
                        } else if (input_buffer.substr(0, 9) == "/restore ") {
                            // Restore a session snapshot
                            if (!active_session_id.empty()) {
                                std::string snapshot_name = input_buffer.substr(9);
                                if (restore_session_snapshot(active_session_id, snapshot_name)) {
                                    target_buffer->emplace_back("You: " + input_buffer, has_colors() ? 7 : 0);
                                    target_buffer->emplace_back("Session snapshot restored: " + snapshot_name, has_colors() ? 5 : 0);
                                } else {
                                    target_buffer->emplace_back("You: " + input_buffer, has_colors() ? 7 : 0);
                                    target_buffer->emplace_back("Failed to restore session snapshot: " + snapshot_name, has_colors() ? 4 : 0);
                                }
                            } else {
                                target_buffer->emplace_back("You: " + input_buffer, has_colors() ? 7 : 0);
                                target_buffer->emplace_back("No active session to restore snapshot", has_colors() ? 4 : 0);
                            }
                            redraw_chat_window();
                        } else if (input_buffer == "/snapshots") {
                            // List session snapshots
                            if (!active_session_id.empty()) {
                                std::vector<std::string> snapshots = list_session_snapshots(active_session_id);
                                target_buffer->emplace_back("You: /snapshots", has_colors() ? 7 : 0);
                                if (snapshots.empty()) {
                                    target_buffer->emplace_back("No snapshots found for session", has_colors() ? 5 : 0);
                                } else {
                                    target_buffer->emplace_back("Available snapshots:", has_colors() ? 5 : 0);
                                    for (const auto& snapshot : snapshots) {
                                        target_buffer->emplace_back("  - " + snapshot, has_colors() ? 7 : 0);
                                    }
                                }
                            } else {
                                target_buffer->emplace_back("You: /snapshots", has_colors() ? 7 : 0);
                                target_buffer->emplace_back("No active session to list snapshots", has_colors() ? 4 : 0);
                            }
                            redraw_chat_window();
                        } else if (input_buffer.substr(0, 7) == "/group ") {
                            // Create a new session group
                            std::string group_info = input_buffer.substr(7);
                            size_t desc_pos = group_info.find(" - ");
                            std::string group_name, group_desc;
                            
                            if (desc_pos != std::string::npos) {
                                group_name = group_info.substr(0, desc_pos);
                                group_desc = group_info.substr(desc_pos + 3);
                            } else {
                                group_name = group_info;
                                group_desc = "Session group created at " + std::to_string(time(nullptr));
                            }
                            
                            std::string group_id = create_session_group(group_name, group_desc);
                            target_buffer->emplace_back("You: " + input_buffer, has_colors() ? 7 : 0);
                            target_buffer->emplace_back("Created session group: " + group_name + " (" + group_id + ")", has_colors() ? 5 : 0);
                            redraw_chat_window();
                        } else if (input_buffer == "/groups") {
                            // List all session groups
                            std::vector<SessionGroup> groups = list_session_groups();
                            target_buffer->emplace_back("You: /groups", has_colors() ? 7 : 0);
                            if (groups.empty()) {
                                target_buffer->emplace_back("No session groups found", has_colors() ? 5 : 0);
                            } else {
                                target_buffer->emplace_back("Session groups:", has_colors() ? 5 : 0);
                                for (const auto& group : groups) {
                                    target_buffer->emplace_back("  - " + group.name + " (" + group.group_id + "): " + group.description, has_colors() ? 7 : 0);
                                    // Show sessions in this group
                                    std::vector<ManagerSessionInfo*> group_sessions = get_sessions_in_group(group.group_id);
                                    if (!group_sessions.empty()) {
                                        for (const auto& session : group_sessions) {
                                            std::string session_type;
                                            switch (session->type) {
                                                case SessionType::MANAGER_PROJECT: session_type = "PROJECT_MGR"; break;
                                                case SessionType::MANAGER_TASK: session_type = "TASK_MGR"; break;
                                                case SessionType::ACCOUNT: session_type = "ACCOUNT"; break;
                                                case SessionType::REPO_MANAGER: session_type = "REPO_MGR"; break;
                                                case SessionType::REPO_WORKER: session_type = "REPO_WRK"; break;
                                                default: session_type = "UNKNOWN"; break;
                                            }
                                            target_buffer->emplace_back("    * " + session_type + " " + session->session_id, has_colors() ? 7 : 0);
                                        }
                                    }
                                }
                            }
                            redraw_chat_window();
                        } else if (input_buffer.substr(0, 11) == "/addtogroup ") {
                            // Add current session to a group
                            if (!active_session_id.empty()) {
                                std::string group_id = input_buffer.substr(11);
                                if (add_session_to_group(active_session_id, group_id)) {
                                    target_buffer->emplace_back("You: " + input_buffer, has_colors() ? 7 : 0);
                                    target_buffer->emplace_back("Added session to group: " + group_id, has_colors() ? 5 : 0);
                                } else {
                                    target_buffer->emplace_back("You: " + input_buffer, has_colors() ? 7 : 0);
                                    target_buffer->emplace_back("Failed to add session to group: " + group_id, has_colors() ? 4 : 0);
                                }
                            } else {
                                target_buffer->emplace_back("You: " + input_buffer, has_colors() ? 7 : 0);
                                target_buffer->emplace_back("No active session to add to group", has_colors() ? 4 : 0);
                            }
                            redraw_chat_window();
                        } else if (active_session_id.empty()) {
                            // No active session, just echo the command
                            target_buffer->emplace_back("You: " + input_buffer, has_colors() ? 7 : 0);
                            target_buffer->emplace_back("AI: No active session selected", has_colors() ? 6 : 0);
                            redraw_chat_window();
                        } else {
                            // Handle session-specific commands
                            bool is_account_session = false;
                            ManagerSessionInfo* active_session = find_session(active_session_id);
                            
                            if (active_session && active_session->type == SessionType::ACCOUNT) {
                                is_account_session = true;
                                
                                // Handle ACCOUNT-specific commands without slash prefix
                                std::istringstream iss(input_buffer);
                                std::string command;
                                iss >> command;
                                
                                // Extract repository ID if present
                                std::string repo_id;
                                iss >> repo_id;
                                
                                if (command == "list") {
                                    target_buffer->emplace_back("You: list", has_colors() ? 7 : 0);
                                    target_buffer->emplace_back("Listing repositories for account " + active_session->session_id + ":", has_colors() ? 5 : 0);
                                    
                                    // Find the account configuration to display repositories
                                    std::lock_guard<std::mutex> lock(account_configs_mutex_);
                                    auto account_it = std::find_if(account_configs_.begin(), account_configs_.end(),
                                        [active_session](const AccountConfig& acc) { return acc.id == active_session->account_id; });
                                    
                                    if (account_it != account_configs_.end()) {
                                        for (const auto& repo : account_it->repositories) {
                                            std::string status = repo.enabled ? "enabled" : "disabled";
                                            target_buffer->emplace_back("  - " + repo.id + " (" + status + ")", has_colors() ? 7 : 0);
                                        }
                                    } else {
                                        target_buffer->emplace_back("  No repository configuration found", has_colors() ? 4 : 0);
                                    }
                                } 
                                else if (command == "enable") {
                                    target_buffer->emplace_back("You: enable " + repo_id, has_colors() ? 7 : 0);
                                    if (!repo_id.empty()) {
                                        target_buffer->emplace_back("Enabling repository: " + repo_id, has_colors() ? 5 : 0);
                                        
                                        // In a real implementation, this would send an enable command to the account
                                        // via the TCP connection
                                    } else {
                                        target_buffer->emplace_back("Error: Repository ID required (usage: enable <repo_id>)", has_colors() ? 4 : 0);
                                    }
                                } 
                                else if (command == "disable") {
                                    target_buffer->emplace_back("You: disable " + repo_id, has_colors() ? 7 : 0);
                                    if (!repo_id.empty()) {
                                        target_buffer->emplace_back("Disabling repository: " + repo_id, has_colors() ? 5 : 0);
                                        
                                        // In a real implementation, this would send a disable command to the account
                                        // via the TCP connection
                                    } else {
                                        target_buffer->emplace_back("Error: Repository ID required (usage: disable <repo_id>)", has_colors() ? 4 : 0);
                                    }
                                } 
                                else if (command == "status") {
                                    target_buffer->emplace_back("You: status " + repo_id, has_colors() ? 7 : 0);
                                    if (!repo_id.empty()) {
                                        target_buffer->emplace_back("Getting status for repository: " + repo_id, has_colors() ? 5 : 0);
                                        
                                        // In a real implementation, this would query the status from the account
                                        // via the TCP connection
                                    } else {
                                        target_buffer->emplace_back("Error: Repository ID required (usage: status <repo_id>)", has_colors() ? 4 : 0);
                                    }
                                } 
                                else if (command == "pause") {
                                    target_buffer->emplace_back("You: pause " + repo_id, has_colors() ? 7 : 0);
                                    if (!repo_id.empty()) {
                                        // Find the session for this repository and pause it
                                        ManagerSessionInfo* repo_session = find_session_by_repo(active_session->account_id, repo_id);
                                        if (repo_session) {
                                            if (pause_session(repo_session->session_id)) {
                                                target_buffer->emplace_back("Repository " + repo_id + " paused successfully", has_colors() ? 5 : 0);
                                            } else {
                                                target_buffer->emplace_back("Failed to pause repository " + repo_id, has_colors() ? 4 : 0);
                                            }
                                        } else {
                                            target_buffer->emplace_back("Repository " + repo_id + " not found or not active", has_colors() ? 4 : 0);
                                        }
                                    } else {
                                        target_buffer->emplace_back("Error: Repository ID required (usage: pause <repo_id>)", has_colors() ? 4 : 0);
                                    }
                                }
                                else if (command == "resume") {
                                    target_buffer->emplace_back("You: resume " + repo_id, has_colors() ? 7 : 0);
                                    if (!repo_id.empty()) {
                                        // Find the session for this repository and resume it
                                        ManagerSessionInfo* repo_session = find_session_by_repo(active_session->account_id, repo_id);
                                        if (repo_session) {
                                            if (resume_session(repo_session->session_id)) {
                                                target_buffer->emplace_back("Repository " + repo_id + " resumed successfully", has_colors() ? 5 : 0);
                                            } else {
                                                target_buffer->emplace_back("Failed to resume repository " + repo_id, has_colors() ? 4 : 0);
                                            }
                                        } else {
                                            target_buffer->emplace_back("Repository " + repo_id + " not found or not active", has_colors() ? 4 : 0);
                                        }
                                    } else {
                                        target_buffer->emplace_back("Error: Repository ID required (usage: resume <repo_id>)", has_colors() ? 4 : 0);
                                    }
                                }
                                else if (command == "save") {
                                    target_buffer->emplace_back("You: save " + repo_id, has_colors() ? 7 : 0);
                                    if (!repo_id.empty()) {
                                        // Find the session for this repository and save a snapshot
                                        ManagerSessionInfo* repo_session = find_session_by_repo(active_session->account_id, repo_id);
                                        if (repo_session) {
                                            std::string snapshot_name = "snapshot-" + std::to_string(time(nullptr));
                                            if (save_session_snapshot(repo_session->session_id, snapshot_name)) {
                                                target_buffer->emplace_back("Repository " + repo_id + " snapshot saved: " + snapshot_name, has_colors() ? 5 : 0);
                                            } else {
                                                target_buffer->emplace_back("Failed to save repository " + repo_id + " snapshot", has_colors() ? 4 : 0);
                                            }
                                        } else {
                                            target_buffer->emplace_back("Repository " + repo_id + " not found or not active", has_colors() ? 4 : 0);
                                        }
                                    } else {
                                        target_buffer->emplace_back("Error: Repository ID required (usage: save <repo_id>)", has_colors() ? 4 : 0);
                                    }
                                }
                                else if (command == "snapshots") {
                                    target_buffer->emplace_back("You: snapshots " + repo_id, has_colors() ? 7 : 0);
                                    if (!repo_id.empty()) {
                                        // Find the session for this repository and list snapshots
                                        ManagerSessionInfo* repo_session = find_session_by_repo(active_session->account_id, repo_id);
                                        if (repo_session) {
                                            std::vector<std::string> snapshots = list_session_snapshots(repo_session->session_id);
                                            if (snapshots.empty()) {
                                                target_buffer->emplace_back("No snapshots found for repository " + repo_id, has_colors() ? 5 : 0);
                                            } else {
                                                target_buffer->emplace_back("Snapshots for repository " + repo_id + ":", has_colors() ? 5 : 0);
                                                for (const auto& snapshot : snapshots) {
                                                    target_buffer->emplace_back("  - " + snapshot, has_colors() ? 7 : 0);
                                                }
                                            }
                                        } else {
                                            target_buffer->emplace_back("Repository " + repo_id + " not found or not active", has_colors() ? 4 : 0);
                                        }
                                    } else {
                                        target_buffer->emplace_back("Error: Repository ID required (usage: snapshots <repo_id>)", has_colors() ? 4 : 0);
                                    }
                                }
                                else if (command == "group") {
                                    target_buffer->emplace_back("You: group " + repo_id, has_colors() ? 7 : 0);
                                    if (!repo_id.empty()) {
                                        // Create a new group for this repository
                                        std::string group_id = create_session_group("Repository Group: " + repo_id, 
                                            "Group for repository " + repo_id + " created at " + std::to_string(time(nullptr)));
                                        target_buffer->emplace_back("Created group for repository " + repo_id + ": " + group_id, has_colors() ? 5 : 0);
                                        
                                        // Find the session for this repository and add it to the group
                                        ManagerSessionInfo* repo_session = find_session_by_repo(active_session->account_id, repo_id);
                                        if (repo_session) {
                                            if (add_session_to_group(repo_session->session_id, group_id)) {
                                                target_buffer->emplace_back("Added repository " + repo_id + " to group " + group_id, has_colors() ? 5 : 0);
                                            } else {
                                                target_buffer->emplace_back("Failed to add repository " + repo_id + " to group", has_colors() ? 4 : 0);
                                            }
                                        } else {
                                            target_buffer->emplace_back("Repository " + repo_id + " not found or not active", has_colors() ? 4 : 0);
                                        }
                                    } else {
                                        target_buffer->emplace_back("Error: Repository ID required (usage: group <repo_id>)", has_colors() ? 4 : 0);
                                    }
                                }
                                else {
                                    // Not a recognized command, treat as a regular message
                                    target_buffer->emplace_back("You: " + input_buffer, has_colors() ? 7 : 0);
                                    target_buffer->emplace_back("ACCOUNT: Unknown command. Valid commands: list, enable <repo>, disable <repo>, status <repo>, pause <repo>, resume <repo>, save <repo>, snapshots <repo>, group <repo>", has_colors() ? 4 : 0);
                                }
                            } else if (active_session && 
                                      (active_session->type == SessionType::REPO_WORKER || 
                                       active_session->type == SessionType::REPO_MANAGER)) {
                                // Handle REPO session commands
                                // Check if the session is in manual override mode
                                if (active_session->workflow_state == SessionState::MANUAL) {
                                    // In manual mode, pass the command directly to the AI
                                    target_buffer->emplace_back("You: " + input_buffer, has_colors() ? 7 : 0);
                                    
                                    // In a real implementation, this is where we would send the input to the actual AI session
                                    std::string ai_prefix = active_session->type == SessionType::REPO_MANAGER ? "REPO_MGR" : "REPO_WRK";
                                    target_buffer->emplace_back(ai_prefix + ": Processing request in manual override mode", has_colors() ? 6 : 0);
                                    
                                    // Increment commit count for test interval triggers
                                    increment_commit_count(active_session_id);
                                } else {
                                    // In automatic mode, we don't allow direct commands without switching to manual first
                                    target_buffer->emplace_back("You: " + input_buffer, has_colors() ? 7 : 0);
                                    target_buffer->emplace_back("Automatic mode: Use '/auto' command to return to automatic workflow or switch to manual mode first", has_colors() ? 5 : 0);
                                }
                            } else {
                                // Handle regular sessions (MANAGER_PROJECT, MANAGER_TASK)
                                target_buffer->emplace_back("You: " + input_buffer, has_colors() ? 7 : 0);
                                
                                // In a real implementation, this is where we would send the input to the appropriate AI session
                                // For now, we'll just show a response placeholder
                                if (active_session) {
                                    std::string ai_prefix = active_session->type == SessionType::MANAGER_PROJECT ? "PROJECT_MGR" :
                                                           active_session->type == SessionType::MANAGER_TASK ? "TASK_MGR" :
                                                           active_session->type == SessionType::ACCOUNT ? "ACCOUNT" :
                                                           active_session->type == SessionType::REPO_MANAGER ? "REPO_MGR" :
                                                           active_session->type == SessionType::REPO_WORKER ? "REPO_WRK" : "AI";
                                    
                                    // Placeholder response - in a real implementation, this would go to the actual qwen session
                                    target_buffer->emplace_back(ai_prefix + ": Processing request (actual AI integration pending)", has_colors() ? 6 : 0);
                                } else {
                                    target_buffer->emplace_back("AI: Processing request (session not found)", has_colors() ? 6 : 0);
                                }
                            }
                            
                            redraw_chat_window();
                        }

                        // Clear input buffer
                        input_buffer.clear();
                        cursor_pos = 0;
                    }
                    redraw_input();
                } 
                // Handle BACKSPACE
                else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
                    if (cursor_pos > 0) {
                        input_buffer.erase(cursor_pos - 1, 1);
                        cursor_pos--;
                        redraw_input();
                    }
                } 
                // Handle DELETE
                else if (ch == KEY_DC) {
                    if (cursor_pos < input_buffer.length()) {
                        input_buffer.erase(cursor_pos, 1);
                        redraw_input();
                    }
                } 
                // Handle LEFT arrow
                else if (ch == KEY_LEFT) {
                    if (cursor_pos > 0) {
                        cursor_pos--;
                        redraw_input();
                    }
                } 
                // Handle RIGHT arrow
                else if (ch == KEY_RIGHT) {
                    if (cursor_pos < input_buffer.length()) {
                        cursor_pos++;
                        redraw_input();
                    }
                } 
                // Handle HOME or Ctrl+A
                else if (ch == KEY_HOME || ch == 1) {
                    cursor_pos = 0;
                    redraw_input();
                } 
                // Handle END or Ctrl+E
                else if (ch == KEY_END || ch == 5) {
                    cursor_pos = input_buffer.length();
                    redraw_input();
                } 
                // Handle Ctrl+C
                else if (ch == 3) {  // Ctrl+C
                    // Double Ctrl+C pattern: first press clears input, second press exits
                    auto now = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_ctrl_c_time).count();

                    if (ctrl_c_pressed_recently && elapsed < 2000) {
                        // Second Ctrl+C within 2 seconds - exit
                        chat_buffer.emplace_back("^C (exiting manager mode)", has_colors() ? 4 : 0);
                        redraw_chat_window();
                        should_exit = true;
                    } else {
                        // First Ctrl+C - clear input buffer and show hint
                        input_buffer.clear();
                        cursor_pos = 0;
                        chat_buffer.emplace_back("^C (press Ctrl+C again to exit)", has_colors() ? 3 : 0);
                        redraw_chat_window();
                        redraw_input();
                        ctrl_c_pressed_recently = true;
                        last_ctrl_c_time = now;
                    }
                } 
                // Handle Ctrl+U (scroll up)
                else if (ch == 21) {  // Ctrl+U
                    // Scroll chat buffer up
                    chat_scroll_offset = std::min(chat_scroll_offset + 5, 
                                                 (int)chat_buffer.size() - (max_y - list_height - 4));
                    redraw_chat_window();
                } 
                // Handle Ctrl+D (scroll down)
                else if (ch == 4) {  // Ctrl+D
                    // Scroll chat buffer down
                    chat_scroll_offset = std::max(chat_scroll_offset - 5, 0);
                    redraw_chat_window();
                } 
                // Handle PAGE UP
                else if (ch == KEY_PPAGE) {
                    // For input window, page up scrolls chat window
                    chat_scroll_offset = std::min(chat_scroll_offset + 5, 
                                                 (int)chat_buffer.size() - (max_y - list_height - 4));
                    redraw_chat_window();
                } 
                // Handle PAGE DOWN
                else if (ch == KEY_NPAGE) {
                    // For input window, page down scrolls chat window
                    chat_scroll_offset = std::max(chat_scroll_offset - 5, 0);
                    redraw_chat_window();
                } 
                // Handle printable ASCII characters
                else if (ch >= 32 && ch <= 126 && ch != 127) {
                    input_buffer.insert(cursor_pos, 1, (char)ch);
                    cursor_pos++;
                    redraw_input();
                }
            }
        }

        // Small delay to avoid busy-waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Cleanup ncurses
    delwin(list_win);
    delwin(status_separator_win);
    delwin(chat_win);
    delwin(input_win);
    endwin();

    return true;
#else
    std::cout << "NCurses not available, falling back to simple mode\n";
    return run_simple_mode();
#endif
}

// Run manager mode in simple mode (stdio)
bool QwenManager::run_simple_mode() {
    std::cout << "qwen Manager Mode - Simple Mode\n";
    std::cout << "TCP Server: " << tcp_host_ << ":" << tcp_port_ << "\n";
    std::cout << "Type 'help' for commands, 'exit' to quit\n\n";
    
    // Show initial session list
    update_session_list();
    
    std::string input;
    while (running_) {
        std::cout << "> ";
        std::cout.flush();
        
        if (!std::getline(std::cin, input)) {
            break;  // EOF
        }
        
        // Process simple commands
        if (input == "exit" || input == "quit") {
            break;
        } else if (input == "help") {
            std::cout << "Available commands:\n";
            std::cout << "  help    - Show this help\n";
            std::cout << "  list    - Show all sessions\n";
            std::cout << "  status  - Show manager status\n";
            std::cout << "  exit    - Exit manager mode\n";
        } else if (input == "list") {
            update_session_list();
        } else if (input == "status") {
            std::cout << "Manager Status:\n";
            std::cout << "  TCP Server: " << (tcp_server_ && tcp_server_->is_running() ? "Running" : "Stopped") 
                     << " on " << tcp_host_ << ":" << tcp_port_ << "\n";
            std::cout << "  Active Sessions: " << sessions_.size() << "\n";
            std::cout << "  Running: " << (running_ ? "Yes" : "No") << "\n";
        } else {
            std::cout << "Unknown command: " << input << "\n";
            std::cout << "Type 'help' for available commands\n";
        }
    }
    
    return true;
}

void QwenManager::update_session_list() {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    std::cout << "Current Sessions:\n";
    std::cout << "Type      | ID          | Computer   | Repo Path                    | Status\n";
    std::cout << "----------|-------------|------------|------------------------------|-------\n";
    for (const auto& session : sessions_) {
        std::string type_str;
        switch (session.type) {
            case SessionType::MANAGER_PROJECT:
                type_str = "MGR-PROJ ";
                break;
            case SessionType::MANAGER_TASK:
                type_str = "MGR-TASK ";
                break;
            case SessionType::ACCOUNT:
                type_str = "ACCOUNT  ";
                break;
            case SessionType::REPO_MANAGER:
                type_str = "REPO-MGR ";
                break;
            case SessionType::REPO_WORKER:
                type_str = "REPO-WRK ";
                break;
            default:
                type_str = "UNKNOWN  ";
                break;
        }
        
        std::string status_str = session.is_active ? "active" : "inactive";
        std::cout << type_str << " | " 
                 << session.session_id.substr(0, 11) << " | "
                 << session.hostname.substr(0, 10) << " | "
                 << session.repo_path.substr(0, 26) << " | "
                 << status_str << "\n";
    }
    std::cout << "\n";
}

} // namespace Qwen