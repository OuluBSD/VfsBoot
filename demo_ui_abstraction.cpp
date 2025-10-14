// Demonstration of UI backend abstraction scalability
#include <iostream>
#include <vector>
#include <string>
#include <unistd.h>  // For sleep()

// Include our UI backend abstraction
#include "VfsShell/ui_backend.h"

// Mock VFS structure for testing
struct MockVfs {
    void write(const std::string& path, const std::string& content, size_t overlay_id) {
        std::cout << "[Mock VFS] Writing " << content.length() << " chars to " << path 
                  << " (overlay " << overlay_id << ")" << std::endl;
    }
};

// Simple text editor implementation using our UI abstraction
class SimpleTextEditor {
private:
    MockVfs& vfs;
    std::string filepath;
    std::vector<std::string> lines;
    size_t current_line;
    size_t top_visible_line;
    bool modified;

public:
    SimpleTextEditor(MockVfs& v, const std::string& path) 
        : vfs(v), filepath(path), current_line(0), top_visible_line(0), modified(false) {
        // Initialize with some sample content
        lines.push_back("// Simple Text Editor");
        lines.push_back("// Using UI backend abstraction");
        lines.push_back("");
        lines.push_back("#include <iostream>");
        lines.push_back("");
        lines.push_back("int main() {");
        lines.push_back("    std::cout << \"Hello, World!\" << std::endl;");
        lines.push_back("    return 0;");
        lines.push_back("}");
    }

    void run() {
        ui_init();
        ui_clear();
        
        bool running = true;
        while (running) {
            draw_screen();
            ui_refresh();
            
            int ch = ui_getch();
            switch (ch) {
                case 'q':
                case 'Q':
                    running = false;
                    break;
                case 'j':  // Move down
                    if (current_line < lines.size() - 1) {
                        current_line++;
                        if (current_line >= top_visible_line + get_visible_lines() - 1) {
                            top_visible_line++;
                        }
                    }
                    break;
                case 'k':  // Move up
                    if (current_line > 0) {
                        current_line--;
                        if (current_line < top_visible_line) {
                            top_visible_line = current_line;
                        }
                    }
                    break;
                case 'i':  // Insert line
                    lines.insert(lines.begin() + current_line, "");
                    modified = true;
                    break;
                case 'd':  // Delete line
                    if (!lines.empty()) {
                        lines.erase(lines.begin() + current_line);
                        if (current_line >= lines.size() && current_line > 0) {
                            current_line = lines.size() - 1;
                        }
                        modified = true;
                    }
                    break;
                case 'w':  // Write file
                    save_file();
                    break;
            }
        }
        
        ui_end();
        std::cout << "Editor closed. File was " << (modified ? "" : "not ") 
                  << "modified during session." << std::endl;
    }

private:
    void draw_screen() {
        ui_clear();
        
        // Draw title
        ui_move(0, 0);
        ui_print(("Simple Text Editor - " + filepath).c_str());
        
        // Draw separator
        ui_move(1, 0);
        for (int i = 0; i < ui_cols() - 1; i++) {
            ui_print("-");
        }
        
        // Draw content
        int content_height = ui_rows() - 4;
        int lines_drawn = 0;
        
        for (size_t i = top_visible_line; 
             i < lines.size() && lines_drawn < content_height; 
             ++i, ++lines_drawn) {
            
            ui_move(lines_drawn + 2, 0);
            
            // Line number
            char line_num[16];
            snprintf(line_num, sizeof(line_num), "%3zu%c ", i + 1, 
                     (i == current_line) ? '>' : ':');
            ui_print(line_num);
            
            // Line content
            ui_print(lines[i].c_str());
        }
        
        // Fill remaining lines with tildes
        for (int i = lines_drawn; i < content_height; ++i) {
            ui_move(i + 2, 0);
            ui_print("~");
        }
        
        // Draw status line
        ui_move(ui_rows() - 1, 0);
        for (int i = 0; i < ui_cols() - 1; i++) {
            ui_print(" ");
        }
        ui_move(ui_rows() - 1, 0);
        std::string status = "Line: " + std::to_string(current_line + 1) + 
                             "/" + std::to_string(lines.size()) +
                             " | Modified: " + (modified ? "Yes" : "No") +
                             " | Commands: q(quit) j/k(move) i(insert) d(delete) w(save)";
        ui_print(status.c_str());
    }
    
    void save_file() {
        // Mock save operation
        std::string content;
        for (size_t i = 0; i < lines.size(); ++i) {
            content += lines[i];
            if (i < lines.size() - 1) content += "\n";
        }
        vfs.write(filepath, content, 0);
        modified = false;
        
        // Show confirmation
        ui_move(ui_rows() - 1, 0);
        for (int i = 0; i < ui_cols() - 1; i++) {
            ui_print(" ");
        }
        ui_move(ui_rows() - 1, 0);
        ui_print("[Saved successfully!]");
        ui_refresh();
        sleep(1);
    }
    
    int get_visible_lines() {
        return ui_rows() - 4;  // Account for title, separator, and status lines
    }
};

int main() {
    std::cout << "=== UI Backend Abstraction Demo ===" << std::endl;
    
#ifdef CODEX_UI_NCURSES
    std::cout << "Compiled with NCURSES backend" << std::endl;
#elif defined(CODEX_UI_BUILTIN)
    std::cout << "Compiled with BUILTIN terminal backend" << std::endl;
#else
    std::cout << "Compiled with FALLBACK backend" << std::endl;
#endif
    
    std::cout << "Starting simple text editor demo..." << std::endl;
    std::cout << "Press any key to continue to editor..." << std::endl;
    std::cin.get();
    
    // Create and run editor
    MockVfs vfs;
    SimpleTextEditor editor(vfs, "/demo/file.cpp");
    editor.run();
    
    std::cout << "\nDemo completed successfully!" << std::endl;
    return 0;
}