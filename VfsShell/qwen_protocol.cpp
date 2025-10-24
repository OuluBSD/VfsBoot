#include "VfsShell.h"
#include <iomanip>

namespace Qwen {

// ============================================================================
// Simple JSON Parser (minimal implementation for our protocol)
// ============================================================================

// Helper: Skip whitespace
static void skip_whitespace(const char*& p) {
    while (*p && std::isspace(*p)) p++;
}

// Helper: Parse string value
static std::string parse_string(const char*& p) {
    if (*p != '"') throw std::runtime_error("Expected '\"'");
    p++; // Skip opening quote

    std::string result;
    while (*p && *p != '"') {
        if (*p == '\\') {
            p++;
            switch (*p) {
                case 'n': result += '\n'; break;
                case 't': result += '\t'; break;
                case 'r': result += '\r'; break;
                case '\\': result += '\\'; break;
                case '"': result += '"'; break;
                default: result += *p;
            }
            p++;
        } else {
            result += *p++;
        }
    }

    if (*p != '"') throw std::runtime_error("Unterminated string");
    p++; // Skip closing quote

    return result;
}

// Helper: Parse number
static int parse_number(const char*& p) {
    int sign = 1;
    if (*p == '-') {
        sign = -1;
        p++;
    }

    int result = 0;
    while (*p && std::isdigit(*p)) {
        result = result * 10 + (*p - '0');
        p++;
    }

    return result * sign;
}

// Helper: Parse boolean
static bool parse_bool(const char*& p) {
    if (strncmp(p, "true", 4) == 0) {
        p += 4;
        return true;
    } else if (strncmp(p, "false", 5) == 0) {
        p += 5;
        return false;
    }
    throw std::runtime_error("Expected boolean");
}

// Helper: Skip value (for fields we don't need)
static void skip_value(const char*& p);

static void skip_object(const char*& p) {
    if (*p != '{') throw std::runtime_error("Expected '{'");
    p++;

    int depth = 1;
    while (*p && depth > 0) {
        if (*p == '{') depth++;
        else if (*p == '}') depth--;
        else if (*p == '"') {
            p++;
            while (*p && *p != '"') {
                if (*p == '\\') p++;
                p++;
            }
        }
        p++;
    }
}

static void skip_array(const char*& p) {
    if (*p != '[') throw std::runtime_error("Expected '['");
    p++;

    int depth = 1;
    while (*p && depth > 0) {
        if (*p == '[') depth++;
        else if (*p == ']') depth--;
        else if (*p == '"') {
            p++;
            while (*p && *p != '"') {
                if (*p == '\\') p++;
                p++;
            }
        }
        p++;
    }
}

static void skip_value(const char*& p) {
    skip_whitespace(p);
    if (*p == '"') {
        parse_string(p);
    } else if (*p == '{') {
        skip_object(p);
    } else if (*p == '[') {
        skip_array(p);
    } else if (std::isdigit(*p) || *p == '-') {
        while (*p && (std::isdigit(*p) || *p == '.' || *p == '-' || *p == 'e' || *p == 'E')) p++;
    } else if (strncmp(p, "true", 4) == 0) {
        p += 4;
    } else if (strncmp(p, "false", 5) == 0) {
        p += 5;
    } else if (strncmp(p, "null", 4) == 0) {
        p += 4;
    }
}

// ============================================================================
// Enum Converters
// ============================================================================

MessageRole ProtocolParser::parse_role(const std::string& role_str) {
    if (role_str == "user") return MessageRole::USER;
    if (role_str == "assistant") return MessageRole::ASSISTANT;
    if (role_str == "system") return MessageRole::SYSTEM;
    throw std::runtime_error("Unknown role: " + role_str);
}

ToolStatus ProtocolParser::parse_tool_status(const std::string& status_str) {
    if (status_str == "pending") return ToolStatus::PENDING;
    if (status_str == "confirming") return ToolStatus::CONFIRMING;
    if (status_str == "executing") return ToolStatus::EXECUTING;
    if (status_str == "success") return ToolStatus::SUCCESS;
    if (status_str == "error") return ToolStatus::ERROR;
    if (status_str == "canceled") return ToolStatus::CANCELED;
    throw std::runtime_error("Unknown tool status: " + status_str);
}

AppState ProtocolParser::parse_app_state(const std::string& state_str) {
    if (state_str == "idle") return AppState::IDLE;
    if (state_str == "responding") return AppState::RESPONDING;
    if (state_str == "waiting_for_confirmation") return AppState::WAITING_FOR_CONFIRMATION;
    throw std::runtime_error("Unknown app state: " + state_str);
}

std::string ProtocolParser::role_to_string(MessageRole role) {
    switch (role) {
        case MessageRole::USER: return "user";
        case MessageRole::ASSISTANT: return "assistant";
        case MessageRole::SYSTEM: return "system";
    }
    return "unknown";
}

std::string ProtocolParser::tool_status_to_string(ToolStatus status) {
    switch (status) {
        case ToolStatus::PENDING: return "pending";
        case ToolStatus::CONFIRMING: return "confirming";
        case ToolStatus::EXECUTING: return "executing";
        case ToolStatus::SUCCESS: return "success";
        case ToolStatus::ERROR: return "error";
        case ToolStatus::CANCELED: return "canceled";
    }
    return "unknown";
}

std::string ProtocolParser::app_state_to_string(AppState state) {
    switch (state) {
        case AppState::IDLE: return "idle";
        case AppState::RESPONDING: return "responding";
        case AppState::WAITING_FOR_CONFIRMATION: return "waiting_for_confirmation";
    }
    return "unknown";
}

// ============================================================================
// Protocol Parser Implementation
// ============================================================================

std::unique_ptr<StateMessage> ProtocolParser::parse_message(const std::string& json_str) {
    const char* p = json_str.c_str();
    skip_whitespace(p);

    if (*p != '{') return nullptr;
    p++; // Skip '{'

    auto msg = std::make_unique<StateMessage>();
    std::string type_str;

    // Storage for conversation message fields
    std::string role_str;
    std::string content;
    int id = 0;
    std::optional<bool> is_streaming;

    // Storage for tool_group fields
    int tool_group_id = 0;
    std::string tools_json;  // Raw JSON for tools array

    // Parse JSON object - collect all fields first
    while (*p && *p != '}') {
        skip_whitespace(p);
        if (*p == '}') break;
        if (*p == ',') {
            p++;
            continue;
        }

        // Parse key
        if (*p != '"') return nullptr;
        std::string key = parse_string(p);

        skip_whitespace(p);
        if (*p != ':') return nullptr;
        p++; // Skip ':'
        skip_whitespace(p);

        // Parse value based on key
        if (key == "type") {
            type_str = parse_string(p);
        } else if (key == "role") {
            role_str = parse_string(p);
        } else if (key == "content") {
            content = parse_string(p);
        } else if (key == "id") {
            id = parse_number(p);
            tool_group_id = id;  // Use same id for tool group
        } else if (key == "isStreaming") {
            is_streaming = parse_bool(p);
        } else if (key == "tools") {
            // Capture raw JSON for tools array (we'll parse it later if needed)
            const char* array_start = p;
            skip_value(p);
            tools_json = std::string(array_start, p - array_start);
        } else {
            // Skip unknown fields
            skip_value(p);
        }

        skip_whitespace(p);
    }

    // Determine message type and construct data
    if (type_str == "init") {
        msg->type = MessageType::INIT;
        // TODO: Parse init fields properly
        msg->data = InitMessage{"0.0.14", "/workspace", "qwen"};
    } else if (type_str == "conversation") {
        msg->type = MessageType::CONVERSATION;
        MessageRole role = MessageRole::USER;
        if (!role_str.empty()) {
            role = parse_role(role_str);
        }
        msg->data = ConversationMessage{role, content, id, std::nullopt, is_streaming};
    } else if (type_str == "tool_group") {
        msg->type = MessageType::TOOL_GROUP;
        // TODO: Implement proper tools array parsing
        // Debug logging - remove after fixing parser
        fprintf(stderr, "\n[PROTOCOL DEBUG] tool_group received:\n");
        fprintf(stderr, "  id=%d\n", tool_group_id);
        fprintf(stderr, "  tools_json='%s'\n", tools_json.c_str());
        fprintf(stderr, "  Full JSON: %s\n\n", json_str.c_str());
        msg->data = ToolGroup{tool_group_id, {}};
    } else if (type_str == "status") {
        msg->type = MessageType::STATUS;
        msg->data = StatusUpdate{AppState::IDLE, std::nullopt, std::nullopt};
    } else if (type_str == "info") {
        msg->type = MessageType::INFO;
        msg->data = InfoMessage{"info", 1};
    } else if (type_str == "error") {
        msg->type = MessageType::ERROR;
        msg->data = ErrorMessage{"error", 1};
    } else if (type_str == "completion_stats") {
        msg->type = MessageType::COMPLETION_STATS;
        msg->data = CompletionStats{"1.5s", std::nullopt, std::nullopt};
    } else {
        return nullptr;
    }

    return msg;
}

std::string ProtocolParser::serialize_command(const Command& cmd) {
    std::ostringstream ss;
    ss << "{\"type\":\"";

    switch (cmd.type) {
        case CommandType::USER_INPUT: {
            ss << "user_input\"";
            if (auto* data = cmd.as_user_input()) {
                ss << ",\"content\":\"";
                // Properly escape string for JSON
                for (unsigned char c : data->content) {
                    if (c == '"') ss << "\\\"";
                    else if (c == '\\') ss << "\\\\";
                    else if (c == '\n') ss << "\\n";
                    else if (c == '\r') ss << "\\r";
                    else if (c == '\t') ss << "\\t";
                    else if (c == '\b') ss << "\\b";
                    else if (c == '\f') ss << "\\f";
                    else if (c < 32) {
                        // Escape other control characters as \uXXXX
                        ss << "\\u" << std::hex << std::setfill('0') << std::setw(4) << (int)c;
                        ss << std::dec;  // Reset to decimal
                    }
                    else ss << c;
                }
                ss << "\"";
            }
            break;
        }

        case CommandType::TOOL_APPROVAL: {
            ss << "tool_approval\"";
            if (auto* data = cmd.as_tool_approval()) {
                ss << ",\"tool_id\":\"" << data->tool_id << "\"";
                ss << ",\"approved\":" << (data->approved ? "true" : "false");
            }
            break;
        }

        case CommandType::INTERRUPT:
            ss << "interrupt\"";
            break;

        case CommandType::MODEL_SWITCH: {
            ss << "model_switch\"";
            if (auto* data = cmd.as_model_switch()) {
                ss << ",\"model_id\":\"" << data->model_id << "\"";
            }
            break;
        }
    }

    ss << "}";
    return ss.str();
}

// Helper command creators
Command ProtocolParser::create_user_input(const std::string& content) {
    Command cmd;
    cmd.type = CommandType::USER_INPUT;
    cmd.data = UserInputCommand{content};
    return cmd;
}

Command ProtocolParser::create_tool_approval(const std::string& tool_id, bool approved) {
    Command cmd;
    cmd.type = CommandType::TOOL_APPROVAL;
    cmd.data = ToolApprovalCommand{tool_id, approved};
    return cmd;
}

Command ProtocolParser::create_interrupt() {
    Command cmd;
    cmd.type = CommandType::INTERRUPT;
    cmd.data = InterruptCommand{};
    return cmd;
}

Command ProtocolParser::create_model_switch(const std::string& model_id) {
    Command cmd;
    cmd.type = CommandType::MODEL_SWITCH;
    cmd.data = ModelSwitchCommand{model_id};
    return cmd;
}

// ============================================================================
// Utility Functions
// ============================================================================

const char* message_type_to_string(MessageType type) {
    switch (type) {
        case MessageType::INIT: return "init";
        case MessageType::CONVERSATION: return "conversation";
        case MessageType::TOOL_GROUP: return "tool_group";
        case MessageType::STATUS: return "status";
        case MessageType::INFO: return "info";
        case MessageType::ERROR: return "error";
        case MessageType::COMPLETION_STATS: return "completion_stats";
    }
    return "unknown";
}

const char* command_type_to_string(CommandType type) {
    switch (type) {
        case CommandType::USER_INPUT: return "user_input";
        case CommandType::TOOL_APPROVAL: return "tool_approval";
        case CommandType::INTERRUPT: return "interrupt";
        case CommandType::MODEL_SWITCH: return "model_switch";
    }
    return "unknown";
}

const char* message_role_to_string(MessageRole role) {
    switch (role) {
        case MessageRole::USER: return "user";
        case MessageRole::ASSISTANT: return "assistant";
        case MessageRole::SYSTEM: return "system";
    }
    return "unknown";
}

const char* tool_status_to_string(ToolStatus status) {
    switch (status) {
        case ToolStatus::PENDING: return "pending";
        case ToolStatus::CONFIRMING: return "confirming";
        case ToolStatus::EXECUTING: return "executing";
        case ToolStatus::SUCCESS: return "success";
        case ToolStatus::ERROR: return "error";
        case ToolStatus::CANCELED: return "canceled";
    }
    return "unknown";
}

const char* app_state_to_string(AppState state) {
    switch (state) {
        case AppState::IDLE: return "idle";
        case AppState::RESPONDING: return "responding";
        case AppState::WAITING_FOR_CONFIRMATION: return "waiting_for_confirmation";
    }
    return "unknown";
}

} // namespace Qwen
