#include "VfsShell.h"

using namespace Qwen;

// Simple test framework
#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    std::cout << "Running " #name "... "; \
    try { \
        test_##name(); \
        std::cout << "PASS\n"; \
        passed++; \
    } catch (const std::exception& e) { \
        std::cout << "FAIL: " << e.what() << "\n"; \
        failed++; \
    } \
    total++; \
} while(0)

static int total = 0;
static int passed = 0;
static int failed = 0;

// ============================================================================
// Tests for Protocol Parser
// ============================================================================

TEST(parse_init_message) {
    std::string json = R"({"type":"init","version":"0.0.14","workspaceRoot":"/test","model":"qwen"})";

    auto msg = ProtocolParser::parse_message(json);
    assert(msg != nullptr);
    assert(msg->type == MessageType::INIT);

    auto* init = msg->as_init();
    assert(init != nullptr);
    // Note: Current parser doesn't extract fields yet, just identifies type
    // TODO: Implement full field parsing
}

TEST(parse_conversation_message) {
    std::string json = R"({"type":"conversation","role":"user","content":"hello world","id":1})";

    auto msg = ProtocolParser::parse_message(json);
    assert(msg != nullptr);
    assert(msg->type == MessageType::CONVERSATION);

    auto* conv = msg->as_conversation();
    assert(conv != nullptr);
}

TEST(parse_tool_group_message) {
    std::string json = R"({"type":"tool_group","id":1,"tools":[]})";

    auto msg = ProtocolParser::parse_message(json);
    assert(msg != nullptr);
    assert(msg->type == MessageType::TOOL_GROUP);

    auto* tools = msg->as_tool_group();
    assert(tools != nullptr);
}

TEST(parse_status_message) {
    std::string json = R"({"type":"status","state":"idle","message":"Ready"})";

    auto msg = ProtocolParser::parse_message(json);
    assert(msg != nullptr);
    assert(msg->type == MessageType::STATUS);

    auto* status = msg->as_status();
    assert(status != nullptr);
}

TEST(parse_info_message) {
    std::string json = R"({"type":"info","message":"Test info","id":1})";

    auto msg = ProtocolParser::parse_message(json);
    assert(msg != nullptr);
    assert(msg->type == MessageType::INFO);
}

TEST(parse_error_message) {
    std::string json = R"({"type":"error","message":"Test error","id":1})";

    auto msg = ProtocolParser::parse_message(json);
    assert(msg != nullptr);
    assert(msg->type == MessageType::ERROR);
}

TEST(parse_invalid_json) {
    std::string json = R"({invalid json})";

    auto msg = ProtocolParser::parse_message(json);
    assert(msg == nullptr);  // Should return nullptr for invalid JSON
}

// ============================================================================
// Tests for Command Serialization
// ============================================================================

TEST(serialize_user_input) {
    auto cmd = ProtocolParser::create_user_input("hello world");
    std::string json = ProtocolParser::serialize_command(cmd);

    assert(json.find("\"type\":\"user_input\"") != std::string::npos);
    assert(json.find("\"content\":\"hello world\"") != std::string::npos);
}

TEST(serialize_user_input_with_escaping) {
    auto cmd = ProtocolParser::create_user_input("hello \"world\"\ntest");
    std::string json = ProtocolParser::serialize_command(cmd);

    assert(json.find("\"type\":\"user_input\"") != std::string::npos);
    assert(json.find("\\\"") != std::string::npos);  // Should have escaped quotes
    assert(json.find("\\n") != std::string::npos);   // Should have escaped newline
}

TEST(serialize_tool_approval) {
    auto cmd = ProtocolParser::create_tool_approval("abc123", true);
    std::string json = ProtocolParser::serialize_command(cmd);

    assert(json.find("\"type\":\"tool_approval\"") != std::string::npos);
    assert(json.find("\"tool_id\":\"abc123\"") != std::string::npos);
    assert(json.find("\"approved\":true") != std::string::npos);
}

TEST(serialize_tool_rejection) {
    auto cmd = ProtocolParser::create_tool_approval("def456", false);
    std::string json = ProtocolParser::serialize_command(cmd);

    assert(json.find("\"approved\":false") != std::string::npos);
}

TEST(serialize_interrupt) {
    auto cmd = ProtocolParser::create_interrupt();
    std::string json = ProtocolParser::serialize_command(cmd);

    assert(json.find("\"type\":\"interrupt\"") != std::string::npos);
}

TEST(serialize_model_switch) {
    auto cmd = ProtocolParser::create_model_switch("qwen2.5-coder-32b");
    std::string json = ProtocolParser::serialize_command(cmd);

    assert(json.find("\"type\":\"model_switch\"") != std::string::npos);
    assert(json.find("\"model_id\":\"qwen2.5-coder-32b\"") != std::string::npos);
}

// ============================================================================
// Tests for Enum Converters
// ============================================================================

TEST(message_type_to_string) {
    assert(std::strcmp(message_type_to_string(MessageType::INIT), "init") == 0);
    assert(std::strcmp(message_type_to_string(MessageType::CONVERSATION), "conversation") == 0);
    assert(std::strcmp(message_type_to_string(MessageType::TOOL_GROUP), "tool_group") == 0);
    assert(std::strcmp(message_type_to_string(MessageType::STATUS), "status") == 0);
}

TEST(command_type_to_string) {
    assert(std::strcmp(command_type_to_string(CommandType::USER_INPUT), "user_input") == 0);
    assert(std::strcmp(command_type_to_string(CommandType::TOOL_APPROVAL), "tool_approval") == 0);
    assert(std::strcmp(command_type_to_string(CommandType::INTERRUPT), "interrupt") == 0);
}

TEST(message_role_to_string) {
    assert(std::strcmp(message_role_to_string(MessageRole::USER), "user") == 0);
    assert(std::strcmp(message_role_to_string(MessageRole::ASSISTANT), "assistant") == 0);
    assert(std::strcmp(message_role_to_string(MessageRole::SYSTEM), "system") == 0);
}

TEST(tool_status_to_string) {
    assert(std::strcmp(tool_status_to_string(ToolStatus::PENDING), "pending") == 0);
    assert(std::strcmp(tool_status_to_string(ToolStatus::EXECUTING), "executing") == 0);
    assert(std::strcmp(tool_status_to_string(ToolStatus::SUCCESS), "success") == 0);
    assert(std::strcmp(tool_status_to_string(ToolStatus::ERROR), "error") == 0);
}

TEST(app_state_to_string) {
    assert(std::strcmp(app_state_to_string(AppState::IDLE), "idle") == 0);
    assert(std::strcmp(app_state_to_string(AppState::RESPONDING), "responding") == 0);
    assert(std::strcmp(app_state_to_string(AppState::WAITING_FOR_CONFIRMATION), "waiting_for_confirmation") == 0);
}

// ============================================================================
// Main Test Runner
// ============================================================================

int qwen_protocol_tests() {
    std::cout << "=== Qwen Protocol Tests ===\n\n";

    // Parse tests
    RUN_TEST(parse_init_message);
    RUN_TEST(parse_conversation_message);
    RUN_TEST(parse_tool_group_message);
    RUN_TEST(parse_status_message);
    RUN_TEST(parse_info_message);
    RUN_TEST(parse_error_message);
    RUN_TEST(parse_invalid_json);

    // Serialize tests
    RUN_TEST(serialize_user_input);
    RUN_TEST(serialize_user_input_with_escaping);
    RUN_TEST(serialize_tool_approval);
    RUN_TEST(serialize_tool_rejection);
    RUN_TEST(serialize_interrupt);
    RUN_TEST(serialize_model_switch);

    // Enum converter tests
    RUN_TEST(message_type_to_string);
    RUN_TEST(command_type_to_string);
    RUN_TEST(message_role_to_string);
    RUN_TEST(tool_status_to_string);
    RUN_TEST(app_state_to_string);

    std::cout << "\n=== Test Summary ===\n";
    std::cout << "Total:  " << total << "\n";
    std::cout << "Passed: " << passed << "\n";
    std::cout << "Failed: " << failed << "\n";

    return (failed == 0) ? 0 : 1;
}
