// Simple test for ncurses functionality
#include "VfsShell/ui_backend.h"
#include <iostream>

int main() {
    ui_init();
    
    ui_clear();
    ui_move(5, 10);
    ui_print("Hello, ncurses world!");
    ui_refresh();
    
    ui_getch();
    
    ui_end();
    
    std::cout << "Ncurses test completed successfully!" << std::endl;
    return 0;
}