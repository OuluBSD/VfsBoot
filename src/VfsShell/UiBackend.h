#ifndef _VfsShell_ui_backend_h_
#define _VfsShell_ui_backend_h_

// UI backend abstraction layer
// Supports multiple UI libraries through preprocessor flags

// Abstract UI operations
#ifdef CODEX_UI_NCURSES

// NCURSES implementation
#define UI_WINDOW WINDOW*
#define UI_NULL NULL

inline void ui_init() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    
    // Initialize colors if supported
    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_BLUE, COLOR_BLACK);   // Title bar
        init_pair(2, COLOR_YELLOW, COLOR_BLACK); // Status bar
        init_pair(3, COLOR_CYAN, COLOR_BLACK);   // Line numbers
        init_pair(4, COLOR_RED, COLOR_BLACK);    // Error/warning
    }
}

inline void ui_end() {
    endwin();
}

inline void ui_refresh() {
    refresh();
}

inline int ui_getch() {
    return getch();
}

inline void ui_clear() {
    clear();
}

inline void ui_move(int y, int x) {
    move(y, x);
}

inline void ui_print(const char* text) {
    addstr(text);
}

inline void ui_print_at(int y, int x, const char* text) {
    mvaddstr(y, x, text);
}

inline int ui_rows() {
    int rows __attribute__((unused)), cols __attribute__((unused));
    getmaxyx(stdscr, rows, cols);
    return rows;
}

inline int ui_cols() {
    int rows __attribute__((unused)), cols __attribute__((unused));
    getmaxyx(stdscr, rows, cols);
    return cols;
}

// Color and attribute support
enum UiColor {
    UI_COLOR_DEFAULT = 0,
    UI_COLOR_BLUE = 1,
    UI_COLOR_YELLOW = 2,
    UI_COLOR_CYAN = 3,
    UI_COLOR_RED = 4
};

inline void ui_color_on(UiColor color) {
    if (has_colors()) {
        attron(COLOR_PAIR(color));
    }
}

inline void ui_color_off(UiColor color) {
    if (has_colors()) {
        attroff(COLOR_PAIR(color));
    }
}

// Attribute support
inline void ui_attr_on(int attr) {
    attron(attr);
}

inline void ui_attr_off(int attr) {
    attroff(attr);
}

// Clear to end of line
inline void ui_clrtoeol() {
    clrtoeol();
}

// Get character without echoing
inline int ui_getch_noecho() {
    nodelay(stdscr, FALSE);
    int ch = getch();
    return ch;
}

// Non-blocking get character
inline int ui_getch_nb() {
    nodelay(stdscr, TRUE);
    int ch = getch();
    nodelay(stdscr, FALSE);  // Return to blocking mode
    return ch;
}

#elif defined(CODEX_UI_BUILTIN)

// Builtin terminal implementation
#include <iostream>
#include <termios.h>
#include <unistd.h>

#define UI_WINDOW int
#define UI_NULL 0

inline void ui_init() {
    // Set terminal to raw mode
    struct termios raw;
    tcgetattr(STDIN_FILENO, &raw);
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

inline void ui_end() {
    // Restore terminal
    struct termios raw;
    tcgetattr(STDIN_FILENO, &raw);
    raw.c_lflag |= (ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

inline void ui_refresh() {
    std::cout.flush();
}

inline int ui_getch() {
    return getchar();
}

inline int ui_getch_nb() {
    // For builtin, this is just a regular getch since we can't make it non-blocking without more complex system calls
    return getchar();
}

inline void ui_clear() {
    std::cout << "\033[2J\033[H";  // Clear screen and move cursor to home
}

inline void ui_move(int y, int x) {
    std::cout << "\033[" << (y+1) << ";" << (x+1) << "H";
}

inline void ui_print(const char* text) {
    std::cout << text;
}

inline void ui_print_at(int y, int x, const char* text) {
    std::cout << "\033[" << (y+1) << ";" << (x+1) << "H" << text;
}

inline int ui_rows() {
    // Simple approximation - in a real implementation we'd query the terminal
    return 24;
}

inline int ui_cols() {
    // Simple approximation - in a real implementation we'd query the terminal
    return 80;
}

// Color and attribute support (using ANSI escape codes)
enum UiColor {
    UI_COLOR_DEFAULT = 0,
    UI_COLOR_BLUE = 1,
    UI_COLOR_YELLOW = 2,
    UI_COLOR_CYAN = 3,
    UI_COLOR_RED = 4
};

inline void ui_color_on(UiColor color) {
    switch(color) {
        case UI_COLOR_BLUE:
            std::cout << "\033[34m";  // Blue text
            break;
        case UI_COLOR_YELLOW:
            std::cout << "\033[33m";  // Yellow text
            break;
        case UI_COLOR_CYAN:
            std::cout << "\033[36m";  // Cyan text
            break;
        case UI_COLOR_RED:
            std::cout << "\033[31m";  // Red text
            break;
        default:
            break;
    }
}

inline void ui_color_off(UiColor color) {
    std::cout << "\033[0m";  // Reset to default
}

inline void ui_attr_on(int attr) {
    // For builtin terminal, basic attributes
    if (attr == 1) {  // Bold
        std::cout << "\033[1m";
    }
}

inline void ui_attr_off(int attr) {
    std::cout << "\033[0m";  // Reset attributes
}

inline void ui_clrtoeol() {
    std::cout << "\033[K";  // Clear from cursor to end of line
}

#else

// Fallback implementation
#define UI_WINDOW void*
#define UI_NULL nullptr

inline void ui_init() {}
inline void ui_end() {}
inline void ui_refresh() {}
inline int ui_getch() { return 0; }
inline int ui_getch_nb() { return 0; }
inline void ui_clear() {}
inline void ui_move(int y, int x) {}
inline void ui_print(const char* text) {}
inline void ui_print_at(int y, int x, const char* text) {}
inline int ui_rows() { return 24; }
inline int ui_cols() { return 80; }

// Color and attribute support
enum UiColor {
    UI_COLOR_DEFAULT = 0,
    UI_COLOR_BLUE = 1,
    UI_COLOR_YELLOW = 2,
    UI_COLOR_CYAN = 3,
    UI_COLOR_RED = 4
};

inline void ui_color_on(UiColor color) {}
inline void ui_color_off(UiColor color) {}
inline void ui_attr_on(int attr) {}
inline void ui_attr_off(int attr) {}
inline void ui_clrtoeol() {}

#endif

#endif // _VfsShell_ui_backend_h_