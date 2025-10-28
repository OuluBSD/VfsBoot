#ifndef _VfsShell_config_h_
#define _VfsShell_config_h_


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



#endif
