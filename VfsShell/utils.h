#ifndef _VfsShell_utils_h_
#define _VfsShell_utils_h_


// String utilities
std::string trim_copy(const std::string& s);
std::string json_escape(const std::string& s);

// Path utilities
std::string join_path(const std::string& base, const std::string& leaf);
std::string normalize_path(const std::string& cwd, const std::string& operand);
std::string path_basename(const std::string& path);
std::string path_dirname(const std::string& path);

// Exec utilities
std::string exec_capture(const std::string& cmd, const std::string& desc = {});
bool has_cmd(const std::string& c);

std::string join_args(const std::vector<std::string>& args, size_t start = 0);

#endif
