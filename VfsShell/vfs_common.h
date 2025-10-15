#pragma once


// Tracing (optional debug feature)
#ifdef CODEX_TRACE
namespace codex_trace {
    extern std::ofstream* trace_file;
    struct Scope {
        std::string fn;
        Scope(const std::string& f);
        ~Scope();
    };
    void log_line(int line, const std::string& msg);
    void log_loop(int line, const std::string& msg);
}
#define TRACE_FN(...) codex_trace::Scope __trace_scope("")
#define TRACE_MSG(msg) codex_trace::log_line(__LINE__, msg)
#define TRACE_LOOP(...) codex_trace::log_loop(__LINE__, "")
#else
#define TRACE_FN(...)
#define TRACE_MSG(...)
#define TRACE_LOOP(...)
#endif

// i18n (internationalization)
namespace i18n {
    enum class MsgId {
        WELCOME, UNKNOWN_COMMAND, DISCUSS_HINT,
        FILE_NOT_FOUND,
        DIR_NOT_FOUND, NOT_A_FILE, NOT_A_DIR,
        PARSE_ERROR, EVAL_ERROR, HELP_TEXT
    };
    
    const char* get(MsgId id);
    void init();
    void set_english_only();
}

// Hash utilities
std::string compute_file_hash(const std::string& filepath);
std::string compute_string_hash(const std::string& data);

// Forward declarations
struct Vfs;
struct VfsNode;
bool run_ncurses_editor(Vfs& vfs, const std::string& vfs_path,
                        std::vector<std::string>& lines,
                        bool file_exists, size_t overlay_id);
