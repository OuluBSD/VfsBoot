#include "qwen_client.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <cerrno>

// POSIX headers for process management
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <poll.h>

namespace Qwen {

// ============================================================================
// QwenClient::Impl - Implementation Details
// ============================================================================

class QwenClient::Impl {
public:
    explicit Impl(const QwenClientConfig& config)
        : config_(config)
        , running_(false)
        , process_id_(-1)
        , stdin_fd_(-1)
        , stdout_fd_(-1)
        , restart_count_(0)
    {}

    ~Impl() {
        stop();
    }

    bool start() {
        if (running_) {
            last_error_ = "Client already running";
            return false;
        }

        bool success = false;

        switch (config_.mode) {
            case CommunicationMode::STDIN_STDOUT:
                success = start_subprocess();
                break;

            case CommunicationMode::NAMED_PIPE:
                last_error_ = "Named pipe mode not yet implemented";
                success = false;
                break;

            case CommunicationMode::TCP:
                last_error_ = "TCP mode not yet implemented";
                success = false;
                break;
        }

        if (success) {
            running_ = true;
            if (config_.verbose) {
                std::cerr << "[QwenClient] Started successfully\n";
            }
        }

        return success;
    }

    void stop() {
        if (!running_) return;

        if (config_.verbose) {
            std::cerr << "[QwenClient] Stopping...\n";
        }

        // Close file descriptors
        if (stdin_fd_ >= 0) {
            close(stdin_fd_);
            stdin_fd_ = -1;
        }
        if (stdout_fd_ >= 0) {
            close(stdout_fd_);
            stdout_fd_ = -1;
        }

        // Terminate subprocess
        if (process_id_ > 0) {
            kill(process_id_, SIGTERM);

            // Wait for process to exit (with timeout)
            int status;
            int wait_attempts = 10;
            while (wait_attempts-- > 0) {
                pid_t result = waitpid(process_id_, &status, WNOHANG);
                if (result != 0) break;
                usleep(100000); // 100ms
            }

            // Force kill if still running
            if (waitpid(process_id_, &status, WNOHANG) == 0) {
                kill(process_id_, SIGKILL);
                waitpid(process_id_, &status, 0);
            }

            process_id_ = -1;
        }

        running_ = false;
        read_buffer_.clear();
    }

    bool is_running() const {
        return running_;
    }

    bool restart() {
        if (config_.verbose) {
            std::cerr << "[QwenClient] Restarting (attempt "
                      << (restart_count_ + 1) << ")\n";
        }

        stop();

        if (restart_count_ >= config_.max_restarts) {
            last_error_ = "Maximum restart attempts exceeded";
            return false;
        }

        restart_count_++;
        return start();
    }

    void set_handlers(const MessageHandlers& handlers) {
        handlers_ = handlers;
    }

    int poll_messages(int timeout_ms) {
        if (!running_) {
            last_error_ = "Client not running";
            return -1;
        }

        // Use poll() to wait for data
        struct pollfd pfd;
        pfd.fd = stdout_fd_;
        pfd.events = POLLIN;
        pfd.revents = 0;

        int poll_result = poll(&pfd, 1, timeout_ms);

        if (poll_result < 0) {
            last_error_ = std::string("poll() failed: ") + strerror(errno);
            return -1;
        }

        if (poll_result == 0) {
            // Timeout - no messages
            return 0;
        }

        // Data available - read and process messages
        return read_and_dispatch_messages();
    }

    bool send_command(const Command& cmd) {
        if (!running_) {
            last_error_ = "Client not running";
            return false;
        }

        std::string json = ProtocolParser::serialize_command(cmd);
        json += '\n'; // Line-buffered protocol

        if (config_.verbose) {
            std::cerr << "[QwenClient] Sending: " << json;
        }

        ssize_t written = write(stdin_fd_, json.c_str(), json.size());
        if (written != static_cast<ssize_t>(json.size())) {
            last_error_ = std::string("write() failed: ") + strerror(errno);
            return false;
        }

        return true;
    }

    std::string get_last_error() const {
        return last_error_;
    }

    int get_restart_count() const {
        return restart_count_;
    }

    int get_process_id() const {
        return process_id_;
    }

private:
    bool start_subprocess() {
        // Create pipes for stdin and stdout
        int stdin_pipe[2];
        int stdout_pipe[2];

        if (pipe(stdin_pipe) < 0) {
            last_error_ = std::string("pipe() failed: ") + strerror(errno);
            return false;
        }

        if (pipe(stdout_pipe) < 0) {
            close(stdin_pipe[0]);
            close(stdin_pipe[1]);
            last_error_ = std::string("pipe() failed: ") + strerror(errno);
            return false;
        }

        // Fork the process
        pid_t pid = fork();

        if (pid < 0) {
            // Fork failed
            close(stdin_pipe[0]);
            close(stdin_pipe[1]);
            close(stdout_pipe[0]);
            close(stdout_pipe[1]);
            last_error_ = std::string("fork() failed: ") + strerror(errno);
            return false;
        }

        if (pid == 0) {
            // Child process
            // Redirect stdin and stdout
            dup2(stdin_pipe[0], STDIN_FILENO);
            dup2(stdout_pipe[1], STDOUT_FILENO);

            // Close unused pipe ends
            close(stdin_pipe[0]);
            close(stdin_pipe[1]);
            close(stdout_pipe[0]);
            close(stdout_pipe[1]);

            // Build argument list
            std::vector<const char*> args;
            args.push_back(config_.qwen_executable.c_str());
            args.push_back("--server-mode");
            args.push_back("stdin");

            for (const auto& arg : config_.qwen_args) {
                args.push_back(arg.c_str());
            }

            args.push_back(nullptr);

            // Execute qwen-code
            execvp(config_.qwen_executable.c_str(),
                   const_cast<char* const*>(args.data()));

            // If execvp returns, it failed
            std::cerr << "execvp() failed: " << strerror(errno) << "\n";
            _exit(1);
        }

        // Parent process
        process_id_ = pid;

        // Close unused pipe ends
        close(stdin_pipe[0]);
        close(stdout_pipe[1]);

        stdin_fd_ = stdin_pipe[1];
        stdout_fd_ = stdout_pipe[0];

        // Set stdout to non-blocking for polling
        int flags = fcntl(stdout_fd_, F_GETFL, 0);
        fcntl(stdout_fd_, F_SETFL, flags | O_NONBLOCK);

        if (config_.verbose) {
            std::cerr << "[QwenClient] Subprocess started with PID " << pid << "\n";
        }

        return true;
    }

    int read_and_dispatch_messages() {
        // Read available data into buffer
        char chunk[4096];
        ssize_t n = read(stdout_fd_, chunk, sizeof(chunk) - 1);

        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No data available (shouldn't happen after poll)
                return 0;
            }
            last_error_ = std::string("read() failed: ") + strerror(errno);
            return -1;
        }

        if (n == 0) {
            // EOF - subprocess closed stdout
            last_error_ = "Subprocess closed stdout";
            running_ = false;

            // Auto-restart if enabled
            if (config_.auto_restart) {
                restart();
            }

            return -1;
        }

        chunk[n] = '\0';

        if (config_.verbose) {
            std::cerr << "[QwenClient] Read " << n << " bytes from subprocess\n";
            std::cerr << "[QwenClient] Raw data: " << chunk << "\n";
        }

        read_buffer_ += chunk;

        // Process complete lines
        int message_count = 0;
        size_t pos;

        while ((pos = read_buffer_.find('\n')) != std::string::npos) {
            std::string line = read_buffer_.substr(0, pos);
            read_buffer_.erase(0, pos + 1);

            if (!line.empty()) {
                if (config_.verbose) {
                    std::cerr << "[QwenClient] Received: " << line << "\n";
                }

                // Parse and dispatch message
                dispatch_message(line);
                message_count++;
            }
        }

        return message_count;
    }

    void dispatch_message(const std::string& json) {
        auto msg = ProtocolParser::parse_message(json);
        if (!msg) {
            last_error_ = "Failed to parse message: " + json;
            return;
        }

        // Dispatch to appropriate handler
        switch (msg->type) {
            case MessageType::INIT:
                if (handlers_.on_init) {
                    if (auto* data = msg->as_init()) {
                        handlers_.on_init(*data);
                    }
                }
                break;

            case MessageType::CONVERSATION:
                if (handlers_.on_conversation) {
                    if (auto* data = msg->as_conversation()) {
                        handlers_.on_conversation(*data);
                    }
                }
                break;

            case MessageType::TOOL_GROUP:
                if (handlers_.on_tool_group) {
                    if (auto* data = msg->as_tool_group()) {
                        handlers_.on_tool_group(*data);
                    }
                }
                break;

            case MessageType::STATUS:
                if (handlers_.on_status) {
                    if (auto* data = msg->as_status()) {
                        handlers_.on_status(*data);
                    }
                }
                break;

            case MessageType::INFO:
                if (handlers_.on_info) {
                    if (auto* data = msg->as_info()) {
                        handlers_.on_info(*data);
                    }
                }
                break;

            case MessageType::ERROR:
                if (handlers_.on_error) {
                    if (auto* data = msg->as_error()) {
                        handlers_.on_error(*data);
                    }
                }
                break;

            case MessageType::COMPLETION_STATS:
                if (handlers_.on_completion_stats) {
                    if (auto* data = msg->as_stats()) {
                        handlers_.on_completion_stats(*data);
                    }
                }
                break;
        }
    }

    QwenClientConfig config_;
    MessageHandlers handlers_;

    bool running_;
    pid_t process_id_;
    int stdin_fd_;
    int stdout_fd_;
    int restart_count_;

    std::string read_buffer_;
    std::string last_error_;
};

// ============================================================================
// QwenClient - Public Interface
// ============================================================================

QwenClient::QwenClient(const QwenClientConfig& config)
    : impl_(std::make_unique<Impl>(config))
{}

QwenClient::~QwenClient() = default;

bool QwenClient::start() {
    return impl_->start();
}

void QwenClient::stop() {
    impl_->stop();
}

bool QwenClient::is_running() const {
    return impl_->is_running();
}

bool QwenClient::restart() {
    return impl_->restart();
}

void QwenClient::set_handlers(const MessageHandlers& handlers) {
    impl_->set_handlers(handlers);
}

int QwenClient::poll_messages(int timeout_ms) {
    return impl_->poll_messages(timeout_ms);
}

bool QwenClient::send_user_input(const std::string& content) {
    return impl_->send_command(ProtocolParser::create_user_input(content));
}

bool QwenClient::send_tool_approval(const std::string& tool_id, bool approved) {
    return impl_->send_command(ProtocolParser::create_tool_approval(tool_id, approved));
}

bool QwenClient::send_interrupt() {
    return impl_->send_command(ProtocolParser::create_interrupt());
}

bool QwenClient::send_model_switch(const std::string& model_id) {
    return impl_->send_command(ProtocolParser::create_model_switch(model_id));
}

bool QwenClient::send_command(const Command& cmd) {
    return impl_->send_command(cmd);
}

std::string QwenClient::get_last_error() const {
    return impl_->get_last_error();
}

int QwenClient::get_restart_count() const {
    return impl_->get_restart_count();
}

int QwenClient::get_process_id() const {
    return impl_->get_process_id();
}

} // namespace Qwen
