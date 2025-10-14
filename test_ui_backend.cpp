// Test program for UI backend abstraction
#include <iostream>
#include "VfsShell/ui_backend.h"

int main() {
    std::cout << "Testing UI backend abstraction..." << std::endl;
    
    // Initialize UI
    ui_init();
    
    // Clear screen
    ui_clear();
    
    // Move cursor and print welcome message
    ui_move(2, 10);
    ui_print("UI Backend Abstraction Test");
    
    ui_move(4, 5);
    ui_print("This demonstrates a simple abstraction layer");
    ui_move(5, 5);
    ui_print("that can support multiple UI libraries.");
    
    ui_move(7, 5);
#ifdef CODEX_UI_NCURSES
    ui_print("Currently using: NCURSES backend");
#elif defined(CODEX_UI_BUILTIN)
    ui_print("Currently using: Builtin terminal backend");
#else
    ui_print("Currently using: Fallback backend");
#endif
    
    ui_move(9, 5);
    ui_print("Screen dimensions: ");
    ui_move(9, 25);
    ui_print(std::to_string(ui_cols()).c_str());
    ui_print(" x ");
    ui_print(std::to_string(ui_rows()).c_str());
    
    ui_move(11, 5);
    ui_print("Press any key to continue...");
    
    // Refresh screen
    ui_refresh();
    
    // Wait for keypress
    ui_getch();
    
    // Cleanup
    ui_end();
    
    std::cout << "Test completed successfully!" << std::endl;
    return 0;
}