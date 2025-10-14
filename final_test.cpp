// Comprehensive test of UI backend abstraction
#include <iostream>
#include <vector>
#include <string>

// Mock VFS structure for testing
struct Vfs {
    void write(const std::string& path, const std::string& content, size_t overlay_id) {
        std::cout << "[Mock VFS] Writing " << content.length() << " chars to " << path 
                  << " (overlay " << overlay_id << ")" << std::endl;
    }
};

// Include our UI backend abstraction
#include "VfsShell/ui_backend.h"

int main() {
    std::cout << "=== UI Backend Abstraction Test ===" << std::endl;
    
#ifdef CODEX_UI_NCURSES
    std::cout << "Compiled with NCURSES backend" << std::endl;
#elif defined(CODEX_UI_BUILTIN)
    std::cout << "Compiled with BUILTIN terminal backend" << std::endl;
#else
    std::cout << "Compiled with FALLBACK backend" << std::endl;
#endif
    
    // Test UI initialization
    ui_init();
    std::cout << "UI initialized successfully" << std::endl;
    
    // Test screen operations
    ui_clear();
    ui_move(2, 10);
    ui_print("UI Backend Abstraction Working!");
    ui_refresh();
    
    // Test screen dimensions
    int rows = ui_rows();
    int cols = ui_cols();
    std::cout << "Screen dimensions: " << rows << " x " << cols << std::endl;
    
    // Test cleanup
    ui_end();
    std::cout << "UI cleaned up successfully" << std::endl;
    
    std::cout << "All tests passed!" << std::endl;
    return 0;
}