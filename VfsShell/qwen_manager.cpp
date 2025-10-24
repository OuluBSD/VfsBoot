#include "qwen_manager.h"
#include "qwen_tcp_server.h"
#include "VfsShell.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <thread>
#include <chrono>
#include <random>
#include <sstream>
#include <iostream>
#include <fstream>

#ifdef CODEX_UI_NCURSES
#include <ncurses.h>
#endif

namespace Qwen {

// Constructor
QwenManager::QwenManager(Vfs* vfs) : vfs_(vfs) {
}

// Destructor
QwenManager::~QwenManager() {
    stop();
}

// Initialize manager mode
bool QwenManager::initialize(const QwenManagerConfig& config) {
    config_ = config;
    
    // Generate special sessions for PROJECT MANAGER and TASK MANAGER
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        
        // Create PROJECT MANAGER session
        SessionInfo project_manager;
        project_manager.session_id = "mgr-project";
        project_manager.type = SessionType::MANAGER_PROJECT;
        project_manager.hostname = "local";
        project_manager.repo_path = config_.management_repo_path;
        project_manager.status = "active";
        project_manager.model = "qwen-openai";
        project_manager.created_at = time(nullptr);
        project_manager.last_activity = project_manager.created_at;
        project_manager.is_active = true;
        
        // Load PROJECT_MANAGER.md instructions if available
        project_manager.instructions = load_instructions_from_file("PROJECT_MANAGER.md");
        sessions_.push_back(project_manager);
        
        // Create TASK MANAGER session
        SessionInfo task_manager;
        task_manager.session_id = "mgr-task";
        task_manager.type = SessionType::MANAGER_TASK;
        task_manager.hostname = "local";
        task_manager.repo_path = config_.management_repo_path;
        task_manager.status = "active";
        task_manager.model = "qwen-auth";
        task_manager.created_at = time(nullptr);
        task_manager.last_activity = task_manager.created_at;
        task_manager.is_active = true;
        
        // Load TASK_MANAGER.md instructions if available
        task_manager.instructions = load_instructions_from_file("TASK_MANAGER.md");
        sessions_.push_back(task_manager);
    }
    
    // Start TCP server
    if (!start_tcp_server()) {
        return false;
    }
    
    running_ = true;
    return true;
}

// Start TCP server
bool QwenManager::start_tcp_server() {
    tcp_host_ = config_.tcp_host;
    tcp_port_ = config_.tcp_port;
    
    // Initialize TCP server
    tcp_server_ = std::make_unique<QwenTCPServer>();
    
    // Set up callbacks
    tcp_server_->set_on_connect([this](int client_fd, const std::string& client_addr) {
        // Add to session registry
        {
            std::lock_guard<std::mutex> lock(sessions_mutex_);
            
            SessionInfo account_session;
            account_session.session_id = "acc-" + std::to_string(client_fd);
            account_session.type = SessionType::ACCOUNT;
            account_session.hostname = client_addr;
            account_session.repo_path = "";
            account_session.status = "connected";
            account_session.model = "unknown";
            account_session.connection_info = "tcp";
            account_session.created_at = time(nullptr);
            account_session.last_activity = account_session.created_at;
            account_session.is_active = true;
            
            sessions_.push_back(account_session);
        }
        
        std::cout << "[QwenManager] New account connection from " << client_addr << "\n";
    });
    
    tcp_server_->set_on_message([this](int client_fd, const std::string& message) {
        // Update last activity for this session
        {
            std::lock_guard<std::mutex> lock(sessions_mutex_);
            for (auto& session : sessions_) {
                if (session.connection_info == std::to_string(client_fd)) {
                    session.last_activity = time(nullptr);
                    break;
                }
            }
        }
        
        // For now, just log the received message
        std::cout << "[QwenManager] Received message from client " << client_fd << ": " 
                 << message << std::endl;
    });
    
    // Start the TCP server
    bool success = tcp_server_->start(tcp_host_, tcp_port_);
    
    if (success) {
        std::cout << "[QwenManager] TCP server listening on " << tcp_host_ << ":" << tcp_port_ << "\n";
    }
    
    return success;
}

// Generate unique session ID
void QwenManager::generate_session_id(std::string& session_id) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    ss << "session-";
    
    for (int i = 0; i < 16; ++i) {
        ss << std::hex << dis(gen);
        if (i == 7 || i == 11) {
            ss << '-';
        }
    }
    
    session_id = ss.str();
}

// Find session by ID (non-const)
SessionInfo* QwenManager::find_session(const std::string& session_id) {
    for (auto& session : sessions_) {
        if (session.session_id == session_id) {
            return &session;
        }
    }
    return nullptr;
}

// Find session by ID (const)
const SessionInfo* QwenManager::find_session(const std::string& session_id) const {
    for (const auto& session : sessions_) {
        if (session.session_id == session_id) {
            return &session;
        }
    }
    return nullptr;
}

// Stop the manager
void QwenManager::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    // Stop TCP server
    stop_tcp_server();
}

// Stop TCP server
void QwenManager::stop_tcp_server() {
    if (tcp_server_) {
        tcp_server_->stop();
    }
}

// Generate unique session ID
void QwenManager::generate_session_id(std::string& session_id) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    ss << "session-";
    
    for (int i = 0; i < 16; ++i) {
        ss << std::hex << dis(gen);
        if (i == 7 || i == 11) {
            ss << '-';
        }
    }
    
    session_id = ss.str();
}

// Find session by ID (non-const)
SessionInfo* QwenManager::find_session(const std::string& session_id) {
    for (auto& session : sessions_) {
        if (session.session_id == session_id) {
            return &session;
        }
    }
    return nullptr;
}

// Find session by ID (const)
const SessionInfo* QwenManager::find_session(const std::string& session_id) const {
    for (const auto& session : sessions_) {
        if (session.session_id == session_id) {
            return &session;
        }
    }
    return nullptr;
}

// Load instructions from file
std::string QwenManager::load_instructions_from_file(const std::string& filename) {
    if (!vfs_) {
        return "";  // Can't load from VFS without vfs pointer
    }

    // Try to load the file from VFS
    Vfs::ReadResult result = vfs_->read_file(filename);
    if (result.success) {
        return result.content;
    }

    // If not in VFS, try to load from the local filesystem
    std::ifstream file(filename);
    if (file.is_open()) {
        std::string content;
        std::string line;
        while (std::getline(file, line)) {
            content += line + "\n";
        }
        file.close();
        return content;
    }

    // File not found
    std::cout << "[QwenManager] Warning: Could not load instructions file: " << filename << std::endl;
    return "";
}

// Helper struct to store colored output lines
struct OutputLine {
    std::string text;
    int color_pair;

    OutputLine(const std::string& t, int cp = 0) : text(t), color_pair(cp) {}
};

// Run manager mode with ncurses UI
bool QwenManager::run_ncurses_mode() {
#ifdef CODEX_UI_NCURSES
    // Initialize ncurses
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    
    // Enable mouse support for scrolling
    mousemask(BUTTON4_PRESSED | BUTTON5_PRESSED, nullptr);
    
    // Initialize colors if supported
    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_CYAN, COLOR_BLACK);     // MANAGER sessions
        init_pair(2, COLOR_YELLOW, COLOR_BLACK);   // ACCOUNT sessions
        init_pair(3, COLOR_GREEN, COLOR_BLACK);    // REPO sessions
        init_pair(4, COLOR_RED, COLOR_BLACK);      // Error messages
        init_pair(5, COLOR_BLUE, COLOR_BLACK);     // Info messages
        init_pair(6, COLOR_MAGENTA, COLOR_BLACK);  // System messages
        init_pair(7, COLOR_WHITE, COLOR_BLACK);    // Default text
    }

    // Get screen dimensions
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    // Create windows for session list and chat/command view
    int list_height = std::min(10, max_y - 5);  // Max 10 rows for list, leave room for status bar and input
    WINDOW* list_win = newwin(list_height, max_x, 0, 0);
    WINDOW* status_separator_win = newwin(1, max_x, list_height, 0);
    WINDOW* chat_win = newwin(max_y - list_height - 4, max_x, list_height + 1, 0);
    WINDOW* input_win = newwin(3, max_x, max_y - 3, 0);

    scrollok(list_win, TRUE);
    scrollok(chat_win, TRUE);

    // Enable keypad for input window
    keypad(input_win, TRUE);

    // Session list buffer (for scrolling)
    std::vector<OutputLine> session_list_buffer;
    int list_scroll_offset = 0;
    
    // Chat buffer (for scrolling)
    std::vector<OutputLine> chat_buffer;
    int chat_scroll_offset = 0;

    // Input buffer and cursor position
    std::string input_buffer;
    size_t cursor_pos = 0;
    
    // Currently selected session (for list navigation)
    int selected_session_idx = 0;
    bool list_focused = false;  // Whether the session list currently has focus
    
    // Current active session for chat view
    std::string active_session_id = "";
    std::map<std::string, std::vector<OutputLine>> session_chat_buffers; // Chat history per session

    // Helper function to update session list
    auto update_session_list = [&]() {
        session_list_buffer.clear();
        
        // Add header
        session_list_buffer.emplace_back("Type      | ID          | Computer   | Repo Path                    | Status", has_colors() ? 7 : 0);
        session_list_buffer.emplace_back("----------|-------------|------------|------------------------------|-------", has_colors() ? 7 : 0);
        
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        for (size_t i = 0; i < sessions_.size(); ++i) {
            const auto& session = sessions_[i];
            std::string type_str;
            int color_pair = 0;
            std::string status_icon = session.is_active ? "●" : "○";  // Active/inactive indicators
            
            switch (session.type) {
                case SessionType::MANAGER_PROJECT:
                    type_str = "MGR-PROJ ";
                    color_pair = has_colors() ? 1 : 0;  // Bright cyan for project manager
                    break;
                case SessionType::MANAGER_TASK:
                    type_str = "MGR-TASK ";
                    color_pair = has_colors() ? 6 : 0;  // Magenta for task manager
                    break;
                case SessionType::ACCOUNT:
                    type_str = "ACCOUNT  ";
                    color_pair = has_colors() ? 2 : 0;  // Yellow for accounts
                    break;
                case SessionType::REPO_MANAGER:
                    type_str = "REPO-MGR ";
                    color_pair = has_colors() ? 3 : 0;  // Green for repo managers
                    break;
                case SessionType::REPO_WORKER:
                    type_str = "REPO-WRK ";
                    color_pair = has_colors() ? 5 : 0;  // Blue for repo workers
                    break;
                default:
                    type_str = "UNKNOWN  ";
                    color_pair = has_colors() ? 6 : 0;  // Magenta for unknown
                    break;
            }
            
            std::string status_str = session.is_active ? "active" : "inactive";
            
            std::string line = status_icon + " " + type_str + " | " +
                              session.session_id.substr(0, 11) + " | " +
                              session.hostname.substr(0, 10) + " | " +
                              session.repo_path.substr(0, 26) + " | " +
                              status_str;
            
            // Highlight selected session
            if (i == selected_session_idx) {
                line = "> " + line;  // Add selection indicator
                session_list_buffer.emplace_back(line, has_colors() ? 7 : 0);  // White for selected
            } else {
                session_list_buffer.emplace_back("  " + line, color_pair);
            }
        }
    };

    // Helper function to redraw session list window
    auto redraw_session_list = [&]() {
        werase(list_win);
        
        // Calculate which lines to display
        int display_lines = list_height;
        int total_lines = session_list_buffer.size();
        int start_line = std::max(0, total_lines - display_lines - list_scroll_offset);
        int end_line = std::min(total_lines, start_line + display_lines);

        // Draw visible lines
        int y = 0;
        for (int i = start_line; i < end_line; ++i) {
            const auto& line = session_list_buffer[i];
            if (has_colors() && line.color_pair > 0) {
                wattron(list_win, COLOR_PAIR(line.color_pair));
                mvwprintw(list_win, y++, 0, "%s", line.text.c_str());
                wattroff(list_win, COLOR_PAIR(line.color_pair));
            } else {
                mvwprintw(list_win, y++, 0, "%s", line.text.c_str());
            }
        }

        box(list_win, 0, 0);
        wrefresh(list_win);
    };

    // Helper function to redraw status separator
    auto redraw_status_separator = [&]() {
        werase(status_separator_win);
        wattron(status_separator_win, A_REVERSE);
        
        std::string separator = "─";
        for (int i = 1; i < max_x; ++i) {
            separator += "─";
        }
        mvwprintw(status_separator_win, 0, 0, "%s", separator.c_str());
        
        wattroff(status_separator_win, A_REVERSE);
        wrefresh(status_separator_win);
    };

    // Helper function to redraw chat window
    auto redraw_chat_window = [&]() {
        werase(chat_win);

        // Get the active session's chat buffer
        std::vector<OutputLine>* active_buffer = nullptr;
        if (!active_session_id.empty()) {
            auto it = session_chat_buffers.find(active_session_id);
            if (it != session_chat_buffers.end()) {
                active_buffer = &(it->second);
            }
        }
        
        // If there's no active session buffer, use the default one
        std::vector<OutputLine> default_buffer;
        if (!active_buffer) {
            // Create a default message about the session view
            default_buffer.emplace_back("Select a session from the list above to view its chat history", has_colors() ? 5 : 0);
            default_buffer.emplace_back("", has_colors() ? 7 : 0);
            default_buffer.emplace_back("MANAGER sessions will show strategic planning and coordination", has_colors() ? 1 : 0);
            default_buffer.emplace_back("ACCOUNT sessions will show connection status and commands", has_colors() ? 2 : 0);
            default_buffer.emplace_back("REPO sessions will show development activity and progress", has_colors() ? 3 : 0);
            active_buffer = &default_buffer;
        }

        // Calculate which lines to display
        int display_lines = max_y - list_height - 4;
        int total_lines = active_buffer->size();
        int start_line = std::max(0, total_lines - display_lines - chat_scroll_offset);
        int end_line = std::min(total_lines, start_line + display_lines);

        // Draw visible lines
        int y = 0;
        for (int i = start_line; i < end_line; ++i) {
            const auto& line = (*active_buffer)[i];
            if (has_colors() && line.color_pair > 0) {
                wattron(chat_win, COLOR_PAIR(line.color_pair));
                mvwprintw(chat_win, y++, 0, "%s", line.text.c_str());
                wattroff(chat_win, COLOR_PAIR(line.color_pair));
            } else {
                mvwprintw(chat_win, y++, 0, "%s", line.text.c_str());
            }
        }

        wrefresh(chat_win);
    };

    // Helper function to draw input window
    auto redraw_input = [&]() {
        werase(input_win);
        box(input_win, 0, 0);

        // Display input text (handle text longer than window width)
        int visible_width = max_x - 4;  // Account for box borders and "> " prompt
        int display_start = 0;

        if ((int)cursor_pos > visible_width - 1) {
            display_start = cursor_pos - visible_width + 1;
        }

        std::string visible_text = input_buffer.substr(display_start, visible_width);
        mvwprintw(input_win, 1, 2, "> %s", visible_text.c_str());

        // Position cursor
        int cursor_x = 4 + (cursor_pos - display_start);
        wmove(input_win, 1, cursor_x);

        wrefresh(input_win);
    };

    // Helper function to redraw status bar
    auto redraw_status = [&]() {
        werase(status_separator_win);
        wattron(status_separator_win, A_REVERSE);

        // Count different types of sessions
        int project_mgr_count = 0, task_mgr_count = 0, account_count = 0, repo_mgr_count = 0, repo_worker_count = 0;
        for (const auto& session : sessions_) {
            switch (session.type) {
                case SessionType::MANAGER_PROJECT: project_mgr_count++; break;
                case SessionType::MANAGER_TASK: task_mgr_count++; break;
                case SessionType::ACCOUNT: account_count++; break;
                case SessionType::REPO_MANAGER: repo_mgr_count++; break;
                case SessionType::REPO_WORKER: repo_worker_count++; break;
                default: break;
            }
        }

        // Left side: Mode indicator and connection info
        std::string left_text = "MANAGER MODE | " + tcp_host_ + ":" + std::to_string(tcp_port_);

        // Right side: Session info with type breakdown
        std::string right_text = "Sessions: " + std::to_string(sessions_.size()) + 
                                " | MGR:" + std::to_string(project_mgr_count + task_mgr_count) +
                                " ACC:" + std::to_string(account_count) +
                                " REPO:" + std::to_string(repo_mgr_count + repo_worker_count) +
                                " | " + (list_focused ? "LIST" : "INPUT");

        // Calculate spacing
        int total_len = left_text.length() + right_text.length();
        int spaces = max_x - total_len - 2;
        if (spaces < 1) spaces = 1;

        // Draw status bar
        mvwprintw(status_separator_win, 0, 0, "%s", left_text.c_str());
        for (int i = 0; i < spaces; ++i) {
            waddch(status_separator_win, ' ');
        }
        wprintw(status_separator_win, "%s", right_text.c_str());

        // Pad to end
        int current_x = left_text.length() + spaces + right_text.length();
        for (int i = current_x; i < max_x - 1; ++i) {
            waddch(status_separator_win, ' ');
        }

        wattroff(status_separator_win, A_REVERSE);
        wrefresh(status_separator_win);
    };

    // Initial population of session list
    update_session_list();

    // Add initial info to chat buffer
    chat_buffer.emplace_back("qwen Manager Mode", has_colors() ? 5 : 0);
    chat_buffer.emplace_back("TCP Server: " + tcp_host_ + ":" + std::to_string(tcp_port_), has_colors() ? 5 : 0);
    chat_buffer.emplace_back("Active Sessions: " + std::to_string(sessions_.size()), has_colors() ? 5 : 0);
    chat_buffer.emplace_back("Use TAB to switch between session list and input/output areas", has_colors() ? 3 : 0);
    chat_buffer.emplace_back("Use UP/DOWN arrows to navigate session list", has_colors() ? 3 : 0);
    chat_buffer.emplace_back("Use Ctrl+C twice to exit", has_colors() ? 3 : 0);
    chat_buffer.emplace_back("");

    // Initial draw
    redraw_session_list();
    redraw_status_separator();
    redraw_chat_window();
    redraw_input();

    // Set non-blocking input mode with 50ms timeout
    wtimeout(input_win, 50);

    bool should_exit = false;
    
    // Ctrl+C handling (double Ctrl+C to exit)
    auto last_ctrl_c_time = std::chrono::steady_clock::now();
    bool ctrl_c_pressed_recently = false;

    // Main event loop
    while (!should_exit && running_) {
        // Poll for connection events, etc. (non-blocking)
        // For now, just update session list periodically
        
        // Repaint session list periodically to show status changes
        update_session_list();
        redraw_session_list();
        redraw_status();

        // Get a character from the appropriate window based on focus
        int ch = ERR;
        if (list_focused) {
            // Check for input on the session list window
            wtimeout(list_win, 50);
            ch = wgetch(list_win);
            // Restore timeout for input window
            wtimeout(input_win, 50);
        } else {
            // Check for input on the input window (default)
            ch = wgetch(input_win);
        }

        if (ch != ERR) {
            // Handle special cases that work regardless of focus
            if (ch == 9) {  // TAB key - switch focus
                list_focused = !list_focused;
                continue;
            } else if (ch == 27) {  // ESC key
                // Escape might be start of arrow key sequence - check for that
                wtimeout(list_focused ? list_win : input_win, 10);
                int next_ch = wgetch(list_focused ? list_win : input_win);
                wtimeout(list_focused ? list_win : input_win, 50);  // Restore normal timeout
                
                if (next_ch != ERR) {
                    // This was an escape sequence (like arrow keys)
                    if (list_focused && next_ch == 65) {  // Up arrow
                        if (selected_session_idx > 1) {  // Skip header lines
                            selected_session_idx--;
                        }
                    } else if (list_focused && next_ch == 66) {  // Down arrow
                        if (selected_session_idx < (int)sessions_.size() - 1) {
                            selected_session_idx++;
                        }
                    }
                    continue;
                }
                // If it was a standalone ESC, ignore it
                continue;
            }

            // Handle mouse events
            if (ch == KEY_MOUSE) {
                MEVENT event;
                if (getmouse(&event) == OK) {
                    // Mouse wheel up = scroll session list or chat up
                    if (event.bstate & BUTTON4_PRESSED) {
                        if (list_focused) {
                            list_scroll_offset = std::min(list_scroll_offset + 3, 
                                                         (int)session_list_buffer.size() - list_height);
                        } else {
                            chat_scroll_offset = std::min(chat_scroll_offset + 3, 
                                                         (int)chat_buffer.size() - (max_y - list_height - 4));
                        }
                        redraw_session_list();
                        redraw_chat_window();
                        continue;
                    }
                    // Mouse wheel down = scroll session list or chat down
                    else if (event.bstate & BUTTON5_PRESSED) {
                        if (list_focused) {
                            list_scroll_offset = std::max(list_scroll_offset - 3, 0);
                        } else {
                            chat_scroll_offset = std::max(chat_scroll_offset - 3, 0);
                        }
                        redraw_session_list();
                        redraw_chat_window();
                        continue;
                    }
                }
                continue;
            }

            // Handle focus-specific input
            if (list_focused) {
                // Session list has focus
                if (ch == KEY_UP || ch == 65) {  // Up arrow
                    if (selected_session_idx > 1) {  // Skip header lines
                        selected_session_idx--;
                        update_session_list();  // Redraw with new selection
                        redraw_session_list();
                    }
                } else if (ch == KEY_DOWN || ch == 66) {  // Down arrow
                    if (selected_session_idx < (int)sessions_.size() + 1) {  // +1 to account for headers
                        selected_session_idx++;
                        update_session_list();  // Redraw with new selection
                        redraw_session_list();
                    }
                } else if (ch == '\n' || ch == KEY_ENTER || ch == 13) {
                    // Enter on a selected session to switch to that session's chat view
                    if (selected_session_idx >= 2 && selected_session_idx < (int)sessions_.size() + 2) {  // Account for header
                        int session_index = selected_session_idx - 2;  // Adjust for headers
                        if (session_index < sessions_.size()) {
                            const auto& session = sessions_[session_index];
                            active_session_id = session.session_id;
                            
                            // Initialize session's chat buffer if it doesn't exist
                            if (session_chat_buffers.find(active_session_id) == session_chat_buffers.end()) {
                                session_chat_buffers[active_session_id] = std::vector<OutputLine>();
                                
                                // Add session-specific initial messages based on type
                                std::string session_type_str;
                                int session_color = has_colors() ? 7 : 0;
                                
                                switch (session.type) {
                                    case SessionType::MANAGER_PROJECT:
                                        session_type_str = "PROJECT MANAGER";
                                        session_color = has_colors() ? 1 : 0; // Cyan
                                        session_chat_buffers[active_session_id].emplace_back("PROJECT MANAGER Session: " + session.session_id, session_color);
                                        session_chat_buffers[active_session_id].emplace_back("Model: " + session.model, has_colors() ? 7 : 0);
                                        session_chat_buffers[active_session_id].emplace_back("Instructions: " + (session.instructions.empty() ? "No instructions loaded" : "Loaded"), has_colors() ? 6 : 0);
                                        session_chat_buffers[active_session_id].emplace_back("", has_colors() ? 7 : 0);
                                        session_chat_buffers[active_session_id].emplace_back("Use this session for high-level project planning and architectural decisions", has_colors() ? 5 : 0);
                                        break;
                                    case SessionType::MANAGER_TASK:
                                        session_type_str = "TASK MANAGER";
                                        session_color = has_colors() ? 6 : 0; // Magenta
                                        session_chat_buffers[active_session_id].emplace_back("TASK MANAGER Session: " + session.session_id, session_color);
                                        session_chat_buffers[active_session_id].emplace_back("Model: " + session.model, has_colors() ? 7 : 0);
                                        session_chat_buffers[active_session_id].emplace_back("Instructions: " + (session.instructions.empty() ? "No instructions loaded" : "Loaded"), has_colors() ? 6 : 0);
                                        session_chat_buffers[active_session_id].emplace_back("", has_colors() ? 7 : 0);
                                        session_chat_buffers[active_session_id].emplace_back("Use this session for task coordination and issue resolution", has_colors() ? 5 : 0);
                                        break;
                                    case SessionType::ACCOUNT:
                                        session_type_str = "ACCOUNT";
                                        session_color = has_colors() ? 2 : 0; // Yellow
                                        session_chat_buffers[active_session_id].emplace_back("ACCOUNT Session: " + session.session_id, session_color);
                                        session_chat_buffers[active_session_id].emplace_back("Host: " + session.hostname, has_colors() ? 7 : 0);
                                        session_chat_buffers[active_session_id].emplace_back("Status: " + session.status, has_colors() ? 7 : 0);
                                        session_chat_buffers[active_session_id].emplace_back("", has_colors() ? 7 : 0);
                                        session_chat_buffers[active_session_id].emplace_back("Use this session for account-specific commands and status checks", has_colors() ? 5 : 0);
                                        break;
                                    case SessionType::REPO_MANAGER:
                                    case SessionType::REPO_WORKER:
                                        session_type_str = session.type == SessionType::REPO_MANAGER ? "REPO MANAGER" : "REPO WORKER";
                                        session_color = has_colors() ? 3 : 0; // Green
                                        session_chat_buffers[active_session_id].emplace_back(session_type_str + " Session: " + session.session_id, session_color);
                                        session_chat_buffers[active_session_id].emplace_back("Model: " + session.model, has_colors() ? 7 : 0);
                                        session_chat_buffers[active_session_id].emplace_back("Repo: " + session.repo_path, has_colors() ? 7 : 0);
                                        session_chat_buffers[active_session_id].emplace_back("", has_colors() ? 7 : 0);
                                        session_chat_buffers[active_session_id].emplace_back("Use this session for repository development work", has_colors() ? 5 : 0);
                                        break;
                                    default:
                                        session_type_str = "UNKNOWN";
                                        session_color = has_colors() ? 6 : 0; // Magenta
                                        session_chat_buffers[active_session_id].emplace_back("UNKNOWN Session: " + session.session_id, session_color);
                                        break;
                                }
                                
                                session_chat_buffers[active_session_id].emplace_back("", has_colors() ? 7 : 0);
                                session_chat_buffers[active_session_id].emplace_back("--- Session started ---", has_colors() ? 5 : 0);
                            }
                            
                            // Update chat window to show the selected session's content
                            redraw_chat_window();
                        }
                    }
                } else if (ch == KEY_PPAGE) {  // Page Up
                    list_scroll_offset = std::min(list_scroll_offset + list_height - 1, 
                                                 (int)session_list_buffer.size() - list_height);
                    redraw_session_list();
                } else if (ch == KEY_NPAGE) {  // Page Down
                    list_scroll_offset = std::max(list_scroll_offset - list_height + 1, 0);
                    redraw_session_list();
                } else if (ch == 3) {  // Ctrl+C
                    // Double Ctrl+C pattern: first press shows message, second press exits
                    auto now = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_ctrl_c_time).count();

                    if (ctrl_c_pressed_recently && elapsed < 2000) {
                        // Second Ctrl+C within 2 seconds - exit
                        chat_buffer.emplace_back("^C (exiting manager mode)", has_colors() ? 4 : 0);
                        redraw_chat_window();
                        should_exit = true;
                    } else {
                        // First Ctrl+C - show message
                        chat_buffer.emplace_back("^C (press Ctrl+C again to exit)", has_colors() ? 3 : 0);
                        redraw_chat_window();
                        ctrl_c_pressed_recently = true;
                        last_ctrl_c_time = now;
                    }
                }
            } else {
                // Input window has focus
                if (ch == '\n' || ch == KEY_ENTER || ch == 13) {
                    if (!input_buffer.empty()) {
                        // Determine which session buffer to add the message to
                        std::vector<OutputLine>* target_buffer = &chat_buffer;
                        
                        if (!active_session_id.empty()) {
                            // Add to active session's specific buffer
                            if (session_chat_buffers.find(active_session_id) != session_chat_buffers.end()) {
                                target_buffer = &session_chat_buffers[active_session_id];
                            }
                        }
                        
                        // Process input command
                        if (input_buffer == "/exit" || input_buffer == "/quit") {
                            should_exit = true;
                        } else if (input_buffer == "/list") {
                            update_session_list();  // Refresh the list
                            target_buffer->emplace_back("Session list refreshed", has_colors() ? 5 : 0);
                            redraw_chat_window();
                        } else if (input_buffer == "/status") {
                            target_buffer->emplace_back("Manager status:", has_colors() ? 5 : 0);
                            target_buffer->emplace_back("  - Running: " + std::string(running_ ? "Yes" : "No"), has_colors() ? 5 : 0);
                            target_buffer->emplace_back("  - TCP Server: " + std::string(tcp_server_ && tcp_server_->is_running() ? "Active" : "Inactive"), has_colors() ? 5 : 0);
                            target_buffer->emplace_back("  - Active Sessions: " + std::to_string(sessions_.size()), has_colors() ? 5 : 0);
                            redraw_chat_window();
                        } else if (input_buffer == "/clear") {
                            target_buffer->clear();
                            target_buffer->emplace_back("Session buffer cleared", has_colors() ? 5 : 0);
                            redraw_chat_window();
                        } else {
                            // Add the user message to the active session's buffer
                            std::string user_prefix = active_session_id.empty() ? "You" : active_session_id.substr(0, 10);
                            target_buffer->emplace_back(user_prefix + ": " + input_buffer, has_colors() ? 7 : 0);
                            
                            // In a real implementation, this is where we would send the input to the appropriate AI session
                            // For now, we'll just show a response placeholder
                            SessionInfo* active_session = find_session(active_session_id);
                            if (active_session) {
                                std::string ai_prefix = active_session->type == SessionType::MANAGER_PROJECT ? "PROJECT_MGR" :
                                                       active_session->type == SessionType::MANAGER_TASK ? "TASK_MGR" :
                                                       active_session->type == SessionType::ACCOUNT ? "ACCOUNT" :
                                                       active_session->type == SessionType::REPO_MANAGER ? "REPO_MGR" :
                                                       active_session->type == SessionType::REPO_WORKER ? "REPO_WRK" : "AI";
                                
                                // Placeholder response - in a real implementation, this would go to the actual qwen session
                                target_buffer->emplace_back(ai_prefix + ": Processing request (actual AI integration pending)", has_colors() ? 6 : 0);
                            } else {
                                target_buffer->emplace_back("AI: Processing request (no active session selected)", has_colors() ? 6 : 0);
                            }
                            
                            redraw_chat_window();
                        }

                        // Clear input buffer
                        input_buffer.clear();
                        cursor_pos = 0;
                    }
                    redraw_input();
                } 
                // Handle BACKSPACE
                else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
                    if (cursor_pos > 0) {
                        input_buffer.erase(cursor_pos - 1, 1);
                        cursor_pos--;
                        redraw_input();
                    }
                } 
                // Handle DELETE
                else if (ch == KEY_DC) {
                    if (cursor_pos < input_buffer.length()) {
                        input_buffer.erase(cursor_pos, 1);
                        redraw_input();
                    }
                } 
                // Handle LEFT arrow
                else if (ch == KEY_LEFT) {
                    if (cursor_pos > 0) {
                        cursor_pos--;
                        redraw_input();
                    }
                } 
                // Handle RIGHT arrow
                else if (ch == KEY_RIGHT) {
                    if (cursor_pos < input_buffer.length()) {
                        cursor_pos++;
                        redraw_input();
                    }
                } 
                // Handle HOME or Ctrl+A
                else if (ch == KEY_HOME || ch == 1) {
                    cursor_pos = 0;
                    redraw_input();
                } 
                // Handle END or Ctrl+E
                else if (ch == KEY_END || ch == 5) {
                    cursor_pos = input_buffer.length();
                    redraw_input();
                } 
                // Handle Ctrl+C
                else if (ch == 3) {  // Ctrl+C
                    // Double Ctrl+C pattern: first press clears input, second press exits
                    auto now = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_ctrl_c_time).count();

                    if (ctrl_c_pressed_recently && elapsed < 2000) {
                        // Second Ctrl+C within 2 seconds - exit
                        chat_buffer.emplace_back("^C (exiting manager mode)", has_colors() ? 4 : 0);
                        redraw_chat_window();
                        should_exit = true;
                    } else {
                        // First Ctrl+C - clear input buffer and show hint
                        input_buffer.clear();
                        cursor_pos = 0;
                        chat_buffer.emplace_back("^C (press Ctrl+C again to exit)", has_colors() ? 3 : 0);
                        redraw_chat_window();
                        redraw_input();
                        ctrl_c_pressed_recently = true;
                        last_ctrl_c_time = now;
                    }
                } 
                // Handle Ctrl+U (scroll up)
                else if (ch == 21) {  // Ctrl+U
                    // Scroll chat buffer up
                    chat_scroll_offset = std::min(chat_scroll_offset + 5, 
                                                 (int)chat_buffer.size() - (max_y - list_height - 4));
                    redraw_chat_window();
                } 
                // Handle Ctrl+D (scroll down)
                else if (ch == 4) {  // Ctrl+D
                    // Scroll chat buffer down
                    chat_scroll_offset = std::max(chat_scroll_offset - 5, 0);
                    redraw_chat_window();
                } 
                // Handle PAGE UP
                else if (ch == KEY_PPAGE) {
                    // For input window, page up scrolls chat window
                    chat_scroll_offset = std::min(chat_scroll_offset + 5, 
                                                 (int)chat_buffer.size() - (max_y - list_height - 4));
                    redraw_chat_window();
                } 
                // Handle PAGE DOWN
                else if (ch == KEY_NPAGE) {
                    // For input window, page down scrolls chat window
                    chat_scroll_offset = std::max(chat_scroll_offset - 5, 0);
                    redraw_chat_window();
                } 
                // Handle printable ASCII characters
                else if (ch >= 32 && ch <= 126 && ch != 127) {
                    input_buffer.insert(cursor_pos, 1, (char)ch);
                    cursor_pos++;
                    redraw_input();
                }
            }
        }

        // Small delay to avoid busy-waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Cleanup ncurses
    delwin(list_win);
    delwin(status_separator_win);
    delwin(chat_win);
    delwin(input_win);
    endwin();

    return true;
#else
    std::cout << "NCurses not available, falling back to simple mode\n";
    return run_simple_mode();
#endif
}

// Run manager mode in simple mode (stdio)
bool QwenManager::run_simple_mode() {
    std::cout << "qwen Manager Mode - Simple Mode\n";
    std::cout << "TCP Server: " << tcp_host_ << ":" << tcp_port_ << "\n";
    std::cout << "Type 'help' for commands, 'exit' to quit\n\n";
    
    // Show initial session list
    update_session_list();
    
    std::string input;
    while (running_) {
        std::cout << "> ";
        std::cout.flush();
        
        if (!std::getline(std::cin, input)) {
            break;  // EOF
        }
        
        // Process simple commands
        if (input == "exit" || input == "quit") {
            break;
        } else if (input == "help") {
            std::cout << "Available commands:\n";
            std::cout << "  help    - Show this help\n";
            std::cout << "  list    - Show all sessions\n";
            std::cout << "  status  - Show manager status\n";
            std::cout << "  exit    - Exit manager mode\n";
        } else if (input == "list") {
            update_session_list();
        } else if (input == "status") {
            std::cout << "Manager Status:\n";
            std::cout << "  TCP Server: " << (tcp_server_ && tcp_server_->is_running() ? "Running" : "Stopped") 
                     << " on " << tcp_host_ << ":" << tcp_port_ << "\n";
            std::cout << "  Active Sessions: " << sessions_.size() << "\n";
            std::cout << "  Running: " << (running_ ? "Yes" : "No") << "\n";
        } else {
            std::cout << "Unknown command: " << input << "\n";
            std::cout << "Type 'help' for available commands\n";
        }
    }
    
    return true;
}

void QwenManager::update_session_list() {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    std::cout << "Current Sessions:\n";
    std::cout << "Type      | ID          | Computer   | Repo Path                    | Status\n";
    std::cout << "----------|-------------|------------|------------------------------|-------\n";
    for (const auto& session : sessions_) {
        std::string type_str;
        switch (session.type) {
            case SessionType::MANAGER_PROJECT:
                type_str = "MGR-PROJ ";
                break;
            case SessionType::MANAGER_TASK:
                type_str = "MGR-TASK ";
                break;
            case SessionType::ACCOUNT:
                type_str = "ACCOUNT  ";
                break;
            case SessionType::REPO_MANAGER:
                type_str = "REPO-MGR ";
                break;
            case SessionType::REPO_WORKER:
                type_str = "REPO-WRK ";
                break;
            default:
                type_str = "UNKNOWN  ";
                break;
        }
        
        std::string status_str = session.is_active ? "active" : "inactive";
        std::cout << type_str << " | " 
                 << session.session_id.substr(0, 11) << " | "
                 << session.hostname.substr(0, 10) << " | "
                 << session.repo_path.substr(0, 26) << " | "
                 << status_str << "\n";
    }
    std::cout << "\n";
}

} // namespace Qwen