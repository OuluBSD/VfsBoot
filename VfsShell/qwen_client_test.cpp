#include "qwen_client.h"
#include <iostream>
#include <chrono>
#include <thread>

using namespace Qwen;

// Simple test to verify basic stdin/stdout protocol
int main(int argc, char* argv[]) {
    std::cout << "=== Qwen Client Test ===\n\n";

    // Configure client
    QwenClientConfig config;
    config.qwen_executable = "./qwen_echo_server";  // Use our test echo server
    config.verbose = true;
    config.auto_restart = false;

    // Optional: Set workspace root
    if (argc > 1) {
        config.qwen_args.push_back("--workspace-root");
        config.qwen_args.push_back(argv[1]);
    }

    // Create client
    QwenClient client(config);

    // Set up message handlers
    MessageHandlers handlers;

    handlers.on_init = [](const InitMessage& msg) {
        std::cout << "[INIT] version=" << msg.version
                  << ", workspace=" << msg.workspace_root
                  << ", model=" << msg.model << "\n";
    };

    handlers.on_conversation = [](const ConversationMessage& msg) {
        std::cout << "[CONVERSATION] role=" << message_role_to_string(msg.role)
                  << ", content=\"" << msg.content << "\"\n";
    };

    handlers.on_status = [](const StatusUpdate& msg) {
        std::cout << "[STATUS] state=" << app_state_to_string(msg.state);
        if (msg.message) {
            std::cout << ", message=\"" << *msg.message << "\"";
        }
        std::cout << "\n";
    };

    handlers.on_info = [](const InfoMessage& msg) {
        std::cout << "[INFO] " << msg.message << "\n";
    };

    handlers.on_error = [](const ErrorMessage& msg) {
        std::cout << "[ERROR] " << msg.message << "\n";
    };

    handlers.on_tool_group = [](const ToolGroup& msg) {
        std::cout << "[TOOL_GROUP] id=" << msg.id
                  << ", tools=" << msg.tools.size() << "\n";
        for (const auto& tool : msg.tools) {
            std::cout << "  - " << tool.tool_name
                      << " (status=" << tool_status_to_string(tool.status) << ")\n";
        }
    };

    handlers.on_completion_stats = [](const CompletionStats& msg) {
        std::cout << "[STATS] duration=" << msg.duration;
        if (msg.prompt_tokens) {
            std::cout << ", prompt_tokens=" << *msg.prompt_tokens;
        }
        if (msg.completion_tokens) {
            std::cout << ", completion_tokens=" << *msg.completion_tokens;
        }
        std::cout << "\n";
    };

    client.set_handlers(handlers);

    // Start the client
    std::cout << "Starting qwen-code subprocess...\n";
    if (!client.start()) {
        std::cerr << "Failed to start client: " << client.get_last_error() << "\n";
        return 1;
    }

    std::cout << "Client started (PID " << client.get_process_id() << ")\n\n";

    // Poll for initial messages (init, etc.)
    std::cout << "Waiting for init message...\n";
    for (int i = 0; i < 50; i++) {  // Wait up to 5 seconds
        int count = client.poll_messages(100);
        if (count > 0) break;
    }

    std::cout << "\nSending test message...\n";
    if (!client.send_user_input("Hello from C++ client!")) {
        std::cerr << "Failed to send message: " << client.get_last_error() << "\n";
        client.stop();
        return 1;
    }

    // Poll for response (echo server should echo back)
    std::cout << "Waiting for response...\n";
    bool got_response = false;
    for (int i = 0; i < 50; i++) {  // Wait up to 5 seconds
        int count = client.poll_messages(100);
        if (count > 0) {
            got_response = true;
            break;
        }
    }

    if (!got_response) {
        std::cout << "No response received (timeout)\n";
    }

    // Send a few more test messages
    std::cout << "\nSending additional test messages...\n";

    client.send_user_input("Test message 2");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    client.poll_messages(100);

    client.send_user_input("Test message 3 with \"quotes\" and\nnewlines");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    client.poll_messages(100);

    // Test tool approval
    std::cout << "\nTesting tool approval...\n";
    client.send_tool_approval("test_tool_123", true);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    client.poll_messages(100);

    // Test interrupt
    std::cout << "\nTesting interrupt...\n";
    client.send_interrupt();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    client.poll_messages(100);

    // Stop the client
    std::cout << "\nStopping client...\n";
    client.stop();

    std::cout << "\n=== Test Complete ===\n";
    std::cout << "Restart count: " << client.get_restart_count() << "\n";

    return 0;
}
