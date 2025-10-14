#ifndef _VfsShell_ui_backend_h_
#define _VfsShell_ui_backend_h_

// UI backend abstraction layer
// Supports multiple UI libraries through preprocessor flags

#ifdef CODEX_UI_NCURSES
#include <ncurses.h>
#elif defined(CODEX_UI_BUILTIN)
// Builtin implementation - no additional headers needed
// Example of how to add Qt support (conceptual):
// #elif defined(CODEX_UI_QT)
// #include "ui_backend_qt_example.h"  // Conceptual Qt implementation
#else
// Default to ncurses if available
#include <ncurses.h>
#define CODEX_UI_NCURSES
#endif

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

inline void ui_clear() {
    std::cout << "\033[2J\033[H";  // Clear screen and move cursor to home
}

inline void ui_move(int y, int x) {
    std::cout << "\033[" << (y+1) << ";" << (x+1) << "H";
}

inline void ui_print(const char* text) {
    std::cout << text;
}

inline int ui_rows() {
    // Simple approximation - in a real implementation we'd query the terminal
    return 24;
}

inline int ui_cols() {
    // Simple approximation - in a real implementation we'd query the terminal
    return 80;
}

#else

// Fallback implementation
#define UI_WINDOW void*
#define UI_NULL nullptr

inline void ui_init() {}
inline void ui_end() {}
inline void ui_refresh() {}
inline int ui_getch() { return 0; }
inline void ui_clear() {}
inline void ui_move(int y, int x) {}
inline void ui_print(const char* text) {}
inline int ui_rows() { return 24; }
inline int ui_cols() { return 80; }

#endif

#endif // _VfsShell_ui_backend_h_