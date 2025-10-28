#pragma once


// WebServer namespace for browser-based terminal
namespace WebServer {
    // Callback type for command execution
    // Returns (success, output) pair
    using CommandCallback = std::function<std::pair<bool, std::string>(const std::string&)>;

    // Start the web server on specified port
    bool start(int port);

    // Stop the web server
    void stop();

    // Check if server is running
    bool is_running();

    // Send output to all connected clients
    void send_output(const std::string& output);

    // Set command execution callback
    void set_command_callback(CommandCallback callback);
}
