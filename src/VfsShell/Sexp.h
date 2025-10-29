#ifndef _VfsShell_Sexp_h_
#define _VfsShell_Sexp_h_

#include <Core/Core.h>
#include <VfsCore/VfsCore.h>  // For VfsNode

// Missing standard library includes
#include <variant>
#include <optional>

using namespace Upp;


struct Env; // forward

//
// Arvot S-kielelle
//
struct SexpValue {
    struct Closure {
        std::vector<std::string> params;
        std::shared_ptr<struct AstNode> body;
        std::shared_ptr<Env> env;
    };
    using Builtin = std::function<SexpValue(std::vector<SexpValue>&, std::shared_ptr<Env>)>;
    using List = std::vector<SexpValue>;

    std::variant<int64_t, bool, std::string, Builtin, Closure, List> v;
    SexpValue() : v(int64_t(0)) {}
    static SexpValue I(int64_t x) { SexpValue r; r.v = x; return r; }
    static SexpValue B(bool b)    { SexpValue r; r.v = b; return r; }
    static SexpValue S(std::string s){ SexpValue r; r.v = std::move(s); return r; }
    static SexpValue Built(Builtin f){ SexpValue r; r.v = std::move(f); return r; }
    static SexpValue Clo(Closure c){ SexpValue r; r.v = std::move(c); return r; }
    static SexpValue L(List xs){ SexpValue r; r.v = std::move(xs); return r; }

    std::string show() const;
};

//
// S-AST (perii VfsNode)
//
struct AstNode : VfsNode {
    using VfsNode::VfsNode;
    AstNode(std::string n) : VfsNode(Kind::Ast, String(n.c_str())) {}
    AstNode(String n) : VfsNode(Kind::Ast, n) {}
    virtual SexpValue eval(std::shared_ptr<Env>) = 0;
};

struct AstInt   : AstNode { int64_t val; AstInt(std::string n, int64_t v); SexpValue eval(std::shared_ptr<Env>) override; };
struct AstBool  : AstNode { bool val;    AstBool(std::string n, bool v);   SexpValue eval(std::shared_ptr<Env>) override; };
struct AstStr   : AstNode { std::string val; AstStr(std::string n, std::string v); SexpValue eval(std::shared_ptr<Env>) override; };
struct AstSym   : AstNode { std::string id; AstSym(std::string n, std::string s);   SexpValue eval(std::shared_ptr<Env>) override; };
struct AstIf    : AstNode {
    std::shared_ptr<AstNode> c,a,b;
    AstIf(std::string n, std::shared_ptr<AstNode> C, std::shared_ptr<AstNode> A, std::shared_ptr<AstNode> B);
    SexpValue eval(std::shared_ptr<Env>) override;
};
struct AstLambda: AstNode {
    std::vector<std::string> params;
    std::shared_ptr<AstNode> body;
    AstLambda(std::string n, std::vector<std::string> ps, std::shared_ptr<AstNode> b);
    SexpValue eval(std::shared_ptr<Env>) override;
};
struct AstCall  : AstNode {
    std::shared_ptr<AstNode> fn;
    std::vector<std::shared_ptr<AstNode>> args;
    AstCall(std::string n, std::shared_ptr<AstNode> f, std::vector<std::shared_ptr<AstNode>> a);
    SexpValue eval(std::shared_ptr<Env>) override;
};
struct AstHolder: AstNode {
    std::shared_ptr<AstNode> inner;
    AstHolder(std::string n, std::shared_ptr<AstNode> in);
    SexpValue eval(std::shared_ptr<Env>) override;
};

//
// Ympäristö
//
struct Env : std::enable_shared_from_this<Env> {
    VectorMap<String, SexpValue> tbl;
    std::shared_ptr<Env> up;
    explicit Env(std::shared_ptr<Env> p = nullptr) : up(std::move(p)) {}
    void set(const std::string& k, const SexpValue& v) { tbl.GetAdd(String(k.c_str())) = v; }
    std::optional<SexpValue> get(const std::string& k){
        int pos = tbl.Find(String(k.c_str()));
        if(pos >= 0) return tbl[pos];
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
#endif
