#pragma once

/**
 * Qwen Protocol - Structured data protocol for qwen-code integration
 *
 * This header defines the C++ structs matching the TypeScript protocol
 * defined in qwenStateSerializer.ts. It provides a thick client interface
 * for receiving semantic data from qwen-code.
 *
 * Protocol flow:
 *   TypeScript → JSON → C++ structs → VfsBoot UI
 *   VfsBoot UI → C++ structs → JSON → TypeScript
 */

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <variant>

namespace Qwen {

// ============================================================================
// Enums
// ============================================================================

enum class MessageRole {
    USER,
    ASSISTANT,
    SYSTEM
};

enum class ToolStatus {
    PENDING,
    CONFIRMING,
    EXECUTING,
    SUCCESS,
    ERROR,
    CANCELED
};

enum class AppState {
    IDLE,
    RESPONDING,
    WAITING_FOR_CONFIRMATION
};

enum class MessageType {
    INIT,
    CONVERSATION,
    TOOL_GROUP,
    STATUS,
    INFO,
    ERROR,
    COMPLETION_STATS
};

enum class CommandType {
    USER_INPUT,
    TOOL_APPROVAL,
    INTERRUPT,
    MODEL_SWITCH
};

// ============================================================================
// Protocol Messages (TypeScript → C++)
// ============================================================================

struct InitMessage {
    std::string version;
    std::string workspace_root;
    std::string model;
};

struct ConversationMessage {
    MessageRole role;
    std::string content;
    int id;
    std::optional<int64_t> timestamp;
    std::optional<bool> is_streaming;  // True if this is a streaming chunk
};

struct ToolConfirmationDetails {
    std::string message;
    bool requires_approval;
};

struct ToolCall {
    std::string tool_id;
    std::string tool_name;
    ToolStatus status;
    std::map<std::string, std::string> args;  // Simplified: string values only
    std::optional<std::string> result;
    std::optional<std::string> error;
    std::optional<ToolConfirmationDetails> confirmation_details;
};

struct ToolGroup {
    int id;
    std::vector<ToolCall> tools;
};

struct StatusUpdate {
    AppState state;
    std::optional<std::string> message;
    std::optional<std::string> thought;
};

struct InfoMessage {
    std::string message;
    int id;
};

struct ErrorMessage {
    std::string message;
    int id;
};

struct CompletionStats {
    std::string duration;
    std::optional<int> prompt_tokens;
    std::optional<int> completion_tokens;
};

// Variant type for all message types
struct StateMessage {
    MessageType type;

    // Use std::variant to hold the actual message data
    // Only one of these will be populated based on 'type'
    std::variant<
        InitMessage,
        ConversationMessage,
        ToolGroup,
        StatusUpdate,
        InfoMessage,
        ErrorMessage,
        CompletionStats
    > data;

    // Helper getters
    const InitMessage* as_init() const {
        return type == MessageType::INIT ? std::get_if<InitMessage>(&data) : nullptr;
    }

    const ConversationMessage* as_conversation() const {
        return type == MessageType::CONVERSATION ? std::get_if<ConversationMessage>(&data) : nullptr;
    }

    const ToolGroup* as_tool_group() const {
        return type == MessageType::TOOL_GROUP ? std::get_if<ToolGroup>(&data) : nullptr;
    }

    const StatusUpdate* as_status() const {
        return type == MessageType::STATUS ? std::get_if<StatusUpdate>(&data) : nullptr;
    }

    const InfoMessage* as_info() const {
        return type == MessageType::INFO ? std::get_if<InfoMessage>(&data) : nullptr;
    }

    const ErrorMessage* as_error() const {
        return type == MessageType::ERROR ? std::get_if<ErrorMessage>(&data) : nullptr;
    }

    const CompletionStats* as_stats() const {
        return type == MessageType::COMPLETION_STATS ? std::get_if<CompletionStats>(&data) : nullptr;
    }
};

// ============================================================================
// Commands (C++ → TypeScript)
// ============================================================================

struct UserInputCommand {
    std::string content;
};

struct ToolApprovalCommand {
    std::string tool_id;
    bool approved;
};

struct InterruptCommand {
    // No additional fields
};

struct ModelSwitchCommand {
    std::string model_id;
};

struct Command {
    CommandType type;

    std::variant<
        UserInputCommand,
        ToolApprovalCommand,
        InterruptCommand,
        ModelSwitchCommand
    > data;

    // Helper getters
    const UserInputCommand* as_user_input() const {
        return type == CommandType::USER_INPUT ? std::get_if<UserInputCommand>(&data) : nullptr;
    }

    const ToolApprovalCommand* as_tool_approval() const {
        return type == CommandType::TOOL_APPROVAL ? std::get_if<ToolApprovalCommand>(&data) : nullptr;
    }

    const InterruptCommand* as_interrupt() const {
        return type == CommandType::INTERRUPT ? std::get_if<InterruptCommand>(&data) : nullptr;
    }

    const ModelSwitchCommand* as_model_switch() const {
        return type == CommandType::MODEL_SWITCH ? std::get_if<ModelSwitchCommand>(&data) : nullptr;
    }
};

// ============================================================================
// Protocol Parser/Serializer Interface
// ============================================================================

class ProtocolParser {
public:
    // Parse JSON string to StateMessage
    // Returns nullptr if parsing fails
    static std::unique_ptr<StateMessage> parse_message(const std::string& json_str);

    // Serialize Command to JSON string
    static std::string serialize_command(const Command& cmd);

    // Helper: Create common commands
    static Command create_user_input(const std::string& content);
    static Command create_tool_approval(const std::string& tool_id, bool approved);
    static Command create_interrupt();
    static Command create_model_switch(const std::string& model_id);

private:
    // Internal parsing helpers (implemented in .cpp)
    static MessageRole parse_role(const std::string& role_str);
    static ToolStatus parse_tool_status(const std::string& status_str);
    static AppState parse_app_state(const std::string& state_str);

    static std::string role_to_string(MessageRole role);
    static std::string tool_status_to_string(ToolStatus status);
    static std::string app_state_to_string(AppState state);
};

// ============================================================================
// Utility Functions
// ============================================================================

// Convert enums to string for display/logging
const char* message_type_to_string(MessageType type);
const char* command_type_to_string(CommandType type);
const char* message_role_to_string(MessageRole role);
const char* tool_status_to_string(ToolStatus status);
const char* app_state_to_string(AppState state);

} // namespace Qwen
