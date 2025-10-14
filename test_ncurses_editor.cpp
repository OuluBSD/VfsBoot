// Test program for ncurses editor functionality
#include <iostream>
#include <vector>
#include <string>
#include <ncurses.h>

// Mock VFS structure for testing
struct Vfs {
    void write(const std::string& path, const std::string& content, size_t overlay_id) {
        // Mock implementation - just print to stdout for testing
        std::cout << "Writing to " << path << ":\n" << content << std::endl;
    }
};

// Simplified version of our ncurses editor function
bool run_ncurses_editor(Vfs& vfs, const std::string& vfs_path, std::vector<std::string>& lines, 
                       bool file_exists, size_t overlay_id) {
    // Initialize ncurses
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    
    // Simple test - just show a message and wait for keypress
    clear();
    mvprintw(0, 0, "Ncurses Editor Test");
    mvprintw(1, 0, "==================");
    mvprintw(3, 0, "VFS Path: %s", vfs_path.c_str());
    mvprintw(4, 0, "Lines: %d", static_cast<int>(lines.size()));
    mvprintw(5, 0, "File exists: %s", file_exists ? "yes" : "no");
    mvprintw(7, 0, "Press any key to exit...");
    refresh();
    
    // Wait for keypress
    getch();
    
    // Clean up ncurses
    endwin();
    
    // Mock save operation
    std::string content;
    for (size_t i = 0; i < lines.size(); ++i) {
        content += lines[i];
        if (i < lines.size() - 1) content += "\n";
    }
    vfs.write(vfs_path, content, overlay_id);
    
    return true;
}

int main() {
    std::cout << "Testing ncurses editor functionality..." << std::endl;
    
    // Create test data
    Vfs vfs;
    std::string vfs_path = "/test/file.txt";
    std::vector<std::string> lines = {
        "First line of test file",
        "Second line of test file",
        "Third line of test file"
    };
    bool file_exists = false;
    size_t overlay_id = 0;
    
    // Run the editor test
    bool result = run_ncurses_editor(vfs, vfs_path, lines, file_exists, overlay_id);
    
    if (result) {
        std::cout << "Ncurses editor test completed successfully!" << std::endl;
    } else {
        std::cout << "Ncurses editor test failed!" << std::endl;
        return 1;
    }
    
    return 0;
}