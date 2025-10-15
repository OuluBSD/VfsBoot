#pragma once


struct BinaryWriter {
    std::string data;

    void u8(uint8_t v){ data.push_back(static_cast<char>(v)); }
    void u32(uint32_t v){
        for(int i = 0; i < 4; ++i){
            data.push_back(static_cast<char>((v >> (i * 8)) & 0xff));
        }
    }
    void i64(int64_t v){
        uint64_t raw = static_cast<uint64_t>(v);
        for(int i = 0; i < 8; ++i){
            data.push_back(static_cast<char>((raw >> (i * 8)) & 0xff));
        }
    }
    void str(const std::string& s){
        if(s.size() > std::numeric_limits<uint32_t>::max())
            throw std::runtime_error("string too large for serialization");
        u32(static_cast<uint32_t>(s.size()));
        data.append(s);
    }
};

struct BinaryReader {
    const std::string& data;
    size_t pos = 0;
    explicit BinaryReader(const std::string& d) : data(d) {}

    uint8_t u8(){
        if(pos >= data.size()) throw std::runtime_error("unexpected EOF while decoding u8");
        return static_cast<uint8_t>(data[pos++]);
    }
    uint32_t u32(){
        if(data.size() - pos < 4) throw std::runtime_error("unexpected EOF while decoding u32");
        uint32_t v = 0;
        for(int i = 0; i < 4; ++i){
            v |= static_cast<uint32_t>(static_cast<unsigned char>(data[pos++])) << (i * 8);
        }
        return v;
    }
    int64_t i64(){
        if(data.size() - pos < 8) throw std::runtime_error("unexpected EOF while decoding i64");
        uint64_t v = 0;
        for(int i = 0; i < 8; ++i){
            v |= static_cast<uint64_t>(static_cast<unsigned char>(data[pos++])) << (i * 8);
        }
        return static_cast<int64_t>(v);
    }
    std::string str(){
        uint32_t len = u32();
        if(data.size() - pos < len) throw std::runtime_error("unexpected EOF while decoding string");
        std::string out = data.substr(pos, len);
        pos += len;
        return out;
    }
    bool eof() const { return pos == data.size(); }
    void expect_eof() const {
        if(pos != data.size()) throw std::runtime_error("extra bytes in AST payload");
    }
};


// Working directory context
struct WorkingDirectory {
    std::string path = "/";
    std::vector<size_t> overlays{0};
    size_t primary_overlay = 0;
    enum class ConflictPolicy { Manual, Oldest, Newest };
    ConflictPolicy conflict_policy = ConflictPolicy::Manual;
};

// Solution context for .cxpkg/.cxasm files
struct SolutionContext {
    bool active = false;
    bool auto_detected = false;
    size_t overlay_id = 0;
    std::string title;
    std::string file_path;
};

// Autosave context for crash recovery
struct AutosaveContext {
    bool enabled = true;
    int delay_seconds = 10;
    int crash_recovery_interval_seconds = 180;  // 3 minutes
    std::atomic<bool> should_stop{false};
    std::mutex mtx;
    std::chrono::steady_clock::time_point last_modification;
    std::chrono::steady_clock::time_point last_crash_recovery;
    std::vector<size_t> solution_overlay_ids;  // Only .cxpkg/.cxasm and their chained .vfs files
};

// Solution file extensions
constexpr const char* kPackageExtension = ".cxpkg";
constexpr const char* kAssemblyExtension = ".cxasm";

// Global callback for save shortcuts
extern std::function<void()> g_on_save_shortcut;

// Utility functions
void sort_unique(std::vector<size_t>& ids);
bool is_solution_file(const std::filesystem::path& p);
std::optional<std::filesystem::path> auto_detect_vfs_path();
std::optional<std::filesystem::path> auto_detect_solution_path();
std::string make_unique_overlay_name(Vfs& vfs, std::string base);
size_t mount_overlay_from_file(Vfs& vfs, const std::string& name, const std::string& hostPath);
void maybe_extend_context(Vfs& vfs, WorkingDirectory& cwd);
bool solution_save(Vfs& vfs, SolutionContext& sol, bool quiet);
void attach_solution_shortcut(Vfs& vfs, SolutionContext& sol);
bool load_solution_from_file(Vfs& vfs, WorkingDirectory& cwd, SolutionContext& sol,
                             const std::filesystem::path& file, bool auto_detected);
std::pair<std::string, std::string> serialize_ast_node(const std::shared_ptr<AstNode>& node);
std::shared_ptr<AstNode> deserialize_ast_node(const std::string& data);
std::shared_ptr<AstNode> deserialize_ast_node(const std::string& type,
                                                     const std::string& payload,
                                                     const std::string& path,
                                                     std::vector<std::function<void()>>& fixups,
                                                     std::unordered_map<std::string, std::shared_ptr<VfsNode>>& path_map);
std::shared_ptr<AstNode> deserialize_s_ast_node(const std::string& type, const std::string& payload);
uint64_t fnv1a64(const std::string& data);
std::string hash_hex(uint64_t value);
std::string sanitize_component(const std::string& s);
std::string unescape_meta(const std::string& s);
std::pair<std::string, std::string> serialize_s_ast_node(const std::shared_ptr<AstNode>& node);
std::shared_ptr<AstNode> deserialize_s_ast_node(const std::string& type, const std::string& payload);
bool is_s_ast_type(const std::string& type);
void serialize_cpp_expr(BinaryWriter& w, const std::shared_ptr<CppExpr>& expr);
std::shared_ptr<CppExpr> deserialize_cpp_expr(BinaryReader& r);
void deserialize_cpp_compound_into(const std::string& payload,
                                          const std::string& node_path,
                                          const std::shared_ptr<CppCompound>& compound,
                                          std::vector<std::function<void()>>& fixups,
                                          std::unordered_map<std::string, std::shared_ptr<VfsNode>>& path_map);
std::string serialize_cpp_compound_payload(const std::shared_ptr<CppCompound>& compound);

// libclang AST Nodes - Phase 1: Foundation (Declarations, Statements, Expressions, Types)
// ============================================================================
//

// Include libclang header
#include <clang-c/Index.h>

// Source location tracking with code span
struct SourceLocation {
    std::string file;
    unsigned int line;
    unsigned int column;
    unsigned int offset;
    unsigned int length;  // Length in bytes of the code span

    SourceLocation() : line(0), column(0), offset(0), length(0) {}
    SourceLocation(std::string f, unsigned int l, unsigned int c, unsigned int o = 0, unsigned int len = 0)
        : file(std::move(f)), line(l), column(c), offset(o), length(len) {}

    std::string toString() const;
    std::string toStringWithLength() const;
};

// Base class for all libclang AST nodes
struct ClangAstNode : AstNode {
    SourceLocation location;
    std::string spelling;  // Name/identifier from clang_getCursorSpelling
    std::map<std::string, std::shared_ptr<VfsNode>> ch;

    ClangAstNode(std::string n, SourceLocation loc, std::string spell)
        : AstNode(std::move(n)), location(std::move(loc)), spelling(std::move(spell)) {}

    bool isDir() const override { return true; }
    std::map<std::string, std::shared_ptr<VfsNode>>& children() override { return ch; }
    Value eval(std::shared_ptr<Env>) override { return Value::S(spelling); }
    std::string read() const override { return spelling; }

    virtual std::string dump(int indent = 0) const = 0;
    static std::string ind(int n) { return std::string(n * 2, ' '); }
};

// Type nodes
struct ClangType : ClangAstNode {
    std::string type_name;

    ClangType(std::string n, SourceLocation loc, std::string spell, std::string ty)
        : ClangAstNode(std::move(n), std::move(loc), std::move(spell)), type_name(std::move(ty)) {}

    std::string dump(int indent) const override;
};

struct ClangBuiltinType : ClangType {
    ClangBuiltinType(std::string n, SourceLocation loc, std::string spell, std::string ty)
        : ClangType(std::move(n), std::move(loc), std::move(spell), std::move(ty)) {}
    std::string dump(int indent) const override;
};

struct ClangPointerType : ClangType {
    std::shared_ptr<ClangType> pointee;

    ClangPointerType(std::string n, SourceLocation loc, std::string spell, std::string ty)
        : ClangType(std::move(n), std::move(loc), std::move(spell), std::move(ty)) {}
    std::string dump(int indent) const override;
};

struct ClangReferenceType : ClangType {
    std::shared_ptr<ClangType> referenced;

    ClangReferenceType(std::string n, SourceLocation loc, std::string spell, std::string ty)
        : ClangType(std::move(n), std::move(loc), std::move(spell), std::move(ty)) {}
    std::string dump(int indent) const override;
};

struct ClangRecordType : ClangType {
    ClangRecordType(std::string n, SourceLocation loc, std::string spell, std::string ty)
        : ClangType(std::move(n), std::move(loc), std::move(spell), std::move(ty)) {}
    std::string dump(int indent) const override;
};

struct ClangFunctionProtoType : ClangType {
    std::shared_ptr<ClangType> return_type;
    std::vector<std::shared_ptr<ClangType>> param_types;

    ClangFunctionProtoType(std::string n, SourceLocation loc, std::string spell, std::string ty)
        : ClangType(std::move(n), std::move(loc), std::move(spell), std::move(ty)) {}
    std::string dump(int indent) const override;
};

// Declaration nodes
struct ClangDecl : ClangAstNode {
    std::shared_ptr<ClangType> type;

    ClangDecl(std::string n, SourceLocation loc, std::string spell)
        : ClangAstNode(std::move(n), std::move(loc), std::move(spell)) {}
};

struct ClangTranslationUnitDecl : ClangDecl {
    ClangTranslationUnitDecl(std::string n, SourceLocation loc, std::string spell)
        : ClangDecl(std::move(n), std::move(loc), std::move(spell)) {}
    std::string dump(int indent) const override;
};

struct ClangFunctionDecl : ClangDecl {
    std::string return_type_str;
    std::vector<std::pair<std::string, std::string>> parameters;  // (type, name)
    std::shared_ptr<ClangAstNode> body;  // CompoundStmt or nullptr

    ClangFunctionDecl(std::string n, SourceLocation loc, std::string spell)
        : ClangDecl(std::move(n), std::move(loc), std::move(spell)) {}
    std::string dump(int indent) const override;
};

struct ClangVarDecl : ClangDecl {
    std::string type_str;
    std::string var_name;
    std::shared_ptr<ClangAstNode> initializer;  // Expr or nullptr

    ClangVarDecl(std::string n, SourceLocation loc, std::string spell)
        : ClangDecl(std::move(n), std::move(loc), std::move(spell)) {}
    std::string dump(int indent) const override;
};

struct ClangParmDecl : ClangDecl {
    std::string type_str;
    std::string param_name;

    ClangParmDecl(std::string n, SourceLocation loc, std::string spell)
        : ClangDecl(std::move(n), std::move(loc), std::move(spell)) {}
    std::string dump(int indent) const override;
};

struct ClangFieldDecl : ClangDecl {
    std::string type_str;
    std::string field_name;

    ClangFieldDecl(std::string n, SourceLocation loc, std::string spell)
        : ClangDecl(std::move(n), std::move(loc), std::move(spell)) {}
    std::string dump(int indent) const override;
};

struct ClangClassDecl : ClangDecl {
    std::string class_name;
    std::vector<std::shared_ptr<ClangAstNode>> bases;  // Base classes
    std::vector<std::shared_ptr<ClangAstNode>> members;  // Fields, methods

    ClangClassDecl(std::string n, SourceLocation loc, std::string spell)
        : ClangDecl(std::move(n), std::move(loc), std::move(spell)) {}
    std::string dump(int indent) const override;
};

struct ClangStructDecl : ClangDecl {
    std::string struct_name;
    std::vector<std::shared_ptr<ClangAstNode>> members;

    ClangStructDecl(std::string n, SourceLocation loc, std::string spell)
        : ClangDecl(std::move(n), std::move(loc), std::move(spell)) {}
    std::string dump(int indent) const override;
};

struct ClangEnumDecl : ClangDecl {
    std::string enum_name;
    std::vector<std::pair<std::string, int64_t>> enumerators;  // (name, value)

    ClangEnumDecl(std::string n, SourceLocation loc, std::string spell)
        : ClangDecl(std::move(n), std::move(loc), std::move(spell)) {}
    std::string dump(int indent) const override;
};

struct ClangNamespaceDecl : ClangDecl {
    std::string namespace_name;

    ClangNamespaceDecl(std::string n, SourceLocation loc, std::string spell)
        : ClangDecl(std::move(n), std::move(loc), std::move(spell)) {}
    std::string dump(int indent) const override;
};

struct ClangTypedefDecl : ClangDecl {
    std::string typedef_name;
    std::string underlying_type;

    ClangTypedefDecl(std::string n, SourceLocation loc, std::string spell)
        : ClangDecl(std::move(n), std::move(loc), std::move(spell)) {}
    std::string dump(int indent) const override;
};

// Statement nodes
struct ClangStmt : ClangAstNode {
    ClangStmt(std::string n, SourceLocation loc, std::string spell)
        : ClangAstNode(std::move(n), std::move(loc), std::move(spell)) {}
};

struct ClangCompoundStmt : ClangStmt {
    std::vector<std::shared_ptr<ClangAstNode>> statements;

    ClangCompoundStmt(std::string n, SourceLocation loc, std::string spell)
        : ClangStmt(std::move(n), std::move(loc), std::move(spell)) {}
    std::string dump(int indent) const override;
};

struct ClangIfStmt : ClangStmt {
    std::shared_ptr<ClangAstNode> condition;
    std::shared_ptr<ClangAstNode> then_branch;
    std::shared_ptr<ClangAstNode> else_branch;  // nullptr if no else

    ClangIfStmt(std::string n, SourceLocation loc, std::string spell)
        : ClangStmt(std::move(n), std::move(loc), std::move(spell)) {}
    std::string dump(int indent) const override;
};

struct ClangForStmt : ClangStmt {
    std::shared_ptr<ClangAstNode> init;
    std::shared_ptr<ClangAstNode> condition;
    std::shared_ptr<ClangAstNode> increment;
    std::shared_ptr<ClangAstNode> body;

    ClangForStmt(std::string n, SourceLocation loc, std::string spell)
        : ClangStmt(std::move(n), std::move(loc), std::move(spell)) {}
    std::string dump(int indent) const override;
};

struct ClangWhileStmt : ClangStmt {
    std::shared_ptr<ClangAstNode> condition;
    std::shared_ptr<ClangAstNode> body;

    ClangWhileStmt(std::string n, SourceLocation loc, std::string spell)
        : ClangStmt(std::move(n), std::move(loc), std::move(spell)) {}
    std::string dump(int indent) const override;
};

struct ClangReturnStmt : ClangStmt {
    std::shared_ptr<ClangAstNode> return_value;  // nullptr for bare return

    ClangReturnStmt(std::string n, SourceLocation loc, std::string spell)
        : ClangStmt(std::move(n), std::move(loc), std::move(spell)) {}
    std::string dump(int indent) const override;
};

struct ClangDeclStmt : ClangStmt {
    std::vector<std::shared_ptr<ClangAstNode>> declarations;

    ClangDeclStmt(std::string n, SourceLocation loc, std::string spell)
        : ClangStmt(std::move(n), std::move(loc), std::move(spell)) {}
    std::string dump(int indent) const override;
};

struct ClangExprStmt : ClangStmt {
    std::shared_ptr<ClangAstNode> expression;

    ClangExprStmt(std::string n, SourceLocation loc, std::string spell)
        : ClangStmt(std::move(n), std::move(loc), std::move(spell)) {}
    std::string dump(int indent) const override;
};

struct ClangBreakStmt : ClangStmt {
    ClangBreakStmt(std::string n, SourceLocation loc, std::string spell)
        : ClangStmt(std::move(n), std::move(loc), std::move(spell)) {}
    std::string dump(int indent) const override;
};

struct ClangContinueStmt : ClangStmt {
    ClangContinueStmt(std::string n, SourceLocation loc, std::string spell)
        : ClangStmt(std::move(n), std::move(loc), std::move(spell)) {}
    std::string dump(int indent) const override;
};

// Expression nodes
struct ClangExpr : ClangAstNode {
    std::shared_ptr<ClangType> expr_type;

    ClangExpr(std::string n, SourceLocation loc, std::string spell)
        : ClangAstNode(std::move(n), std::move(loc), std::move(spell)) {}
};

struct ClangBinaryOperator : ClangExpr {
    std::string opcode;  // +, -, *, /, ==, etc.
    std::shared_ptr<ClangAstNode> lhs;
    std::shared_ptr<ClangAstNode> rhs;

    ClangBinaryOperator(std::string n, SourceLocation loc, std::string spell, std::string op)
        : ClangExpr(std::move(n), std::move(loc), std::move(spell)), opcode(std::move(op)) {}
    std::string dump(int indent) const override;
};

struct ClangUnaryOperator : ClangExpr {
    std::string opcode;  // ++, --, -, !, etc.
    std::shared_ptr<ClangAstNode> operand;
    bool is_prefix;

    ClangUnaryOperator(std::string n, SourceLocation loc, std::string spell, std::string op, bool prefix)
        : ClangExpr(std::move(n), std::move(loc), std::move(spell)), opcode(std::move(op)), is_prefix(prefix) {}
    std::string dump(int indent) const override;
};

struct ClangCallExpr : ClangExpr {
    std::shared_ptr<ClangAstNode> callee;
    std::vector<std::shared_ptr<ClangAstNode>> arguments;

    ClangCallExpr(std::string n, SourceLocation loc, std::string spell)
        : ClangExpr(std::move(n), std::move(loc), std::move(spell)) {}
    std::string dump(int indent) const override;
};

struct ClangDeclRefExpr : ClangExpr {
    std::string referenced_decl;

    ClangDeclRefExpr(std::string n, SourceLocation loc, std::string spell, std::string ref)
        : ClangExpr(std::move(n), std::move(loc), std::move(spell)), referenced_decl(std::move(ref)) {}
    std::string dump(int indent) const override;
};

struct ClangIntegerLiteral : ClangExpr {
    int64_t value;

    ClangIntegerLiteral(std::string n, SourceLocation loc, std::string spell, int64_t val)
        : ClangExpr(std::move(n), std::move(loc), std::move(spell)), value(val) {}
    std::string dump(int indent) const override;
};

struct ClangStringLiteral : ClangExpr {
    std::string value;

    ClangStringLiteral(std::string n, SourceLocation loc, std::string spell, std::string val)
        : ClangExpr(std::move(n), std::move(loc), std::move(spell)), value(std::move(val)) {}
    std::string dump(int indent) const override;
};

struct ClangMemberRefExpr : ClangExpr {
    std::shared_ptr<ClangAstNode> base;
    std::string member_name;
    bool is_arrow;  // true for ->, false for .

    ClangMemberRefExpr(std::string n, SourceLocation loc, std::string spell, std::string member, bool arrow)
        : ClangExpr(std::move(n), std::move(loc), std::move(spell)), member_name(std::move(member)), is_arrow(arrow) {}
    std::string dump(int indent) const override;
};

struct ClangArraySubscriptExpr : ClangExpr {
    std::shared_ptr<ClangAstNode> base;
    std::shared_ptr<ClangAstNode> index;

    ClangArraySubscriptExpr(std::string n, SourceLocation loc, std::string spell)
        : ClangExpr(std::move(n), std::move(loc), std::move(spell)) {}
    std::string dump(int indent) const override;
};

// Preprocessor nodes (Phase 2)
struct ClangPreprocessor : ClangAstNode {
    ClangPreprocessor(std::string n, SourceLocation loc, std::string spell)
        : ClangAstNode(std::move(n), std::move(loc), std::move(spell)) {}
};

struct ClangMacroDefinition : ClangPreprocessor {
    std::string macro_name;
    std::vector<std::string> params;       // Empty if no parameters
    std::string replacement_text;
    bool is_function_like;                  // true if macro takes parameters

    ClangMacroDefinition(std::string n, SourceLocation loc, std::string spell)
        : ClangPreprocessor(std::move(n), std::move(loc), std::move(spell)), is_function_like(false) {}
    std::string dump(int indent) const override;
};

struct ClangMacroExpansion : ClangPreprocessor {
    std::string macro_name;
    SourceLocation definition_location;    // Where macro was defined

    ClangMacroExpansion(std::string n, SourceLocation loc, std::string spell)
        : ClangPreprocessor(std::move(n), std::move(loc), std::move(spell)) {}
    std::string dump(int indent) const override;
};

struct ClangInclusionDirective : ClangPreprocessor {
    std::string included_file;
    bool is_angled;                        // true for <file.h>, false for "file.h"
    std::string resolved_path;             // Full path to included file

    ClangInclusionDirective(std::string n, SourceLocation loc, std::string spell)
        : ClangPreprocessor(std::move(n), std::move(loc), std::move(spell)), is_angled(false) {}
    std::string dump(int indent) const override;
};

enum class CppExprTag : uint8_t {
    Id = 1,
    String = 2,
    Int = 3,
    Call = 4,
    BinOp = 5,
    StreamOut = 6,
    Raw = 7
};

enum class CppStmtTag : uint8_t {
    ExprStmt = 1,
    Return = 2,
    Raw = 3,
    VarDecl = 4,
    RangeForRef = 5
};

//
// libclang Parser Context and Visitor
//

struct ClangParser {
    Vfs& vfs;
    CXTranslationUnit tu;
    std::string filename;
    size_t node_counter;

    ClangParser(Vfs& v) : vfs(v), tu(nullptr), node_counter(0) {}
    ~ClangParser();

    // Parse a C++ file
    bool parseFile(const std::string& filepath, const std::string& vfs_target_path);
    bool parseString(const std::string& source, const std::string& filename, const std::string& vfs_target_path);

    // Convert libclang cursor to VFS AST node
    std::shared_ptr<ClangAstNode> convertCursor(CXCursor cursor);

    // Extract source location from cursor
    static SourceLocation getLocation(CXCursor cursor);

    // Extract type information
    static std::string getTypeString(CXType type);
    std::shared_ptr<ClangType> convertType(CXType type);

    // Node naming helpers
    std::string generateNodeName(const std::string& kind);

private:
    // Recursive visitor
    void visitChildren(CXCursor cursor, std::shared_ptr<ClangAstNode> parent_node);

    // Cursor kind dispatch
    std::shared_ptr<ClangAstNode> handleDeclaration(CXCursor cursor);
    std::shared_ptr<ClangAstNode> handleStatement(CXCursor cursor);
    std::shared_ptr<ClangAstNode> handleExpression(CXCursor cursor);
    std::shared_ptr<ClangAstNode> handlePreprocessor(CXCursor cursor);  // Phase 2
};

// Shell commands for parsing
void cmd_parse_file(Vfs& vfs, const std::vector<std::string>& args);
void cmd_parse_dump(Vfs& vfs, const std::vector<std::string>& args);
void cmd_parse_generate(Vfs& vfs, const std::vector<std::string>& args);


void save_overlay_to_file(Vfs& vfs, size_t overlayId, const std::string& hostPath);
void update_directory_context(Vfs& vfs, WorkingDirectory& cwd, const std::string& absPath);

const char* policy_label(WorkingDirectory::ConflictPolicy policy);
std::optional<WorkingDirectory::ConflictPolicy> parse_policy(const std::string& name);
void adjust_context_after_unmount(Vfs& vfs, WorkingDirectory& cwd, size_t removedId);
