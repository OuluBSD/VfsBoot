// Complete Text Editor Demo using VfsShell UI Backend Abstraction
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>

// Include our UI backend abstraction
#include "VfsShell/ui_backend.h"
#include "VfsShell/file_browser.h"

// Mock VFS structure for demonstration
struct MockVfs {
    void write(const std::string& path, const std::string& content, size_t overlay_id) {
        std::cout << "[Mock VFS] Writing " << content.length() << " chars to " << path 
                  << " (overlay " << overlay_id << ")" << std::endl;
        
        // In a real implementation, this would write to the actual VFS
        // For demo, we'll just show the content being written
        std::cout << "--- Content written ---" << std::endl;
        std::cout << content << std::endl;
        std::cout << "--- End of content ---" << std::endl;
    }
    
    std::string read(const std::string& path, size_t overlay_id) {
        std::cout << "[Mock VFS] Reading from " << path 
                  << " (overlay " << overlay_id << ")" << std::endl;
                  
        // For demo purposes, return some sample content if it's a .cpp file
        if (path.substr(path.length() - 4) == ".cpp" || path.substr(path.length() - 2) == ".h") {
            return "// Sample C++ file\n#include <iostream>\n\nint main() {\n    std::cout << \"Hello, World!\" << std::endl;\n    return 0;\n}";
        } else if (path.substr(path.length() - 4) == ".txt") {
            return "This is a sample text file.\nYou can edit this in the text editor.\n";
        }
        return "";
    }
    
    bool exists(const std::string& path, size_t overlay_id) {
        std::cout << "[Mock VFS] Checking if " << path 
                  << " exists (overlay " << overlay_id << ")" << std::endl;
        // For demo, assume the file exists if it's not a specific test case
        return true;
    }
};

// Function to load file content into lines vector
void load_file_content(const std::string& content, std::vector<std::string>& lines) {
    lines.clear();
    std::istringstream iss(content);
    std::string line;
    while (std::getline(iss, line)) {
        lines.push_back(line);
    }
    
    // If file is empty or doesn't exist, add one empty line
    if (lines.empty()) {
        lines.push_back("");
    }
}

// Include editor functions
bool run_ncurses_editor(MockVfs& vfs, const std::string& vfs_path, std::vector<std::string>& lines, 
                       bool file_exists, size_t overlay_id);

int main() {
    std::cout << "=== VfsShell Complete Text Editor Demo ===" << std::endl;
    
#ifdef CODEX_UI_NCURSES
    std::cout << "Compiled with NCURSES backend" << std::endl;
#elif defined(CODEX_UI_BUILTIN)
    std::cout << "Compiled with BUILTIN terminal backend" << std::endl;
#else
    std::cout << "Compiled with FALLBACK backend" << std::endl;
#endif
    
    std::cout << "Starting text editor demo..." << std::endl;
    std::cout << "Press any key to continue to editor..." << std::endl;
    std::cin.get();
    
    // Create and run editor
    MockVfs vfs;
    
    // Option 1: Use file browser to select a file
    FileBrowser browser;
    std::string filepath;
    
    ui_init();
    ui_clear();
    ui_print_at(0, 0, "VfsShell Text Editor");
    ui_print_at(1, 0, "===================");
    ui_print_at(3, 0, "A) Use file browser to select a file");
    ui_print_at(4, 0, "B) Enter file path manually");
    ui_print_at(5, 0, "Q) Quit");
    ui_print_at(7, 0, "Choose an option (A/B/Q): ");
    ui_refresh();
    
    int ch = ui_getch();
    ui_end();
    
    if (ch == 'A' || ch == 'a') {
        // Use file browser
        if (browser.browse()) {
            filepath = "/demo/selected_file.cpp"; // In a real implementation, this would be the selected file
        } else {
            std::cout << "No file selected. Using default." << std::endl;
            filepath = "/demo/default_file.cpp";
        }
    } else if (ch == 'B' || ch == 'b') {
        std::cout << "Enter file path: ";
        std::getline(std::cin, filepath);
        if (filepath.empty()) {
            filepath = "/demo/default_file.cpp";
        }
    } else {
        std::cout << "Quitting demo." << std::endl;
        return 0;
    }
    
    // Load file content
    std::vector<std::string> lines;
    bool file_exists = vfs.exists(filepath, 0);
    std::string content = vfs.read(filepath, 0);
    load_file_content(content, lines);
    
    // Run the editor
    run_ncurses_editor(vfs, filepath, lines, file_exists, 0);
    
    std::cout << "Demo completed successfully!" << std::endl;
    std::cout << "Would you like to see the final content? (Y/N): ";
    char response;
    std::cin >> response;
    if (response == 'Y' || response == 'y') {
        std::cout << "\n--- Final Content ---" << std::endl;
        for (size_t i = 0; i < lines.size(); ++i) {
            std::cout << (i + 1) << ": " << lines[i] << std::endl;
        }
        std::cout << "--- End of Content ---" << std::endl;
    }
    
    std::cout << "\nText editor demo finished." << std::endl;
    return 0;
}

// NCURSES editor implementation (copy from editor_functions.cpp)
#ifdef CODEX_UI_NCURSES
#include <ncurses.h>
#include <iomanip>
#include <sstream>
#include <unistd.h>

// Helper function to determine if a word is a C++ keyword
bool is_cpp_keyword(const std::string& word) {
    static const char* keywords[] = {
        "auto", "break", "case", "char", "const", "continue", "default", "do", 
        "double", "else", "enum", "extern", "float", "for", "goto", "if", 
        "int", "long", "register", "return", "short", "signed", "sizeof", 
        "static", "struct", "switch", "typedef", "union", "unsigned", "void", 
        "volatile", "while", "asm", "bool", "catch", "class", "const_cast", 
        "delete", "dynamic_cast", "explicit", "false", "friend", "inline", 
        "mutable", "namespace", "new", "operator", "private", "protected", 
        "public", "reinterpret_cast", "static_cast", "template", "this", 
        "throw", "true", "try", "typeid", "typename", "using", "virtual", 
        "wchar_t", "and", "and_eq", "bitand", "bitor", "compl", "not", 
        "not_eq", "or", "or_eq", "xor", "xor_eq", "override", "final", "nullptr"
    };
    
    for (const char* kw : keywords) {
        if (word == kw) {
            return true;
        }
    }
    return false;
}

bool run_ncurses_editor(MockVfs& vfs, const std::string& vfs_path, std::vector<std::string>& lines, 
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
        init_pair(3, COLOR_CYAN, COLOR_BLACK);   // Line numbers
        init_pair(4, COLOR_RED, COLOR_BLACK);    // Modified indicator
        init_pair(5, COLOR_GREEN, COLOR_BLACK);  // Keywords
        init_pair(6, COLOR_MAGENTA, COLOR_BLACK); // Strings
        init_pair(7, COLOR_WHITE, COLOR_BLACK);   // Comments
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
        
        // Draw content area with syntax highlighting
        for (int i = 0; i < content_height && (top_line + i) < static_cast<int>(lines.size()); ++i) {
            int line_idx = top_line + i;
            int screen_row = i + 2;
            
            // Line number
            attron(COLOR_PAIR(3));
            mvprintw(screen_row, 0, "%3d:", line_idx + 1);
            attroff(COLOR_PAIR(3));
            
            // Line content with syntax highlighting
            if (line_idx < static_cast<int>(lines.size())) {
                std::string line = lines[line_idx];
                
                // Truncate if too long
                if (static_cast<int>(line.length()) > cols - 6) {
                    line = line.substr(0, cols - 9) + "...";
                }
                
                // Parse the line for syntax highlighting
                int col = 5; // Start position after line number
                int in_string = 0; // 0 = not in string, 1 = "string", 2 = 'char'
                bool in_comment = false;
                
                // This is a simplified version of syntax highlighting
                for (size_t j = 0; j < line.length(); j++) {
                    char ch = line[j];
                    char next_ch = (j + 1 < line.length()) ? line[j + 1] : 0;
                    
                    // Check for comment start
                    if (!in_string && !in_comment && ch == '/' && next_ch == '/') {
                        in_comment = true;
                        attron(COLOR_PAIR(7)); // Comment color
                    }
                    // Check for string start/end
                    else if (!in_comment && ch == '"') {
                        if (in_string == 0) {
                            in_string = 1; // Start of string
                            attron(COLOR_PAIR(6)); // String color
                        } else if (in_string == 1) {
                            in_string = 0; // End of string
                            attroff(COLOR_PAIR(6));
                        }
                    }
                    else if (!in_comment && ch == '\'') {
                        if (in_string == 0) {
                            in_string = 2; // Start of char
                            attron(COLOR_PAIR(6)); // String/char color
                        } else if (in_string == 2) {
                            in_string = 0; // End of char
                            attroff(COLOR_PAIR(6));
                        }
                    }
                    
                    // Print character
                    mvprintw(screen_row, col++, "%c", ch);
                }
                
                // Turn off highlighting if we're still in it
                if (in_comment) {
                    attroff(COLOR_PAIR(7));
                } else if (in_string) {
                    attroff(COLOR_PAIR(6));
                }
                
                // For demonstration purposes, let's also highlight keywords
                // by re-printing them in keyword color
                std::string line_content = lines[line_idx];
                col = 5; // Reset position
                std::string word = "";
                
                // We'll do a second pass to highlight keywords
                for (size_t j = 0; j <= line_content.length(); j++) {
                    char ch = (j < line_content.length()) ? line_content[j] : ' '; // Add space at end to catch last word
                    
                    if (isalpha(ch) || ch == '_') {
                        word += ch;
                    } else {
                        if (!word.empty()) {
                            if (is_cpp_keyword(word)) {
                                // Find the keyword in the line and highlight it
                                size_t pos = line_content.find(word, j - word.length());
                                if (pos != std::string::npos) {
                                    attroff(COLOR_PAIR(0)); // Turn off any previous color
                                    attron(COLOR_PAIR(5)); // Keyword color
                                    mvprintw(screen_row, 5 + pos, "%s", word.c_str());
                                    attroff(COLOR_PAIR(5));
                                    attron(COLOR_PAIR(0)); // Reset to default
                                }
                            }
                            word = "";
                        }
                    }
                }
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
                
            case KEY_PPAGE:  // Page Up
                if (current_line > 0) {
                    int lines_to_move = std::min(content_height - 1, current_line);
                    current_line -= lines_to_move;
                    if (current_line < top_line) {
                        top_line = current_line;
                    }
                    // Adjust cursor_x if needed
                    if (cursor_x > static_cast<int>(lines[current_line].length())) {
                        cursor_x = lines[current_line].length();
                    }
                }
                break;
                
            case KEY_NPAGE:  // Page Down
                if (current_line < static_cast<int>(lines.size()) - 1) {
                    int lines_to_move = std::min(content_height - 1, 
                                                static_cast<int>(lines.size()) - 1 - current_line);
                    current_line += std::min(lines_to_move, content_height - 1);
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
                        mvprintw(4, 2, "Page Up/Dn - Scroll page");
                        mvprintw(5, 2, "ESC        - Enter command mode");
                        mvprintw(6, 0, "Editing:");
                        mvprintw(7, 2, "Type       - Insert text");
                        mvprintw(8, 2, "Backspace  - Delete character before cursor");
                        mvprintw(9, 2, "Delete     - Delete character at cursor");
                        mvprintw(10, 2, "Enter      - Insert new line");
                        mvprintw(11, 0, "Commands (in command mode):");
                        mvprintw(12, 2, ":w         - Save file");
                        mvprintw(13, 2, ":q         - Quit");
                        mvprintw(14, 2, ":q!        - Quit without saving");
                        mvprintw(15, 2, ":wq or :x  - Save and quit");
                        mvprintw(16, 2, ":/text     - Search for text");
                        mvprintw(17, 2, ":help      - Show this help");
                        mvprintw(19, 0, "Press any key to continue...");
                        refresh();
                        getch();
                    } else if (command.length() > 0 && command[0] == '/') {
                        // Search command: /text
                        std::string search_text = command.substr(1);
                        if (!search_text.empty()) {
                            bool found = false;
                            int start_line = current_line + 1; // Start searching from next line
                            
                            // Search from current position to end of file
                            for (size_t i = start_line; i < lines.size(); ++i) {
                                size_t pos = lines[i].find(search_text);
                                if (pos != std::string::npos) {
                                    current_line = i;
                                    cursor_x = pos;
                                    found = true;
                                    // Adjust view if needed
                                    if (current_line < top_line) {
                                        top_line = current_line;
                                    } else if (current_line >= top_line + content_height) {
                                        top_line = current_line - content_height + 1;
                                    }
                                    break;
                                }
                            }
                            
                            // If not found from current position, search from beginning
                            if (!found) {
                                for (size_t i = 0; i < start_line; ++i) {
                                    size_t pos = lines[i].find(search_text);
                                    if (pos != std::string::npos) {
                                        current_line = i;
                                        cursor_x = pos;
                                        found = true;
                                        // Adjust view if needed
                                        if (current_line < top_line) {
                                            top_line = current_line;
                                        } else if (current_line >= top_line + content_height) {
                                            top_line = current_line - content_height + 1;
                                        }
                                        break;
                                    }
                                }
                            }
                            
                            if (!found) {
                                // Show not found message
                                move(rows - 1, 0);
                                clrtoeol();
                                attron(COLOR_PAIR(2) | A_BOLD);
                                printw("Pattern not found: %s", search_text.c_str());
                                attroff(COLOR_PAIR(2) | A_BOLD);
                                refresh();
                                sleep(1);
                            }
                        }
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