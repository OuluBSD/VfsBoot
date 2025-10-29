#ifndef _Qwen_QwenProtocol_h_
#define _Qwen_QwenProtocol_h_

// All includes have been moved to Qwen.h - the main header
// This header is not self-contained as per U++ convention
// For reference: This header needs standard C++ types like strings and vectors
// #include <Core/Core.h>  // Commented for U++ convention - included in main header

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
    String version;
    String workspace_root;
    String model;
};

struct ConversationMessage {
    MessageRole role;
    String content;
    int id;
    std::optional<int64_t> timestamp;
    std::optional<bool> is_streaming;  // True if this is a streaming chunk
};

struct ToolConfirmationDetails {
    String message;
    bool requires_approval;
};

struct ToolCall {
    String tool_id;
    String tool_name;
    ToolStatus status;
    VectorMap<String, String> args;  // Simplified: string values only
    std::optional<String> result;
    std::optional<String> error;
    std::optional<ToolConfirmationDetails> confirmation_details;
};

struct ToolGroup {
    int id;
    Vector<ToolCall> tools;
};

struct StatusUpdate {
    AppState state;
    std::optional<String> message;
    std::optional<String> thought;
};

struct InfoMessage {
    String message;
    int id;
};

struct ErrorMessage {
    String message;
    int id;
};

struct CompletionStats {
    String duration;
    std::optional<int> prompt_tokens;
    std::optional<int> completion_tokens;
};

// Variant type for all message types
struct StateMessage {
    MessageType type;
    // In U++, we'll use a simpler approach due to the complexity of std::variant
    // This is a simplified representation - in practice, you might use a different approach
    // For now, we'll represent the data as a generic structure
    Value data;  // Using U++ Value type to hold variant data

    // Helper getters (simplified implementation)
    const InitMessage* as_init() const { /* Need implementation */ return nullptr; }
    const ConversationMessage* as_conversation() const { /* Need implementation */ return nullptr; }
    const ToolGroup* as_tool_group() const { /* Need implementation */ return nullptr; }
    const StatusUpdate* as_status() const { /* Need implementation */ return nullptr; }
    const InfoMessage* as_info() const { /* Need implementation */ return nullptr; }
    const ErrorMessage* as_error() const { /* Need implementation */ return nullptr; }
    const CompletionStats* as_stats() const { /* Need implementation */ return nullptr; }
    typedef StateMessage CLASSNAME;  // Required for THISBACK macros if used
};

// ============================================================================
// Commands (C++ → TypeScript)
// ============================================================================

struct UserInputCommand {
    String content;
};

struct ToolApprovalCommand {
    String tool_id;
    bool approved;
};

struct InterruptCommand {
    // No additional fields
};

struct ModelSwitchCommand {
    String model_id;
};

struct Command {
    CommandType type;
    Value data;  // Using U++ Value type to hold variant data for commands

    // Helper getters (simplified implementation)
    const UserInputCommand* as_user_input() const { /* Need implementation */ return nullptr; }
    const ToolApprovalCommand* as_tool_approval() const { /* Need implementation */ return nullptr; }
    const InterruptCommand* as_interrupt() const { /* Need implementation */ return nullptr; }
    const ModelSwitchCommand* as_model_switch() const { /* Need implementation */ return nullptr; }
    typedef Command CLASSNAME;  // Required for THISBACK macros if used
};

// ============================================================================
// Protocol Parser/Serializer Interface
// ============================================================================

class ProtocolParser {
public:
    // Parse JSON string to StateMessage
    // Returns nullptr if parsing fails
    static One<StateMessage> parse_message(const String& json_str);

    // Serialize Command to JSON string
    static String serialize_command(const Command& cmd);

    // Helper: Create common commands
    static Command create_user_input(const String& content);
    static Command create_tool_approval(const String& tool_id, bool approved);
    static Command create_interrupt();
    static Command create_model_switch(const String& model_id);

private:
    // Internal parsing helpers (implemented in .cpp)
    static MessageRole parse_role(const String& role_str);
    static ToolStatus parse_tool_status(const String& status_str);
    static AppState parse_app_state(const String& state_str);

    static String role_to_string(MessageRole role);
    static String tool_status_to_string(ToolStatus status);
    static String app_state_to_string(AppState state);
    typedef ProtocolParser CLASSNAME;  // Required for THISBACK macros if used
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

#endif
