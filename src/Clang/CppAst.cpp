#include "Clang.h"  // Include main header instead of individual header

// ====== C++ AST nodes ======
CppInclude::CppInclude(String n, String h, bool a)
    : CppNode(pick(n)), header(pick(h)), angled(a) { kind=Kind::Ast; }
String CppInclude::dump(int) const {
    return String("#include ") + (angled?"<":"\"") + header + (angled?">":"\"") + "\n";
}
CppId::CppId(String n, String i) : CppExpr(pick(n)), id(pick(i)) { kind=Kind::Ast; }
String CppId::dump(int) const { return id; }
namespace {
bool is_octal_digit(char c){ return c>='0' && c<='7'; }
bool is_hex_digit(char c){ return std::isxdigit(static_cast<unsigned char>(c)) != 0; }

void verify_cpp_string_literal(const String& lit){
    for(int i=0;i<lit.GetCount();++i){
        unsigned char uc = static_cast<unsigned char>(lit[i]);
        if(uc=='\n' || uc=='\r') throw std::runtime_error("cpp string literal contains raw newline");
        if(uc=='\\'){
            if(++i>=lit.GetCount()) throw std::runtime_error("unterminated escape in cpp string literal");
            unsigned char esc = static_cast<unsigned char>(lit[i]);
            switch(esc){
                case '"': case '\\': case 'n': case 'r': case 't':
                case 'b': case 'f': case 'v': case 'a': case '?':
                    break;
                case 'x': {
                    int digits=0;
                    while(i+1<lit.GetCount() && is_hex_digit(lit[i+1]) && digits<2){ ++i; ++digits; }
                    if(digits==0) throw std::runtime_error("\\x escape missing hex digits");
                    break;
                }
                case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': {
                    int digits=0;
                    while(i+1<lit.GetCount() && is_octal_digit(lit[i+1]) && digits<2){ ++i; ++digits; }
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

CppString::CppString(String n, String v) : CppExpr(pick(n)), s(pick(v)) { kind=Kind::Ast; }
String CppString::esc(const String& x){
    String out;
    // Reserve space - this is a simplified version for U++
    auto append_octal = [&out](unsigned char uc){
        out.Cat('\\');
        out.Cat(static_cast<char>('0' + ((uc >> 6) & 0x7)));
        out.Cat(static_cast<char>('0' + ((uc >> 3) & 0x7)));
        out.Cat(static_cast<char>('0' + (uc & 0x7)));
    };

    bool escape_next_question = false;
    for(int i=0;i<x.GetCount();++i){
        unsigned char uc = static_cast<unsigned char>(x[i]);
        if(uc=='?'){
            if(escape_next_question || (i+1<x.GetCount() && x[i+1]=='?')){
                out.Cat("\\?");
                escape_next_question = (i+1<x.GetCount() && x[i+1]=='?');
            } else {
                out.Cat('?');
                escape_next_question = false;
            }
            continue;
        }

        escape_next_question = false;
        switch(uc){
            case '"': out.Cat("\\\""); break;
            case '\\': out.Cat("\\\\"); break;
            case '\n': out.Cat("\\n"); break;
            case '\r': out.Cat("\\r"); break;
            case '\t': out.Cat("\\t"); break;
            case '\b': out.Cat("\\b"); break;
            case '\f': out.Cat("\\f"); break;
            case '\v': out.Cat("\\v"); break;
            case '\a': out.Cat("\\a"); break;
            default:
                if(uc < 0x20 || uc == 0x7f){
                    append_octal(uc);
                } else if(uc >= 0x80){
                    append_octal(uc);
                } else {
                    out.Cat(static_cast<char>(uc));
                }
        }
    }
    return out;
}
String CppString::dump(int) const {
    String escaped = esc(s);
    verify_cpp_string_literal(escaped);
    return String("\"") + escaped + "\"";
}
CppInt::CppInt(String n, long long x): CppExpr(pick(n)), v(x) { kind=Kind::Ast; }
String CppInt::dump(int) const { return IntStr(v); }  // Using U++ IntStr function
One<CppExpr> make_shared_cppexpr(One<CppExpr> e) { return e; }
One<CppCompound> make_shared_cppcompound(String name) { return One<CppCompound>(new CppCompound(pick(name))); }
CppCall::CppCall(String n, One<CppExpr> f, Vector<One<CppExpr>> a)
    : CppExpr(pick(n)), fn(pick(f)), args(pick(a)) { kind=Kind::Ast; }
String CppCall::dump(int) const {
    String s = fn->dump(); s+='(';
    bool first=true; for(const auto& a : args){ if(!first) s+=", "; first=false; s+=a->dump(); } s+=')';
    return s;
}
CppBinOp::CppBinOp(String n, String o, One<CppExpr> A, One<CppExpr> B)
    : CppExpr(pick(n)), op(pick(o)), a(pick(A)), b(pick(B)) { kind=Kind::Ast; }
String CppBinOp::dump(int) const { return a->dump()+" "+op+" "+b->dump(); }
CppStreamOut::CppStreamOut(String n, Vector<One<CppExpr>> xs)
    : CppExpr(pick(n)), chain(pick(xs)) { kind=Kind::Ast; }
String CppStreamOut::dump(int) const {
    String s="std::cout"; for(const auto& e : chain){ s+=" << "; s+=e->dump(); } return s;
}
CppRawExpr::CppRawExpr(String n, String t)
    : CppExpr(pick(n)), text(pick(t)) { kind=Kind::Ast; }
String CppRawExpr::dump(int) const { return text; }
CppExprStmt::CppExprStmt(String n, One<CppExpr> E)
    : CppStmt(pick(n)), e(pick(E)) { kind=Kind::Ast; }
String CppExprStmt::dump(int indent) const { return ind(indent)+e->dump()+";\n"; }
CppReturn::CppReturn(String n, One<CppExpr> E)
    : CppStmt(pick(n)), e(pick(E)) { kind=Kind::Ast; }
String CppReturn::dump(int indent) const {
    String s = ind(indent) + "return";
    if (e) s += " " + e->dump();
    s += ";\n";
    return s;
}
CppRawStmt::CppRawStmt(String n, String t)
    : CppStmt(pick(n)), text(pick(t)) { kind=Kind::Ast; }
String CppRawStmt::dump(int indent) const {
    String pad = ind(indent);
    String out;
    int start = 0;
    while (start <= text.GetCount()){
        int end = text.Find('\n', start);
        String line = end<0 ? text.Mid(start) : text.Mid(start, end-start);
        if(line.GetCount()>0 || end>=0) out += pad + line + "\n";
        if (end<0) break;
        start = end + 1;
    }
    if(out.IsEmpty()) out = pad + "\n";
    return out;
}
CppVarDecl::CppVarDecl(String n, String ty, String nm, String in, bool has)
    : CppStmt(pick(n)), type(pick(ty)), name(pick(nm)), init(pick(in)), hasInit(has) { kind=Kind::Ast; }
String CppVarDecl::dump(int indent) const {
    String s = ind(indent) + type + " " + name;
    if (hasInit){
        if (!init.IsEmpty() && (init[0]=='{' || init[0]=='(')) s += init;
        else if (!init.IsEmpty() && init[0]=='=') s += " " + init;
        else if (!init.IsEmpty()) s += " = " + init;
    }
    s += ";\n";
    return s;
}
CppCompound::CppCompound(String n) : CppStmt(pick(n)) { kind=Kind::Ast; }
String CppCompound::dump(int indent) const {
    String s = ind(indent) + "{\n";
    for(const auto& st : stmts) s += st->dump(indent+2);
    s += ind(indent) + "}\n"; return s;
}
CppFunction::CppFunction(String n, String rt, String nm)
    : CppNode(pick(n)), retType(pick(rt)), name(pick(nm)) { kind=Kind::Ast; body = make_shared_cppcompound("body"); }
String CppFunction::dump(int indent) const {
    String s; s += retType + " " + name + "(";
    for(int i=0;i<params.GetCount();++i){ if(i) s += ", "; s += params[i].type + " " + params[i].name; }
    s += ")\n"; s += body->dump(indent); return s;
}
CppRangeFor::CppRangeFor(String n, String d, String r)
    : CppStmt(pick(n)), decl(pick(d)), range(pick(r)), body(make_shared_cppcompound("body")) { kind=Kind::Ast; }
String CppRangeFor::dump(int indent) const {
    String s = ind(indent) + "for (" + decl + " : " + range + ")\n";
    s += body->dump(indent);
    return s;
}
CppTranslationUnit::CppTranslationUnit(String n) : CppNode(pick(n)) { kind=Kind::Ast; }
String CppTranslationUnit::dump(int) const {
    String s;
    for(const auto& i : includes) s += i->dump(0);
    s += "\n";
    for(const auto& f : funcs){ s += f->dump(0); s += "\n"; }
    return s;
}

One<CppTranslationUnit> expect_tu(One<VfsNode> n){
    auto tu = dynamic_cast<CppTranslationUnit*>(n.Get());
    if(!tu) throw std::runtime_error("not a CppTranslationUnit node");
    return pick(n);
}
One<CppFunction> expect_fn(One<VfsNode> n){
    auto fn = dynamic_cast<CppFunction*>(n.Get());
    if(!fn) throw std::runtime_error("not a CppFunction node");
    return pick(n);
}
One<CppCompound> expect_block(One<VfsNode> n){
    if(auto fn = dynamic_cast<CppFunction*>(n.Get())) return pick(fn->body);
    if(auto block = dynamic_cast<CppCompound*>(n.Get())) return pick(n);
    if(auto loop = dynamic_cast<CppRangeFor*>(n.Get())) return pick(loop->body);
    throw std::runtime_error("node does not own a compound body");
}
void vfs_add(Vfs& vfs, const String& path, One<VfsNode> node, size_t overlayId){
    int last_slash = path.ReverseFind('/');
    String dir = last_slash >= 0 ? path.Mid(0, last_slash) : String("/");
    String name = last_slash >= 0 ? path.Mid(last_slash+1) : path;
    node->name = name;
    vfs.addNode(dir.ToStd(), node, overlayId);
}
void cpp_dump_to_vfs(Vfs& vfs, size_t overlayId, const String& tuPath, const String& filePath){
    auto n = vfs.resolveForOverlay(tuPath.ToStd(), overlayId);
    auto tu = expect_tu(n);
    String code = tu->dump(0);
    vfs.write(filePath.ToStd(), code.ToStd(), overlayId);
}

