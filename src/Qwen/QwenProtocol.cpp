#include "Qwen.h"
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

MessageRole ProtocolParser::parse_role(const String& role_str) {
    if (role_str == "user") return MessageRole::USER;
    if (role_str == "assistant") return MessageRole::ASSISTANT;
    if (role_str == "system") return MessageRole::SYSTEM;
    throw std::runtime_error("Unknown role: " + role_str.ToStd());
}

ToolStatus ProtocolParser::parse_tool_status(const String& status_str) {
    if (status_str == "pending") return ToolStatus::PENDING;
    if (status_str == "confirming") return ToolStatus::CONFIRMING;
    if (status_str == "executing") return ToolStatus::EXECUTING;
    if (status_str == "success") return ToolStatus::SUCCESS;
    if (status_str == "error") return ToolStatus::ERROR;
    if (status_str == "canceled") return ToolStatus::CANCELED;
    throw std::runtime_error("Unknown tool status: " + status_str.ToStd());
}

AppState ProtocolParser::parse_app_state(const String& state_str) {
    if (state_str == "idle") return AppState::IDLE;
    if (state_str == "responding") return AppState::RESPONDING;
    if (state_str == "waiting_for_confirmation") return AppState::WAITING_FOR_CONFIRMATION;
    throw std::runtime_error("Unknown app state: " + state_str.ToStd());
}

String ProtocolParser::role_to_string(MessageRole role) {
    switch (role) {
        case MessageRole::USER: return "user";
        case MessageRole::ASSISTANT: return "assistant";
        case MessageRole::SYSTEM: return "system";
    }
    return "unknown";
}

String ProtocolParser::tool_status_to_string(ToolStatus status) {
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

String ProtocolParser::app_state_to_string(AppState state) {
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

One<StateMessage> ProtocolParser::parse_message(const String& json_str) {
    std::string json_std = json_str.ToStd();  // Store to avoid dangling pointer
    const char* p = json_std.c_str();
    skip_whitespace(p);

    if (*p != '{') return One<StateMessage>();
    p++; // Skip '{'

    One<StateMessage> msg = new StateMessage();
    std::string type_str;

    // Storage for conversation message fields
    std::string role_str;
    std::string content;
    int id = 0;
    std::optional<bool> is_streaming;

    // Storage for init message fields
    std::string version;
    std::string workspace_root;
    std::string model;

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
        if (*p != '"') return One<StateMessage>();
        std::string key = parse_string(p);

        skip_whitespace(p);
        if (*p != ':') return One<StateMessage>();
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
        } else if (key == "version") {
            version = parse_string(p);
        } else if (key == "workspaceRoot") {
            workspace_root = parse_string(p);
        } else if (key == "model") {
            model = parse_string(p);
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
        // Use parsed fields from JSON
        msg->data = RawToValue(InitMessage{String(version), String(workspace_root), String(model)});
    } else if (type_str == "conversation") {
        msg->type = MessageType::CONVERSATION;
        MessageRole role = MessageRole::USER;
        if (!role_str.empty()) {
            role = parse_role(String(role_str));
        }
        msg->data = RawToValue(ConversationMessage{role, String(content), id, std::nullopt, is_streaming});
    } else if (type_str == "tool_group") {
        msg->type = MessageType::TOOL_GROUP;

        // Parse tools array
        Vector<ToolCall> tools;

        if (!tools_json.empty()) {
            const char* tp = tools_json.c_str();
            skip_whitespace(tp);

            if (*tp == '[') {
                tp++; // Skip '['

                while (*tp && *tp != ']') {
                    skip_whitespace(tp);
                    if (*tp == ']') break;
                    if (*tp == ',') {
                        tp++;
                        continue;
                    }

                    // Parse tool object
                    if (*tp == '{') {
                        tp++; // Skip '{'

                        ToolCall tool;
                        tool.status = ToolStatus::PENDING;

                        // Parse tool fields
                        while (*tp && *tp != '}') {
                            skip_whitespace(tp);
                            if (*tp == '}') break;
                            if (*tp == ',') {
                                tp++;
                                continue;
                            }

                            // Parse key
                            if (*tp != '"') break;
                            std::string key = parse_string(tp);

                            skip_whitespace(tp);
                            if (*tp != ':') break;
                            tp++; // Skip ':'
                            skip_whitespace(tp);

                            // Parse value based on key
                            if (key == "tool_id") {
                                tool.tool_id = String(parse_string(tp));
                            } else if (key == "tool_name") {
                                tool.tool_name = String(parse_string(tp));
                            } else if (key == "status") {
                                tool.status = parse_tool_status(String(parse_string(tp)));
                            } else if (key == "args") {
                                // Parse args object into map
                                if (*tp == '{') {
                                    tp++; // Skip '{'
                                    while (*tp && *tp != '}') {
                                        skip_whitespace(tp);
                                        if (*tp == '}') break;
                                        if (*tp == ',') {
                                            tp++;
                                            continue;
                                        }

                                        if (*tp == '"') {
                                            std::string arg_key = parse_string(tp);
                                            skip_whitespace(tp);
                                            if (*tp == ':') {
                                                tp++;
                                                skip_whitespace(tp);
                                                if (*tp == '"') {
                                                    std::string arg_value = parse_string(tp);
                                                    tool.args.Add(String(arg_key), String(arg_value));
                                                } else {
                                                    skip_value(tp);
                                                }
                                            }
                                        }
                                    }
                                    if (*tp == '}') tp++; // Skip '}'
                                }
                            } else if (key == "confirmation_details") {
                                // Parse confirmation_details object
                                if (*tp == '{') {
                                    tp++; // Skip '{'
                                    ToolConfirmationDetails details;
                                    details.requires_approval = true;

                                    while (*tp && *tp != '}') {
                                        skip_whitespace(tp);
                                        if (*tp == '}') break;
                                        if (*tp == ',') {
                                            tp++;
                                            continue;
                                        }

                                        if (*tp == '"') {
                                            std::string detail_key = parse_string(tp);
                                            skip_whitespace(tp);
                                            if (*tp == ':') {
                                                tp++;
                                                skip_whitespace(tp);

                                                if (detail_key == "message") {
                                                    details.message = String(parse_string(tp));
                                                } else if (detail_key == "requires_approval") {
                                                    details.requires_approval = parse_bool(tp);
                                                } else {
                                                    skip_value(tp);
                                                }
                                            }
                                        }
                                    }
                                    if (*tp == '}') tp++; // Skip '}'
                                    tool.confirmation_details = details;
                                }
                            } else {
                                skip_value(tp);
                            }

                            skip_whitespace(tp);
                        }

                        if (*tp == '}') tp++; // Skip '}'
                        tools.push_back(tool);
                    }

                    skip_whitespace(tp);
                }
            }
        }

        ToolGroup tg;
        tg.id = tool_group_id;
        tg.tools = pick(tools);
        msg->data = RawToValue(tg);
    } else if (type_str == "status") {
        msg->type = MessageType::STATUS;
        msg->data = RawToValue(StatusUpdate{AppState::IDLE, std::nullopt, std::nullopt});
    } else if (type_str == "info") {
        msg->type = MessageType::INFO;
        msg->data = RawToValue(InfoMessage{String("info"), 1});
    } else if (type_str == "error") {
        msg->type = MessageType::ERROR;
        msg->data = RawToValue(ErrorMessage{String("error"), 1});
    } else if (type_str == "completion_stats") {
        msg->type = MessageType::COMPLETION_STATS;
        msg->data = RawToValue(CompletionStats{String("1.5s"), std::nullopt, std::nullopt});
    } else {
        return One<StateMessage>();
    }

    return msg;
}

String ProtocolParser::serialize_command(const Command& cmd) {
    StringStream ss;
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
                        ss << "\\u" << Sprintf("%04x", (int)c);
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
    return ss.GetResult();
}

// Helper command creators
Command ProtocolParser::create_user_input(const String& content) {
    Command cmd;
    cmd.type = CommandType::USER_INPUT;
    cmd.data = RawToValue(UserInputCommand{content});
    return cmd;
}

Command ProtocolParser::create_tool_approval(const String& tool_id, bool approved) {
    Command cmd;
    cmd.type = CommandType::TOOL_APPROVAL;
    cmd.data = RawToValue(ToolApprovalCommand{tool_id, approved});
    return cmd;
}

Command ProtocolParser::create_interrupt() {
    Command cmd;
    cmd.type = CommandType::INTERRUPT;
    cmd.data = RawToValue(InterruptCommand{});
    return cmd;
}

Command ProtocolParser::create_model_switch(const String& model_id) {
    Command cmd;
    cmd.type = CommandType::MODEL_SWITCH;
    cmd.data = RawToValue(ModelSwitchCommand{model_id});
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
