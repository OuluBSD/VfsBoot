#include "VfsShell.h"

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
void vfs_add(Vfs& vfs, const std::string& path, std::shared_ptr<VfsNode> node, size_t overlayId){
    std::string dir = path.substr(0, path.find_last_of('/'));
    if(dir.empty()) dir = "/";
    std::string name = path.substr(path.find_last_of('/')+1);
    node->name = name;
    vfs.addNode(dir, node, overlayId);
}
void cpp_dump_to_vfs(Vfs& vfs, size_t overlayId, const std::string& tuPath, const std::string& filePath){
    auto n = vfs.resolveForOverlay(tuPath, overlayId);
    auto tu = expect_tu(n);
    std::string code = tu->dump(0);
    vfs.write(filePath, code, overlayId);
}

