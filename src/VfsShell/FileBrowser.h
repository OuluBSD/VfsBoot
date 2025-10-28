#ifndef _VfsShell_file_browser_h_
#define _VfsShell_file_browser_h_

#include "ui_backend.h"
#include <string>
#include <vector>

// Simple file browser interface
class FileBrowser {
private:
    std::string current_dir;
    std::vector<std::string> entries;
    std::vector<bool> is_directory;  // true if entry is a directory
    int selected_index;
    int top_index;
    int max_display_items;

public:
    FileBrowser() : current_dir("."), selected_index(0), top_index(0), max_display_items(15) {}

    void set_directory(const std::string& dir) {
        current_dir = dir;
        refresh();
    }

    void refresh() {
        // In a real implementation, this would populate entries from the VFS
        // For now, we'll just add some dummy entries
        entries.clear();
        is_directory.clear();
        
        // Add directory entries
        entries.push_back("..");
        is_directory.push_back(true);
        
        entries.push_back("file1.txt");
        is_directory.push_back(false);
        
        entries.push_back("file2.cpp");
        is_directory.push_back(false);
        
        entries.push_back("src/");
        is_directory.push_back(true);
        
        entries.push_back("include/");
        is_directory.push_back(true);
        
        entries.push_back("docs/");
        is_directory.push_back(true);
        
        entries.push_back("README.md");
        is_directory.push_back(false);
        
        entries.push_back("Makefile");
        is_directory.push_back(false);
        
        selected_index = 0;
        top_index = 0;
    }

    bool browse() {
        ui_init();
        ui_clear();
        
        bool browsing = true;
        
        while (browsing) {
            draw_screen();
            ui_refresh();
            
            int ch = ui_getch();
            
            switch (ch) {
                case 'q':
                case 'Q':
                    browsing = false;
                    break;
                    
                case 259:  // KEY_UP in ncurses
                case 'k':
                    if (selected_index > 0) {
                        selected_index--;
                        if (selected_index < top_index) {
                            top_index = selected_index;
                        }
                    }
                    break;
                    
                case 258:  // KEY_DOWN in ncurses
                case 'j':
                    if (selected_index < static_cast<int>(entries.size()) - 1) {
                        selected_index++;
                        if (selected_index >= top_index + max_display_items) {
                            top_index = selected_index - max_display_items + 1;
                        }
                    }
                    break;
                    
                case 10:   // Enter key
                case 13:   // Carriage return
                case 330:  // KEY_ENTER in ncurses
                    if (selected_index >= 0 && selected_index < static_cast<int>(entries.size())) {
                        if (is_directory[selected_index]) {
                            // In a real implementation, we'd change directory
                            // For now, just show a message
                            ui_move(ui_rows() - 1, 0);
                            ui_clrtoeol();
                            ui_color_on(UI_COLOR_YELLOW);
                            ui_print("Directory selected: ");
                            ui_print(entries[selected_index].c_str());
                            ui_color_off(UI_COLOR_YELLOW);
                            ui_refresh();
                            ui_getch(); // Wait for key press
                        } else {
                            // Return the selected file
                            ui_end();
                            return true;
                        }
                    }
                    break;
                    
                case 27:  // ESC
                    browsing = false;
                    break;
            }
        }
        
        ui_end();
        return false;
    }

private:
    void draw_screen() {
        ui_clear();
        
        // Draw title
        ui_color_on(UI_COLOR_BLUE);
        ui_attr_on(A_BOLD);
        ui_print_at(0, 0, "File Browser - ");
        ui_print(current_dir.c_str());
        ui_attr_off(A_BOLD);
        ui_color_off(UI_COLOR_BLUE);
        
        // Draw separator
        for (int i = 0; i < ui_cols(); i++) {
            ui_print_at(1, i, "-");
        }
        
        // Draw file entries
        int display_count = std::min(max_display_items, 
                                    static_cast<int>(entries.size() - top_index));
        
        for (int i = 0; i < display_count; i++) {
            int actual_index = top_index + i;
            int row = i + 2;
            
            if (actual_index == selected_index) {
                ui_color_on(UI_COLOR_CYAN);
                ui_attr_on(A_BOLD);
                ui_print_at(row, 0, "> ");
            } else {
                ui_print_at(row, 0, "  ");
            }
            
            if (is_directory[actual_index]) {
                ui_color_on(UI_COLOR_BLUE);
                ui_print("[DIR] ");
                ui_print(entries[actual_index].c_str());
                ui_color_off(UI_COLOR_BLUE);
            } else {
                ui_print("      ");
                ui_print(entries[actual_index].c_str());
            }
            
            if (actual_index == selected_index) {
                ui_attr_off(A_BOLD);
                ui_color_off(UI_COLOR_CYAN);
            }
        }
        
        // Fill remaining space with tildes
        for (int i = display_count; i < max_display_items; i++) {
            ui_print_at(i + 2, 0, "~");
        }
        
        // Draw status line
        ui_color_on(UI_COLOR_YELLOW);
        ui_move(ui_rows() - 1, 0);
        ui_clrtoeol();
        std::string status = "Use j/k or arrows to navigate, Enter to select, q to quit";
        ui_print_at(ui_rows() - 1, 0, status.c_str());
        ui_color_off(UI_COLOR_YELLOW);
    }
};

#endif // _VfsShell_file_browser_h_