#include "VfsShell.h"

namespace Qwen {

QwenTCPServer::QwenTCPServer() {
}

QwenTCPServer::~QwenTCPServer() {
    stop();
}

bool QwenTCPServer::start(const std::string& host, int port) {
    host_ = host;
    port_ = port;
    
    // Create socket
    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_ < 0) {
        std::cerr << "[QwenTCPServer] Failed to create TCP socket\n";
        return false;
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "[QwenTCPServer] Failed to set socket options\n";
        close(server_socket_);
        return false;
    }
    
    // Bind to address
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(host_.c_str());
    server_addr.sin_port = htons(port_);
    
    if (bind(server_socket_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "[QwenTCPServer] Failed to bind to " << host_ << ":" << port_ << "\n";
        close(server_socket_);
        return false;
    }
    
    // Listen for connections
    if (listen(server_socket_, 10) < 0) {
        std::cerr << "[QwenTCPServer] Failed to listen on socket\n";
        close(server_socket_);
        return false;
    }
    
    // Mark socket as non-blocking
    int flags = fcntl(server_socket_, F_GETFL, 0);
    fcntl(server_socket_, F_SETFL, flags | O_NONBLOCK);
    
    running_ = true;
    
    // Start server thread
    server_thread_ = std::thread(&QwenTCPServer::server_thread, this);
    
    std::cout << "[QwenTCPServer] Listening on " << host_ << ":" << port_ << "\n";
    return true;
}

void QwenTCPServer::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    // Close the server socket to break the poll() call in the server thread
    {
        std::lock_guard<std::mutex> lock(socket_mutex_);
        if (server_socket_ >= 0) {
            shutdown(server_socket_, SHUT_RDWR);
            close(server_socket_);
            server_socket_ = -1;
        }
    }
    
    // Wait for server thread to finish
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
}

void QwenTCPServer::server_thread() {
    std::vector<struct pollfd> poll_fds;
    struct pollfd server_pollfd;
    server_pollfd.fd = server_socket_;
    server_pollfd.events = POLLIN;
    poll_fds.push_back(server_pollfd);
    
    while (running_) {
        // Poll for events (with 100ms timeout)
        int ret = poll(poll_fds.data(), poll_fds.size(), 100);
        
        if (ret < 0) {
            if (errno == EINTR) {
                continue;  // Interrupted by signal, continue
            }
            std::cerr << "[QwenTCPServer] Poll error: " << strerror(errno) << "\n";
            break;
        }
        
        if (ret == 0) {
            // Timeout - continue to next iteration
            continue;
        }
        
        // Check for new connections on server socket
        if (poll_fds[0].revents & POLLIN) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            
            int client_socket = accept(server_socket_, 
                                      (struct sockaddr*)&client_addr, 
                                      &client_len);
            
            if (client_socket >= 0) {
                // Mark client socket as non-blocking
                int flags = fcntl(client_socket, F_GETFL, 0);
                fcntl(client_socket, F_SETFL, flags | O_NONBLOCK);
                
                // Add client socket to poll_fds for monitoring
                struct pollfd client_pollfd;
                client_pollfd.fd = client_socket;
                client_pollfd.events = POLLIN;
                poll_fds.push_back(client_pollfd);
                
                // Call the on_connect callback if provided
                if (on_connect_) {
                    std::string client_ip = inet_ntoa(client_addr.sin_addr);
                    on_connect_(client_socket, client_ip);
                }
                
                std::cout << "[QwenTCPServer] New connection from " 
                         << inet_ntoa(client_addr.sin_addr) << "\n";
            }
        }
        
        // Check for data from existing clients (skip server socket at index 0)
        for (size_t i = 1; i < poll_fds.size(); ++i) {
            if (poll_fds[i].revents & (POLLIN | POLLHUP | POLLERR)) {
                char buffer[1024];
                ssize_t bytes_read = read(poll_fds[i].fd, buffer, sizeof(buffer) - 1);
                
                if (bytes_read > 0) {
                    // Null-terminate the buffer
                    buffer[bytes_read] = '\0';
                    std::string message(buffer);
                    
                    // Call the on_message callback if provided
                    if (on_message_) {
                        on_message_(poll_fds[i].fd, message);
                    }
                } else if (bytes_read == 0 || (poll_fds[i].revents & (POLLHUP | POLLERR))) {
                    // Client disconnected or error occurred
                    close(poll_fds[i].fd);
                    
                    // Call disconnect callback or handle cleanup
                    // (for now, just remove from poll_fds)
                    
                    // Remove from poll_fds
                    poll_fds.erase(poll_fds.begin() + i);
                    --i;  // Adjust index after removal
                    
                    std::cout << "[QwenTCPServer] Connection closed\n";
                } else if (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                    // Error occurred
                    std::cerr << "[QwenTCPServer] Read error: " << strerror(errno) << "\n";
                    
                    // Close the socket
                    close(poll_fds[i].fd);
                    
                    // Remove from poll_fds
                    poll_fds.erase(poll_fds.begin() + i);
                    --i;  // Adjust index after removal
                }
            }
        }
    }
    
    // Close all remaining client sockets
    for (size_t i = 1; i < poll_fds.size(); ++i) {
        close(poll_fds[i].fd);
    }
    
    std::cout << "[QwenTCPServer] Server thread exiting\n";
}

bool QwenTCPServer::send_to_client(int client_fd, const std::string& message) {
    if (!running_) {
        return false;
    }
    
    ssize_t bytes_sent = write(client_fd, message.c_str(), message.length());
    return bytes_sent > 0;
}

void QwenTCPServer::close_client(int client_fd) {
    close(client_fd);
}

} // namespace Qwen