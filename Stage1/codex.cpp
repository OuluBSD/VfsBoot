#include "codex.h"
#include <fstream>
#include <sstream>
#include <sys/types.h>
#include <unistd.h>

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
    std::vector<std::string> parts; std::string cur;
    for(char c: p){ if(c=='/'){ if(!cur.empty()){ parts.push_back(cur); cur.clear(); } } else cur.push_back(c); }
    if(!cur.empty()) parts.push_back(cur);
    return parts;
}
std::shared_ptr<VfsNode> Vfs::resolve(const std::string& path){
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
void Vfs::mkdir(const std::string& path){ ensureDir(path); }
void Vfs::touch(const std::string& path){
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
std::string Vfs::read(const std::string& path){ return resolve(path)->read(); }
void Vfs::addNode(const std::string& dirpath, std::shared_ptr<VfsNode> n){
    auto d = ensureDir(dirpath);
    n->parent=d;
    d->children()[n->name]=n;
}
void Vfs::rm(const std::string& path){
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
    auto s = resolve(src);
    auto parts = splitPath(dst);
    std::string name=parts.back(); parts.pop_back();
    std::string dir="/";
    for (auto& x: parts) dir += (dir.size()>1?"/":"") + x;
    auto d = ensureDir(dir);
    d->children()[name]=s; // alias
}
void Vfs::ls(const std::string& p){
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
std::string exec_capture(const std::string& cmd){
    std::array<char, 4096> buf{};
    std::string out;
    FILE* pipe = popen(cmd.c_str(), "r");
    if(!pipe) return out;
    while(true){
        size_t n = fread(buf.data(), 1, buf.size(), pipe);
        if(n>0) out.append(buf.data(), n);
        if(n<buf.size()) break;
    }
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
CppString::CppString(std::string n, std::string v) : CppExpr(std::move(n)), s(std::move(v)) { kind=Kind::Ast; }
std::string CppString::esc(const std::string& x){
    std::string o; o.reserve(x.size()+8);
    for(char c: x){
        if(c=='"'||c=='\\') { o.push_back('\\'); o.push_back(c); }
        else if(c=='\n') o += "\\n";
        else o.push_back(c);
    }
    return o;
}
std::string CppString::dump(int) const { return std::string("\"")+esc(s)+"\""; }
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
CppExprStmt::CppExprStmt(std::string n, std::shared_ptr<CppExpr> E)
    : CppStmt(std::move(n)), e(std::move(E)) { kind=Kind::Ast; }
std::string CppExprStmt::dump(int indent) const { return ind(indent)+e->dump()+";\n"; }
CppReturn::CppReturn(std::string n, std::shared_ptr<CppExpr> E)
    : CppStmt(std::move(n)), e(std::move(E)) { kind=Kind::Ast; }
std::string CppReturn::dump(int indent) const { return ind(indent)+std::string("return ")+(e?e->dump():"")+";\n"; }
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
std::string tool_list_text(){
    return
R"(Tools:
- ls /path, tree, mkdir /path, touch /path, cat /path, echo /path <data>, rm /path, mv /src /dst, link /src /dst
- parse /src/file.sexp /ast/name, eval /ast/name
- export <vfs> <host>, sys "<cmd>"
- Builtins: + - * = < print, if, lambda(1), strings, bool #t/#f
- Lists: list cons head tail null? ; Strings: str.cat str.sub str.find
- VFS ops: vfs-write vfs-read vfs-ls
- C++ AST ops via shell: cpp.tu /astcpp/X ; cpp.include TU header [0/1] ; cpp.func TU name rettype ; cpp.param FN type name ; cpp.print FN text ; cpp.returni FN int ; cpp.dump TU /cpp/file.cpp
)";
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
    std::string sys = std::string("You are a codex-like assistant embedded in a tiny single-binary IDE.\n") +
                      tool_list_text() + "\nRespond concisely in Finnish.";
    std::string js = std::string("{")+
        "\"model\":\""+json_escape(model)+"\","+
        "\"input\":[{\"role\":\"system\",\"content\":[{\"type\":\"text\",\"text\":\""+json_escape(sys)+"\"}]},"+
        "{\"role\":\"user\",\"content\":[{\"type\":\"text\",\"text\":\""+json_escape(user_prompt)+"\"}]}]}";
    return js;
}
std::string call_openai(const std::string& prompt){
    const char* key = std::getenv("OPENAI_API_KEY");
    if(!key || !*key) return "error: OPENAI_API_KEY puuttuu ympäristöstä";
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

    std::string raw = exec_capture(cmd);
    std::remove(tmp.c_str());
    if(raw.empty()) return "error: tyhjä vastaus OpenAI:lta\n";

    // very crude output_text extraction
    std::string marker = "\"output_text\"";
    size_t p = raw.find(marker);
    if(p!=std::string::npos){
        size_t colon = raw.find(':', p);
        size_t q1 = raw.find('"', colon+1);
        size_t q2 = raw.find('"', q1+1);
        if(q1!=std::string::npos && q2!=std::string::npos && q2>q1){
            std::string text = raw.substr(q1+1, q2-q1-1);
            for(size_t i=0;(i=text.find("\\n", i))!=std::string::npos; ) { text.replace(i,2,"\n"); i+=1; }
            return std::string("AI: ")+text+"\n";
        }
    }
    return raw + "\n";
}

// ====== REPL ======
static void help(){
    std::cout <<
R"(Commands:
  ls <path>
  tree
  mkdir <path>
  touch <path>
  cat <path>
  echo <path> <data...>
  rm <path>
  mv <src> <dst>
  link <src> <dst>
  parse <src-file> <dst-ast>
  eval <ast-path>
  # AI
  ai <prompt...>
  tools
  # C++ builder
  cpp.tu <ast-path>
  cpp.include <tu-path> <header> [angled0/1]
  cpp.func <tu-path> <name> <ret>
  cpp.param <fn-path> <type> <name>
  cpp.print <fn-path> <text>
  cpp.returni <fn-path> <int>
  cpp.dump <tu-path> <vfs-file-path>
Notes:
  - OPENAI_API_KEY pakollinen 'ai' komentoon. OPENAI_MODEL (oletus gpt-4o-mini), OPENAI_BASE_URL (oletus https://api.openai.com/v1).
)"<<std::endl;
}

int main(){
    using std::string; using std::shared_ptr;
    std::ios::sync_with_stdio(false); std::cin.tie(nullptr);

    Vfs vfs; auto env = std::make_shared<Env>(); install_builtins(env);
    vfs.mkdir("/src"); vfs.mkdir("/ast"); vfs.mkdir("/env"); vfs.mkdir("/astcpp"); vfs.mkdir("/cpp");

    std::cout << "codex-mini 🌲 VFS+AST+AI — 'help' kertoo karun totuuden.\n";
    string line;
    while(true){
        std::cout << "> " << std::flush;
        if(!std::getline(std::cin, line)) break;
        if(line.empty()) continue;
        std::stringstream ss(line);
        string cmd; ss >> cmd;
        try{
            if(cmd=="ls"){ string p; ss>>p; if(p.empty()) p="/"; vfs.ls(p);
            } else if(cmd=="tree"){ vfs.tree();
            } else if(cmd=="mkdir"){ string p; ss>>p; vfs.mkdir(p);
            } else if(cmd=="touch"){ string p; ss>>p; vfs.touch(p);
            } else if(cmd=="cat"){ string p; ss>>p; std::cout<<vfs.read(p)<<"\n";
            } else if(cmd=="echo"){ string p; ss>>p; string rest; std::getline(ss,rest); if(!rest.empty()&&rest[0]==' ') rest.erase(0,1); vfs.write(p,rest);
            } else if(cmd=="rm"){ string p; ss>>p; vfs.rm(p);
            } else if(cmd=="mv"){ string a,b; ss>>a>>b; vfs.mv(a,b);
            } else if(cmd=="link"){ string a,b; ss>>a>>b; vfs.link(a,b);

            } else if(cmd=="parse"){ string src,dst; ss>>src>>dst; auto text=vfs.read(src); auto ast=parse(text);
                auto holder=std::make_shared<AstHolder>(dst.substr(dst.find_last_of('/')+1), ast);
                string dir=dst.substr(0, dst.find_last_of('/')); if(dir.empty()) dir="/"; vfs.addNode(dir, holder);
                std::cout<<"AST @ "<<dst<<" valmis.\n";

            } else if(cmd=="eval"){ string p; ss>>p; auto n=vfs.resolve(p);
                if(n->kind!=VfsNode::Kind::Ast) throw std::runtime_error("not AST");
                auto a=std::dynamic_pointer_cast<AstNode>(n); auto val=a->eval(env);
                std::cout<<val.show()<<"\n";

            } else if(cmd=="ai"){ string prompt; std::getline(ss, prompt); if(!prompt.empty()&&prompt[0]==' ') prompt.erase(0,1);
                if(prompt.empty()){ std::cout<<"anna promptti.\n"; continue; }
                std::cout<<call_openai(prompt);

            } else if(cmd=="tools"){ std::cout<<tool_list_text()<<"\n";

            // ---- C++ builder ----
            } else if(cmd=="cpp.tu"){ string p; ss>>p;
                auto tu = std::make_shared<CppTranslationUnit>(p.substr(p.find_last_of('/')+1)); vfs_add(vfs, p, tu);
                std::cout<<"cpp tu @ "<<p<<"\n";

            } else if(cmd=="cpp.include"){ string tuP, hdr; int ang=0; ss>>tuP>>hdr; if(!(ss>>ang)) ang=0;
                auto tu = expect_tu(vfs.resolve(tuP)); auto inc = std::make_shared<CppInclude>("include", hdr, ang!=0);
                tu->includes.push_back(inc); std::cout<<"+include "<<hdr<<"\n";

            } else if(cmd=="cpp.func"){ string tuP, name, ret; ss>>tuP>>name>>ret;
                auto tu = expect_tu(vfs.resolve(tuP)); auto fn = std::make_shared<CppFunction>(name, ret, name);
                vfs_add(vfs, tuP+"/"+name, fn); tu->funcs.push_back(fn); std::cout<<"+func "<<name<<"\n";

            } else if(cmd=="cpp.param"){ string fnP, ty, nm; ss>>fnP>>ty>>nm;
                auto fn = expect_fn(vfs.resolve(fnP)); fn->params.push_back(CppParam{ty, nm}); std::cout<<"+param "<<ty<<" "<<nm<<"\n";

            } else if(cmd=="cpp.print"){ string fnP; ss>>fnP; string text; std::getline(ss, text); if(!text.empty()&&text[0]==' ') text.erase(0,1);
                auto fn = expect_fn(vfs.resolve(fnP));
                auto s = std::make_shared<CppString>("s", text);
                auto chain = std::vector<std::shared_ptr<CppExpr>>{ s, std::make_shared<CppId>("endl","std::endl") };
                auto coutline = std::make_shared<CppStreamOut>("cout", chain);
                fn->body->stmts.push_back(std::make_shared<CppExprStmt>("es", coutline));
                std::cout<<"+print '"<<text<<"'\n";

            } else if(cmd=="cpp.returni"){ string fnP; long long x; ss>>fnP>>x;
                auto fn = expect_fn(vfs.resolve(fnP));
                fn->body->stmts.push_back(std::make_shared<CppReturn>("ret", std::make_shared<CppInt>("i", x)));
                std::cout<<"+return "<<x<<"\n";

            } else if(cmd=="cpp.dump"){ string tuP, outP; ss>>tuP>>outP;
                cpp_dump_to_vfs(vfs, tuP, outP); std::cout<<"dump -> "<<outP<<"\n";

            } else if(cmd=="help"){ help();
            } else if(cmd=="quit" || cmd=="exit"){ break;
            } else { std::cout << "tuntematon komento. 'help' kertoo karun totuuden.\n"; }
        } catch(const std::exception& e){
            std::cout << "error: " << e.what() << "\n";
        }
    }
    return 0;
}
