#ifndef _Clang_ClangParser_h_
#define _Clang_ClangParser_h_

#include <Core/Core.h>
#include <atomic>                     // For std::atomic<bool>
#include "../VfsCore/VfsCore.h"   // For VfsNode and Vfs
#include "../VfsShell/Sexp.h"   // For AstNode, Value, Env

using namespace Upp;

// Forward declarations for types defined elsewhere
struct Vfs;
struct VfsNode;
struct CppExpr;
struct CppCompound;
struct SolutionContext;
// WorkingDirectory now defined in VfsCore.h
// struct WorkingDirectory {
//     std::string path = "/";
//     std::vector<size_t> overlays{0};
//     size_t primary_overlay = 0;
//     enum class ConflictPolicy { Manual, Oldest, Newest };
//     ConflictPolicy conflict_policy = ConflictPolicy::Manual;
// };
using TagId = size_t; // Assuming TagId is a typedef

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
String make_unique_overlay_name(Vfs& vfs, String base);
size_t mount_overlay_from_file(Vfs& vfs, const String& name, const String& hostPath);
void maybe_extend_context(Vfs& vfs, WorkingDirectory& cwd);
bool solution_save(Vfs& vfs, SolutionContext& sol, bool quiet);
void attach_solution_shortcut(Vfs& vfs, SolutionContext& sol);
bool load_solution_from_file(Vfs& vfs, WorkingDirectory& cwd, SolutionContext& sol,
                             const std::filesystem::path& file, bool auto_detected);
std::pair<std::string, std::string> serialize_ast_node(const std::shared_ptr<AstNode>& node);
One<AstNode> deserialize_ast_node(const String& data);
One<AstNode> deserialize_ast_node(const String& type,
                                                     const String& payload,
                                                     const String& path,
                                                     std::vector<std::function<void()>>& fixups,
                                                     std::unordered_map<std::string, std::shared_ptr<VfsNode>>& path_map);
One<AstNode> deserialize_s_ast_node(const String& type, const String& payload);
uint64_t fnv1a64(const String& data);
String hash_hex(uint64_t value);
String sanitize_component(const String& s);
String unescape_meta(const String& s);
std::pair<String, String> serialize_s_ast_node(const std::shared_ptr<AstNode>& node);
One<AstNode> deserialize_s_ast_node(const String& type, const String& payload);
bool is_s_ast_type(const String& type);
void serialize_cpp_expr(BinaryWriter& w, const std::shared_ptr<CppExpr>& expr);
std::shared_ptr<CppExpr> deserialize_cpp_expr(BinaryReader& r);
void deserialize_cpp_compound_into(const String& payload,
                                          const String& node_path,
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
    String file;
    unsigned int line;
    unsigned int column;
    unsigned int offset;
    unsigned int length;  // Length in bytes of the code span

    SourceLocation() : line(0), column(0), offset(0), length(0) {}
    SourceLocation(String f, unsigned int l, unsigned int c, unsigned int o = 0, unsigned int len = 0)
        : file(pick(f)), line(l), column(c), offset(o), length(len) {}

    String toString() const;
    String toStringWithLength() const;
};

// Base class for all libclang AST nodes
struct ClangAstNode : AstNode {
    SourceLocation location;
    String spelling;  // Name/identifier from clang_getCursorSpelling
    std::unordered_map<std::string, std::shared_ptr<VfsNode>> ch;

    ClangAstNode(String n, SourceLocation loc, String spell)
        : AstNode(pick(n)), location(loc), spelling(pick(spell)) {}

    bool isDir() const override { return true; }
    std::unordered_map<std::string, std::shared_ptr<VfsNode>>& children() override { return ch; }
    SexpValue eval(Shared<Env>) override { return SexpValue::S(spelling.ToStd()); }
    String read() const override { return spelling; }

    virtual String dump(int indent = 0) const = 0;
    static String ind(int n) { return String().Cat() << n * 2 << ' '; }
    typedef ClangAstNode CLASSNAME;  // Required for THISBACK macros if used
};

// Type nodes
struct ClangType : ClangAstNode {
    String type_name;

    ClangType(String n, SourceLocation loc, String spell, String ty)
        : ClangAstNode(pick(n), loc, pick(spell)), type_name(pick(ty)) {}

    String dump(int indent) const override;
};

struct ClangBuiltinType : ClangType {
    ClangBuiltinType(String n, SourceLocation loc, String spell, String ty)
        : ClangType(pick(n), loc, pick(spell), pick(ty)) {}
    String dump(int indent) const override;
};

struct ClangPointerType : ClangType {
    One<ClangType> pointee;

    ClangPointerType(String n, SourceLocation loc, String spell, String ty)
        : ClangType(pick(n), loc, pick(spell), pick(ty)) {}
    String dump(int indent) const override;
};

struct ClangReferenceType : ClangType {
    One<ClangType> referenced;

    ClangReferenceType(String n, SourceLocation loc, String spell, String ty)
        : ClangType(pick(n), loc, pick(spell), pick(ty)) {}
    String dump(int indent) const override;
};

struct ClangRecordType : ClangType {
    ClangRecordType(String n, SourceLocation loc, String spell, String ty)
        : ClangType(pick(n), loc, pick(spell), pick(ty)) {}
    String dump(int indent) const override;
};

struct ClangFunctionProtoType : ClangType {
    One<ClangType> return_type;
    Vector<One<ClangType>> param_types;

    ClangFunctionProtoType(String n, SourceLocation loc, String spell, String ty)
        : ClangType(pick(n), loc, pick(spell), pick(ty)) {}
    String dump(int indent) const override;
};

// Declaration nodes
struct ClangDecl : ClangAstNode {
    One<ClangType> type;

    ClangDecl(String n, SourceLocation loc, String spell)
        : ClangAstNode(pick(n), loc, pick(spell)) {}
};

struct ClangTranslationUnitDecl : ClangDecl {
    ClangTranslationUnitDecl(String n, SourceLocation loc, String spell)
        : ClangDecl(pick(n), loc, pick(spell)) {}
    String dump(int indent) const override;
};

struct ClangFunctionDecl : ClangDecl {
    String return_type_str;
    Vector<std::pair<String, String>> parameters;  // (type, name)
    One<ClangAstNode> body;  // CompoundStmt or nullptr

    ClangFunctionDecl(String n, SourceLocation loc, String spell)
        : ClangDecl(pick(n), loc, pick(spell)) {}
    String dump(int indent) const override;
};

struct ClangVarDecl : ClangDecl {
    String type_str;
    String var_name;
    One<ClangAstNode> initializer;  // Expr or nullptr

    ClangVarDecl(String n, SourceLocation loc, String spell)
        : ClangDecl(pick(n), loc, pick(spell)) {}
    String dump(int indent) const override;
};

struct ClangParmDecl : ClangDecl {
    String type_str;
    String param_name;

    ClangParmDecl(String n, SourceLocation loc, String spell)
        : ClangDecl(pick(n), loc, pick(spell)) {}
    String dump(int indent) const override;
};

struct ClangFieldDecl : ClangDecl {
    String type_str;
    String field_name;

    ClangFieldDecl(String n, SourceLocation loc, String spell)
        : ClangDecl(pick(n), loc, pick(spell)) {}
    String dump(int indent) const override;
};

struct ClangClassDecl : ClangDecl {
    String class_name;
    Vector<One<ClangAstNode>> bases;  // Base classes
    Vector<One<ClangAstNode>> members;  // Fields, methods

    ClangClassDecl(String n, SourceLocation loc, String spell)
        : ClangDecl(pick(n), loc, pick(spell)) {}
    String dump(int indent) const override;
};

struct ClangStructDecl : ClangDecl {
    String struct_name;
    Vector<One<ClangAstNode>> members;

    ClangStructDecl(String n, SourceLocation loc, String spell)
        : ClangDecl(pick(n), loc, pick(spell)) {}
    String dump(int indent) const override;
};

struct ClangEnumDecl : ClangDecl {
    String enum_name;
    Vector<std::pair<String, int64_t>> enumerators;  // (name, value)

    ClangEnumDecl(String n, SourceLocation loc, String spell)
        : ClangDecl(pick(n), loc, pick(spell)) {}
    String dump(int indent) const override;
};

struct ClangNamespaceDecl : ClangDecl {
    String namespace_name;

    ClangNamespaceDecl(String n, SourceLocation loc, String spell)
        : ClangDecl(pick(n), loc, pick(spell)) {}
    String dump(int indent) const override;
};

struct ClangTypedefDecl : ClangDecl {
    String typedef_name;
    String underlying_type;

    ClangTypedefDecl(String n, SourceLocation loc, String spell)
        : ClangDecl(pick(n), loc, pick(spell)) {}
    String dump(int indent) const override;
};

// Statement nodes
struct ClangStmt : ClangAstNode {
    ClangStmt(String n, SourceLocation loc, String spell)
        : ClangAstNode(pick(n), loc, pick(spell)) {}
};

struct ClangCompoundStmt : ClangStmt {
    Vector<One<ClangAstNode>> statements;

    ClangCompoundStmt(String n, SourceLocation loc, String spell)
        : ClangStmt(pick(n), loc, pick(spell)) {}
    String dump(int indent) const override;
};

struct ClangIfStmt : ClangStmt {
    One<ClangAstNode> condition;
    One<ClangAstNode> then_branch;
    One<ClangAstNode> else_branch;  // nullptr if no else

    ClangIfStmt(String n, SourceLocation loc, String spell)
        : ClangStmt(pick(n), loc, pick(spell)) {}
    String dump(int indent) const override;
};

struct ClangForStmt : ClangStmt {
    One<ClangAstNode> init;
    One<ClangAstNode> condition;
    One<ClangAstNode> increment;
    One<ClangAstNode> body;

    ClangForStmt(String n, SourceLocation loc, String spell)
        : ClangStmt(pick(n), loc, pick(spell)) {}
    String dump(int indent) const override;
};

struct ClangWhileStmt : ClangStmt {
    One<ClangAstNode> condition;
    One<ClangAstNode> body;

    ClangWhileStmt(String n, SourceLocation loc, String spell)
        : ClangStmt(pick(n), loc, pick(spell)) {}
    String dump(int indent) const override;
};

struct ClangReturnStmt : ClangStmt {
    One<ClangAstNode> return_value;  // nullptr for bare return

    ClangReturnStmt(String n, SourceLocation loc, String spell)
        : ClangStmt(pick(n), loc, pick(spell)) {}
    String dump(int indent) const override;
};

struct ClangDeclStmt : ClangStmt {
    Vector<One<ClangAstNode>> declarations;

    ClangDeclStmt(String n, SourceLocation loc, String spell)
        : ClangStmt(pick(n), loc, pick(spell)) {}
    String dump(int indent) const override;
};

struct ClangExprStmt : ClangStmt {
    One<ClangAstNode> expression;

    ClangExprStmt(String n, SourceLocation loc, String spell)
        : ClangStmt(pick(n), loc, pick(spell)) {}
    String dump(int indent) const override;
};

struct ClangBreakStmt : ClangStmt {
    ClangBreakStmt(String n, SourceLocation loc, String spell)
        : ClangStmt(pick(n), loc, pick(spell)) {}
    String dump(int indent) const override;
};

struct ClangContinueStmt : ClangStmt {
    ClangContinueStmt(String n, SourceLocation loc, String spell)
        : ClangStmt(pick(n), loc, pick(spell)) {}
    String dump(int indent) const override;
};

// Expression nodes
struct ClangExpr : ClangAstNode {
    One<ClangType> expr_type;

    ClangExpr(String n, SourceLocation loc, String spell)
        : ClangAstNode(pick(n), loc, pick(spell)) {}
};

struct ClangBinaryOperator : ClangExpr {
    String opcode;  // +, -, *, /, ==, etc.
    One<ClangAstNode> lhs;
    One<ClangAstNode> rhs;

    ClangBinaryOperator(String n, SourceLocation loc, String spell, String op)
        : ClangExpr(pick(n), loc, pick(spell)), opcode(pick(op)) {}
    String dump(int indent) const override;
};

struct ClangUnaryOperator : ClangExpr {
    String opcode;  // ++, --, -, !, etc.
    One<ClangAstNode> operand;
    bool is_prefix;

    ClangUnaryOperator(String n, SourceLocation loc, String spell, String op, bool prefix)
        : ClangExpr(pick(n), loc, pick(spell)), opcode(pick(op)), is_prefix(prefix) {}
    String dump(int indent) const override;
};

struct ClangCallExpr : ClangExpr {
    One<ClangAstNode> callee;
    Vector<One<ClangAstNode>> arguments;

    ClangCallExpr(String n, SourceLocation loc, String spell)
        : ClangExpr(pick(n), loc, pick(spell)) {}
    String dump(int indent) const override;
};

struct ClangDeclRefExpr : ClangExpr {
    String referenced_decl;

    ClangDeclRefExpr(String n, SourceLocation loc, String spell, String ref)
        : ClangExpr(pick(n), loc, pick(spell)), referenced_decl(pick(ref)) {}
    String dump(int indent) const override;
};

struct ClangIntegerLiteral : ClangExpr {
    int64_t value;

    ClangIntegerLiteral(String n, SourceLocation loc, String spell, int64_t val)
        : ClangExpr(pick(n), loc, pick(spell)), value(val) {}
    String dump(int indent) const override;
};

struct ClangStringLiteral : ClangExpr {
    String value;

    ClangStringLiteral(String n, SourceLocation loc, String spell, String val)
        : ClangExpr(pick(n), loc, pick(spell)), value(pick(val)) {}
    String dump(int indent) const override;
};

struct ClangMemberRefExpr : ClangExpr {
    One<ClangAstNode> base;
    String member_name;
    bool is_arrow;  // true for ->, false for .

    ClangMemberRefExpr(String n, SourceLocation loc, String spell, String member, bool arrow)
        : ClangExpr(pick(n), loc, pick(spell)), member_name(pick(member)), is_arrow(arrow) {}
    String dump(int indent) const override;
};

struct ClangArraySubscriptExpr : ClangExpr {
    One<ClangAstNode> base;
    One<ClangAstNode> index;

    ClangArraySubscriptExpr(String n, SourceLocation loc, String spell)
        : ClangExpr(pick(n), loc, pick(spell)) {}
    String dump(int indent) const override;
};

// Preprocessor nodes (Phase 2)
struct ClangPreprocessor : ClangAstNode {
    ClangPreprocessor(String n, SourceLocation loc, String spell)
        : ClangAstNode(pick(n), loc, pick(spell)) {}
};

struct ClangMacroDefinition : ClangPreprocessor {
    String macro_name;
    Vector<String> params;       // Empty if no parameters
    String replacement_text;
    bool is_function_like;                  // true if macro takes parameters

    ClangMacroDefinition(String n, SourceLocation loc, String spell)
        : ClangPreprocessor(pick(n), loc, pick(spell)), is_function_like(false) {}
    String dump(int indent) const override;
};

struct ClangMacroExpansion : ClangPreprocessor {
    String macro_name;
    SourceLocation definition_location;    // Where macro was defined

    ClangMacroExpansion(String n, SourceLocation loc, String spell)
        : ClangPreprocessor(pick(n), loc, pick(spell)) {}
    String dump(int indent) const override;
};

struct ClangInclusionDirective : ClangPreprocessor {
    String included_file;
    bool is_angled;                        // true for <file.h>, false for "file.h"
    String resolved_path;             // Full path to included file

    ClangInclusionDirective(String n, SourceLocation loc, String spell)
        : ClangPreprocessor(pick(n), loc, pick(spell)), is_angled(false) {}
    String dump(int indent) const override;
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
    String filename;
    size_t node_counter;

    ClangParser(Vfs& v) : vfs(v), tu(nullptr), node_counter(0) {}
    ~ClangParser();

    // Parse a C++ file
    bool parseFile(const String& filepath, const String& vfs_target_path);
    bool parseString(const String& source, const String& filename, const String& vfs_target_path);

    // Convert libclang cursor to VFS AST node
    One<ClangAstNode> convertCursor(CXCursor cursor);

    // Extract source location from cursor
    static SourceLocation getLocation(CXCursor cursor);

    // Extract type information
    static String getTypeString(CXType type);
    One<ClangType> convertType(CXType type);

    // Node naming helpers
    String generateNodeName(const String& kind);

private:
    // Recursive visitor
    void visitChildren(CXCursor cursor, One<ClangAstNode> parent_node);

    // Cursor kind dispatch
    One<ClangAstNode> handleDeclaration(CXCursor cursor);
    One<ClangAstNode> handleStatement(CXCursor cursor);
    One<ClangAstNode> handleExpression(CXCursor cursor);
    One<ClangAstNode> handlePreprocessor(CXCursor cursor);  // Phase 2
};

// Shell commands for parsing
void cmd_parse_file(Vfs& vfs, const Vector<String>& args);
void cmd_parse_dump(Vfs& vfs, const Vector<String>& args);
void cmd_parse_generate(Vfs& vfs, const Vector<String>& args);


void save_overlay_to_file(Vfs& vfs, size_t overlayId, const String& hostPath);
void update_directory_context(Vfs& vfs, WorkingDirectory& cwd, const String& absPath);

const char* policy_label(WorkingDirectory::ConflictPolicy policy);
std::optional<WorkingDirectory::ConflictPolicy> parse_policy(const String& name);
void adjust_context_after_unmount(Vfs& vfs, WorkingDirectory& cwd, size_t removedId);
#endif
