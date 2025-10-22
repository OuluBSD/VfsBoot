// Simple integration test for QwenClient with mock server
// Bypasses the main codex REPL to test client directly

#include <iostream>
#include <thread>
#include <chrono>

#include "tag_system.h"
#include "logic_engine.h"
#include "vfs_core.h"
#include "qwen_protocol.h"
#include "qwen_client.h"
#include "qwen_state_manager.h"

using namespace Qwen;

int main() {
    std::cout << "=== QwenClient Integration Test ===\n\n";

    // Create minimal VFS
    Vfs vfs;

    // Create state manager
    QwenStateManager state_mgr(&vfs);
    std::string session_id = state_mgr.create_session("gpt-4o-mini", "/common/active/sblo/Dev/VfsBoot");
    std::cout << "Created session: " << session_id << "\n\n";

    // Set up message handlers
    MessageHandlers handlers;

    handlers.on_init = [](const InitMessage& msg) {
        std::cout << "[INIT] Version: " << msg.version
                  << ", Model: " << msg.model << "\n";
    };

    handlers.on_conversation = [&](const ConversationMessage& msg) {
        if (msg.role == MessageRole::USER) {
            std::cout << "[YOU] " << msg.content << "\n";
        } else if (msg.role == MessageRole::ASSISTANT) {
            if (msg.is_streaming.value_or(false)) {
                std::cout << msg.content << std::flush;
            } else {
                if (!msg.content.empty()) {
                    std::cout << "\n";
                }
            }
        }
        state_mgr.add_message(msg);
    };

    handlers.on_status = [](const StatusUpdate& msg) {
        std::cout << "[STATUS] ";
        if (msg.message.has_value()) {
            std::cout << msg.message.value();
        }
        std::cout << "\n";
    };

    handlers.on_info = [](const InfoMessage& msg) {
        std::cout << "[INFO] " << msg.message << "\n";
    };

    handlers.on_error = [](const ErrorMessage& msg) {
        std::cout << "[ERROR] " << msg.message << "\n";
    };

    handlers.on_tool_group = [](const ToolGroup& group) {
        std::cout << "\n[TOOL REQUEST]\n";
        for (const auto& tool : group.tools) {
            std::cout << "  - " << tool.tool_name << " (id: " << tool.tool_id << ")\n";
        }
    };

    handlers.on_completion_stats = [](const CompletionStats& stats) {
        std::cout << "\n[STATS] Tokens: ";
        if (stats.prompt_tokens.has_value()) {
            std::cout << "in=" << stats.prompt_tokens.value() << " ";
        }
        if (stats.completion_tokens.has_value()) {
            std::cout << "out=" << stats.completion_tokens.value();
        }
        std::cout << "\n";
    };

    // Configure client
    QwenClientConfig config;
    config.qwen_executable = "/common/active/sblo/Dev/VfsBoot/qwen-code";
    config.auto_restart = false;
    config.verbose = false;
    config.handlers = handlers;

    // Create client
    QwenClient client(config);

    std::cout << "Starting qwen-code subprocess...\n";
    if (!client.start()) {
        std::cerr << "Failed to start client: " << client.get_last_error() << "\n";
        return 1;
    }
    std::cout << "Subprocess started successfully!\n\n";

    // Wait for init message
    std::cout << "Waiting for init message...\n";
    for (int i = 0; i < 30; ++i) {
        client.poll_messages(100);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Test 1: Simple message
    std::cout << "\n=== Test 1: Simple Message ===\n";
    std::cout << "Sending: 'hello world'\n\n";
    client.send_user_input("hello world");

    // Poll for 8 seconds to get streaming response
    std::cout << "[AI] ";
    for (int i = 0; i < 80; ++i) {
        client.poll_messages(100);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << "\n";

    // Test 2: Tool trigger
    std::cout << "\n=== Test 2: Tool Trigger ===\n";
    std::cout << "Sending: 'test tool please'\n\n";
    client.send_user_input("test tool please");

    // Poll for response + tool call
    std::cout << "[AI] ";
    for (int i = 0; i < 100; ++i) {
        client.poll_messages(100);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << "\n";

    // Save session
    std::cout << "\n=== Saving Session ===\n";
    state_mgr.save_session();
    std::cout << "Session saved. Message count: " << state_mgr.get_message_count() << "\n";

    // Stop client
    std::cout << "\nStopping client...\n";
    client.stop();

    std::cout << "\n=== Test Complete ===\n";
    return 0;
}
