// Simple echo server for testing qwen protocol
// Reads commands from stdin, echoes messages to stdout

#include <iostream>
#include <string>
#include <cstdlib>

int main() {
    // Send init message
    std::cout << R"({"type":"init","version":"0.0.14","workspaceRoot":"/test","model":"qwen-echo"})" << "\n";
    std::cout.flush();

    // Send idle status
    std::cout << R"({"type":"status","state":"idle","message":"Ready"})" << "\n";
    std::cout.flush();

    // Read commands and echo them back
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;

        // Parse command type (simple string search)
        if (line.find("\"user_input\"") != std::string::npos) {
            // Extract content (simple approach for testing)
            size_t content_pos = line.find("\"content\":\"");
            if (content_pos != std::string::npos) {
                content_pos += 11; // Skip "content":"
                size_t end_pos = line.find("\"", content_pos);
                std::string content = line.substr(content_pos, end_pos - content_pos);

                // Echo back as assistant message
                std::cout << R"({"type":"conversation","role":"assistant","content":"Echo: )"
                          << content << R"(","id":1})" << "\n";
                std::cout.flush();
            }

            // Send status back to idle
            std::cout << R"({"type":"status","state":"idle","message":"Ready"})" << "\n";
            std::cout.flush();
        }
        else if (line.find("\"tool_approval\"") != std::string::npos) {
            // Acknowledge tool approval
            std::cout << R"({"type":"info","message":"Tool approval received","id":1})" << "\n";
            std::cout.flush();
        }
        else if (line.find("\"interrupt\"") != std::string::npos) {
            // Acknowledge interrupt
            std::cout << R"({"type":"info","message":"Interrupt received","id":1})" << "\n";
            std::cout.flush();
        }
        else if (line.find("\"model_switch\"") != std::string::npos) {
            // Acknowledge model switch
            std::cout << R"({"type":"info","message":"Model switch received","id":1})" << "\n";
            std::cout.flush();
        }
    }

    return 0;
}
