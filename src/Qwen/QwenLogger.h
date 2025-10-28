#ifndef _VFSSHELL_QWEN_LOGGER_H_
#define _VFSSHELL_QWEN_LOGGER_H_

#include <string>
#include <iostream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <sys/types.h>
#include <unistd.h>

namespace Qwen {

// ============================================================================
// Fine-grained timestamp logger for qwen client/server debugging
// ============================================================================
//
// Format: [YYYY-MM-DD HH:MM:SS.microseconds] [PID:12345] [tag:id] message
//
// This allows merging and sorting logs from both C++ client and TypeScript
// server to understand the exact sequence of events.
//
// Usage:
//   QwenLogger logger("qwen-client", "session-abc123");
//   logger.log("Connected to server");
//   logger.log("Sent message: ", json_str);
//
// Environment variables:
//   QWEN_LOG_FILE - Path to log file (default: stderr)
//   QWEN_LOG_LEVEL - Minimum log level (default: INFO)

class QwenLogger {
public:
    enum class Level {
        DEBUG,
        INFO,
        WARN,
        ERROR
    };

    QwenLogger(const std::string& tag, const std::string& id = "")
        : tag_(tag), id_(id), pid_(getpid())
    {
        // Check environment for log file
        const char* log_file = std::getenv("QWEN_LOG_FILE");
        if (log_file) {
            log_file_path_ = log_file;
            use_file_ = true;
        }

        // Check environment for log level
        const char* log_level = std::getenv("QWEN_LOG_LEVEL");
        if (log_level) {
            std::string level_str = log_level;
            if (level_str == "DEBUG") min_level_ = Level::DEBUG;
            else if (level_str == "INFO") min_level_ = Level::INFO;
            else if (level_str == "WARN") min_level_ = Level::WARN;
            else if (level_str == "ERROR") min_level_ = Level::ERROR;
        }
    }

    // Log with level
    template<typename... Args>
    void debug(Args&&... args) {
        log_with_level(Level::DEBUG, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void info(Args&&... args) {
        log_with_level(Level::INFO, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void warn(Args&&... args) {
        log_with_level(Level::WARN, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void error(Args&&... args) {
        log_with_level(Level::ERROR, std::forward<Args>(args)...);
    }

    // Log with default INFO level
    template<typename... Args>
    void log(Args&&... args) {
        log_with_level(Level::INFO, std::forward<Args>(args)...);
    }

    // Update the ID (e.g., when session ID becomes available)
    void set_id(const std::string& id) {
        id_ = id;
    }

private:
    std::string tag_;
    std::string id_;
    pid_t pid_;
    bool use_file_ = false;
    std::string log_file_path_;
    Level min_level_ = Level::INFO;

    std::string level_to_string(Level level) const {
        switch (level) {
            case Level::DEBUG: return "DEBUG";
            case Level::INFO:  return "INFO ";
            case Level::WARN:  return "WARN ";
            case Level::ERROR: return "ERROR";
            default: return "?????";
        }
    }

    std::string get_timestamp() const {
        auto now = std::chrono::system_clock::now();
        auto now_time_t = std::chrono::system_clock::to_time_t(now);
        auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch()) % 1000000;

        std::tm tm_buf;
        localtime_r(&now_time_t, &tm_buf);

        std::ostringstream oss;
        oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
            << '.' << std::setfill('0') << std::setw(6) << now_us.count();
        return oss.str();
    }

    template<typename... Args>
    void log_with_level(Level level, Args&&... args) {
        if (level < min_level_) return;

        std::ostringstream oss;

        // [timestamp] [PID:12345] [LEVEL] [tag:id] message
        oss << "[" << get_timestamp() << "] "
            << "[PID:" << pid_ << "] "
            << "[" << level_to_string(level) << "] "
            << "[" << tag_;
        if (!id_.empty()) {
            oss << ":" << id_;
        }
        oss << "] ";

        // Append all arguments
        (oss << ... << args);
        oss << std::endl;

        std::string log_line = oss.str();

        if (use_file_) {
            // Append to file
            FILE* f = fopen(log_file_path_.c_str(), "a");
            if (f) {
                fputs(log_line.c_str(), f);
                fclose(f);
            } else {
                // Fallback to stderr if file open fails
                std::cerr << log_line;
            }
        } else {
            // Write to stderr
            std::cerr << log_line;
        }
    }
};

} // namespace Qwen

#endif // _VFSSHELL_QWEN_LOGGER_H_
