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

struct LineSplit{
	std::vector<std::string> lines;
	bool trailing_newline;
};

LineSplit split_lines(const std::string& s);
size_t count_lines(const std::string& s);
size_t parse_size_arg(const std::string& s, const char* ctx);
long long parse_int_arg(const std::string& s, const char* ctx);
std::string join_line_range(const LineSplit& split, size_t begin, size_t end);

std::mt19937& rng();

#endif
