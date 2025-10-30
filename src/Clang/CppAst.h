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
    SexpValue eval(Shared<Env>) override { return SexpValue::S(dump().ToStd()); }
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
    std::shared_ptr<CppExpr> fn; std::vector<std::shared_ptr<CppExpr>> args;
    CppCall(String n, std::shared_ptr<CppExpr> f, std::vector<std::shared_ptr<CppExpr>> a);
    String dump(int) const override;
};

struct CppBinOp : CppExpr {
    String op; std::shared_ptr<CppExpr> a,b;
    CppBinOp(String n, String o, std::shared_ptr<CppExpr> A, std::shared_ptr<CppExpr> B);
    String dump(int) const override;
};

struct CppStreamOut : CppExpr {
    std::vector<std::shared_ptr<CppExpr>> chain;
    CppStreamOut(String n, std::vector<std::shared_ptr<CppExpr>> xs);
    String dump(int) const override;
};

struct CppRawExpr : CppExpr {
    String text;
    CppRawExpr(String n, String t);
    String dump(int) const override;
};

struct CppStmt : CppNode { using CppNode::CppNode; };
struct CppExprStmt : CppStmt {
    std::shared_ptr<CppExpr> e;
    CppExprStmt(String n, std::shared_ptr<CppExpr> E);
    String dump(int indent) const override;
};
struct CppReturn : CppStmt {
    std::shared_ptr<CppExpr> e;
    CppReturn(String n, std::shared_ptr<CppExpr> E);
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
    std::vector<std::shared_ptr<CppStmt>> stmts;
    explicit CppCompound(String n);
    std::unordered_map<std::string, std::shared_ptr<VfsNode>> ch;
    bool isDir() const override { return true; }
    std::unordered_map<std::string, std::shared_ptr<VfsNode>>& children() override { return ch; }
    String dump(int indent) const override;
};
struct CppParam { String type, name; };
struct CppFunction : CppNode {
    String retType, name;
    std::vector<CppParam> params;
    std::shared_ptr<CppCompound> body;
    std::unordered_map<std::string, std::shared_ptr<VfsNode>> ch;
    CppFunction(String n, String rt, String nm);
    bool isDir() const override { return true; }
    std::unordered_map<std::string, std::shared_ptr<VfsNode>>& children() override { return ch; }
    String dump(int indent) const override;
};
struct CppRangeFor : CppStmt {
    String decl;
    String range;
    std::shared_ptr<CppCompound> body;
    std::unordered_map<std::string, std::shared_ptr<VfsNode>> ch;
    CppRangeFor(String n, String d, String r);
    bool isDir() const override { return true; }
    std::unordered_map<std::string, std::shared_ptr<VfsNode>>& children() override { return ch; }
    String dump(int indent) const override;
};
struct CppTranslationUnit : CppNode {
    std::vector<std::shared_ptr<CppInclude>> includes;
    std::vector<std::shared_ptr<CppFunction>> funcs;
    std::unordered_map<std::string, std::shared_ptr<VfsNode>> ch;
    explicit CppTranslationUnit(String n);
    bool isDir() const override { return true; }
    std::unordered_map<std::string, std::shared_ptr<VfsNode>>& children() override { return ch; }
    String dump(int) const override;
};

std::shared_ptr<CppTranslationUnit> expect_tu(std::shared_ptr<VfsNode> n);
std::shared_ptr<CppCompound> expect_block(std::shared_ptr<VfsNode> n);
void cpp_dump_to_vfs(Vfs& vfs, size_t overlayId, const String& tuPath, const String& filePath);
std::shared_ptr<CppFunction> expect_fn(std::shared_ptr<VfsNode> n);

#endif
