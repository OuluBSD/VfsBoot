#ifndef _VfsShell_qwen_tcp_server_h_
#define _VfsShell_qwen_tcp_server_h_

#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <memory>

namespace Qwen {

// Callback types for handling incoming connections and messages
using ConnectionCallback = std::function<void(int client_fd, const std::string& client_addr)>;
using MessageCallback = std::function<void(int client_fd, const std::string& message)>;

// TCP Server class for handling incoming connections
class QwenTCPServer {
public:
    QwenTCPServer();
    ~QwenTCPServer();
    
    // Start the TCP server on the specified host and port
    bool start(const std::string& host, int port);
    
    // Stop the TCP server
    void stop();
    
    // Check if server is running
    bool is_running() const { return running_; }
    
    // Send a message to a specific client
    bool send_to_client(int client_fd, const std::string& message);
    
    // Close a specific client connection
    void close_client(int client_fd);
    
    // Set callback for new connections
    void set_on_connect(ConnectionCallback callback) { on_connect_ = std::move(callback); }
    
    // Set callback for received messages
    void set_on_message(MessageCallback callback) { on_message_ = std::move(callback); }
    
    // Get TCP server port
    int get_port() const { return port_; }
    
    // Get TCP server host
    const std::string& get_host() const { return host_; }

private:
    void server_thread();
    
    std::string host_;
    int port_;
    int server_socket_ = -1;
    
    std::thread server_thread_;
    std::atomic<bool> running_{false};
    
    // Callbacks
    ConnectionCallback on_connect_;
    MessageCallback on_message_;
    
    // Mutex for protecting shared resources
    std::mutex socket_mutex_;
};

} // namespace Qwen

#endif // _VfsShell_qwen_tcp_server_h_