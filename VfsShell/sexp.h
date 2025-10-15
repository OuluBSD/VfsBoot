#pragma once


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
// Parseri (S-expr)
//
struct Token { std::string s; };
std::vector<Token> lex(const std::string& src);
std::shared_ptr<AstNode> parse(const std::string& src);

//
// Builtins
//
void install_builtins(std::shared_ptr<Env> g);
