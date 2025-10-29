#ifndef _Clang_CppAst_h_
#define _Clang_CppAst_h_

// All includes have been moved to Clang.h - the main header
// This header is not self-contained as per U++ convention
// For reference: This header needs types from VfsShell/AstNode.h and VfsCore/VfsCore.h
// struct VfsNode;  // Forward declaration instead of include
// struct AstNode;  // Forward declaration instead of include
// struct Value;    // Forward declaration instead of include
// struct Env;      // Forward declaration instead of include

// Forward declarations for types defined elsewhere
struct VfsNode;
struct AstNode;
struct Value;
struct Env;
struct CppTranslationUnit;
struct CppCompound;

//
// C++ AST (perii AstNode → VfsNode)
//
struct CppNode : AstNode {
    using AstNode::AstNode;
    virtual String dump(int indent = 0) const = 0;
    static String ind(int n) { return String().Cat() << n; }  // Simplified implementation
    SexpValue eval(std::shared_ptr<Env>) override { return SexpValue::S(dump().ToStd()); }
    typedef CppNode CLASSNAME;  // Required for THISBACK macros if used
};

struct CppInclude : CppNode {
    String header; bool angled;
    CppInclude(String n, String h, bool a);
    String dump(int) const override;
};

struct CppExpr : CppNode { using CppNode::CppNode; };
struct CppId   : CppExpr { String id; CppId(String n, String i); String dump(int) const override; };
struct CppString : CppExpr {
    String s; CppString(String n, String v);
    static String esc(const String& x);
    String dump(int) const override;
};
struct CppInt  : CppExpr { long long v; CppInt(String n, long long x); String dump(int) const override; };

struct CppCall : CppExpr {
    One<CppExpr> fn; Vector<One<CppExpr>> args;
    CppCall(String n, One<CppExpr> f, Vector<One<CppExpr>> a);
    String dump(int) const override;
};

struct CppBinOp : CppExpr {
    String op; One<CppExpr> a,b;
    CppBinOp(String n, String o, One<CppExpr> A, One<CppExpr> B);
    String dump(int) const override;
};

struct CppStreamOut : CppExpr {
    Vector<One<CppExpr>> chain;
    CppStreamOut(String n, Vector<One<CppExpr>> xs);
    String dump(int) const override;
};

struct CppRawExpr : CppExpr {
    String text;
    CppRawExpr(String n, String t);
    String dump(int) const override;
};

struct CppStmt : CppNode { using CppNode::CppNode; };
struct CppExprStmt : CppStmt {
    One<CppExpr> e;
    CppExprStmt(String n, One<CppExpr> E);
    String dump(int indent) const override;
};
struct CppReturn : CppStmt {
    One<CppExpr> e;
    CppReturn(String n, One<CppExpr> E);
    String dump(int indent) const override;
};
struct CppRawStmt : CppStmt {
    String text;
    CppRawStmt(String n, String t);
    String dump(int indent) const override;
};
struct CppVarDecl : CppStmt {
    String type;
    String name;
    String init;
    bool hasInit;
    CppVarDecl(String n, String ty, String nm, String in, bool has);
    String dump(int indent) const override;
};
struct CppCompound : CppStmt {
    Vector<One<CppStmt>> stmts;
    explicit CppCompound(String n);
    std::unordered_map<std::string, std::shared_ptr<VfsNode>> ch;
    bool isDir() const override { return true; }
    std::unordered_map<std::string, std::shared_ptr<VfsNode>>& children() override { return ch; }
    String dump(int indent) const override;
};
struct CppParam { String type, name; };
struct CppFunction : CppNode {
    String retType, name;
    Vector<CppParam> params;
    One<CppCompound> body;
    std::unordered_map<std::string, std::shared_ptr<VfsNode>> ch;
    CppFunction(String n, String rt, String nm);
    bool isDir() const override { return true; }
    std::unordered_map<std::string, std::shared_ptr<VfsNode>>& children() override { return ch; }
    String dump(int indent) const override;
};
struct CppRangeFor : CppStmt {
    String decl;
    String range;
    One<CppCompound> body;
    std::unordered_map<std::string, std::shared_ptr<VfsNode>> ch;
    CppRangeFor(String n, String d, String r);
    bool isDir() const override { return true; }
    std::unordered_map<std::string, std::shared_ptr<VfsNode>>& children() override { return ch; }
    String dump(int indent) const override;
};
struct CppTranslationUnit : CppNode {
    Vector<One<CppInclude>> includes;
    Vector<One<CppFunction>> funcs;
    std::unordered_map<std::string, std::shared_ptr<VfsNode>> ch;
    explicit CppTranslationUnit(String n);
    bool isDir() const override { return true; }
    std::unordered_map<std::string, std::shared_ptr<VfsNode>>& children() override { return ch; }
    String dump(int) const override;
};

One<CppTranslationUnit> expect_tu(One<VfsNode> n);
One<CppCompound> expect_block(One<VfsNode> n);
void cpp_dump_to_vfs(Vfs& vfs, size_t overlayId, const String& tuPath, const String& filePath);
One<CppFunction> expect_fn(One<VfsNode> n);

#endif
