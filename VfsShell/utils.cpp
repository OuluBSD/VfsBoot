#include "VfsShell.h"

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

// Note: normalize_path needs Vfs::splitPath, so we include codex.h here

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

std::string path_basename(const std::string& path){
    if(path.empty() || path=="/") return "/";
    auto pos = path.find_last_of('/');
    if(pos==std::string::npos) return path;
    return path.substr(pos+1);
}

std::string path_dirname(const std::string& path){
    if(path.empty() || path=="/") return "/";
    auto pos = path.find_last_of('/');
    if(pos==std::string::npos) return ".";
    if(pos==0) return "/";
    return path.substr(0, pos);
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

size_t count_lines(const std::string& s){
    if(s.empty()) return 0;
    size_t n = std::count(s.begin(), s.end(), '\n');
    if(s.back() != '\n') ++n;
    return n;
}

LineSplit split_lines(const std::string& s){
    LineSplit result;
    std::string current;
    bool last_was_newline = false;
    for(char c : s){
        if(c == '\n'){
            result.lines.push_back(current);
            current.clear();
            last_was_newline = true;
        } else {
            current.push_back(c);
            last_was_newline = false;
        }
    }
    if(!current.empty()){
        result.lines.push_back(current);
    }
    result.trailing_newline = last_was_newline;
    return result;
}

size_t parse_size_arg(const std::string& s, const char* ctx){
    if(s.empty()) throw std::runtime_error(std::string(ctx) + " must be non-negative integer");
    size_t idx = 0;
    while(idx < s.size()){
        if(!std::isdigit(static_cast<unsigned char>(s[idx])))
            throw std::runtime_error(std::string(ctx) + " must be non-negative integer");
        ++idx;
    }
    try{
        return static_cast<size_t>(std::stoull(s));
    } catch(const std::exception&){
        throw std::runtime_error(std::string(ctx) + " out of range");
    }
}

std::string join_line_range(const LineSplit& split, size_t begin, size_t end){
    if(begin >= end || begin >= split.lines.size()) return {};
    end = std::min(end, split.lines.size());
    std::ostringstream oss;
    for(size_t idx = begin; idx < end; ++idx){
        oss << split.lines[idx];
        bool had_newline = (idx < split.lines.size() - 1) || split.trailing_newline;
        if(had_newline) oss << '\n';
    }
    return oss.str();
}

long long parse_int_arg(const std::string& s, const char* ctx){
    if(s.empty()) throw std::runtime_error(std::string(ctx) + " must be integer");
    size_t idx = 0;
    if(s[0] == '+' || s[0] == '-'){
        idx = 1;
        if(idx == s.size()) throw std::runtime_error(std::string(ctx) + " must be integer");
    }
    while(idx < s.size()){
        if(!std::isdigit(static_cast<unsigned char>(s[idx])))
            throw std::runtime_error(std::string(ctx) + " must be integer");
        ++idx;
    }
    try{
        return std::stoll(s);
    } catch(const std::exception&){
        throw std::runtime_error(std::string(ctx) + " out of range");
    }
}


std::mt19937& rng(){
    static std::mt19937 gen(std::random_device{}());
    return gen;
}
