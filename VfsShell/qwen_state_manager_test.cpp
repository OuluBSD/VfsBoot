#include "VfsShell.h"

// Global instance needed by upp_workspace_build.cpp
extern UppBuilderRegistry g_upp_builder_registry;

using namespace Qwen;

// Test helpers
void test_header(const std::string& name) {
    std::cout << "\n=== " << name << " ===" << std::endl;
}

void test_pass(const std::string& msg) {
    std::cout << "  [PASS] " << msg << std::endl;
}

void test_fail(const std::string& msg) {
    std::cout << "  [FAIL] " << msg << std::endl;
    exit(1);
}

// ============================================================================
// Test Cases
// ============================================================================

void test_session_creation(QwenStateManager& mgr) {
    test_header("Session Creation");

    // Create a new session
    std::string session_id = mgr.create_session("qwen2.5-coder-7b", "/workspace");
    if (session_id.empty()) {
        test_fail("Failed to create session");
    }
    test_pass("Session created: " + session_id);

    // Verify session exists
    if (!mgr.session_exists(session_id)) {
        test_fail("Session does not exist");
    }
    test_pass("Session exists");

    // Get session info
    auto info = mgr.get_session_info(session_id);
    if (!info) {
        test_fail("Failed to get session info");
    }
    test_pass("Session info retrieved");

    // Verify session info
    if (info->model != "qwen2.5-coder-7b") {
        test_fail("Model mismatch");
    }
    if (info->workspace_root != "/workspace") {
        test_fail("Workspace root mismatch");
    }
    test_pass("Session info matches");
}

void test_conversation_history(QwenStateManager& mgr) {
    test_header("Conversation History");

    // Create a session
    std::string session_id = mgr.create_session();
    if (session_id.empty()) {
        test_fail("Failed to create session");
    }

    // Add messages
    ConversationMessage msg1;
    msg1.role = MessageRole::USER;
    msg1.content = "Hello, world!";
    msg1.id = 1;

    if (!mgr.add_message(msg1)) {
        test_fail("Failed to add message 1");
    }
    test_pass("Added message 1");

    ConversationMessage msg2;
    msg2.role = MessageRole::ASSISTANT;
    msg2.content = "Hello! How can I help you?";
    msg2.id = 2;

    if (!mgr.add_message(msg2)) {
        test_fail("Failed to add message 2");
    }
    test_pass("Added message 2");

    // Get history
    auto history = mgr.get_history();
    if (history.size() != 2) {
        test_fail("History size mismatch: expected 2, got " + std::to_string(history.size()));
    }
    test_pass("History has 2 messages");

    // Verify messages
    if (history[0].content != "Hello, world!") {
        test_fail("Message 1 content mismatch");
    }
    if (history[1].content != "Hello! How can I help you?") {
        test_fail("Message 2 content mismatch");
    }
    test_pass("Messages match");

    // Get recent messages
    auto recent = mgr.get_recent_messages(1);
    if (recent.size() != 1) {
        test_fail("Recent messages count mismatch");
    }
    if (recent[0].content != "Hello! How can I help you?") {
        test_fail("Recent message content mismatch");
    }
    test_pass("Recent messages work");

    // Get message count
    int count = mgr.get_message_count();
    if (count != 2) {
        test_fail("Message count mismatch");
    }
    test_pass("Message count correct");
}

void test_file_storage(QwenStateManager& mgr) {
    test_header("File Storage");

    // Create a session
    std::string session_id = mgr.create_session();
    if (session_id.empty()) {
        test_fail("Failed to create session");
    }

    // Store a file
    std::string file_path = mgr.store_file("test.txt", "Hello from file!");
    if (file_path.empty()) {
        test_fail("Failed to store file");
    }
    test_pass("File stored: " + file_path);

    // List files
    auto files = mgr.list_files();
    if (files.size() != 1) {
        test_fail("File count mismatch");
    }
    if (files[0] != "test.txt") {
        test_fail("File name mismatch");
    }
    test_pass("File listed correctly");

    // Retrieve file
    auto content = mgr.retrieve_file("test.txt");
    if (!content) {
        test_fail("Failed to retrieve file");
    }
    if (*content != "Hello from file!") {
        test_fail("File content mismatch");
    }
    test_pass("File content matches");

    // Delete file
    if (!mgr.delete_file("test.txt")) {
        test_fail("Failed to delete file");
    }
    test_pass("File deleted");

    // Verify deletion
    files = mgr.list_files();
    if (files.size() != 0) {
        test_fail("File not deleted");
    }
    test_pass("File deletion verified");
}

void test_session_metadata(QwenStateManager& mgr) {
    test_header("Session Metadata");

    // Create a session
    std::string session_id = mgr.create_session();
    if (session_id.empty()) {
        test_fail("Failed to create session");
    }

    // Set workspace root
    if (!mgr.set_workspace_root("/new/workspace")) {
        test_fail("Failed to set workspace root");
    }
    test_pass("Workspace root set");

    // Get workspace root
    std::string workspace = mgr.get_workspace_root();
    if (workspace != "/new/workspace") {
        test_fail("Workspace root mismatch");
    }
    test_pass("Workspace root matches");

    // Set model
    if (!mgr.set_model("gpt-4")) {
        test_fail("Failed to set model");
    }
    test_pass("Model set");

    // Get model
    std::string model = mgr.get_model();
    if (model != "gpt-4") {
        test_fail("Model mismatch");
    }
    test_pass("Model matches");

    // Add tags
    if (!mgr.add_session_tag("important")) {
        test_fail("Failed to add tag");
    }
    test_pass("Tag added");

    if (!mgr.add_session_tag("work")) {
        test_fail("Failed to add second tag");
    }
    test_pass("Second tag added");

    // Get tags
    auto tags = mgr.get_session_tags();
    if (tags.size() != 2) {
        test_fail("Tag count mismatch");
    }
    test_pass("Tags count correct");

    // Remove tag
    if (!mgr.remove_session_tag("work")) {
        test_fail("Failed to remove tag");
    }
    test_pass("Tag removed");

    tags = mgr.get_session_tags();
    if (tags.size() != 1) {
        test_fail("Tag count after removal mismatch");
    }
    test_pass("Tag removal verified");
}

void test_session_list_and_load(QwenStateManager& mgr) {
    test_header("Session List and Load");

    // Create multiple sessions
    std::string session1 = mgr.create_session("model1", "/workspace1");
    std::string session2 = mgr.create_session("model2", "/workspace2");

    if (session1.empty() || session2.empty()) {
        test_fail("Failed to create sessions");
    }
    test_pass("Created 2 sessions");

    // List sessions
    auto sessions = mgr.list_sessions();
    if (sessions.size() < 2) {
        test_fail("Session list too short");
    }
    test_pass("Sessions listed");

    // Load first session
    if (!mgr.load_session(session1)) {
        test_fail("Failed to load session 1");
    }
    test_pass("Session 1 loaded");

    // Verify current session
    if (mgr.get_current_session() != session1) {
        test_fail("Current session mismatch");
    }
    test_pass("Current session matches");

    // Load second session
    if (!mgr.load_session(session2)) {
        test_fail("Failed to load session 2");
    }
    test_pass("Session 2 loaded");

    if (mgr.get_current_session() != session2) {
        test_fail("Current session mismatch after switch");
    }
    test_pass("Current session switched");
}

void test_session_save_and_delete(QwenStateManager& mgr) {
    test_header("Session Save and Delete");

    // Create a session
    std::string session_id = mgr.create_session();
    if (session_id.empty()) {
        test_fail("Failed to create session");
    }

    // Add some data
    ConversationMessage msg;
    msg.role = MessageRole::USER;
    msg.content = "Test message";
    msg.id = 1;
    mgr.add_message(msg);

    // Save session
    if (!mgr.save_session()) {
        test_fail("Failed to save session");
    }
    test_pass("Session saved");

    // Delete session
    if (!mgr.delete_session(session_id)) {
        test_fail("Failed to delete session");
    }
    test_pass("Session deleted");

    // Verify deletion
    if (mgr.session_exists(session_id)) {
        test_fail("Session still exists after deletion");
    }
    test_pass("Session deletion verified");
}

void test_storage_stats(QwenStateManager& mgr) {
    test_header("Storage Stats");

    // Create a session with data
    std::string session_id = mgr.create_session();

    ConversationMessage msg;
    msg.role = MessageRole::USER;
    msg.content = "Test message";
    msg.id = 1;
    mgr.add_message(msg);

    mgr.store_file("test.txt", "Test content");

    // Get stats
    auto stats = mgr.get_storage_stats();
    if (stats.total_sessions == 0) {
        test_fail("No sessions in stats");
    }
    test_pass("Stats have sessions: " + std::to_string(stats.total_sessions));

    if (stats.total_messages == 0) {
        test_fail("No messages in stats");
    }
    test_pass("Stats have messages: " + std::to_string(stats.total_messages));

    if (stats.total_files == 0) {
        test_fail("No files in stats");
    }
    test_pass("Stats have files: " + std::to_string(stats.total_files));
}

void test_tool_groups(QwenStateManager& mgr) {
    test_header("Tool Groups");

    // Create a session
    std::string session_id = mgr.create_session();
    if (session_id.empty()) {
        test_fail("Failed to create session");
    }

    // Create a tool group
    ToolGroup group;
    group.id = 1;

    ToolCall tool1;
    tool1.tool_id = "tool-1";
    tool1.tool_name = "read_file";
    tool1.status = ToolStatus::PENDING;
    tool1.args["path"] = "/test.txt";

    group.tools.push_back(tool1);

    // Add tool group
    if (!mgr.add_tool_group(group)) {
        test_fail("Failed to add tool group");
    }
    test_pass("Tool group added");

    // Get tool groups
    auto groups = mgr.get_tool_groups();
    if (groups.size() != 1) {
        test_fail("Tool groups count mismatch");
    }
    test_pass("Tool groups retrieved");

    // Get specific tool group
    auto retrieved = mgr.get_tool_group(1);
    if (!retrieved) {
        test_fail("Failed to get tool group by ID");
    }
    test_pass("Tool group retrieved by ID");

    // Update tool status
    if (!mgr.update_tool_status(1, "tool-1", ToolStatus::SUCCESS)) {
        test_fail("Failed to update tool status");
    }
    test_pass("Tool status updated");
}

// ============================================================================
// Main
// ============================================================================

int qwen_state_tests() {
    std::cout << "Qwen State Manager Test Suite" << std::endl;
    std::cout << "==============================" << std::endl;

    // Create VFS instance
    Vfs vfs;

    // Create state manager
    StateManagerConfig config;
    QwenStateManager mgr(&vfs, config);

    try {
        // Run all tests
        test_session_creation(mgr);
        test_conversation_history(mgr);
        test_file_storage(mgr);
        test_session_metadata(mgr);
        test_session_list_and_load(mgr);
        test_session_save_and_delete(mgr);
        test_storage_stats(mgr);
        test_tool_groups(mgr);

        std::cout << "\n==================================" << std::endl;
        std::cout << "All tests PASSED!" << std::endl;
        std::cout << "==================================" << std::endl;

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n[ERROR] Exception: " << e.what() << std::endl;
        return 1;
    }
}
