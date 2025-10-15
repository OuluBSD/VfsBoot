#ifndef _VfsShell_codex_h_
#define _VfsShell_codex_h_

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#ifdef CODEX_TRACE
#include <mutex>
#include <sstream>

namespace codex_trace {
    void log_line(const std::string& line);

    inline std::string concat(){ return {}; }

    template<typename... Args>
    std::string concat(Args&&... args){
        std::ostringstream oss;
        (oss << ... << std::forward<Args>(args));
        return oss.str();
    }

    struct Scope {
        std::string name;
        Scope(const char* fn, const std::string& details);
        ~Scope();
    };

    void log_loop(const char* tag, const std::string& details);
}

#define CODEX_TRACE_CAT(a,b) CODEX_TRACE_CAT_1(a,b)
#define CODEX_TRACE_CAT_1(a,b) a##b

#endif

struct Env; // forward

//
// Builtins
//
void install_builtins(std::shared_ptr<Env> g);

// helpers
std::shared_ptr<CppTranslationUnit> expect_tu(std::shared_ptr<VfsNode> n);
std::shared_ptr<CppFunction>        expect_fn(std::shared_ptr<VfsNode> n);
std::shared_ptr<CppCompound>        expect_block(std::shared_ptr<VfsNode> n);
void vfs_add(Vfs& vfs, const std::string& path, std::shared_ptr<VfsNode> node, size_t overlayId = 0);
void cpp_dump_to_vfs(Vfs& vfs, size_t overlayId, const std::string& tuPath, const std::string& filePath);



struct LineSplit {
    std::vector<std::string> lines;
    bool trailing_newline = false;
};

//
// AI helpers (OpenAI + llama.cpp bridge)
//
std::string json_escape(const std::string& s);
std::string build_responses_payload(const std::string& model, const std::string& user_prompt);
std::string call_openai(const std::string& prompt);
std::string call_llama(const std::string& prompt);
std::string call_ai(const std::string& prompt);

void load_history(std::vector<std::string>& history);
LineSplit split_lines(const std::string& s);
size_t count_lines(const std::string& s);
size_t parse_size_arg(const std::string& s, const char* ctx);
std::mt19937& rng();

const char* policy_label(WorkingDirectory::ConflictPolicy policy);
std::optional<WorkingDirectory::ConflictPolicy> parse_policy(const std::string& name);
void adjust_context_after_unmount(Vfs& vfs, WorkingDirectory& cwd, size_t removedId);
bool read_line_with_history(Vfs& vfs, const std::string& prompt, std::string& out, const std::vector<std::string>& history, const std::string& cwd_path = "/");
std::string join_line_range(const LineSplit& split, size_t begin, size_t end);
long long parse_int_arg(const std::string& s, const char* ctx);
void help();
std::vector<std::string> tokenize_command_line(const std::string& line);
std::vector<CommandChainEntry> parse_command_chain(const std::vector<std::string>& tokens);
void save_history(const std::vector<std::string>& history);

#endif
