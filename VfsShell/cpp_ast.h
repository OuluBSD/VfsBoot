#pragma once



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

std::shared_ptr<CppTranslationUnit> expect_tu(std::shared_ptr<VfsNode> n);
std::shared_ptr<CppCompound> expect_block(std::shared_ptr<VfsNode> n);
void cpp_dump_to_vfs(Vfs& vfs, size_t overlayId, const std::string& tuPath, const std::string& filePath);
std::shared_ptr<CppFunction> expect_fn(std::shared_ptr<VfsNode> n);
