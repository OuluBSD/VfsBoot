#include "codex.h"
#include "snippet_catalog.h"
#include <fstream>
#include <sstream>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <mutex>
#include <optional>
#include <random>
#include <regex>
#include <system_error>
#include <thread>
#include <sys/types.h>
#include <unistd.h>

#ifdef CODEX_TRACE
#include <fstream>

namespace codex_trace {
    namespace {
        std::mutex& trace_mutex(){ static std::mutex m; return m; }
        std::ofstream& trace_stream(){
            static std::ofstream s("codex_trace.log", std::ios::app);
            return s;
        }
        void write_line(const std::string& line){
            auto& os = trace_stream();
            os << line << '\n';
            os.flush();
        }
    }

    void log_line(const std::string& line){
        std::lock_guard<std::mutex> lock(trace_mutex());
        write_line(line);
    }

    Scope::Scope(const char* fn, const std::string& details) : name(fn ? fn : "?"){
        if(!name.empty()){
            std::string msg = std::string("enter ") + name;
            if(!details.empty()) msg += " | " + details;
            log_line(msg);
        }
    }

    Scope::~Scope(){
        if(!name.empty()){
            log_line(std::string("exit ") + name);
        }
    }

    void log_loop(const char* tag, const std::string& details){
        std::lock_guard<std::mutex> lock(trace_mutex());
        std::string msg = std::string("loop ") + (tag ? tag : "?");
        if(!details.empty()) msg += " | " + details;
        write_line(msg);
    }
}
#endif

static std::string trim_copy(const std::string& s){
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b-1]))) --b;
    return s.substr(a, b - a);
}
static std::string join_path(const std::string& base, const std::string& leaf){
    if (base.empty() || base == "/") return std::string("/") + leaf;
    if (!leaf.empty() && leaf[0]=='/') return leaf;
    if (base.back()=='/') return base + leaf;
    return base + "/" + leaf;
}
static std::string normalize_path(const std::string& cwd, const std::string& operand){
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
static std::string path_basename(const std::string& path){
    if(path.empty() || path=="/") return "/";
    auto pos = path.find_last_of('/');
    if(pos==std::string::npos) return path;
    return path.substr(pos+1);
}
static std::string unescape_meta(const std::string& s){
    std::string out; out.reserve(s.size());
    for(size_t i=0;i<s.size();++i){
        char c = s[i];
        if(c=='\\' && i+1<s.size()){
            char n = s[++i];
            switch(n){
                case 'n': out.push_back('\n'); break;
                case 't': out.push_back('\t'); break;
                case 'r': out.push_back('\r'); break;
                case '\\': out.push_back('\\'); break;
                case '"': out.push_back('"'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'v': out.push_back('\v'); break;
                case 'a': out.push_back('\a'); break;
                default: out.push_back(n); break;
            }
        } else {
            out.push_back(c);
        }
    }
    return out;
}

static std::string sanitize_component(const std::string& s){
    std::string out;
    out.reserve(s.size());
    for(char c : s){
        unsigned char uc = static_cast<unsigned char>(c);
        if(std::isalnum(uc) || c=='-' || c=='_'){
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('_');
        }
    }
    if(out.empty()) out.push_back('_');
    return out;
}

static uint64_t fnv1a64(const std::string& data){
    const uint64_t offset = 1469598103934665603ull;
    const uint64_t prime  = 1099511628211ull;
    uint64_t h = offset;
    for(unsigned char c : data){
        h ^= c;
        h *= prime;
    }
    return h;
}

static std::string hash_hex(uint64_t value){
    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << value;
    return oss.str();
}

static std::string join_args(const std::vector<std::string>& args, size_t start = 0){
    std::string out;
    for(size_t i = start; i < args.size(); ++i){
        if(i != start) out.push_back(' ');
        out += args[i];
    }
    return out;
}

struct CommandInvocation {
    std::string name;
    std::vector<std::string> args;
};

struct CommandPipeline {
    std::vector<CommandInvocation> commands;
};

struct CommandChainEntry {
    std::string logical; // "", "&&", "||"
    CommandPipeline pipeline;
};

struct CommandResult {
    bool success = true;
    bool exit_requested = false;
    std::string output;
};

struct ScopedCoutCapture {
    std::ostringstream buffer;
    std::streambuf* old = nullptr;
    ScopedCoutCapture(){
        old = std::cout.rdbuf(buffer.rdbuf());
    }
    ~ScopedCoutCapture(){
        std::cout.rdbuf(old);
    }
    std::string str() const { return buffer.str(); }
};

static std::vector<std::string> tokenize_command_line(const std::string& line){
    std::vector<std::string> tokens;
    std::string cur;
    bool in_single = false;
    bool in_double = false;
    bool escape = false;
    auto flush = [&]{
        if(!cur.empty()){
            tokens.push_back(cur);
            cur.clear();
        }
    };
    for(size_t i = 0; i < line.size(); ++i){
        char c = line[i];
        if(escape){
            cur.push_back(c);
            escape = false;
            continue;
        }
        if(!in_single && c == '\\'){
            escape = true;
            continue;
        }
        if(c == '"' && !in_single){
            in_double = !in_double;
            continue;
        }
        if(c == '\'' && !in_double){
            in_single = !in_single;
            continue;
        }
        if(!in_single && !in_double){
            if(std::isspace(static_cast<unsigned char>(c))){
                flush();
                continue;
            }
            if(c == '|'){
                flush();
                if(i + 1 < line.size() && line[i+1] == '|'){
                    tokens.emplace_back("||");
                    ++i;
                } else {
                    tokens.emplace_back("|");
                }
                continue;
            }
            if(c == '&' && i + 1 < line.size() && line[i+1] == '&'){
                flush();
                tokens.emplace_back("&&");
                ++i;
                continue;
            }
        }
        cur.push_back(c);
    }
    if(escape) throw std::runtime_error("line ended with unfinished escape");
    if(in_single || in_double) throw std::runtime_error("unterminated quote");
    flush();
    return tokens;
}

static std::vector<CommandChainEntry> parse_command_chain(const std::vector<std::string>& tokens){
    std::vector<CommandChainEntry> chain;
    CommandPipeline current_pipe;
    CommandInvocation current_cmd;
    std::string next_logic;

    auto flush_command = [&](){
        if(current_cmd.name.empty()) throw std::runtime_error("expected command before operator");
        current_pipe.commands.push_back(current_cmd);
        current_cmd = CommandInvocation{};
    };

    auto flush_pipeline = [&](){
        if(current_pipe.commands.empty()) throw std::runtime_error("missing command sequence");
        chain.push_back(CommandChainEntry{next_logic, current_pipe});
        current_pipe = CommandPipeline{};
        next_logic.clear();
    };

    for(const auto& tok : tokens){
        if(tok == "|"){
            flush_command();
            continue;
        }
        if(tok == "&&" || tok == "||"){
            flush_command();
            flush_pipeline();
            next_logic = tok;
            continue;
        }
        if(current_cmd.name.empty()){
            current_cmd.name = tok;
        } else {
            current_cmd.args.push_back(tok);
        }
    }

    if(!current_cmd.name.empty()) flush_command();
    if(!current_pipe.commands.empty()){
        chain.push_back(CommandChainEntry{next_logic, current_pipe});
        next_logic.clear();
    }
    if(!next_logic.empty()) throw std::runtime_error("dangling logical operator");
    return chain;
}

static size_t count_lines(const std::string& s){
    if(s.empty()) return 0;
    size_t n = std::count(s.begin(), s.end(), '\n');
    if(s.back() != '\n') ++n;
    return n;
}

struct LineSplit {
    std::vector<std::string> lines;
    bool trailing_newline = false;
};

static LineSplit split_lines(const std::string& s){
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

static std::string join_line_range(const LineSplit& split, size_t begin, size_t end){
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

static size_t parse_size_arg(const std::string& s, const char* ctx){
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

static long long parse_int_arg(const std::string& s, const char* ctx){
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

static std::mt19937& rng(){
    static std::mt19937 gen(std::random_device{}());
    return gen;
}

static std::filesystem::path ai_cache_root(){
    if(const char* env = std::getenv("CODEX_AI_CACHE_DIR"); env && *env){
        return std::filesystem::path(env);
    }
    return std::filesystem::path("cache") / "ai";
}

static std::filesystem::path ai_cache_file_path(const std::string& providerLabel, const std::string& key_material){
    std::filesystem::path dir = ai_cache_root() / sanitize_component(providerLabel);
    std::string hash = hash_hex(fnv1a64(key_material));
    return dir / (hash + ".txt");
}

static std::string make_cache_key_material(const std::string& providerSignature, const std::string& prompt){
    return providerSignature + '\x1f' + prompt;
}

static std::optional<std::string> ai_cache_read(const std::string& providerLabel, const std::string& key_material){
    auto path = ai_cache_file_path(providerLabel, key_material);
    std::ifstream in(path, std::ios::binary);
    if(!in) return std::nullopt;
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

static void ai_cache_write(const std::string& providerLabel, const std::string& key_material, const std::string& payload){
    auto path = ai_cache_file_path(providerLabel, key_material);
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::binary);
    if(!out) return;
    out.write(payload.data(), static_cast<std::streamsize>(payload.size()));
}

// ====== Value::show ======
std::string Value::show() const {
    if (std::holds_alternative<int64_t>(v)) return std::to_string(std::get<int64_t>(v));
    if (std::holds_alternative<bool>(v))    return std::get<bool>(v) ? "#t" : "#f";
    if (std::holds_alternative<std::string>(v))  return std::string("\"")+std::get<std::string>(v)+"\"";
    if (std::holds_alternative<Builtin>(v)) return "<builtin>";
    if (std::holds_alternative<Closure>(v)) return "<closure>";
    const auto& xs = std::get<List>(v);
    std::string s="("; bool first=true; for(const auto& e: xs){ if(!first) s+=' '; first=false; s+=e.show(); } s+=')'; return s;
}

// ====== AST node ctors ======
AstInt::AstInt(std::string n, int64_t v) : AstNode(std::move(n)), val(v) { kind=Kind::Ast; }
AstBool::AstBool(std::string n, bool v)  : AstNode(std::move(n)), val(v) { kind=Kind::Ast; }
AstStr::AstStr(std::string n, std::string v): AstNode(std::move(n)), val(std::move(v)) { kind=Kind::Ast; }
AstSym::AstSym(std::string n, std::string s): AstNode(std::move(n)), id(std::move(s)) { kind=Kind::Ast; }
AstIf::AstIf(std::string n, std::shared_ptr<AstNode> C, std::shared_ptr<AstNode> A, std::shared_ptr<AstNode> B)
    : AstNode(std::move(n)), c(std::move(C)), a(std::move(A)), b(std::move(B)) { kind=Kind::Ast; }
AstLambda::AstLambda(std::string n, std::vector<std::string> ps, std::shared_ptr<AstNode> b)
    : AstNode(std::move(n)), params(std::move(ps)), body(std::move(b)){ kind=Kind::Ast; }
AstCall::AstCall(std::string n, std::shared_ptr<AstNode> f, std::vector<std::shared_ptr<AstNode>> a)
    : AstNode(std::move(n)), fn(std::move(f)), args(std::move(a)){ kind=Kind::Ast; }
AstHolder::AstHolder(std::string n, std::shared_ptr<AstNode> in)
    : AstNode(std::move(n)), inner(std::move(in)){ kind=Kind::Ast; }

// ====== AST eval ======
Value AstInt::eval(std::shared_ptr<Env>) { return Value::I(val); }
Value AstBool::eval(std::shared_ptr<Env>) { return Value::B(val); }
Value AstStr::eval(std::shared_ptr<Env>) { return Value::S(val); }
Value AstSym::eval(std::shared_ptr<Env> e) {
    auto v = e->get(id); if(!v) throw std::runtime_error("unbound "+id); return *v;
}
Value AstIf::eval(std::shared_ptr<Env> e){
    auto cv=c->eval(e);
    bool t = std::visit([](auto&& x)->bool{
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<T,bool>)        return x;
        if constexpr (std::is_same_v<T,int64_t>)     return x!=0;
        if constexpr (std::is_same_v<T,std::string>) return !x.empty();
        if constexpr (std::is_same_v<T,Value::List>) return !x.empty();
        else return true;
    }, cv.v);
    return (t? a: b)->eval(e);
}
Value AstLambda::eval(std::shared_ptr<Env> e){
    return Value::Clo(Value::Closure{params, body, e});
}
Value AstCall::eval(std::shared_ptr<Env> e){
    Value f = fn->eval(e);
    std::vector<Value> av; av.reserve(args.size());
    for (auto& a: args) av.push_back(a->eval(e));
    if (std::holds_alternative<Value::Builtin>(f.v)) {
        auto& b = std::get<Value::Builtin>(f.v);
        return b(av, e);
    } else if (std::holds_alternative<Value::Closure>(f.v)) {
        auto clo = std::get<Value::Closure>(f.v);
        if (clo.params.size()!=av.size()) throw std::runtime_error("arity mismatch");
        auto child = std::make_shared<Env>(clo.env);
        for (size_t i=0;i<av.size();++i) child->set(clo.params[i], av[i]);
        return clo.body->eval(child);
    }
    throw std::runtime_error("call of non-function");
}
Value AstHolder::eval(std::shared_ptr<Env> e){ return inner->eval(e); }

// ====== VFS ======
Vfs* G_VFS = nullptr;

Vfs::Vfs(){ G_VFS = this; }

std::vector<std::string> Vfs::splitPath(const std::string& p){
    TRACE_FN("p=", p);
    std::vector<std::string> parts; std::string cur;
    for(char c: p){ if(c=='/'){ if(!cur.empty()){ parts.push_back(cur); cur.clear(); } } else cur.push_back(c); }
    if(!cur.empty()) parts.push_back(cur);
    return parts;
}
std::shared_ptr<VfsNode> Vfs::resolve(const std::string& path){
    TRACE_FN("path=", path);
    if(path.empty()||path[0]!='/') throw std::runtime_error("abs path required");
    auto parts = splitPath(path);
    std::shared_ptr<VfsNode> cur = root;
    for (auto& s: parts){
        if (!cur->isDir()) throw std::runtime_error("not dir: "+s);
        auto& ch = cur->children();
        auto it = ch.find(s);
        if (it==ch.end()) throw std::runtime_error("not found: "+s);
        cur = it->second;
    }
    return cur;
}
std::shared_ptr<DirNode> Vfs::ensureDir(const std::string& path){
    TRACE_FN("path=", path);
    if (path=="/") return root;
    auto parts = splitPath(path);
    std::shared_ptr<VfsNode> cur = root;
    for (auto& s: parts){
        if (!cur->isDir()) throw std::runtime_error("not dir: "+s);
        auto& ch = cur->children();
        auto it = ch.find(s);
        if (it==ch.end()){
            auto d = std::make_shared<DirNode>(s);
            d->parent = cur;
            ch[s]=d;
            cur = d;
        } else cur = it->second;
    }
    if (!cur->isDir()) throw std::runtime_error("exists but not dir");
    return std::static_pointer_cast<DirNode>(cur);
}
void Vfs::mkdir(const std::string& path){
    TRACE_FN("path=", path);
    ensureDir(path);
}
void Vfs::touch(const std::string& path){
    TRACE_FN("path=", path);
    auto parts = splitPath(path);
    if (parts.empty()) throw std::runtime_error("bad path");
    std::string fname = parts.back(); parts.pop_back();
    std::string dir = "/";
    for (auto& s: parts) dir += (dir.size()>1?"/":"") + s;
    auto d = ensureDir(dir);
    if (d->children().count(fname)==0){
        auto f = std::make_shared<FileNode>(fname,"");
        f->parent=d;
        d->children()[fname]=f;
    }
}
void Vfs::write(const std::string& path, const std::string& data){
    TRACE_FN("path=", path, ", size=", data.size());
    // create file if needed
    auto parts = splitPath(path);
    if (parts.empty()) throw std::runtime_error("bad path");
    std::string fname = parts.back(); parts.pop_back();
    std::string dir = "/";
    for (auto& s: parts) dir += (dir.size()>1?"/":"") + s;
    auto d = ensureDir(dir);
    auto& ch = d->children();
    std::shared_ptr<VfsNode> n;
    if (ch.count(fname)==0){
        auto f = std::make_shared<FileNode>(fname,"");
        f->parent=d; ch[fname]=f; n=f;
    } else n=ch[fname];
    if (n->kind!=VfsNode::Kind::File) throw std::runtime_error("write non-file");
    n->write(data);
}
std::string Vfs::read(const std::string& path){
    TRACE_FN("path=", path);
    return resolve(path)->read();
}
void Vfs::addNode(const std::string& dirpath, std::shared_ptr<VfsNode> n){
    TRACE_FN("dirpath=", dirpath, ", node=", n ? n->name : std::string("<null>"));
    auto d = ensureDir(dirpath);
    n->parent=d;
    d->children()[n->name]=n;
}
void Vfs::rm(const std::string& path){
    TRACE_FN("path=", path);
    if (path=="/") throw std::runtime_error("rm / not allowed");
    auto parts = splitPath(path);
    std::string name=parts.back(); parts.pop_back();
    std::string dir="/";
    for (auto& s: parts) dir += (dir.size()>1?"/":"") + s;
    auto d = resolve(dir);
    if (!d->isDir()) throw std::runtime_error("parent not dir");
    d->children().erase(name);
}
void Vfs::mv(const std::string& src, const std::string& dst){
    TRACE_FN("src=", src, ", dst=", dst);
    auto s = resolve(src);
    auto parts = splitPath(dst);
    std::string name=parts.back(); parts.pop_back();
    std::string dir="/";
    for (auto& x: parts) dir += (dir.size()>1?"/":"") + x;
    auto d = ensureDir(dir);
    if (auto p = s->parent.lock()){
        p->children().erase(s->name);
    }
    s->name=name;
    s->parent=d;
    d->children()[name]=s;
}
void Vfs::link(const std::string& src, const std::string& dst){
    TRACE_FN("src=", src, ", dst=", dst);
    auto s = resolve(src);
    auto parts = splitPath(dst);
    std::string name=parts.back(); parts.pop_back();
    std::string dir="/";
    for (auto& x: parts) dir += (dir.size()>1?"/":"") + x;
    auto d = ensureDir(dir);
    d->children()[name]=s; // alias
}
void Vfs::ls(const std::string& p){
    TRACE_FN("p=", p);
    auto n = resolve(p);
    if (!n->isDir()){
        std::cout << p << "\n";
        return;
    }
    for (auto& kv : n->children()){
        auto& v=kv.second;
        char t = v->kind==VfsNode::Kind::Dir? 'd' : (v->kind==VfsNode::Kind::File? 'f' : 'a');
        std::cout << t << " " << kv.first << "\n";
    }
}
void Vfs::tree(std::shared_ptr<VfsNode> n, std::string pref){
    TRACE_FN("node=", n ? n->name : std::string("<root>"), ", pref=", pref);
    if (!n) n=root;
    char t = n->kind==VfsNode::Kind::Dir? 'd' : (n->kind==VfsNode::Kind::File? 'f' : 'a');
    std::cout << pref << t << " " << n->name << "\n";
    if (n->isDir()){
        for (auto& kv: n->children()){
            tree(kv.second, pref+"  ");
        }
    }
}

// ====== Parser ======
static size_t POS;
static std::shared_ptr<AstNode> parseExpr(const std::vector<Token>& T);

std::vector<Token> lex(const std::string& src){
    std::vector<Token> t; std::string cur;
    auto push=[&]{ if(!cur.empty()){ t.push_back({cur}); cur.clear(); } };
    for (size_t i=0;i<src.size();++i){
        char c=src[i];
        if (std::isspace(static_cast<unsigned char>(c))) { push(); continue; }
        if (c=='('||c==')'){ push(); t.push_back({std::string(1,c)}); continue; }
        if (c=='"'){ push(); std::string s; ++i;
            while(i<src.size() && src[i]!='"'){
                if(src[i]=='\\' && i+1<src.size()){ s.push_back(src[i+1]); i+=2; }
                else s.push_back(src[i++]);
            }
            t.push_back({std::string("\"")+s+"\""}); continue;
        }
        cur.push_back(c);
    }
    push(); return t;
}

static bool isInt(const std::string& s){
    if (s.empty()) return false;
    size_t i=(s[0]=='-'?1:0); if (i==s.size()) return false;
    for (; i<s.size(); ++i) if(!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
    return true;
}

static std::shared_ptr<AstNode> atom(const std::string& s){
    if (s=="#t") return std::make_shared<AstBool>("<b>", true);
    if (s=="#f") return std::make_shared<AstBool>("<b>", false);
    if (s.size()>=2 && s.front()=='"' && s.back()=='"')
        return std::make_shared<AstStr>("<s>", s.substr(1, s.size()-2));
    if (isInt(s)) return std::make_shared<AstInt>("<i>", std::stoll(s));
    return std::make_shared<AstSym>("<sym>", s);
}

static std::shared_ptr<AstNode> parseList(const std::vector<Token>& T){
    if (POS>=T.size() || T[POS].s!="(") throw std::runtime_error("expected (");
    ++POS;
    if (POS<T.size() && T[POS].s==")"){ ++POS; return std::make_shared<AstStr>("<s>",""); }
    auto head = parseExpr(T);
    auto sym  = std::dynamic_pointer_cast<AstSym>(head);
    std::vector<std::shared_ptr<AstNode>> items;
    while (POS<T.size() && T[POS].s!=")") items.push_back(parseExpr(T));
    if (POS>=T.size()) throw std::runtime_error("missing )");
    ++POS;

    std::string H = sym? sym->id : std::string();
    if (H=="if"){
        if (items.size()!=3) throw std::runtime_error("if needs 3 args");
        return std::make_shared<AstIf>("<if>", items[0], items[1], items[2]);
    }
    if (H=="lambda"){
        if (items.size()<2) throw std::runtime_error("lambda needs params and body");
        // proto: single param only: (lambda x body)
        std::vector<std::string> ps;
        if (auto sp=std::dynamic_pointer_cast<AstSym>(items[0])) ps.push_back(sp->id);
        else throw std::runtime_error("lambda single param only");
        auto body=items.back();
        return std::make_shared<AstLambda>("<lam>", ps, body);
    }
    // generic call
    return std::make_shared<AstCall>("<call>", head, items);
}

static std::shared_ptr<AstNode> parseExpr(const std::vector<Token>& T){
    if (POS>=T.size()) throw std::runtime_error("unexpected EOF");
    auto s = T[POS].s;
    if (s=="(") return parseList(T);
    if (s==")") throw std::runtime_error("unexpected )");
    ++POS;
    return atom(s);
}

std::shared_ptr<AstNode> parse(const std::string& src){
    POS=0; auto T = lex(src);
    auto n = parseExpr(T);
    if (POS!=T.size()) throw std::runtime_error("extra tokens");
    return n;
}

// ====== Builtins ======
void install_builtins(std::shared_ptr<Env> g){
    auto wrap = [&](auto op){
        return Value::Built([op](std::vector<Value>& av, std::shared_ptr<Env>){
            if (av.size()<2) throw std::runtime_error("need at least 2 args");
            auto gi=[](const Value& v)->int64_t{
                if (!std::holds_alternative<int64_t>(v.v)) throw std::runtime_error("int expected");
                return std::get<int64_t>(v.v);
            };
            int64_t acc = gi(av[0]);
            for (size_t i=1;i<av.size();++i) acc = op(acc, gi(av[i]));
            return Value::I(acc);
        });
    };
    g->set("+", wrap(std::plus<int64_t>()));
    g->set("-", wrap(std::minus<int64_t>()));
    g->set("*", wrap(std::multiplies<int64_t>()));

    g->set("=", Value::Built([](std::vector<Value>& av, std::shared_ptr<Env>){
        if (av.size()!=2) throw std::runtime_error("= needs 2 args");
        return Value::B(av[0].show()==av[1].show());
    }));
    g->set("<", Value::Built([](std::vector<Value>& av, std::shared_ptr<Env>){
        if (av.size()!=2) throw std::runtime_error("< needs 2 args");
        if (!std::holds_alternative<int64_t>(av[0].v) || !std::holds_alternative<int64_t>(av[1].v))
            throw std::runtime_error("int expected");
        return Value::B(std::get<int64_t>(av[0].v) < std::get<int64_t>(av[1].v));
    }));
    g->set("print", Value::Built([](std::vector<Value>& av, std::shared_ptr<Env>){
        for (size_t i=0;i<av.size();++i){ if(i) std::cout<<" "; std::cout<<av[i].show(); }
        std::cout<<"\n"; return av.empty()? Value() : av.back();
    }));

    // lists
    g->set("list", Value::Built([](std::vector<Value>& av, std::shared_ptr<Env>){ return Value::L(av); }));
    g->set("cons", Value::Built([](std::vector<Value>& av, std::shared_ptr<Env>){
        if (av.size()!=2) throw std::runtime_error("cons x xs");
        if (!std::holds_alternative<Value::List>(av[1].v)) throw std::runtime_error("cons expects list");
        Value::List out; const auto& xs = std::get<Value::List>(av[1].v);
        out.reserve(xs.size()+1); out.push_back(av[0]); out.insert(out.end(), xs.begin(), xs.end());
        return Value::L(out);
    }));
    g->set("head", Value::Built([](std::vector<Value>& av, std::shared_ptr<Env>){
        if (av.size()!=1 || !std::holds_alternative<Value::List>(av[0].v)) throw std::runtime_error("head xs");
        const auto& xs = std::get<Value::List>(av[0].v); if(xs.empty()) throw std::runtime_error("head of empty");
        return xs.front();
    }));
    g->set("tail", Value::Built([](std::vector<Value>& av, std::shared_ptr<Env>){
        if (av.size()!=1 || !std::holds_alternative<Value::List>(av[0].v)) throw std::runtime_error("tail xs");
        const auto& xs = std::get<Value::List>(av[0].v); if(xs.empty()) throw std::runtime_error("tail of empty");
        return Value::L(Value::List(xs.begin()+1, xs.end()));
    }));
    g->set("null?", Value::Built([](std::vector<Value>& av, std::shared_ptr<Env>){
        if (av.size()!=1) throw std::runtime_error("null? xs");
        return Value::B(std::holds_alternative<Value::List>(av[0].v) && std::get<Value::List>(av[0].v).empty());
    }));

    // strings
    g->set("str.cat", Value::Built([](std::vector<Value>& av, std::shared_ptr<Env>){
        std::string s; for(auto& v: av){ if(!std::holds_alternative<std::string>(v.v)) throw std::runtime_error("str.cat expects strings"); s += std::get<std::string>(v.v); }
        return Value::S(s);
    }));
    g->set("str.sub", Value::Built([](std::vector<Value>& av, std::shared_ptr<Env>){
        if(av.size()!=3) throw std::runtime_error("str.sub s start len");
        if(!std::holds_alternative<std::string>(av[0].v) || !std::holds_alternative<int64_t>(av[1].v) || !std::holds_alternative<int64_t>(av[2].v))
            throw std::runtime_error("str.sub types");
        const std::string& s = std::get<std::string>(av[0].v);
        size_t st = static_cast<size_t>(std::max<int64_t>(0, std::get<int64_t>(av[1].v)));
        size_t ln = static_cast<size_t>(std::max<int64_t>(0, std::get<int64_t>(av[2].v)));
        if (st > s.size()) return Value::S("");
        return Value::S(s.substr(st, ln));
    }));
    g->set("str.find", Value::Built([](std::vector<Value>& av, std::shared_ptr<Env>){
        if(av.size()!=2 || !std::holds_alternative<std::string>(av[0].v) || !std::holds_alternative<std::string>(av[1].v))
            throw std::runtime_error("str.find s sub");
        auto pos = std::get<std::string>(av[0].v).find(std::get<std::string>(av[1].v));
        return Value::I(pos==std::string::npos? -1 : static_cast<int64_t>(pos));
    }));

    // VFS helpers
    g->set("vfs-write", Value::Built([](std::vector<Value>& av, std::shared_ptr<Env>){
        if(!G_VFS) throw std::runtime_error("no vfs");
        if(av.size()!=2 || !std::holds_alternative<std::string>(av[0].v) || !std::holds_alternative<std::string>(av[1].v))
            throw std::runtime_error("vfs-write path string");
        G_VFS->write(std::get<std::string>(av[0].v), std::get<std::string>(av[1].v));
        return av[0];
    }));
    g->set("vfs-read", Value::Built([](std::vector<Value>& av, std::shared_ptr<Env>){
        if(!G_VFS) throw std::runtime_error("no vfs");
        if(av.size()!=1 || !std::holds_alternative<std::string>(av[0].v))
            throw std::runtime_error("vfs-read path");
        return Value::S(G_VFS->read(std::get<std::string>(av[0].v)));
    }));
    g->set("vfs-ls", Value::Built([](std::vector<Value>& av, std::shared_ptr<Env>){
        if(!G_VFS) throw std::runtime_error("no vfs");
        if(av.size()!=1 || !std::holds_alternative<std::string>(av[0].v))
            throw std::runtime_error("vfs-ls \"/path\"");
        auto n = G_VFS->resolve(std::get<std::string>(av[0].v));
        if(!n->isDir()) throw std::runtime_error("vfs-ls: not dir");
        Value::List entries;
        for(auto& kv : n->children()){
            const std::string& name = kv.first;
            auto& node = kv.second;
            std::string t = node->kind==VfsNode::Kind::Dir? "dir" : (node->kind==VfsNode::Kind::File? "file" : "ast");
            entries.push_back(Value::L(Value::List{ Value::S(name), Value::S(t) }));
        }
        return Value::L(entries);
    }));

    // export & sys
    g->set("export", Value::Built([](std::vector<Value>& av, std::shared_ptr<Env>){
        if(!G_VFS) throw std::runtime_error("no vfs");
        if(av.size()!=2 || !std::holds_alternative<std::string>(av[0].v) || !std::holds_alternative<std::string>(av[1].v))
            throw std::runtime_error("export vfs host");
        std::string data = G_VFS->read(std::get<std::string>(av[0].v));
        std::string host = std::get<std::string>(av[1].v);
        std::ofstream f(host, std::ios::binary);
        if(!f) throw std::runtime_error("export: cannot open host file");
        f.write(data.data(), static_cast<std::streamsize>(data.size()));
        return Value::S(host);
    }));
    g->set("sys", Value::Built([](std::vector<Value>& av, std::shared_ptr<Env>){
        if(av.size()!=1 || !std::holds_alternative<std::string>(av[0].v))
            throw std::runtime_error("sys \"cmd\"");
        std::string cmd = std::get<std::string>(av[0].v);
        // kevyt saneeraus
        for(char c: cmd){
            if(!(std::isalnum(static_cast<unsigned char>(c)) ||
                 std::isspace(static_cast<unsigned char>(c)) ||
                 std::strchr("/._-+:*\"'()=", c)))
                 throw std::runtime_error("sys: kielletty merkki");
        }
        std::string out = exec_capture(cmd + " 2>&1");
        return Value::S(out);
    }));

    // C++ apuri: hello-koodi
    g->set("cpp:hello", Value::Built([](std::vector<Value>&, std::shared_ptr<Env>){
        std::string code = "#include <iostream>\nint main(){ std::cout<<\"Hello, world!\\n\"; return 0; }\n";
        return Value::S(code);
    }));
}

// ====== Exec utils ======
std::string exec_capture(const std::string& cmd, const std::string& desc){
    TRACE_FN("cmd=", cmd, ", desc=", desc);
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
        TRACE_LOOP("exec_capture.read", std::string("bytes=") + std::to_string(n));
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

// ====== C++ AST nodes ======
CppInclude::CppInclude(std::string n, std::string h, bool a)
    : CppNode(std::move(n)), header(std::move(h)), angled(a) { kind=Kind::Ast; }
std::string CppInclude::dump(int) const {
    return std::string("#include ") + (angled?"<":"\"") + header + (angled?">":"\"") + "\n";
}
CppId::CppId(std::string n, std::string i) : CppExpr(std::move(n)), id(std::move(i)) { kind=Kind::Ast; }
std::string CppId::dump(int) const { return id; }
namespace {
bool is_octal_digit(char c){ return c>='0' && c<='7'; }
bool is_hex_digit(char c){ return std::isxdigit(static_cast<unsigned char>(c)) != 0; }

void verify_cpp_string_literal(const std::string& lit){
    for(size_t i=0;i<lit.size();++i){
        unsigned char uc = static_cast<unsigned char>(lit[i]);
        if(uc=='\n' || uc=='\r') throw std::runtime_error("cpp string literal contains raw newline");
        if(uc=='\\'){
            if(++i>=lit.size()) throw std::runtime_error("unterminated escape in cpp string literal");
            unsigned char esc = static_cast<unsigned char>(lit[i]);
            switch(esc){
                case '"': case '\\': case 'n': case 'r': case 't':
                case 'b': case 'f': case 'v': case 'a': case '?':
                    break;
                case 'x': {
                    size_t digits=0;
                    while(i+1<lit.size() && is_hex_digit(lit[i+1]) && digits<2){ ++i; ++digits; }
                    if(digits==0) throw std::runtime_error("\\x escape missing hex digits");
                    break;
                }
                case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': {
                    size_t digits=0;
                    while(i+1<lit.size() && is_octal_digit(lit[i+1]) && digits<2){ ++i; ++digits; }
                    break;
                }
                default:
                    throw std::runtime_error("unsupported escape sequence in cpp string literal");
            }
        } else if(uc < 0x20 || uc == 0x7f){
            throw std::runtime_error("cpp string literal contains unescaped control byte");
        }
    }
}
}

CppString::CppString(std::string n, std::string v) : CppExpr(std::move(n)), s(std::move(v)) { kind=Kind::Ast; }
std::string CppString::esc(const std::string& x){
    std::string out;
    out.reserve(x.size()+8);
    auto append_octal = [&out](unsigned char uc){
        out.push_back('\\');
        out.push_back(static_cast<char>('0' + ((uc >> 6) & 0x7)));
        out.push_back(static_cast<char>('0' + ((uc >> 3) & 0x7)));
        out.push_back(static_cast<char>('0' + (uc & 0x7)));
    };

    bool escape_next_question = false;
    for(size_t i=0;i<x.size();++i){
        unsigned char uc = static_cast<unsigned char>(x[i]);
        if(uc=='?'){
            if(escape_next_question || (i+1<x.size() && x[i+1]=='?')){
                out += "\\?";
                escape_next_question = (i+1<x.size() && x[i+1]=='?');
            } else {
                out.push_back('?');
                escape_next_question = false;
            }
            continue;
        }

        escape_next_question = false;
        switch(uc){
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\v': out += "\\v"; break;
            case '\a': out += "\\a"; break;
            default:
                if(uc < 0x20 || uc == 0x7f){
                    append_octal(uc);
                } else if(uc >= 0x80){
                    append_octal(uc);
                } else {
                    out.push_back(static_cast<char>(uc));
                }
        }
    }
    return out;
}
std::string CppString::dump(int) const {
    std::string escaped = esc(s);
    verify_cpp_string_literal(escaped);
    return std::string("\"") + escaped + "\"";
}
CppInt::CppInt(std::string n, long long x): CppExpr(std::move(n)), v(x) { kind=Kind::Ast; }
std::string CppInt::dump(int) const { return std::to_string(v); }
CppCall::CppCall(std::string n, std::shared_ptr<CppExpr> f, std::vector<std::shared_ptr<CppExpr>> a)
    : CppExpr(std::move(n)), fn(std::move(f)), args(std::move(a)) { kind=Kind::Ast; }
std::string CppCall::dump(int) const {
    std::string s = fn->dump(); s+='(';
    bool first=true; for(auto& a: args){ if(!first) s+=", "; first=false; s+=a->dump(); } s+=')';
    return s;
}
CppBinOp::CppBinOp(std::string n, std::string o, std::shared_ptr<CppExpr> A, std::shared_ptr<CppExpr> B)
    : CppExpr(std::move(n)), op(std::move(o)), a(std::move(A)), b(std::move(B)) { kind=Kind::Ast; }
std::string CppBinOp::dump(int) const { return a->dump()+" "+op+" "+b->dump(); }
CppStreamOut::CppStreamOut(std::string n, std::vector<std::shared_ptr<CppExpr>> xs)
    : CppExpr(std::move(n)), chain(std::move(xs)) { kind=Kind::Ast; }
std::string CppStreamOut::dump(int) const {
    std::string s="std::cout"; for(auto& e: chain){ s+=" << "; s+=e->dump(); } return s;
}
CppRawExpr::CppRawExpr(std::string n, std::string t)
    : CppExpr(std::move(n)), text(std::move(t)) { kind=Kind::Ast; }
std::string CppRawExpr::dump(int) const { return text; }
CppExprStmt::CppExprStmt(std::string n, std::shared_ptr<CppExpr> E)
    : CppStmt(std::move(n)), e(std::move(E)) { kind=Kind::Ast; }
std::string CppExprStmt::dump(int indent) const { return ind(indent)+e->dump()+";\n"; }
CppReturn::CppReturn(std::string n, std::shared_ptr<CppExpr> E)
    : CppStmt(std::move(n)), e(std::move(E)) { kind=Kind::Ast; }
std::string CppReturn::dump(int indent) const {
    std::string s = ind(indent) + "return";
    if (e) s += " " + e->dump();
    s += ";\n";
    return s;
}
CppRawStmt::CppRawStmt(std::string n, std::string t)
    : CppStmt(std::move(n)), text(std::move(t)) { kind=Kind::Ast; }
std::string CppRawStmt::dump(int indent) const {
    std::string pad = ind(indent);
    std::string out;
    size_t start = 0;
    while (start <= text.size()){
        size_t end = text.find('\n', start);
        std::string line = end==std::string::npos ? text.substr(start) : text.substr(start, end-start);
        if(!line.empty() || end!=std::string::npos) out += pad + line + "\n";
        if (end==std::string::npos) break;
        start = end + 1;
    }
    if(out.empty()) out = pad + "\n";
    return out;
}
CppVarDecl::CppVarDecl(std::string n, std::string ty, std::string nm, std::string in, bool has)
    : CppStmt(std::move(n)), type(std::move(ty)), name(std::move(nm)), init(std::move(in)), hasInit(has) { kind=Kind::Ast; }
std::string CppVarDecl::dump(int indent) const {
    std::string s = ind(indent) + type + " " + name;
    if (hasInit){
        if (!init.empty() && (init[0]=='{' || init[0]=='(')) s += init;
        else if (!init.empty() && init[0]=='=') s += " " + init;
        else if (!init.empty()) s += " = " + init;
    }
    s += ";\n";
    return s;
}
CppCompound::CppCompound(std::string n) : CppStmt(std::move(n)) { kind=Kind::Ast; }
std::string CppCompound::dump(int indent) const {
    std::string s = ind(indent) + "{\n";
    for(auto& st: stmts) s += st->dump(indent+2);
    s += ind(indent) + "}\n"; return s;
}
CppFunction::CppFunction(std::string n, std::string rt, std::string nm)
    : CppNode(std::move(n)), retType(std::move(rt)), name(std::move(nm)) { kind=Kind::Ast; body=std::make_shared<CppCompound>("body"); }
std::string CppFunction::dump(int indent) const {
    std::string s; s += retType + " " + name + "(";
    for(size_t i=0;i<params.size();++i){ if(i) s += ", "; s += params[i].type + " " + params[i].name; }
    s += ")\n"; s += body->dump(indent); return s;
}
CppRangeFor::CppRangeFor(std::string n, std::string d, std::string r)
    : CppStmt(std::move(n)), decl(std::move(d)), range(std::move(r)), body(std::make_shared<CppCompound>("body")) { kind=Kind::Ast; }
std::string CppRangeFor::dump(int indent) const {
    std::string s = ind(indent) + "for (" + decl + " : " + range + ")\n";
    s += body->dump(indent);
    return s;
}
CppTranslationUnit::CppTranslationUnit(std::string n) : CppNode(std::move(n)) { kind=Kind::Ast; }
std::string CppTranslationUnit::dump(int) const {
    std::string s;
    for(auto& i: includes) s += i->dump(0);
    s += "\n";
    for(auto& f: funcs){ s += f->dump(0); s += "\n"; }
    return s;
}

std::shared_ptr<CppTranslationUnit> expect_tu(std::shared_ptr<VfsNode> n){
    auto tu = std::dynamic_pointer_cast<CppTranslationUnit>(n);
    if(!tu) throw std::runtime_error("not a CppTranslationUnit node");
    return tu;
}
std::shared_ptr<CppFunction> expect_fn(std::shared_ptr<VfsNode> n){
    auto fn = std::dynamic_pointer_cast<CppFunction>(n);
    if(!fn) throw std::runtime_error("not a CppFunction node");
    return fn;
}
std::shared_ptr<CppCompound> expect_block(std::shared_ptr<VfsNode> n){
    if(auto fn = std::dynamic_pointer_cast<CppFunction>(n)) return fn->body;
    if(auto block = std::dynamic_pointer_cast<CppCompound>(n)) return block;
    if(auto loop = std::dynamic_pointer_cast<CppRangeFor>(n)) return loop->body;
    throw std::runtime_error("node does not own a compound body");
}
void vfs_add(Vfs& vfs, const std::string& path, std::shared_ptr<VfsNode> node){
    std::string dir = path.substr(0, path.find_last_of('/')); if(dir.empty()) dir = "/";
    std::string name = path.substr(path.find_last_of('/')+1);
    node->name = name; vfs.addNode(dir, node);
}
void cpp_dump_to_vfs(Vfs& vfs, const std::string& tuPath, const std::string& filePath){
    auto n = vfs.resolve(tuPath);
    auto tu = expect_tu(n);
    std::string code = tu->dump(0);
    vfs.write(filePath, code);
}

// ====== OpenAI helpers ======
static std::string system_prompt_text(){
    return std::string("You are a codex-like assistant embedded in a tiny single-binary IDE.\n") +
           snippets::tool_list() + "\nRespond concisely in Finnish.";
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
std::string build_responses_payload(const std::string& model, const std::string& user_prompt){
    std::string sys = system_prompt_text();
    const char* content_type = "input_text";
    std::string js = std::string("{")+
        "\"model\":\""+json_escape(model)+"\","+
        "\"input\":[{\"role\":\"system\",\"content\":[{\"type\":\""+content_type+"\",\"text\":\""+json_escape(sys)+"\"}]},"+
        "{\"role\":\"user\",\"content\":[{\"type\":\""+content_type+"\",\"text\":\""+json_escape(user_prompt)+"\"}]}]}";
    return js;
}
static std::string build_chat_payload(const std::string& model, const std::string& system_prompt, const std::string& user_prompt){
    return std::string("{") +
        "\"model\":\"" + json_escape(model) + "\"," +
        "\"messages\":[" +
            "{\"role\":\"system\",\"content\":\"" + json_escape(system_prompt) + "\"}," +
            "{\"role\":\"user\",\"content\":\"" + json_escape(user_prompt) + "\"}" +
        "]," +
        "\"temperature\":0.0" +
    "}";
}
static std::optional<std::string> decode_json_string(const std::string& raw, size_t quote_pos){
    if(quote_pos>=raw.size() || raw[quote_pos] != '"') return std::nullopt;
    std::string out;
    bool escape=false;
    for(size_t i = quote_pos + 1; i < raw.size(); ++i){
        char c = raw[i];
        if(escape){
            switch(c){
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'v': out.push_back('\v'); break;
                case 'a': out.push_back('\a'); break;
                case '\\': out.push_back('\\'); break;
                case '"': out.push_back('"'); break;
                case 'u':
                    out.push_back('\\');
                    out.push_back('u');
                    break;
                default:
                    out.push_back(c);
                    break;
            }
            escape=false;
            continue;
        }
        if(c=='\\'){
            escape=true;
            continue;
        }
        if(c=='"') return out;
        out.push_back(c);
    }
    return std::nullopt;
}
static std::optional<std::string> json_string_value_after_colon(const std::string& raw, size_t colon_pos){
    if(colon_pos==std::string::npos) return std::nullopt;
    size_t value_pos = raw.find_first_not_of(" \t\r\n", colon_pos + 1);
    if(value_pos==std::string::npos || raw[value_pos] != '"') return std::nullopt;
    return decode_json_string(raw, value_pos);
}
static std::optional<std::string> find_json_string_field(const std::string& raw, const std::string& field, size_t startPos=0){
    std::string marker = std::string("\"") + field + "\"";
    size_t pos = raw.find(marker, startPos);
    if(pos==std::string::npos) return std::nullopt;
    size_t colon = raw.find(':', pos + marker.size());
    if(colon==std::string::npos) return std::nullopt;
    size_t quote = raw.find('"', colon+1);
    if(quote==std::string::npos) return std::nullopt;
    return decode_json_string(raw, quote);
}
static std::optional<std::string> openai_extract_output_text(const std::string& raw){
    size_t search_pos = 0;
    while(true){
        size_t type_pos = raw.find("\"type\"", search_pos);
        if(type_pos==std::string::npos) break;
        size_t colon = raw.find(':', type_pos);
        if(colon==std::string::npos) break;
        auto type_val = json_string_value_after_colon(raw, colon);
        if(type_val && *type_val == "output_text"){
            size_t text_pos = raw.find("\"text\"", colon);
            while(text_pos!=std::string::npos){
                size_t text_colon = raw.find(':', text_pos);
                if(text_colon==std::string::npos) break;
                if(auto text_val = json_string_value_after_colon(raw, text_colon)) return text_val;
                text_pos = raw.find("\"text\"", text_pos + 6);
            }
        }
        search_pos = colon + 1;
    }

    std::string legacy_marker = "\"output_text\"";
    if(size_t legacy_pos = raw.find(legacy_marker); legacy_pos!=std::string::npos){
        size_t colon = raw.find(':', legacy_pos);
        if(auto legacy_val = json_string_value_after_colon(raw, colon)) return legacy_val;
        if(colon!=std::string::npos){
            size_t quote = raw.find('"', colon);
            if(auto legacy_val = decode_json_string(raw, quote)) return legacy_val;
        }
    }
    return std::nullopt;
}
static std::string build_llama_completion_payload(const std::string& system_prompt, const std::string& user_prompt){
    std::string prompt = std::string("<|system|>\n") + system_prompt + "\n<|user|>\n" + user_prompt + "\n<|assistant|>";
    return std::string("{") +
        "\"prompt\":\"" + json_escape(prompt) + "\"," +
        "\"temperature\":0.0,\"stream\":false" +
    "}";
}
static std::optional<std::string> load_openai_key(){
    if(const char* envKey = std::getenv("OPENAI_API_KEY")){ if(*envKey) return std::string(envKey); }
    const char* home = std::getenv("HOME");
    if(!home || !*home) return std::nullopt;
    std::string path = std::string(home) + "/openai-key.txt";
    std::ifstream f(path, std::ios::binary);
    if(!f) return std::nullopt;
    std::string contents((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    while(!contents.empty() && (contents.back()=='\n' || contents.back()=='\r')) contents.pop_back();
    if(contents.empty()) return std::nullopt;
    return contents;
}

std::string call_openai(const std::string& prompt){
    auto keyOpt = load_openai_key();
    if(!keyOpt) return "error: OPENAI_API_KEY puuttuu ympäristöstä tai ~/openai-key.txt-tiedostosta";
    const std::string& key = *keyOpt;
    std::string base = std::getenv("OPENAI_BASE_URL") ? std::getenv("OPENAI_BASE_URL") : std::string("https://api.openai.com/v1");
    if(base.size() && base.back()=='/') base.pop_back();
    std::string model = std::getenv("OPENAI_MODEL") ? std::getenv("OPENAI_MODEL") : std::string("gpt-4o-mini");

    std::string payload = build_responses_payload(model, prompt);

    bool curl_ok = has_cmd("curl"); bool wget_ok = has_cmd("wget");
    if(!curl_ok && !wget_ok) return "error: curl tai wget ei löydy PATHista";

    std::string tmp = "/tmp/oai_req_" + std::to_string(::getpid()) + ".json";
    { std::ofstream f(tmp, std::ios::binary); if(!f) return "error: ei voi avata temp-tiedostoa"; f.write(payload.data(), (std::streamsize)payload.size()); }

    std::string cmd;
    if(curl_ok){
        cmd = std::string("curl -sS -X POST ")+base+"/responses -H 'Content-Type: application/json' -H 'Authorization: Bearer "+key+"' --data-binary @"+tmp;
    } else {
        cmd = std::string("wget -qO- --method=POST --header=Content-Type:application/json ")+
              std::string("--header=Authorization:'Bearer ")+key+"' "+base+"/responses --body-file="+tmp;
    }

    std::string raw = exec_capture(cmd, "ai:openai");
    std::remove(tmp.c_str());
    if(raw.empty()) return "error: tyhjä vastaus OpenAI:lta\n";

    // very crude output_text extraction
    if(auto text = openai_extract_output_text(raw)){
        return std::string("AI: ") + *text + "\n";
    }
    return raw + "\n";
}

std::string call_llama(const std::string& prompt){
    auto env_or_empty = [](const char* name) -> std::string {
        if(const char* val = std::getenv(name); val && *val) return std::string(val);
        return {};
    };
    std::string base = env_or_empty("LLAMA_BASE_URL");
    if(base.empty()) base = env_or_empty("LLAMA_SERVER");
    if(base.empty()) base = env_or_empty("LLAMA_URL");
    if(base.empty()) base = "http://192.168.1.169:8080";
    if(!base.empty() && base.back()=='/') base.pop_back();

    std::string model = env_or_empty("LLAMA_MODEL");
    if(model.empty()) model = "coder";

    bool curl_ok = has_cmd("curl"); bool wget_ok = has_cmd("wget");
    if(!curl_ok && !wget_ok) return "error: curl tai wget ei löydy PATHista";

    std::string system_prompt = system_prompt_text();

    static unsigned long long llama_req_counter = 0;
    auto send_request = [&](const std::string& endpoint, const std::string& payload) -> std::string {
        std::string tmp = "/tmp/llama_req_" + std::to_string(::getpid()) + "_" + std::to_string(++llama_req_counter) + ".json";
        {
            std::ofstream f(tmp, std::ios::binary);
            if(!f) return std::string();
            f.write(payload.data(), static_cast<std::streamsize>(payload.size()));
        }
        std::string url = base + endpoint;
        std::string cmd;
        if(curl_ok){
            cmd = std::string("curl -sS -X POST \"") + url + "\" -H \"Content-Type: application/json\" --data-binary @" + tmp;
        } else {
            cmd = std::string("wget -qO- --method=POST --header=Content-Type:application/json --body-file=") + tmp + " \"" + url + "\"";
        }
        std::string raw = exec_capture(cmd, std::string("ai:llama ")+endpoint);
        std::remove(tmp.c_str());
        return raw;
    };

    auto parse_chat_response = [&](const std::string& raw) -> std::optional<std::string> {
        if(raw.empty()) return std::nullopt;
        if(auto err = find_json_string_field(raw, "error")) return std::string("error: llama: ") + *err;
        size_t assistantPos = raw.find("\"role\":\"assistant\"");
        size_t searchPos = assistantPos==std::string::npos ? 0 : assistantPos;
        if(auto content = find_json_string_field(raw, "content", searchPos)) return std::string("AI: ") + *content;
        if(auto text = find_json_string_field(raw, "text", searchPos)) return std::string("AI: ") + *text;
        if(auto generic = find_json_string_field(raw, "result")) return std::string("AI: ") + *generic;
        return std::nullopt;
    };

    std::string chat_payload = build_chat_payload(model, system_prompt, prompt);
    std::string chat_raw = send_request("/v1/chat/completions", chat_payload);
    if(auto parsed = parse_chat_response(chat_raw)){
        if(parsed->rfind("error:", 0)==0) return *parsed + "\n";
        return *parsed + "\n";
    }

    std::string comp_payload = build_llama_completion_payload(system_prompt, prompt);
    std::string comp_raw = send_request("/completion", comp_payload);
    if(comp_raw.empty()){
        if(!chat_raw.empty()) return std::string("error: llama: unexpected response: ") + chat_raw + "\n";
        return "error: tyhjä vastaus llama-palvelimelta\n";
    }
    if(auto err = find_json_string_field(comp_raw, "error")) return std::string("error: llama: ") + *err + "\n";
    if(auto completion = find_json_string_field(comp_raw, "completion")) return std::string("AI: ") + *completion + "\n";
    size_t choicesPos = comp_raw.find("\"choices\"");
    if(auto text = find_json_string_field(comp_raw, "text", choicesPos==std::string::npos ? 0 : choicesPos))
        return std::string("AI: ") + *text + "\n";
    return std::string("error: llama: unexpected response: ") + comp_raw + "\n";
}

static bool env_truthy(const char* name){
    if(const char* val = std::getenv(name)) return *val != '\0';
    return false;
}

static std::string env_string(const char* name){
    if(const char* val = std::getenv(name); val && *val) return std::string(val);
    return {};
}

static std::string openai_cache_signature(){
    std::string base = env_string("OPENAI_BASE_URL");
    if(base.empty()) base = "https://api.openai.com/v1";
    if(!base.empty() && base.back()=='/') base.pop_back();
    std::string model = env_string("OPENAI_MODEL");
    if(model.empty()) model = "gpt-4o-mini";
    return std::string("openai|") + model + '|' + base;
}

static std::string llama_cache_signature(){
    std::string base = env_string("LLAMA_BASE_URL");
    if(base.empty()) base = env_string("LLAMA_SERVER");
    if(base.empty()) base = env_string("LLAMA_URL");
    if(base.empty()) base = "http://192.168.1.169:8080";
    if(!base.empty() && base.back()=='/') base.pop_back();
    std::string model = env_string("LLAMA_MODEL");
    if(model.empty()) model = "coder";
    return std::string("llama|") + model + '|' + base;
}

std::string call_ai(const std::string& prompt){
    auto dispatch_with_cache = [&](const std::string& providerLabel, const std::string& signature, auto&& fn) -> std::string {
        std::string key_material = make_cache_key_material(signature, prompt);
        if(auto cached = ai_cache_read(providerLabel, key_material)){
            return *cached;
        }
        std::string response = fn();
        ai_cache_write(providerLabel, key_material, response);
        return response;
    };

    auto use_llama = [&]() -> std::string {
        const std::string signature = llama_cache_signature();
        return dispatch_with_cache("llama", signature, [&](){ return call_llama(prompt); });
    };

    auto use_openai = [&]() -> std::string {
        const std::string signature = openai_cache_signature();
        return dispatch_with_cache("openai", signature, [&](){ return call_openai(prompt); });
    };

    std::string provider;
    if(const char* p = std::getenv("CODEX_AI_PROVIDER")) provider = p;
    std::transform(provider.begin(), provider.end(), provider.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });

    if(provider == "llama") return use_llama();
    if(provider == "openai") return use_openai();

    bool llama_hint = env_truthy("LLAMA_BASE_URL") || env_truthy("LLAMA_SERVER") || env_truthy("LLAMA_URL");
    auto keyOpt = load_openai_key();

    if(!keyOpt){
        return use_llama();
    }

    if(llama_hint) return use_llama();
    return use_openai();
}

// ====== REPL ======
static void help(){
    TRACE_FN();
    std::cout <<
R"(Commands:
  pwd
  cd [path]
  ls [path]
  tree [path]
  mkdir <path>
  touch <path>
  rm <path>
  mv <src> <dst>
  link <src> <dst>
  export <vfs> <host>
  cat [paths...] (tai stdin jos ei polkuja)
  grep [-i] <pattern> [path]
  rg [-i] <pattern> [path]
  head [-n N] [path]
  tail [-n N] [path]
  uniq [path]
  count [path]
  random [min [max]]
  true / false
  echo <path> <data...>
  parse <src-file> <dst-ast>
  eval <ast-path>
  putkita komentoja: a | b | c, a && b, a || b
  # AI
  ai <prompt...>
  ai.brief <key> [extra...]
  tools
  # C++ builder
  cpp.tu <ast-path>
  cpp.include <tu-path> <header> [angled0/1]
  cpp.func <tu-path> <name> <ret>
  cpp.param <fn-path> <type> <name>
  cpp.print <scope-path> <text>
  cpp.vardecl <scope-path> <type> <name> [init]
  cpp.expr <scope-path> <expression>
  cpp.stmt <scope-path> <raw>
  cpp.return <scope-path> [expression]
  cpp.returni <scope-path> <int>
  cpp.rangefor <scope-path> <loop-name> <decl> | <range>
  cpp.dump <tu-path> <vfs-file-path>
Notes:
  - Polut voivat olla suhteellisia nykyiseen VFS-hakemistoon (cd).
  - ./codex <skripti> suorittaa komennot tiedostosta ilman REPL-kehotetta.
  - ./codex <skripti> - suorittaa skriptin ja palaa interaktiiviseen tilaan.
  - ai.brief lukee promptit snippets/-hakemistosta (CODEX_SNIPPET_DIR ylikirjoittaa polun).
  - OPENAI_API_KEY pakollinen 'ai' komentoon OpenAI-tilassa. OPENAI_MODEL (oletus gpt-4o-mini), OPENAI_BASE_URL (oletus https://api.openai.com/v1).
  - Llama-palvelin: LLAMA_BASE_URL / LLAMA_SERVER (oletus http://192.168.1.169:8080), LLAMA_MODEL (oletus coder), CODEX_AI_PROVIDER=llama pakottaa käyttöön.
)"<<std::endl;
}

int main(int argc, char** argv){
    TRACE_FN();
    using std::string; using std::shared_ptr;
    std::ios::sync_with_stdio(false); std::cin.tie(nullptr);
    snippets::initialize(argc > 0 ? argv[0] : nullptr);

    auto usage = [&](const std::string& msg){
        std::cerr << msg << "\n";
        return 1;
    };

    const string usage_text = string("usage: ") + argv[0] + " [script-file [-]]";

    if(argc > 3){
        return usage(usage_text);
    }

    bool interactive = true;
    bool script_active = false;
    bool fallback_after_script = false;
    std::unique_ptr<std::ifstream> scriptStream;
    std::istream* input = &std::cin;

    if(argc >= 2){
        if(argc == 3){
            if(string(argv[2]) != "-"){
                return usage(usage_text);
            }
            fallback_after_script = true;
        }

        scriptStream = std::make_unique<std::ifstream>(argv[1]);
        if(!*scriptStream){
            std::cerr << "failed to open script '" << argv[1] << "'\n";
            return 1;
        }
        input = scriptStream.get();
        interactive = false;
        script_active = true;
    }

    Vfs vfs; auto env = std::make_shared<Env>(); install_builtins(env);
    vfs.mkdir("/src"); vfs.mkdir("/ast"); vfs.mkdir("/env"); vfs.mkdir("/astcpp"); vfs.mkdir("/cpp");

    std::cout << "codex-mini 🌲 VFS+AST+AI — 'help' kertoo karun totuuden.\n";
    string line;
    size_t repl_iter = 0;
    std::string cwd = "/";

    auto execute_single = [&](const CommandInvocation& inv, const std::string& stdin_data) -> CommandResult {
        ScopedCoutCapture capture;
        CommandResult result;
        const std::string& cmd = inv.name;

        if(cmd == "pwd"){
            result.output = cwd + "\n";

        } else if(cmd == "cd"){
            std::string target = inv.args.empty() ? std::string("/") : inv.args[0];
            std::string abs = normalize_path(cwd, target);
            auto node = vfs.resolve(abs);
            if(!node->isDir()) throw std::runtime_error("cd: not a directory");
            cwd = abs;

        } else if(cmd == "ls"){
            std::string abs = inv.args.empty() ? cwd : normalize_path(cwd, inv.args[0]);
            vfs.ls(abs);

        } else if(cmd == "tree"){
            if(inv.args.empty()){
                vfs.tree();
            } else {
                std::string abs = normalize_path(cwd, inv.args[0]);
                vfs.tree(vfs.resolve(abs));
            }

        } else if(cmd == "mkdir"){
            if(inv.args.empty()) throw std::runtime_error("mkdir <path>");
            vfs.mkdir(normalize_path(cwd, inv.args[0]));

        } else if(cmd == "touch"){
            if(inv.args.empty()) throw std::runtime_error("touch <path>");
            vfs.touch(normalize_path(cwd, inv.args[0]));

        } else if(cmd == "cat"){
            if(inv.args.empty()){
                result.output = stdin_data;
            } else {
                std::ostringstream oss;
                for(size_t i = 0; i < inv.args.size(); ++i){
                    std::string data = vfs.read(normalize_path(cwd, inv.args[i]));
                    oss << data;
                    if(data.empty() || data.back() != '\n') oss << '\n';
                }
                result.output = oss.str();
            }

        } else if(cmd == "grep"){
            if(inv.args.empty()) throw std::runtime_error("grep [-i] <pattern> [path]");
            size_t idx = 0;
            bool ignore_case = false;
            if(inv.args[idx] == "-i"){
                ignore_case = true;
                ++idx;
                if(idx >= inv.args.size()) throw std::runtime_error("grep [-i] <pattern> [path]");
            }
            std::string pattern = inv.args[idx++];
            std::string data = idx < inv.args.size() ? vfs.read(normalize_path(cwd, inv.args[idx])) : stdin_data;
            auto lines = split_lines(data);
            std::ostringstream oss;
            bool matched = false;
            std::string needle = pattern;
            if(ignore_case){
                std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
            }
            for(size_t i = 0; i < lines.lines.size(); ++i){
                std::string hay = lines.lines[i];
                if(ignore_case){
                    std::transform(hay.begin(), hay.end(), hay.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
                }
                if(hay.find(needle) != std::string::npos){
                    matched = true;
                    oss << lines.lines[i];
                    bool had_newline = (i < lines.lines.size() - 1) || lines.trailing_newline;
                    if(had_newline) oss << '\n';
                }
            }
            result.output = oss.str();
            result.success = matched;

        } else if(cmd == "rg"){
            if(inv.args.empty()) throw std::runtime_error("rg [-i] <pattern> [path]");
            size_t idx = 0;
            bool ignore_case = false;
            if(inv.args[idx] == "-i"){
                ignore_case = true;
                ++idx;
                if(idx >= inv.args.size()) throw std::runtime_error("rg [-i] <pattern> [path]");
            }
            std::string pattern = inv.args[idx++];
            std::regex_constants::syntax_option_type flags = std::regex_constants::ECMAScript;
            if(ignore_case) flags = static_cast<std::regex_constants::syntax_option_type>(flags | std::regex_constants::icase);
            std::regex re;
            try{
                re = std::regex(pattern, flags);
            } catch(const std::regex_error& e){
                throw std::runtime_error(std::string("rg regex error: ") + e.what());
            }
            std::string data = idx < inv.args.size() ? vfs.read(normalize_path(cwd, inv.args[idx])) : stdin_data;
            auto lines = split_lines(data);
            std::ostringstream oss;
            bool matched = false;
            for(size_t i = 0; i < lines.lines.size(); ++i){
                if(std::regex_search(lines.lines[i], re)){
                    matched = true;
                    oss << lines.lines[i];
                    bool had_newline = (i < lines.lines.size() - 1) || lines.trailing_newline;
                    if(had_newline) oss << '\n';
                }
            }
            result.output = oss.str();
            result.success = matched;

        } else if(cmd == "count"){
            std::string data = inv.args.empty() ? stdin_data : vfs.read(normalize_path(cwd, inv.args[0]));
            size_t lines = count_lines(data);
            result.output = std::to_string(lines) + "\n";

        } else if(cmd == "true"){
            result.success = true;

        } else if(cmd == "false"){
            result.success = false;

        } else if(cmd == "tail"){
            size_t idx = 0;
            size_t take = 10;
            auto is_number = [](const std::string& s){
                return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c){ return std::isdigit(c); });
            };
            if(idx < inv.args.size()){
                if(inv.args[idx] == "-n"){
                    if(idx + 1 >= inv.args.size()) throw std::runtime_error("tail -n <count> [path]");
                    take = parse_size_arg(inv.args[idx + 1], "tail count");
                    idx += 2;
                } else if(inv.args.size() - idx > 1 && is_number(inv.args[idx])){
                    take = parse_size_arg(inv.args[idx], "tail count");
                    ++idx;
                }
            }
            std::string data = idx < inv.args.size() ? vfs.read(normalize_path(cwd, inv.args[idx])) : stdin_data;
            auto lines = split_lines(data);
            size_t total = lines.lines.size();
            size_t begin = take >= total ? 0 : total - take;
            result.output = join_line_range(lines, begin, total);

        } else if(cmd == "head"){
            size_t idx = 0;
            size_t take = 10;
            auto is_number = [](const std::string& s){
                return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c){ return std::isdigit(c); });
            };
            if(idx < inv.args.size()){
                if(inv.args[idx] == "-n"){
                    if(idx + 1 >= inv.args.size()) throw std::runtime_error("head -n <count> [path]");
                    take = parse_size_arg(inv.args[idx + 1], "head count");
                    idx += 2;
                } else if(inv.args.size() - idx > 1 && is_number(inv.args[idx])){
                    take = parse_size_arg(inv.args[idx], "head count");
                    ++idx;
                }
            }
            std::string data = idx < inv.args.size() ? vfs.read(normalize_path(cwd, inv.args[idx])) : stdin_data;
            auto lines = split_lines(data);
            size_t end = std::min(take, lines.lines.size());
            result.output = join_line_range(lines, 0, end);

        } else if(cmd == "uniq"){
            std::string data = inv.args.empty() ? stdin_data : vfs.read(normalize_path(cwd, inv.args[0]));
            auto lines = split_lines(data);
            std::ostringstream oss;
            std::string prev;
            bool have_prev = false;
            for(size_t i = 0; i < lines.lines.size(); ++i){
                const auto& line = lines.lines[i];
                if(!have_prev || line != prev){
                    oss << line;
                    bool had_newline = (i < lines.lines.size() - 1) || lines.trailing_newline;
                    if(had_newline) oss << '\n';
                    prev = line;
                    have_prev = true;
                }
            }
            result.output = oss.str();

        } else if(cmd == "random"){
            long long lo = 0;
            long long hi = 1000000;
            if(inv.args.size() > 2) throw std::runtime_error("random [min [max]]");
            if(inv.args.size() == 1){
                hi = parse_int_arg(inv.args[0], "random max");
            } else if(inv.args.size() >= 2){
                lo = parse_int_arg(inv.args[0], "random min");
                hi = parse_int_arg(inv.args[1], "random max");
            }
            if(lo > hi) throw std::runtime_error("random range invalid (min > max)");
            std::uniform_int_distribution<long long> dist(lo, hi);
            long long value = dist(rng());
            result.output = std::to_string(value) + "\n";

        } else if(cmd == "echo"){
            if(inv.args.empty()) throw std::runtime_error("echo <path> <data>");
            std::string rest = join_args(inv.args, 1);
            vfs.write(normalize_path(cwd, inv.args[0]), rest);

        } else if(cmd == "rm"){
            if(inv.args.empty()) throw std::runtime_error("rm <path>");
            vfs.rm(normalize_path(cwd, inv.args[0]));

        } else if(cmd == "mv"){
            if(inv.args.size() < 2) throw std::runtime_error("mv <src> <dst>");
            vfs.mv(normalize_path(cwd, inv.args[0]), normalize_path(cwd, inv.args[1]));

        } else if(cmd == "link"){
            if(inv.args.size() < 2) throw std::runtime_error("link <src> <dst>");
            vfs.link(normalize_path(cwd, inv.args[0]), normalize_path(cwd, inv.args[1]));

        } else if(cmd == "export"){
            if(inv.args.size() < 2) throw std::runtime_error("export <vfs> <host>");
            std::string abs = normalize_path(cwd, inv.args[0]);
            std::string data = vfs.read(abs);
            std::ofstream out(inv.args[1], std::ios::binary);
            if(!out) throw std::runtime_error("export: cannot open host file");
            out.write(data.data(), static_cast<std::streamsize>(data.size()));
            std::cout << "export -> " << inv.args[1] << "\n";

        } else if(cmd == "parse"){
            if(inv.args.size() < 2) throw std::runtime_error("parse <src> <dst>");
            std::string absSrc = normalize_path(cwd, inv.args[0]);
            std::string absDst = normalize_path(cwd, inv.args[1]);
            auto text = vfs.read(absSrc);
            auto ast = parse(text);
            auto holder = std::make_shared<AstHolder>(path_basename(absDst), ast);
            std::string dir = absDst.substr(0, absDst.find_last_of('/'));
            if(dir.empty()) dir = "/";
            vfs.addNode(dir, holder);
            std::cout << "AST @ " << absDst << " valmis.\n";

        } else if(cmd == "eval"){
            if(inv.args.empty()) throw std::runtime_error("eval <path>");
            auto n = vfs.resolve(normalize_path(cwd, inv.args[0]));
            if(n->kind != VfsNode::Kind::Ast) throw std::runtime_error("not AST");
            auto a = std::dynamic_pointer_cast<AstNode>(n);
            auto val = a->eval(env);
            std::cout << val.show() << "\n";

        } else if(cmd == "ai"){
            std::string prompt = join_args(inv.args);
            if(prompt.empty()){
                std::cout << "anna promptti.\n";
                result.success = false;
            } else {
                result.output = call_ai(prompt);
            }

        } else if(cmd == "ai.brief"){
            if(inv.args.empty()) throw std::runtime_error("ai.brief <key> [extra...]");
            auto key = inv.args[0];
            std::optional<std::string> prompt;
            if(key == "ai-bridge-hello" || key == "bridge.hello" || key == "bridge-hello"){
                prompt = snippets::ai_bridge_hello_briefing();
            }

            if(!prompt || prompt->empty()){
                std::cout << "unknown briefing key\n";
                result.success = false;
            } else {
                if(inv.args.size() > 1){
                    auto extra = join_args(inv.args, 1);
                    if(!extra.empty()){
                        if(!prompt->empty() && prompt->back() != '\n') prompt->push_back(' ');
                        prompt->append(extra);
                    }
                }
                result.output = call_ai(*prompt);
            }

        } else if(cmd == "tools"){
            auto tools = snippets::tool_list();
            std::cout << tools;
            if(tools.empty() || tools.back() != '\n') std::cout << '\n';

        } else if(cmd == "cpp.tu"){
            if(inv.args.empty()) throw std::runtime_error("cpp.tu <path>");
            std::string abs = normalize_path(cwd, inv.args[0]);
            auto tu = std::make_shared<CppTranslationUnit>(path_basename(abs));
            vfs_add(vfs, abs, tu);
            std::cout << "cpp tu @ " << abs << "\n";

        } else if(cmd == "cpp.include"){
            if(inv.args.size() < 2) throw std::runtime_error("cpp.include <tu> <header> [angled]");
            auto tu = expect_tu(vfs.resolve(normalize_path(cwd, inv.args[0])));
            int angled = 0;
            if(inv.args.size() > 2){
                try{
                    angled = std::stoi(inv.args[2]);
                } catch(const std::exception&){
                    angled = 0;
                }
            }
            auto inc = std::make_shared<CppInclude>("include", inv.args[1], angled != 0);
            tu->includes.push_back(inc);
            std::cout << "+include " << inv.args[1] << "\n";

        } else if(cmd == "cpp.func"){
            if(inv.args.size() < 3) throw std::runtime_error("cpp.func <tu> <name> <ret>");
            std::string absTu = normalize_path(cwd, inv.args[0]);
            auto tu = expect_tu(vfs.resolve(absTu));
            auto fn = std::make_shared<CppFunction>(inv.args[1], inv.args[2], inv.args[1]);
            std::string fnPath = join_path(absTu, inv.args[1]);
            vfs_add(vfs, fnPath, fn);
            tu->funcs.push_back(fn);
            vfs_add(vfs, join_path(fnPath, "body"), fn->body);
            std::cout << "+func " << inv.args[1] << "\n";

        } else if(cmd == "cpp.param"){
            if(inv.args.size() < 3) throw std::runtime_error("cpp.param <fn> <type> <name>");
            auto fn = expect_fn(vfs.resolve(normalize_path(cwd, inv.args[0])));
            fn->params.push_back(CppParam{inv.args[1], inv.args[2]});
            std::cout << "+param " << inv.args[1] << " " << inv.args[2] << "\n";

        } else if(cmd == "cpp.print"){
            if(inv.args.empty()) throw std::runtime_error("cpp.print <scope> <text>");
            auto block = expect_block(vfs.resolve(normalize_path(cwd, inv.args[0])));
            std::string text = unescape_meta(join_args(inv.args, 1));
            auto s = std::make_shared<CppString>("s", text);
            auto chain = std::vector<std::shared_ptr<CppExpr>>{ s, std::make_shared<CppId>("endl", "std::endl") };
            auto coutline = std::make_shared<CppStreamOut>("cout", chain);
            block->stmts.push_back(std::make_shared<CppExprStmt>("es", coutline));
            std::cout << "+print '" << text << "'\n";

        } else if(cmd == "cpp.returni"){
            if(inv.args.size() < 2) throw std::runtime_error("cpp.returni <scope> <int>");
            auto block = expect_block(vfs.resolve(normalize_path(cwd, inv.args[0])));
            long long value = std::stoll(inv.args[1]);
            block->stmts.push_back(std::make_shared<CppReturn>("ret", std::make_shared<CppInt>("i", value)));
            std::cout << "+return " << value << "\n";

        } else if(cmd == "cpp.return"){
            if(inv.args.empty()) throw std::runtime_error("cpp.return <scope> [expr]");
            auto block = expect_block(vfs.resolve(normalize_path(cwd, inv.args[0])));
            std::string trimmed = unescape_meta(trim_copy(join_args(inv.args, 1)));
            std::shared_ptr<CppExpr> expr;
            if(!trimmed.empty()) expr = std::make_shared<CppRawExpr>("rexpr", trimmed);
            block->stmts.push_back(std::make_shared<CppReturn>("ret", expr));
            std::cout << "+return expr\n";

        } else if(cmd == "cpp.expr"){
            if(inv.args.empty()) throw std::runtime_error("cpp.expr <scope> <expr>");
            auto block = expect_block(vfs.resolve(normalize_path(cwd, inv.args[0])));
            block->stmts.push_back(std::make_shared<CppExprStmt>("expr", std::make_shared<CppRawExpr>("rexpr", unescape_meta(join_args(inv.args, 1)))));
            std::cout << "+expr " << inv.args[0] << "\n";

        } else if(cmd == "cpp.vardecl"){
            if(inv.args.size() < 3) throw std::runtime_error("cpp.vardecl <scope> <type> <name> [init]");
            auto block = expect_block(vfs.resolve(normalize_path(cwd, inv.args[0])));
            std::string init = unescape_meta(trim_copy(join_args(inv.args, 3)));
            bool hasInit = !init.empty();
            block->stmts.push_back(std::make_shared<CppVarDecl>("var", inv.args[1], inv.args[2], init, hasInit));
            std::cout << "+vardecl " << inv.args[1] << " " << inv.args[2] << "\n";

        } else if(cmd == "cpp.stmt"){
            if(inv.args.empty()) throw std::runtime_error("cpp.stmt <scope> <stmt>");
            auto block = expect_block(vfs.resolve(normalize_path(cwd, inv.args[0])));
            block->stmts.push_back(std::make_shared<CppRawStmt>("stmt", unescape_meta(join_args(inv.args, 1))));
            std::cout << "+stmt " << inv.args[0] << "\n";

        } else if(cmd == "cpp.rangefor"){
            if(inv.args.size() < 2) throw std::runtime_error("cpp.rangefor <scope> <loop> decl | range");
            std::string rest = trim_copy(join_args(inv.args, 2));
            auto bar = rest.find('|');
            if(bar == std::string::npos) throw std::runtime_error("cpp.rangefor expects 'decl | range'");
            std::string decl = unescape_meta(trim_copy(rest.substr(0, bar)));
            std::string range = unescape_meta(trim_copy(rest.substr(bar + 1)));
            if(decl.empty() || range.empty()) throw std::runtime_error("cpp.rangefor missing decl or range");
            std::string absScope = normalize_path(cwd, inv.args[0]);
            auto block = expect_block(vfs.resolve(absScope));
            auto loop = std::make_shared<CppRangeFor>(inv.args[1], decl, range);
            block->stmts.push_back(loop);
            std::string loopPath = join_path(absScope, inv.args[1]);
            vfs_add(vfs, loopPath, loop);
            vfs_add(vfs, join_path(loopPath, "body"), loop->body);
            std::cout << "+rangefor " << inv.args[1] << "\n";

        } else if(cmd == "cpp.dump"){
            if(inv.args.size() < 2) throw std::runtime_error("cpp.dump <tu> <out>");
            std::string absTu = normalize_path(cwd, inv.args[0]);
            std::string absOut = normalize_path(cwd, inv.args[1]);
            cpp_dump_to_vfs(vfs, absTu, absOut);
            std::cout << "dump -> " << absOut << "\n";

        } else if(cmd == "help"){
            help();

        } else if(cmd == "quit" || cmd == "exit"){
            result.exit_requested = true;

        } else if(cmd.empty()){
            // nothing

        } else {
            std::cout << "tuntematon komento. 'help' kertoo karun totuuden.\n";
            result.success = false;
        }

        result.output += capture.str();
        return result;
    };

    auto run_pipeline = [&](const CommandPipeline& pipeline, const std::string& initial_input) -> CommandResult {
        if(pipeline.commands.empty()) return CommandResult{};
        CommandResult last;
        std::string next_input = initial_input;
        for(size_t i = 0; i < pipeline.commands.size(); ++i){
            last = execute_single(pipeline.commands[i], next_input);
            if(last.exit_requested) return last;
            next_input = last.output;
        }
        return last;
    };

    while(true){
        TRACE_LOOP("repl.iter", std::string("iter=") + std::to_string(repl_iter));
        ++repl_iter;
        if(interactive){
            std::cout << "> " << std::flush;
        }
        if(!std::getline(*input, line)){
            if(script_active && fallback_after_script){
                script_active = false;
                fallback_after_script = false;
                scriptStream.reset();
                input = &std::cin;
                interactive = true;
                if(!std::cin.good()) std::cin.clear();
                continue;
            }
            break;
        }

        if(line.empty()) continue;
        try{
            auto tokens = tokenize_command_line(line);
            if(tokens.empty()) continue;
            auto chain = parse_command_chain(tokens);
            bool exit_requested = false;
            bool last_success = true;
            for(const auto& entry : chain){
                if(entry.logical == "&&" && !last_success) continue;
                if(entry.logical == "||" && last_success) continue;
                CommandResult res = run_pipeline(entry.pipeline, "");
                if(!res.output.empty()){
                    std::cout << res.output;
                    std::cout.flush();
                }
                last_success = res.success;
                if(res.exit_requested){
                    exit_requested = true;
                    break;
                }
            }
            if(exit_requested) break;
        } catch(const std::exception& e){
            std::cout << "error: " << e.what() << "\n";
        }
    }
    return 0;
}
