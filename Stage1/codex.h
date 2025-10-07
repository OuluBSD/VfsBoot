#ifndef _Stage1_codex_h_
#define _Stage1_codex_h_

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

#define TRACE_FN(...) auto CODEX_TRACE_CAT(_codex_trace_scope_, __LINE__) = ::codex_trace::Scope(__func__, ::codex_trace::concat(__VA_ARGS__))
#define TRACE_MSG(...) ::codex_trace::log_line(::codex_trace::concat(__VA_ARGS__))
#define TRACE_LOOP(tag, ...) ::codex_trace::log_loop(tag, ::codex_trace::concat(__VA_ARGS__))
#else
#define TRACE_FN(...) (void)0
#define TRACE_MSG(...) (void)0
#define TRACE_LOOP(...) (void)0
#endif

//
// VFS perus
//
struct VfsNode : std::enable_shared_from_this<VfsNode> {
    enum class Kind { Dir, File, Ast };
    std::string name;
    std::weak_ptr<VfsNode> parent;
    Kind kind;
    explicit VfsNode(std::string n, Kind k) : name(std::move(n)), kind(k) {}
    virtual ~VfsNode() = default;
    virtual bool isDir() const { return kind == Kind::Dir; }
    virtual std::string read() const { return ""; }
    virtual void write(const std::string&) {}
    virtual std::map<std::string, std::shared_ptr<VfsNode>>& children() {
        static std::map<std::string, std::shared_ptr<VfsNode>> empty;
        return empty;
    }
};

struct DirNode : VfsNode {
    std::map<std::string, std::shared_ptr<VfsNode>> ch;
    explicit DirNode(std::string n) : VfsNode(std::move(n), Kind::Dir) {}
    bool isDir() const override { return true; }
    std::map<std::string, std::shared_ptr<VfsNode>>& children() override { return ch; }
};

struct FileNode : VfsNode {
    std::string content;
    FileNode(std::string n, std::string c = "")
        : VfsNode(std::move(n), Kind::File), content(std::move(c)) {}
    std::string read() const override { return content; }
    void write(const std::string& s) override { const_cast<std::string&>(content) = s; }
};

struct Env; // forward

//
// Arvot S-kielelle
//
struct Value {
    struct Closure {
        std::vector<std::string> params;
        std::shared_ptr<struct AstNode> body;
        std::shared_ptr<Env> env;
    };
    using Builtin = std::function<Value(std::vector<Value>&, std::shared_ptr<Env>)>;
    using List = std::vector<Value>;

    std::variant<int64_t, bool, std::string, Builtin, Closure, List> v;
    Value() : v(int64_t(0)) {}
    static Value I(int64_t x) { Value r; r.v = x; return r; }
    static Value B(bool b)    { Value r; r.v = b; return r; }
    static Value S(std::string s){ Value r; r.v = std::move(s); return r; }
    static Value Built(Builtin f){ Value r; r.v = std::move(f); return r; }
    static Value Clo(Closure c){ Value r; r.v = std::move(c); return r; }
    static Value L(List xs){ Value r; r.v = std::move(xs); return r; }

    std::string show() const;
};

//
// S-AST (perii VfsNode)
//
struct AstNode : VfsNode {
    using VfsNode::VfsNode;
    AstNode(std::string n) : VfsNode(std::move(n), Kind::Ast) {}
    virtual Value eval(std::shared_ptr<Env>) = 0;
};

struct AstInt   : AstNode { int64_t val; AstInt(std::string n, int64_t v); Value eval(std::shared_ptr<Env>) override; };
struct AstBool  : AstNode { bool val;    AstBool(std::string n, bool v);   Value eval(std::shared_ptr<Env>) override; };
struct AstStr   : AstNode { std::string val; AstStr(std::string n, std::string v); Value eval(std::shared_ptr<Env>) override; };
struct AstSym   : AstNode { std::string id; AstSym(std::string n, std::string s);   Value eval(std::shared_ptr<Env>) override; };
struct AstIf    : AstNode {
    std::shared_ptr<AstNode> c,a,b;
    AstIf(std::string n, std::shared_ptr<AstNode> C, std::shared_ptr<AstNode> A, std::shared_ptr<AstNode> B);
    Value eval(std::shared_ptr<Env>) override;
};
struct AstLambda: AstNode {
    std::vector<std::string> params;
    std::shared_ptr<AstNode> body;
    AstLambda(std::string n, std::vector<std::string> ps, std::shared_ptr<AstNode> b);
    Value eval(std::shared_ptr<Env>) override;
};
struct AstCall  : AstNode {
    std::shared_ptr<AstNode> fn;
    std::vector<std::shared_ptr<AstNode>> args;
    AstCall(std::string n, std::shared_ptr<AstNode> f, std::vector<std::shared_ptr<AstNode>> a);
    Value eval(std::shared_ptr<Env>) override;
};
struct AstHolder: AstNode {
    std::shared_ptr<AstNode> inner;
    AstHolder(std::string n, std::shared_ptr<AstNode> in);
    Value eval(std::shared_ptr<Env>) override;
};

//
// Ympäristö
//
struct Env : std::enable_shared_from_this<Env> {
    std::map<std::string, Value> tbl;
    std::shared_ptr<Env> up;
    explicit Env(std::shared_ptr<Env> p = nullptr) : up(std::move(p)) {}
    void set(const std::string& k, const Value& v) { tbl[k]=v; }
    std::optional<Value> get(const std::string& k){
        auto it = tbl.find(k);
        if(it!=tbl.end()) return it->second;
        if(up) return up->get(k);
        return std::nullopt;
    }
};

//
// VFS
//
struct Vfs {
    std::shared_ptr<DirNode> root = std::make_shared<DirNode>("/");
    Vfs();

    static std::vector<std::string> splitPath(const std::string& p);
    std::shared_ptr<VfsNode> resolve(const std::string& path);
    std::shared_ptr<DirNode> ensureDir(const std::string& path);

    void mkdir(const std::string& p);
    void touch(const std::string& p);
    void write(const std::string& p, const std::string& data);
    std::string read(const std::string& p);
    void addNode(const std::string& dirpath, std::shared_ptr<VfsNode> n);
    void rm(const std::string& p);
    void mv(const std::string& src, const std::string& dst);
    void link(const std::string& src, const std::string& dst);
    void ls(const std::string& p);
    void tree(std::shared_ptr<VfsNode> n = nullptr, std::string pref = "");
};

extern Vfs* G_VFS; // glob aputinta varten

//
// Parseri (S-expr)
//
struct Token { std::string s; };
std::vector<Token> lex(const std::string& src);
std::shared_ptr<AstNode> parse(const std::string& src);

//
// Builtins
//
void install_builtins(std::shared_ptr<Env> g);

//
// Exec util
//
std::string exec_capture(const std::string& cmd, const std::string& desc = {});
bool has_cmd(const std::string& c);

//
// C++ AST (perii AstNode → VfsNode)
//
struct CppNode : AstNode {
    using AstNode::AstNode;
    virtual std::string dump(int indent = 0) const = 0;
    static std::string ind(int n) { return std::string(n, ' '); }
    Value eval(std::shared_ptr<Env>) override { return Value::S(dump()); }
};

struct CppInclude : CppNode {
    std::string header; bool angled;
    CppInclude(std::string n, std::string h, bool a);
    std::string dump(int) const override;
};

struct CppExpr : CppNode { using CppNode::CppNode; };
struct CppId   : CppExpr { std::string id; CppId(std::string n, std::string i); std::string dump(int) const override; };
struct CppString : CppExpr {
    std::string s; CppString(std::string n, std::string v);
    static std::string esc(const std::string& x);
    std::string dump(int) const override;
};
struct CppInt  : CppExpr { long long v; CppInt(std::string n, long long x); std::string dump(int) const override; };

struct CppCall : CppExpr {
    std::shared_ptr<CppExpr> fn; std::vector<std::shared_ptr<CppExpr>> args;
    CppCall(std::string n, std::shared_ptr<CppExpr> f, std::vector<std::shared_ptr<CppExpr>> a);
    std::string dump(int) const override;
};

struct CppBinOp : CppExpr {
    std::string op; std::shared_ptr<CppExpr> a,b;
    CppBinOp(std::string n, std::string o, std::shared_ptr<CppExpr> A, std::shared_ptr<CppExpr> B);
    std::string dump(int) const override;
};

struct CppStreamOut : CppExpr {
    std::vector<std::shared_ptr<CppExpr>> chain;
    CppStreamOut(std::string n, std::vector<std::shared_ptr<CppExpr>> xs);
    std::string dump(int) const override;
};

struct CppRawExpr : CppExpr {
    std::string text;
    CppRawExpr(std::string n, std::string t);
    std::string dump(int) const override;
};

struct CppStmt : CppNode { using CppNode::CppNode; };
struct CppExprStmt : CppStmt {
    std::shared_ptr<CppExpr> e;
    CppExprStmt(std::string n, std::shared_ptr<CppExpr> E);
    std::string dump(int indent) const override;
};
struct CppReturn : CppStmt {
    std::shared_ptr<CppExpr> e;
    CppReturn(std::string n, std::shared_ptr<CppExpr> E);
    std::string dump(int indent) const override;
};
struct CppRawStmt : CppStmt {
    std::string text;
    CppRawStmt(std::string n, std::string t);
    std::string dump(int indent) const override;
};
struct CppVarDecl : CppStmt {
    std::string type;
    std::string name;
    std::string init;
    bool hasInit;
    CppVarDecl(std::string n, std::string ty, std::string nm, std::string in, bool has);
    std::string dump(int indent) const override;
};
struct CppCompound : CppStmt {
    std::vector<std::shared_ptr<CppStmt>> stmts;
    explicit CppCompound(std::string n);
    std::map<std::string, std::shared_ptr<VfsNode>> ch;
    bool isDir() const override { return true; }
    std::map<std::string, std::shared_ptr<VfsNode>>& children() override { return ch; }
    std::string dump(int indent) const override;
};
struct CppParam { std::string type, name; };
struct CppFunction : CppNode {
    std::string retType, name;
    std::vector<CppParam> params;
    std::shared_ptr<CppCompound> body;
    std::map<std::string, std::shared_ptr<VfsNode>> ch;
    CppFunction(std::string n, std::string rt, std::string nm);
    bool isDir() const override { return true; }
    std::map<std::string, std::shared_ptr<VfsNode>>& children() override { return ch; }
    std::string dump(int indent) const override;
};
struct CppRangeFor : CppStmt {
    std::string decl;
    std::string range;
    std::shared_ptr<CppCompound> body;
    std::map<std::string, std::shared_ptr<VfsNode>> ch;
    CppRangeFor(std::string n, std::string d, std::string r);
    bool isDir() const override { return true; }
    std::map<std::string, std::shared_ptr<VfsNode>>& children() override { return ch; }
    std::string dump(int indent) const override;
};
struct CppTranslationUnit : CppNode {
    std::vector<std::shared_ptr<CppInclude>> includes;
    std::vector<std::shared_ptr<CppFunction>> funcs;
    std::map<std::string, std::shared_ptr<VfsNode>> ch;
    explicit CppTranslationUnit(std::string n);
    bool isDir() const override { return true; }
    std::map<std::string, std::shared_ptr<VfsNode>>& children() override { return ch; }
    std::string dump(int) const override;
};

// helpers
std::shared_ptr<CppTranslationUnit> expect_tu(std::shared_ptr<VfsNode> n);
std::shared_ptr<CppFunction>        expect_fn(std::shared_ptr<VfsNode> n);
std::shared_ptr<CppCompound>        expect_block(std::shared_ptr<VfsNode> n);
void vfs_add(Vfs& vfs, const std::string& path, std::shared_ptr<VfsNode> node);
void cpp_dump_to_vfs(Vfs& vfs, const std::string& tuPath, const std::string& filePath);

//
// AI helpers (OpenAI + llama.cpp bridge)
//
std::string tool_list_text();
std::string json_escape(const std::string& s);
std::string build_responses_payload(const std::string& model, const std::string& user_prompt);
std::string call_openai(const std::string& prompt);
std::string call_llama(const std::string& prompt);
std::string call_ai(const std::string& prompt);

#endif
