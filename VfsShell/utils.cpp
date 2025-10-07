#include "utils.h"
#include <cctype>
#include <sstream>
#include <array>
#include <cstdio>
#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>
#include <iostream>
#include <cstdlib>

#ifdef CODEX_TRACE
#include "codex.h"
#endif

// String utilities
std::string trim_copy(const std::string& s){
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b-1]))) --b;
    return s.substr(a, b - a);
}

std::string join_args(const std::vector<std::string>& args, size_t start){
    std::string out;
    for(size_t i = start; i < args.size(); ++i){
        if(i > start) out.push_back(' ');
        out += args[i];
    }
    return out;
}

std::string json_escape(const std::string& s){
    std::string o; o.reserve(s.size()+8);
    for(char c: s){
        switch(c){
            case '"': o+="\\\""; break;
            case '\\': o+="\\\\"; break;
            case '\n': o+="\\n"; break;
            case '\r': o+="\\r"; break;
            case '\t': o+="\\t"; break;
            default: o+=c;
        }
    }
    return o;
}

// Path utilities
std::string join_path(const std::string& base, const std::string& leaf){
    if (base.empty() || base == "/") return std::string("/") + leaf;
    if (!leaf.empty() && leaf[0]=='/') return leaf;
    if (base.back()=='/') return base + leaf;
    return base + "/" + leaf;
}

// Note: normalize_path needs Vfs::splitPath, so it's implemented after including codex.h
#ifdef CODEX_TRACE
#include "codex.h"
std::string normalize_path(const std::string& cwd, const std::string& operand){
    auto stack = (operand.empty() || operand[0]!='/') ? Vfs::splitPath(cwd.empty() ? "/" : cwd)
                                                    : std::vector<std::string>{};
    auto apply = [&stack](const std::string& part){
        if(part.empty() || part==".") return;
        if(part==".."){ if(!stack.empty()) stack.pop_back(); return; }
        stack.push_back(part);
    };
    if(!operand.empty()){
        for(auto& part : Vfs::splitPath(operand)) apply(part);
    }
    if(stack.empty()) return "/";
    std::string out;
    for(const auto& part : stack){
        out.push_back('/');
        out += part;
    }
    return out.empty() ? std::string("/") : out;
}
#else
// Stub when CODEX_TRACE is not defined - will be linked from codex.cpp
std::string normalize_path(const std::string& cwd, const std::string& operand);
#endif

std::string path_basename(const std::string& path){
    if(path.empty() || path=="/") return "/";
    auto pos = path.find_last_of('/');
    if(pos==std::string::npos) return path;
    return path.substr(pos+1);
}

// Exec utilities
std::string exec_capture(const std::string& cmd, const std::string& desc){
#ifdef CODEX_TRACE
    TRACE_FN("cmd=", cmd, ", desc=", desc);
#endif
    std::array<char, 4096> buf{};
    std::string out;
    FILE* pipe = popen(cmd.c_str(), "r");
    if(!pipe) return out;

    static std::mutex output_mutex;
    std::atomic<bool> done{false};
    auto start_time = std::chrono::steady_clock::now();
    std::string label = desc.empty() ? std::string("external command") : desc;

    std::thread keep_alive([&](){
        using namespace std::chrono_literals;
        bool warned=false;
        auto next_report = std::chrono::steady_clock::now() + 10s;
        while(!done.load(std::memory_order_relaxed)){
            std::this_thread::sleep_for(200ms);
            if(done.load(std::memory_order_relaxed)) break;
            auto now = std::chrono::steady_clock::now();
            if(now < next_report) continue;
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
            {
                std::lock_guard<std::mutex> lock(output_mutex);
                std::cout << "[keepalive] " << label << " running for " << elapsed << "s...\n";
                if(!warned && elapsed >= 300){
                    std::cout << "[keepalive] " << label << " exceeded 300s; check connectivity or abort if needed.\n";
                    warned=true;
                }
                std::cout.flush();
            }
            next_report = now + 10s;
        }
    });

    while(true){
        size_t n = fread(buf.data(), 1, buf.size(), pipe);
#ifdef CODEX_TRACE
        TRACE_LOOP("exec_capture.read", std::string("bytes=") + std::to_string(n));
#endif
        if(n>0) out.append(buf.data(), n);
        if(n<buf.size()) break;
    }
    done.store(true, std::memory_order_relaxed);
    if(keep_alive.joinable()) keep_alive.join();
    pclose(pipe);
    return out;
}

bool has_cmd(const std::string& c){
    std::string cmd = "command -v "+c+" >/dev/null 2>&1";
    int r = system(cmd.c_str());
    return r==0;
}
