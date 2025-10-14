// ========================================================================
// Enhanced Editor Implementation with UI Backend Abstraction
// ========================================================================

#ifdef CODEX_UI_NCURSES
#include <ncurses.h>
#endif
#include <iomanip>
#include <sstream>
#include <unistd.h>

// Forward declarations for editor functions
bool run_ncurses_editor(Vfs& vfs, const std::string& vfs_path, std::vector<std::string>& lines, 
                       bool file_exists, size_t overlay_id);
bool run_simple_editor(Vfs& vfs, const std::string& vfs_path, std::vector<std::string>& lines, 
                      bool file_exists, size_t overlay_id);

#ifdef CODEX_UI_NCURSES
// NCURSES-based editor implementation
bool run_ncurses_editor(Vfs& vfs, const std::string& vfs_path, std::vector<std::string>& lines, 
                       bool file_exists, size_t overlay_id) {
    // Initialize ncurses
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    
    // Colors (if supported)
    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_BLUE, COLOR_BLACK);   // Title bar
        init_pair(2, COLOR_YELLOW, COLOR_BLACK); // Status bar
        init_pair(3, COLOR_CYAN, COLOR_BLACK);    // Line numbers
        init_pair(4, COLOR_RED, COLOR_BLACK);     // Modified indicator
    }
    
    int current_line = 0;
    int top_line = 0;
    int cursor_x = 0;
    bool file_modified = false;
    bool editor_active = true;
    int content_height = rows - 4;  // Leave space for status bars
    
    // Main editor loop
    while (editor_active) {
        // Clear screen
        clear();
        
        // Draw title bar
        attron(COLOR_PAIR(1) | A_BOLD);
        mvprintw(0, 0, "VfsShell Text Editor - %s", vfs_path.c_str());
        attroff(COLOR_PAIR(1) | A_BOLD);
        
        // Draw separator
        mvhline(1, 0, '-', cols);
        
        // Draw content area
        for (int i = 0; i < content_height && (top_line + i) < static_cast<int>(lines.size()); ++i) {
            int line_idx = top_line + i;
            int screen_row = i + 2;
            
            // Line number
            attron(COLOR_PAIR(3));
            mvprintw(screen_row, 0, "%3d:", line_idx + 1);
            attroff(COLOR_PAIR(3));
            
            // Line content
            if (line_idx < static_cast<int>(lines.size())) {
                std::string display_line = lines[line_idx];
                // Truncate if too long
                if (static_cast<int>(display_line.length()) > cols - 6) {
                    display_line = display_line.substr(0, cols - 9) + "...";
                }
                mvprintw(screen_row, 5, "%s", display_line.c_str());
            }
        }
        
        // Fill remaining lines with tildes
        for (int i = lines.size() - top_line; i < content_height; ++i) {
            int screen_row = i + 2;
            if (screen_row < rows - 2) {
                mvprintw(screen_row, 0, "~");
            }
        }
        
        // Draw separator
        mvhline(rows - 2, 0, '-', cols);
        
        // Draw status bar
        attron(COLOR_PAIR(2));
        mvprintw(rows - 1, 0, "Line:%d/%d Col:%d | %s%s | :w (save) :q (quit) :wq (save&quit)",
                current_line + 1, static_cast<int>(lines.size()), cursor_x,
                file_modified ? "[Modified] " : "",
                !file_exists ? "[New File] " : "");
        attroff(COLOR_PAIR(2));
        
        // Position cursor
        if (current_line >= top_line && current_line < top_line + content_height) {
            move(current_line - top_line + 2, std::min(cursor_x + 5, cols - 1));
        }
        
        // Refresh screen
        refresh();
        
        // Get user input
        int ch = getch();
        
        // Process input
        switch (ch) {
            case KEY_UP:
                if (current_line > 0) {
                    current_line--;
                    if (current_line < top_line) {
                        top_line = current_line;
                    }
                    // Adjust cursor_x if needed
                    if (cursor_x > static_cast<int>(lines[current_line].length())) {
                        cursor_x = lines[current_line].length();
                    }
                }
                break;
                
            case KEY_DOWN:
                if (current_line < static_cast<int>(lines.size()) - 1) {
                    current_line++;
                    if (current_line >= top_line + content_height) {
                        top_line = current_line - content_height + 1;
                    }
                    // Adjust cursor_x if needed
                    if (cursor_x > static_cast<int>(lines[current_line].length())) {
                        cursor_x = lines[current_line].length();
                    }
                }
                break;
                
            case KEY_LEFT:
                if (cursor_x > 0) {
                    cursor_x--;
                } else if (current_line > 0) {
                    // Move to end of previous line
                    current_line--;
                    if (current_line < top_line) {
                        top_line = current_line;
                    }
                    cursor_x = lines[current_line].length();
                }
                break;
                
            case KEY_RIGHT:
                if (cursor_x < static_cast<int>(lines[current_line].length())) {
                    cursor_x++;
                } else if (current_line < static_cast<int>(lines.size()) - 1) {
                    // Move to beginning of next line
                    current_line++;
                    if (current_line >= top_line + content_height) {
                        top_line = current_line - content_height + 1;
                    }
                    cursor_x = 0;
                }
                break;
                
            case KEY_BACKSPACE:
            case 127:  // Delete key
            case '\b':
                if (cursor_x > 0) {
                    lines[current_line].erase(cursor_x - 1, 1);
                    cursor_x--;
                    file_modified = true;
                } else if (current_line > 0) {
                    // Join with previous line
                    std::string current_content = lines[current_line];
                    lines.erase(lines.begin() + current_line);
                    current_line--;
                    cursor_x = lines[current_line].length();
                    lines[current_line] += current_content;
                    file_modified = true;
                    
                    if (current_line < top_line) {
                        top_line = current_line;
                    }
                }
                break;
                
            case KEY_DC:  // Delete key
                if (cursor_x < static_cast<int>(lines[current_line].length())) {
                    lines[current_line].erase(cursor_x, 1);
                    file_modified = true;
                } else if (current_line < static_cast<int>(lines.size()) - 1) {
                    // Join with next line
                    std::string next_content = lines[current_line + 1];
                    lines[current_line] += next_content;
                    lines.erase(lines.begin() + current_line + 1);
                    file_modified = true;
                }
                break;
                
            case KEY_ENTER:
            case '\n':
            case '\r': {
                // Split line at cursor position
                std::string new_line = lines[current_line].substr(cursor_x);
                lines[current_line] = lines[current_line].substr(0, cursor_x);
                lines.insert(lines.begin() + current_line + 1, new_line);
                current_line++;
                cursor_x = 0;
                file_modified = true;
                
                if (current_line >= top_line + content_height) {
                    top_line = current_line - content_height + 1;
                }
                break;
            }
                
            case 27:  // ESC key - command mode
                {
                    // Show command prompt at bottom
                    move(rows - 1, 0);
                    clrtoeol();
                    attron(COLOR_PAIR(2));
                    printw(":");
                    attroff(COLOR_PAIR(2));
                    refresh();
                    
                    // Get command
                    echo();
                    char cmd[256];
                    getnstr(cmd, sizeof(cmd) - 1);
                    noecho();
                    
                    std::string command = cmd;
                    
                    if (command == "q") {
                        if (file_modified) {
                            // Show warning
                            move(rows - 1, 0);
                            clrtoeol();
                            attron(COLOR_PAIR(2) | A_BOLD);
                            printw("File modified. Use :wq to save or :q! to quit without saving.");
                            attroff(COLOR_PAIR(2) | A_BOLD);
                            refresh();
                            getch();  // Wait for keypress
                        } else {
                            editor_active = false;
                        }
                    } else if (command == "q!") {
                        editor_active = false;
                    } else if (command == "w") {
                        // Save file
                        std::ostringstream oss;
                        for (size_t i = 0; i < lines.size(); ++i) {
                            oss << lines[i];
                            if (i < lines.size() - 1) oss << "\n";
                        }
                        std::string new_content = oss.str();
                        vfs.write(vfs_path, new_content, overlay_id);
                        file_modified = false;
                        
                        // Show confirmation
                        move(rows - 1, 0);
                        clrtoeol();
                        attron(COLOR_PAIR(2));
                        printw("[Saved %d lines to %s]", static_cast<int>(lines.size()), vfs_path.c_str());
                        attroff(COLOR_PAIR(2));
                        refresh();
                        sleep(1);
                    } else if (command == "wq" || command == "x") {
                        // Save and quit
                        std::ostringstream oss;
                        for (size_t i = 0; i < lines.size(); ++i) {
                            oss << lines[i];
                            if (i < lines.size() - 1) oss << "\n";
                        }
                        std::string new_content = oss.str();
                        vfs.write(vfs_path, new_content, overlay_id);
                        editor_active = false;
                    } else if (command == "help") {
                        // Show help
                        clear();
                        mvprintw(0, 0, "VfsShell Editor Help");
                        mvprintw(1, 0, "=====================");
                        mvprintw(2, 0, "Navigation:");
                        mvprintw(3, 2, "Arrow Keys - Move cursor");
                        mvprintw(4, 2, "ESC        - Enter command mode");
                        mvprintw(5, 0, "Editing:");
                        mvprintw(6, 2, "Type       - Insert text");
                        mvprintw(7, 2, "Backspace  - Delete character before cursor");
                        mvprintw(8, 2, "Delete     - Delete character at cursor");
                        mvprintw(9, 2, "Enter      - Insert new line");
                        mvprintw(10, 0, "Commands (in command mode):");
                        mvprintw(11, 2, ":w         - Save file");
                        mvprintw(12, 2, ":q         - Quit");
                        mvprintw(13, 2, ":q!        - Quit without saving");
                        mvprintw(14, 2, ":wq or :x  - Save and quit");
                        mvprintw(15, 2, ":help      - Show this help");
                        mvprintw(17, 0, "Press any key to continue...");
                        refresh();
                        getch();
                    } else if (!command.empty()) {
                        // Unknown command
                        move(rows - 1, 0);
                        clrtoeol();
                        attron(COLOR_PAIR(2) | A_BOLD);
                        printw("Unknown command: %s", command.c_str());
                        attroff(COLOR_PAIR(2) | A_BOLD);
                        refresh();
                        sleep(1);
                    }
                }
                break;
                
            default:
                // Regular character input
                if (ch >= 32 && ch <= 126) {  // Printable ASCII
                    lines[current_line].insert(cursor_x, 1, static_cast<char>(ch));
                    cursor_x++;
                    file_modified = true;
                }
                break;
        }
    }
    
    // Clean up ncurses
    endwin();
    
    return true;
}
#endif

// Simple terminal-based editor (fallback implementation)
bool run_simple_editor(Vfs& vfs, const std::string& vfs_path, std::vector<std::string>& lines, 
                      bool file_exists, size_t overlay_id) {
    // Show the editor interface immediately - simple approach
    std::cout << "\033[2J\033[H"; // Clear screen
    std::cout << "\033[34;1mVfsShell Text Editor - " << vfs_path << "\033[0m" << std::endl;
    std::cout << "\033[34m" << std::string(60, '=') << "\033[0m" << std::endl;
    
    // Display content with line numbers
    for(size_t i = 0; i < lines.size(); ++i) {
        std::cout << std::right << std::setw(3) << (i + 1) << ": " << lines[i] << std::endl;
    }
    
    // If there are fewer than 10 lines, show some tildes for empty space
    for(size_t i = lines.size(); i < 10 && i < 20; ++i) {
        std::cout << std::right << std::setw(3) << (i + 1) << ": ~" << std::endl;
    }
    
    std::cout << "\033[34m" << std::string(60, '=') << "\033[0m" << std::endl;
    std::cout << "\033[33mStatus: " << lines.size() << " lines | ";
    if (!file_exists) std::cout << "[New File] | ";
    std::cout << "Type :wq to save&quit, :q to quit, :help for commands\033[0m" << std::endl;
    std::cout << std::endl;
    
    bool editor_active = true;
    bool file_modified = false;
    
    while(editor_active) {
        std::cout << "Editor> ";
        std::cout.flush();
        
        std::string command;
        if(!std::getline(std::cin, command)) {
            break; // EOF
        }
        
        // Process editor commands - simpler approach
        if(command == ":q") {
            editor_active = false;
        } else if(command == ":wq" || command == ":x") {
            // Write and quit
            std::ostringstream oss;
            for(size_t i = 0; i < lines.size(); ++i) {
                oss << lines[i];
                if(i < lines.size() - 1) oss << "\n";
            }
            std::string new_content = oss.str();
            
            // Write to VFS
            vfs.write(vfs_path, new_content, overlay_id);
            std::cout << "[Saved " << lines.size() << " lines to " << vfs_path << " and exited]\n";
            editor_active = false;
        } else if(command == ":w") {
            // Write only
            std::ostringstream oss;
            for(size_t i = 0; i < lines.size(); ++i) {
                oss << lines[i];
                if(i < lines.size() - 1) oss << "\n";
            }
            std::string new_content = oss.str();
            
            // Write to VFS
            vfs.write(vfs_path, new_content, overlay_id);
            file_modified = false;
            std::cout << "[Saved " << lines.size() << " lines to " << vfs_path << "]\n";
        } else if(command == ":help") {
            std::cout << "Editor Commands:\n";
            std::cout << "  :w          - Write (save) file\n";
            std::cout << "  :wq         - Write file and quit\n";
            std::cout << "  :q          - Quit without saving\n";
            std::cout << "  i<line> <text> - Insert line (e.g., 'i5 hello')\n";
            std::cout << "  d<line>     - Delete line (e.g., 'd5')\n";
            std::cout << "  c<line> <text> - Change line (e.g., 'c5 new text')\n";
            std::cout << "  p           - Print current content\n";
            std::cout << "  :help       - Show this help\n";
        } else if(command.substr(0, 1) == "i" && command.size() > 1 && std::isdigit(command[1])) {
            // Insert line: i<line_num> <text>
            size_t pos = 1;
            std::string num_str = "";
            while(pos < command.size() && std::isdigit(command[pos])) {
                num_str += command[pos];
                pos++;
            }
            
            if(!num_str.empty()) {
                int line_num = std::stoi(num_str);
                if(line_num >= 1 && line_num <= static_cast<int>(lines.size() + 1)) {
                    std::string text = (pos < command.size()) ? command.substr(pos + 1) : "";
                    lines.insert(lines.begin() + (line_num - 1), text);
                    file_modified = true;
                    std::cout << "[Inserted line " << line_num << "]\n";
                } else {
                    std::cout << "[Error: line number out of range]\n";
                }
            } else {
                std::cout << "[Invalid insert command]\n";
            }
        } else if(command.substr(0, 1) == "d" && command.size() > 1 && std::isdigit(command[1])) {
            // Delete line: d<line_num>
            size_t pos = 1;
            std::string num_str = "";
            while(pos < command.size() && std::isdigit(command[pos])) {
                num_str += command[pos];
                pos++;
            }
            
            if(!num_str.empty()) {
                int line_num = std::stoi(num_str);
                if(line_num >= 1 && line_num <= static_cast<int>(lines.size())) {
                    lines.erase(lines.begin() + (line_num - 1));
                    file_modified = true;
                    std::cout << "[Deleted line " << line_num << "]\n";
                    if(line_num <= static_cast<int>(lines.size())) {
                        // Refresh display after deletion
                        std::cout << "\033[2J\033[H"; // Clear and show current state
                        std::cout << "\033[34;1mVfsShell Text Editor - " << vfs_path << "\033[0m" << std::endl;
                        std::cout << "\033[34m" << std::string(60, '=') << "\033[0m" << std::endl;
                        
                        for(size_t i = 0; i < lines.size(); ++i) {
                            std::cout << std::right << std::setw(3) << (i + 1) << ": " << lines[i] << std::endl;
                        }
                        
                        // Show more empty lines if needed
                        for(size_t i = lines.size(); i < 10 && i < 20; ++i) {
                            std::cout << std::right << std::setw(3) << (i + 1) << ": ~" << std::endl;
                        }
                        
                        std::cout << "\033[34m" << std::string(60, '=') << "\033[0m" << std::endl;
                        std::cout << "\033[33mStatus: " << lines.size() << " lines | ";
                        if (!file_exists && !file_modified) std::cout << "[New File] | ";
                        if (file_modified) std::cout << "[Modified] | ";
                        std::cout << "Type :wq to save&quit, :q to quit, :help for commands\033[0m" << std::endl;
                        std::cout << std::endl;
                    }
                } else {
                    std::cout << "[Error: line number out of range]\n";
                }
            } else {
                std::cout << "[Invalid delete command]\n";
            }
        } else if(command.substr(0, 1) == "c" && command.size() > 1 && std::isdigit(command[1])) {
            // Change line: c<line_num> <text>
            size_t pos = 1;
            std::string num_str = "";
            while(pos < command.size() && std::isdigit(command[pos])) {
                num_str += command[pos];
                pos++;
            }
            
            if(!num_str.empty()) {
                int line_num = std::stoi(num_str);
                if(line_num >= 1 && line_num <= static_cast<int>(lines.size())) {
                    std::string text = (pos < command.size()) ? command.substr(pos + 1) : "";
                    lines[line_num - 1] = text;
                    file_modified = true;
                    std::cout << "[Changed line " << line_num << "]\n";
                } else {
                    std::cout << "[Error: line number out of range]\n";
                }
            } else {
                std::cout << "[Invalid change command]\n";
            }
        } else if(command == "p") {
            // Print current content
            std::cout << "\033[2J\033[H"; // Clear and show current state
            std::cout << "\033[34;1mVfsShell Text Editor - " << vfs_path << "\033[0m" << std::endl;
            std::cout << "\033[34m" << std::string(60, '=') << "\033[0m" << std::endl;
            
            for(size_t i = 0; i < lines.size(); ++i) {
                std::cout << std::right << std::setw(3) << (i + 1) << ": " << lines[i] << std::endl;
            }
            
            // Show more empty lines if needed
            for(size_t i = lines.size(); i < 10 && i < 20; ++i) {
                std::cout << std::right << std::setw(3) << (i + 1) << ": ~" << std::endl;
            }
            
            std::cout << "\033[34m" << std::string(60, '=') << "\033[0m" << std::endl;
            std::cout << "\033[33mStatus: " << lines.size() << " lines | ";
            if (!file_exists && !file_modified) std::cout << "[New File] | ";
            if (file_modified) std::cout << "[Modified] | ";
            std::cout << "Type :wq to save&quit, :q to quit, :help for commands\033[0m" << std::endl;
            std::cout << std::endl;
        } else if(command.empty()) {
            // Do nothing, just continue
        } else {
            std::cout << "[Unknown command. Type :help for options]\n";
        }
    }
    
    std::cout << "\033[2J\033[H"; // Clear screen on exit
    std::cout << "Editor closed. Return to shell.\n";
    
    return true;
}