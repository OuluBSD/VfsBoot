#include "snippet_catalog.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace snippets {
namespace {
std::mutex g_mutex;
std::unordered_map<std::string, std::string> g_cache;
std::filesystem::path g_directory;
bool g_directory_forced = false;
bool g_initialized = false;

std::filesystem::path executable_path(const char* argv0){
#ifdef __linux__
    std::error_code ec;
    auto resolved = std::filesystem::read_symlink("/proc/self/exe", ec);
    if(!ec) return resolved;
#endif
    if(argv0 && *argv0){
        std::error_code ec;
        auto abs = std::filesystem::absolute(argv0, ec);
        if(!ec) return abs;
    }
    return {};
}

std::optional<std::filesystem::path> validate_dir(const std::filesystem::path& candidate){
    if(candidate.empty()) return std::nullopt;
    std::error_code ec;
    auto canonical = std::filesystem::weakly_canonical(candidate, ec);
    if(ec) return std::nullopt;
    if(!std::filesystem::exists(canonical)) return std::nullopt;
    if(!std::filesystem::is_directory(canonical)) return std::nullopt;
    return canonical;
}

std::filesystem::path pick_default_directory(const char* argv0){
    if(const char* env = std::getenv("CODEX_SNIPPET_DIR")){
        if(*env){
            if(auto path = validate_dir(env)) return *path;
        }
    }

    auto exe = executable_path(argv0);
    std::vector<std::filesystem::path> candidates;
    if(!exe.empty()){
        auto exe_dir = exe.parent_path();
        candidates.push_back(exe_dir / "snippets");
        candidates.push_back(exe_dir / "Stage1" / "snippets");
    }

    candidates.push_back(std::filesystem::current_path() / "snippets");
    candidates.push_back(std::filesystem::current_path() / "Stage1" / "snippets");

    for(const auto& cand : candidates){
        if(auto path = validate_dir(cand)) return *path;
    }

    return {};
}

std::filesystem::path snippet_path(const std::filesystem::path& dir, const std::string& key){
    if(dir.empty()) return {};
    return dir / (key + ".txt");
}

std::string read_file_contents(const std::filesystem::path& path){
    std::ifstream in(path, std::ios::binary);
    if(!in) return {};
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

void ensure_initialized(const char* argv0){
    if(g_initialized) return;
    g_directory = pick_default_directory(argv0);
    g_initialized = true;
}

} // namespace

void initialize(const char* argv0){
    std::lock_guard<std::mutex> lock(g_mutex);
    if(g_directory_forced) return;
    ensure_initialized(argv0);
}

void set_directory(std::string path){
    std::lock_guard<std::mutex> lock(g_mutex);
    g_directory = path.empty() ? std::filesystem::path{} : std::filesystem::path(path);
    g_directory_forced = true;
    g_cache.clear();
}

std::string get_or(const std::string& key, const std::string& fallback){
    std::lock_guard<std::mutex> lock(g_mutex);
    ensure_initialized(nullptr);

    if(auto it = g_cache.find(key); it != g_cache.end()) return it->second;

    std::string value;
    auto path = snippet_path(g_directory, key);
    if(!path.empty()){
        value = read_file_contents(path);
    }
    if(value.empty()){
        value = fallback;
    }
    g_cache.insert({key, value});
    return value;
}

std::string tool_list(){
    static const std::string fallback =
        "Tools:\n"
        "- cd <dir>, pwd, ls [path], tree [path], mkdir <path>, touch <path>, rm <path>, mv <src> <dst>, link <src> <dst>, export <vfs> <host>\n"
        "- Files & text: cat [paths...] | stdin, grep/rg [-i] <pattern> [path], head|tail [-n N] [path], uniq [path], count [path], random [min [max]], true, false\n"
        "- Manage source: echo <path> <data>, parse /src/file.sexp /ast/name, eval /ast/name\n"
        "- AI bridge: ai <prompt...>, ai.brief <key> [extra...]\n"
        "- Builtins: + - * = < print, if, lambda(1), strings, bool #t/#f\n"
        "- Lists: list cons head tail null? ; Strings: str.cat str.sub str.find\n"
        "- Pipelines & chaining: command | command, command && command, command || command\n"
        "- VFS ops: vfs-write vfs-read vfs-ls\n"
        "- C++ AST ops via shell: cpp.tu /astcpp/X ; cpp.include TU header [0/1] ; cpp.func TU name rettype ; cpp.param FN type name ; cpp.print FN text ; cpp.vardecl scope type name [init] ; cpp.expr scope expr ; cpp.stmt scope raw ; cpp.return scope [expr] ; cpp.returni scope int ; cpp.rangefor scope name decl | range ; cpp.dump TU /cpp/file.cpp\n";
    return get_or("tools-list", fallback);
}

std::string ai_bridge_hello_briefing(){
    static const std::string fallback =
        "Briefing: use cpp.tu, cpp.include, cpp.func, cpp.print, cpp.returni, cpp.dump to build hello world.\n";
    return get_or("ai-bridge-hello", fallback);
}

} // namespace snippets
