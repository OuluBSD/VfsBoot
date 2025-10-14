// Editor functions header
#ifndef _VfsShell_editor_functions_h_
#define _VfsShell_editor_functions_h_

#include "codex.h"

// Forward declarations for editor functions
bool run_ncurses_editor(Vfs& vfs, const std::string& vfs_path, std::vector<std::string>& lines, 
                       bool file_exists, size_t overlay_id);
bool run_simple_editor(Vfs& vfs, const std::string& vfs_path, std::vector<std::string>& lines, 
                      bool file_exists, size_t overlay_id);

#endif // _VfsShell_editor_functions_h_