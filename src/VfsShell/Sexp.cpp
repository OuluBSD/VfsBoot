#include "VfsShell.h"

// ====== SexpValue::show ======
std::string SexpValue::show() const {
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
AstIf::AstIf(std::string n, Shared<AstNode> C, Shared<AstNode> A, Shared<AstNode> B)
    : AstNode(std::move(n)), c(std::move(C)), a(std::move(A)), b(std::move(B)) { kind=Kind::Ast; }
AstLambda::AstLambda(std::string n, std::vector<std::string> ps, Shared<AstNode> b)
    : AstNode(std::move(n)), params(std::move(ps)), body(std::move(b)){ kind=Kind::Ast; }
AstCall::AstCall(std::string n, Shared<AstNode> f, std::vector<Shared<AstNode>> a)
    : AstNode(std::move(n)), fn(std::move(f)), args(std::move(a)){ kind=Kind::Ast; }
AstHolder::AstHolder(std::string n, Shared<AstNode> in)
    : AstNode(std::move(n)), inner(std::move(in)){ kind=Kind::Ast; }

// ====== AST eval ======
SexpValue AstInt::eval(Shared<Env>) { return SexpValue::I(val); }
SexpValue AstBool::eval(Shared<Env>) { return SexpValue::B(val); }
SexpValue AstStr::eval(Shared<Env>) { return SexpValue::S(val); }
SexpValue AstSym::eval(Shared<Env> e) {
    auto v = e->get(id); if(!v) throw std::runtime_error("unbound "+id); return *v;
}
SexpValue AstIf::eval(Shared<Env> e){
    auto cv=c->eval(e);
    bool t = std::visit([](auto&& x)->bool{
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<T,bool>)        return x;
        if constexpr (std::is_same_v<T,int64_t>)     return x!=0;
        if constexpr (std::is_same_v<T,std::string>) return !x.empty();
        if constexpr (std::is_same_v<T,SexpValue::List>) return !x.empty();
        else return true;
    }, cv.v);
    return (t? a: b)->eval(e);
}
SexpValue AstLambda::eval(Shared<Env> e){
    return SexpValue::Clo(SexpValue::Closure{params, body, e});
}
SexpValue AstCall::eval(Shared<Env> e){
    SexpValue f = fn->eval(e);
    std::vector<SexpValue> av; av.reserve(args.size());
    for (auto& a: args) av.push_back(a->eval(e));
    if (std::holds_alternative<SexpValue::Builtin>(f.v)) {
        auto& b = std::get<SexpValue::Builtin>(f.v);
        return b(av, e);
    } else if (std::holds_alternative<SexpValue::Closure>(f.v)) {
        auto clo = std::get<SexpValue::Closure>(f.v);
        if (clo.params.size()!=av.size()) throw std::runtime_error("arity mismatch");
        auto child = Shared<Env>(new Env(clo.env));
        for (size_t i=0;i<av.size();++i) child->set(clo.params[i], av[i]);
        return clo.body->eval(child);
    }
    throw std::runtime_error("call of non-function");
}
SexpValue AstHolder::eval(Shared<Env> e){ return inner->eval(e); }

// ====== Parser ======
static size_t POS;
static Shared<AstNode> parseExpr(const std::vector<Token>& T);

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

static Shared<AstNode> atom(const std::string& s){
    if (s=="#t") return Shared<AstNode>(new AstBool("<b>", true));
    if (s=="#f") return Shared<AstNode>(new AstBool("<b>", false));
    if (s.size()>=2 && s.front()=='"' && s.back()=='"')
        return Shared<AstNode>(new AstStr("<s>", s.substr(1, s.size()-2)));
    if (isInt(s)) return Shared<AstNode>(new AstInt("<i>", std::stoll(s)));
    return Shared<AstNode>(new AstSym("<sym>", s));
}

static Shared<AstNode> parseList(const std::vector<Token>& T){
    if (POS>=T.size() || T[POS].s!="(") throw std::runtime_error("expected (");
    ++POS;
    if (POS<T.size() && T[POS].s==")"){ ++POS; return Shared<AstNode>(new AstStr("<s>","")); }
    auto head = parseExpr(T);
    auto sym  = dynamic_cast<AstSym*>(head.get());  // Note: dynamic_pointer_cast not available with U++ Shared, using raw pointer method
    std::vector<Shared<AstNode>> items;
    while (POS<T.size() && T[POS].s!=")") items.push_back(parseExpr(T));
    if (POS>=T.size()) throw std::runtime_error("missing )");
    ++POS;

    std::string H = sym? sym->id : std::string();
    if (H=="if"){
        if (items.size()!=3) throw std::runtime_error("if needs 3 args");
        return Shared<AstNode>(new AstIf("<if>", items[0], items[1], items[2]));
    }
    if (H=="lambda"){
        if (items.size()<2) throw std::runtime_error("lambda needs params and body");
        // proto: single param only: (lambda x body)
        std::vector<std::string> ps;
        if (auto sp=dynamic_cast<AstSym*>(items[0].get())) ps.push_back(sp->id);  // Note: dynamic_pointer_cast not available with U++ Shared
        else throw std::runtime_error("lambda single param only");
        auto body=items.back();
        return Shared<AstNode>(new AstLambda("<lam>", ps, body));
    }
    // generic call
    return Shared<AstNode>(new AstCall("<call>", head, items));
}

static Shared<AstNode> parseExpr(const std::vector<Token>& T){
    if (POS>=T.size()) throw std::runtime_error("unexpected EOF");
    auto s = T[POS].s;
    if (s=="(") return parseList(T);
    if (s==")") throw std::runtime_error("unexpected )");
    ++POS;
    return atom(s);
}

Shared<AstNode> parse(const std::string& src){
    POS=0; auto T = lex(src);
    auto n = parseExpr(T);
    if (POS!=T.size()) throw std::runtime_error("extra tokens");
    return n;
}

// ====== Builtins ======
void install_builtins(Shared<Env> g){
    auto wrap = [&](auto op){
        return SexpValue::Built([op](std::vector<SexpValue>& av, Shared<Env>){
            if (av.size()<2) throw std::runtime_error("need at least 2 args");
            auto gi=[](const SexpValue& v)->int64_t{
                if (!std::holds_alternative<int64_t>(v.v)) throw std::runtime_error("int expected");
                return std::get<int64_t>(v.v);
            };
            int64_t acc = gi(av[0]);
            for (size_t i=1;i<av.size();++i) acc = op(acc, gi(av[i]));
            return SexpValue::I(acc);
        });
    };
    g->set("+", wrap(std::plus<int64_t>()));
    g->set("-", wrap(std::minus<int64_t>()));
    g->set("*", wrap(std::multiplies<int64_t>()));

    g->set("=", SexpValue::Built([](std::vector<SexpValue>& av, Shared<Env>){
        if (av.size()!=2) throw std::runtime_error("= needs 2 args");
        return SexpValue::B(av[0].show()==av[1].show());
    }));
    g->set("<", SexpValue::Built([](std::vector<SexpValue>& av, Shared<Env>){
        if (av.size()!=2) throw std::runtime_error("< needs 2 args");
        if (!std::holds_alternative<int64_t>(av[0].v) || !std::holds_alternative<int64_t>(av[1].v))
            throw std::runtime_error("int expected");
        return SexpValue::B(std::get<int64_t>(av[0].v) < std::get<int64_t>(av[1].v));
    }));
    g->set("print", SexpValue::Built([](std::vector<SexpValue>& av, Shared<Env>){
        for (size_t i=0;i<av.size();++i){ if(i) std::cout<<" "; std::cout<<av[i].show(); }
        std::cout<<"\n"; return av.empty()? SexpValue() : av.back();
    }));

    // lists
    g->set("list", SexpValue::Built([](std::vector<SexpValue>& av, Shared<Env>){ return SexpValue::L(av); }));
    g->set("cons", SexpValue::Built([](std::vector<SexpValue>& av, Shared<Env>){
        if (av.size()!=2) throw std::runtime_error("cons x xs");
        if (!std::holds_alternative<SexpValue::List>(av[1].v)) throw std::runtime_error("cons expects list");
        SexpValue::List out; const auto& xs = std::get<SexpValue::List>(av[1].v);
        out.reserve(xs.size()+1); out.push_back(av[0]); out.insert(out.end(), xs.begin(), xs.end());
        return SexpValue::L(out);
    }));
    g->set("head", SexpValue::Built([](std::vector<SexpValue>& av, Shared<Env>){
        if (av.size()!=1 || !std::holds_alternative<SexpValue::List>(av[0].v)) throw std::runtime_error("head xs");
        const auto& xs = std::get<SexpValue::List>(av[0].v); if(xs.empty()) throw std::runtime_error("head of empty");
        return xs.front();
    }));
    g->set("tail", SexpValue::Built([](std::vector<SexpValue>& av, Shared<Env>){
        if (av.size()!=1 || !std::holds_alternative<SexpValue::List>(av[0].v)) throw std::runtime_error("tail xs");
        const auto& xs = std::get<SexpValue::List>(av[0].v); if(xs.empty()) throw std::runtime_error("tail of empty");
        return SexpValue::L(SexpValue::List(xs.begin()+1, xs.end()));
    }));
    g->set("null?", SexpValue::Built([](std::vector<SexpValue>& av, Shared<Env>){
        if (av.size()!=1) throw std::runtime_error("null? xs");
        return SexpValue::B(std::holds_alternative<SexpValue::List>(av[0].v) && std::get<SexpValue::List>(av[0].v).empty());
    }));

    // strings
    g->set("str.cat", SexpValue::Built([](std::vector<SexpValue>& av, Shared<Env>){
        std::string s; for(auto& v: av){ if(!std::holds_alternative<std::string>(v.v)) throw std::runtime_error("str.cat expects strings"); s += std::get<std::string>(v.v); }
        return SexpValue::S(s);
    }));
    g->set("str.sub", SexpValue::Built([](std::vector<SexpValue>& av, Shared<Env>){
        if(av.size()!=3) throw std::runtime_error("str.sub s start len");
        if(!std::holds_alternative<std::string>(av[0].v) || !std::holds_alternative<int64_t>(av[1].v) || !std::holds_alternative<int64_t>(av[2].v))
            throw std::runtime_error("str.sub types");
        const std::string& s = std::get<std::string>(av[0].v);
        size_t st = static_cast<size_t>(std::max<int64_t>(0, std::get<int64_t>(av[1].v)));
        size_t ln = static_cast<size_t>(std::max<int64_t>(0, std::get<int64_t>(av[2].v)));
        if (st > s.size()) return SexpValue::S("");
        return SexpValue::S(s.substr(st, ln));
    }));
    g->set("str.find", SexpValue::Built([](std::vector<SexpValue>& av, Shared<Env>){
        if(av.size()!=2 || !std::holds_alternative<std::string>(av[0].v) || !std::holds_alternative<std::string>(av[1].v))
            throw std::runtime_error("str.find s sub");
        auto pos = std::get<std::string>(av[0].v).find(std::get<std::string>(av[1].v));
        return SexpValue::I(pos==std::string::npos? -1 : static_cast<int64_t>(pos));
    }));

    // VFS helpers
    g->set("vfs-write", SexpValue::Built([](std::vector<SexpValue>& av, Shared<Env>){
        if(!G_VFS) throw std::runtime_error("no vfs");
        if(av.size()!=2 || !std::holds_alternative<std::string>(av[0].v) || !std::holds_alternative<std::string>(av[1].v))
            throw std::runtime_error("vfs-write path string");
        G_VFS->write(std::get<std::string>(av[0].v), std::get<std::string>(av[1].v), 0);
        return av[0];
    }));
    g->set("vfs-read", SexpValue::Built([](std::vector<SexpValue>& av, Shared<Env>){
        if(!G_VFS) throw std::runtime_error("no vfs");
        if(av.size()!=1 || !std::holds_alternative<std::string>(av[0].v))
            throw std::runtime_error("vfs-read path");
        return SexpValue::S(G_VFS->read(std::get<std::string>(av[0].v), 0));
    }));
    g->set("vfs-ls", SexpValue::Built([](std::vector<SexpValue>& av, Shared<Env>){
        if(!G_VFS) throw std::runtime_error("no vfs");
        if(av.size()!=1 || !std::holds_alternative<std::string>(av[0].v))
            throw std::runtime_error("vfs-ls \"/path\"");
        auto n = G_VFS->resolveForOverlay(std::get<std::string>(av[0].v), 0);
        if(!n->isDir()) throw std::runtime_error("vfs-ls: not dir");
        SexpValue::List entries;
        for(auto& kv : n->children()){
            const std::string& name = kv.first;
            auto& node = kv.second;
            std::string t = node->kind==VfsNode::Kind::Dir? "dir" : (node->kind==VfsNode::Kind::File? "file" : "ast");
            entries.push_back(SexpValue::L(SexpValue::List{ SexpValue::S(name), SexpValue::S(t) }));
        }
        return SexpValue::L(entries);
    }));

    // export & sys
    g->set("export", SexpValue::Built([](std::vector<SexpValue>& av, Shared<Env>){
        if(!G_VFS) throw std::runtime_error("no vfs");
        if(av.size()!=2 || !std::holds_alternative<std::string>(av[0].v) || !std::holds_alternative<std::string>(av[1].v))
            throw std::runtime_error("export vfs host");
        std::string data = G_VFS->read(std::get<std::string>(av[0].v), 0);
        std::string host = std::get<std::string>(av[1].v);
        std::ofstream f(host, std::ios::binary);
        if(!f) throw std::runtime_error("export: cannot open host file");
        f.write(data.data(), static_cast<std::streamsize>(data.size()));
        return SexpValue::S(host);
    }));
    g->set("sys", SexpValue::Built([](std::vector<SexpValue>& av, Shared<Env>){
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
        return SexpValue::S(out);
    }));

    // C++ apuri: hello-koodi
    g->set("cpp:hello", SexpValue::Built([](std::vector<SexpValue>&, Shared<Env>){
        std::string code = "#include <iostream>\nint main(){ std::cout<<\"Hello, world!\\n\"; return 0; }\n";
        return SexpValue::S(code);
    }));
}

