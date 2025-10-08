#ifndef _VfsShell_codex_h_
#define _VfsShell_codex_h_

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <queue>
#include <unordered_set>

#ifdef CODEX_TRACE
#include <mutex>
#include <sstream>

namespace codex_trace {
    void log_line(const std::string& line);

    inline std::string concat(){ return {}; }

    template<typename... Args>
    std::string concat(Args&&... args){
        std::ostringstream oss;
        (oss << ... << std::forward<Args>(args));
        return oss.str();
    }

    struct Scope {
        std::string name;
        Scope(const char* fn, const std::string& details);
        ~Scope();
    };

    void log_loop(const char* tag, const std::string& details);
}

#define CODEX_TRACE_CAT(a,b) CODEX_TRACE_CAT_1(a,b)
#define CODEX_TRACE_CAT_1(a,b) a##b

#define TRACE_FN(...) auto CODEX_TRACE_CAT(_codex_trace_scope_, __LINE__) = ::codex_trace::Scope(__func__, ::codex_trace::concat(__VA_ARGS__))
#define TRACE_MSG(...) ::codex_trace::log_line(::codex_trace::concat(__VA_ARGS__))
#define TRACE_LOOP(tag, ...) ::codex_trace::log_loop(tag, ::codex_trace::concat(__VA_ARGS__))
#else
#define TRACE_FN(...) (void)0
#define TRACE_MSG(...) (void)0
#define TRACE_LOOP(...) (void)0
#endif

//
// Internationalization (i18n)
//
#ifndef CODEX_DISABLE_I18N
#define CODEX_I18N_ENABLED
#endif

namespace i18n {
    enum class MsgId {
        WELCOME,
        UNKNOWN_COMMAND,
        DISCUSS_HINT,
        // Add more message IDs as needed
    };

    const char* get(MsgId id);
    void init();
}

#define _(msg_id) ::i18n::get(::i18n::MsgId::msg_id)

//
// Tag Registry (enumerated tags for memory efficiency)
//
using TagId = uint32_t;
constexpr TagId TAG_INVALID = 0;
using TagSet = std::set<TagId>;

struct TagRegistry {
    std::map<std::string, TagId> name_to_id;
    std::map<TagId, std::string> id_to_name;
    TagId next_id = 1;

    TagId registerTag(const std::string& name);
    TagId getTagId(const std::string& name) const;
    std::string getTagName(TagId id) const;
    bool hasTag(const std::string& name) const;
    std::vector<std::string> allTags() const;
};

//
// Logic system for tag theorem proving
//
enum class LogicOp { VAR, NOT, AND, OR, IMPLIES };

struct LogicFormula {
    LogicOp op;
    TagId var_id;  // for VAR
    std::vector<std::shared_ptr<LogicFormula>> children;

    static std::shared_ptr<LogicFormula> makeVar(TagId id);
    static std::shared_ptr<LogicFormula> makeNot(std::shared_ptr<LogicFormula> f);
    static std::shared_ptr<LogicFormula> makeAnd(std::vector<std::shared_ptr<LogicFormula>> fs);
    static std::shared_ptr<LogicFormula> makeOr(std::vector<std::shared_ptr<LogicFormula>> fs);
    static std::shared_ptr<LogicFormula> makeImplies(std::shared_ptr<LogicFormula> lhs, std::shared_ptr<LogicFormula> rhs);

    bool evaluate(const TagSet& tags) const;
    std::string toString(const TagRegistry& reg) const;
};

struct ImplicationRule {
    std::string name;
    std::shared_ptr<LogicFormula> premise;
    std::shared_ptr<LogicFormula> conclusion;
    float confidence;  // 0.0 to 1.0, 1.0 = always true
    std::string source;  // "hardcoded", "learned", "ai-generated", "user"

    ImplicationRule(std::string n, std::shared_ptr<LogicFormula> p, std::shared_ptr<LogicFormula> c,
                   float conf = 1.0f, std::string src = "hardcoded")
        : name(std::move(n)), premise(p), conclusion(c), confidence(conf), source(std::move(src)) {}
};

struct LogicEngine {
    std::vector<ImplicationRule> rules;
    TagRegistry* tag_registry;

    explicit LogicEngine(TagRegistry* reg) : tag_registry(reg) {}

    // Add rules
    void addRule(const ImplicationRule& rule);
    void addHardcodedRules();  // built-in domain knowledge

    // Forward chaining: infer all implied tags from given tags
    TagSet inferTags(const TagSet& initial_tags, float min_confidence = 0.8f) const;

    // Consistency checking
    struct ConflictInfo {
        std::string description;
        std::vector<std::string> conflicting_tags;
        std::vector<std::string> suggestions;
    };
    std::optional<ConflictInfo> checkConsistency(const TagSet& tags) const;

    // Find contradictions in formula (SAT solving)
    bool isSatisfiable(std::shared_ptr<LogicFormula> formula) const;

    // Explain why tags are inferred
    std::vector<std::string> explainInference(TagId tag, const TagSet& initial_tags) const;
};

struct TagMiningSession {
    TagSet user_provided_tags;
    TagSet inferred_tags;
    std::vector<std::string> pending_questions;  // questions to ask user
    std::map<std::string, bool> user_feedback;   // tag_name -> confirmed

    void addUserTag(TagId tag);
    void recordFeedback(const std::string& tag_name, bool confirmed);
};

//
// VFS perus
//
struct VfsNode : std::enable_shared_from_this<VfsNode> {
    enum class Kind { Dir, File, Ast, Mount, Library };
    std::string name;
    std::weak_ptr<VfsNode> parent;
    Kind kind;
    explicit VfsNode(std::string n, Kind k) : name(std::move(n)), kind(k) {}
    virtual ~VfsNode() = default;
    virtual bool isDir() const { return kind == Kind::Dir; }
    virtual std::string read() const { return ""; }
    virtual void write(const std::string&) {}
    virtual std::map<std::string, std::shared_ptr<VfsNode>>& children() {
        static std::map<std::string, std::shared_ptr<VfsNode>> empty;
        return empty;
    }
};

struct DirNode : VfsNode {
    std::map<std::string, std::shared_ptr<VfsNode>> ch;
    explicit DirNode(std::string n) : VfsNode(std::move(n), Kind::Dir) {}
    bool isDir() const override { return true; }
    std::map<std::string, std::shared_ptr<VfsNode>>& children() override { return ch; }
};

struct FileNode : VfsNode {
    std::string content;
    FileNode(std::string n, std::string c = "")
        : VfsNode(std::move(n), Kind::File), content(std::move(c)) {}
    std::string read() const override { return content; }
    void write(const std::string& s) override { const_cast<std::string&>(content) = s; }
};

struct MountNode : VfsNode {
    std::string host_path;
    mutable std::map<std::string, std::shared_ptr<VfsNode>> cache;
    MountNode(std::string n, std::string hp);
    bool isDir() const override;
    std::string read() const override;
    void write(const std::string& s) override;
    std::map<std::string, std::shared_ptr<VfsNode>>& children() override;
private:
    void populateCache() const;
};

struct LibraryNode : VfsNode {
    std::string lib_path;
    void* handle;
    std::map<std::string, std::shared_ptr<VfsNode>> symbols;
    LibraryNode(std::string n, std::string lp);
    ~LibraryNode() override;
    bool isDir() const override { return true; }
    std::map<std::string, std::shared_ptr<VfsNode>>& children() override { return symbols; }
};

struct LibrarySymbolNode : VfsNode {
    void* func_ptr;
    std::string signature;
    LibrarySymbolNode(std::string n, void* ptr, std::string sig);
    std::string read() const override { return signature; }
};

struct RemoteNode : VfsNode {
    std::string host;
    int port;
    std::string remote_path;  // VFS path on remote server
    mutable int sock_fd;
    mutable std::map<std::string, std::shared_ptr<VfsNode>> cache;
    mutable bool cache_valid;
    mutable std::mutex conn_mutex;

    RemoteNode(std::string n, std::string h, int p, std::string rp);
    ~RemoteNode() override;

    bool isDir() const override;
    std::string read() const override;
    void write(const std::string& s) override;
    std::map<std::string, std::shared_ptr<VfsNode>>& children() override;

private:
    void ensureConnected() const;
    void disconnect() const;
    std::string execRemote(const std::string& command) const;
    void populateCache() const;
};

struct Env; // forward

//
// Arvot S-kielelle
//
struct Value {
    struct Closure {
        std::vector<std::string> params;
        std::shared_ptr<struct AstNode> body;
        std::shared_ptr<Env> env;
    };
    using Builtin = std::function<Value(std::vector<Value>&, std::shared_ptr<Env>)>;
    using List = std::vector<Value>;

    std::variant<int64_t, bool, std::string, Builtin, Closure, List> v;
    Value() : v(int64_t(0)) {}
    static Value I(int64_t x) { Value r; r.v = x; return r; }
    static Value B(bool b)    { Value r; r.v = b; return r; }
    static Value S(std::string s){ Value r; r.v = std::move(s); return r; }
    static Value Built(Builtin f){ Value r; r.v = std::move(f); return r; }
    static Value Clo(Closure c){ Value r; r.v = std::move(c); return r; }
    static Value L(List xs){ Value r; r.v = std::move(xs); return r; }

    std::string show() const;
};

//
// S-AST (perii VfsNode)
//
struct AstNode : VfsNode {
    using VfsNode::VfsNode;
    AstNode(std::string n) : VfsNode(std::move(n), Kind::Ast) {}
    virtual Value eval(std::shared_ptr<Env>) = 0;
};

struct AstInt   : AstNode { int64_t val; AstInt(std::string n, int64_t v); Value eval(std::shared_ptr<Env>) override; };
struct AstBool  : AstNode { bool val;    AstBool(std::string n, bool v);   Value eval(std::shared_ptr<Env>) override; };
struct AstStr   : AstNode { std::string val; AstStr(std::string n, std::string v); Value eval(std::shared_ptr<Env>) override; };
struct AstSym   : AstNode { std::string id; AstSym(std::string n, std::string s);   Value eval(std::shared_ptr<Env>) override; };
struct AstIf    : AstNode {
    std::shared_ptr<AstNode> c,a,b;
    AstIf(std::string n, std::shared_ptr<AstNode> C, std::shared_ptr<AstNode> A, std::shared_ptr<AstNode> B);
    Value eval(std::shared_ptr<Env>) override;
};
struct AstLambda: AstNode {
    std::vector<std::string> params;
    std::shared_ptr<AstNode> body;
    AstLambda(std::string n, std::vector<std::string> ps, std::shared_ptr<AstNode> b);
    Value eval(std::shared_ptr<Env>) override;
};
struct AstCall  : AstNode {
    std::shared_ptr<AstNode> fn;
    std::vector<std::shared_ptr<AstNode>> args;
    AstCall(std::string n, std::shared_ptr<AstNode> f, std::vector<std::shared_ptr<AstNode>> a);
    Value eval(std::shared_ptr<Env>) override;
};
struct AstHolder: AstNode {
    std::shared_ptr<AstNode> inner;
    AstHolder(std::string n, std::shared_ptr<AstNode> in);
    Value eval(std::shared_ptr<Env>) override;
};

//
// Ympäristö
//
struct Env : std::enable_shared_from_this<Env> {
    std::map<std::string, Value> tbl;
    std::shared_ptr<Env> up;
    explicit Env(std::shared_ptr<Env> p = nullptr) : up(std::move(p)) {}
    void set(const std::string& k, const Value& v) { tbl[k]=v; }
    std::optional<Value> get(const std::string& k){
        auto it = tbl.find(k);
        if(it!=tbl.end()) return it->second;
        if(up) return up->get(k);
        return std::nullopt;
    }
};

//
// Tag Storage (must be declared after VfsNode)
//
struct TagStorage {
    std::unordered_map<VfsNode*, TagSet> node_tags;

    void addTag(VfsNode* node, TagId tag);
    void removeTag(VfsNode* node, TagId tag);
    bool hasTag(VfsNode* node, TagId tag) const;
    const TagSet* getTags(VfsNode* node) const;
    void clearTags(VfsNode* node);
    std::vector<VfsNode*> findByTag(TagId tag) const;
    std::vector<VfsNode*> findByTags(const TagSet& tags, bool match_all) const;
};

//
// VFS
//
struct Vfs {
    struct Overlay {
        std::string name;
        std::shared_ptr<DirNode> root;
        std::string source_file;  // path to original source file (e.g., "foo.cpp")
        std::string source_hash;  // BLAKE3 hash of source file
    };
    struct OverlayHit {
        size_t overlay_id;
        std::shared_ptr<VfsNode> node;
    };
    struct DirListingEntry {
        std::set<char> types;
        std::vector<size_t> overlays;
        std::vector<std::shared_ptr<VfsNode>> nodes;
    };
    using DirListing = std::map<std::string, DirListingEntry>;

    std::shared_ptr<DirNode> root = std::make_shared<DirNode>("/");
    std::vector<Overlay> overlay_stack;
    std::vector<bool> overlay_dirty;
    std::vector<std::string> overlay_source;

    // Tag system (separate from VfsNode to keep it POD-friendly)
    TagRegistry tag_registry;
    TagStorage tag_storage;
    LogicEngine logic_engine;
    std::optional<TagMiningSession> mining_session;

    Vfs();

    static std::vector<std::string> splitPath(const std::string& p);

    size_t overlayCount() const;
    const std::string& overlayName(size_t id) const;
    std::optional<size_t> findOverlayByName(const std::string& name) const;
    std::shared_ptr<DirNode> overlayRoot(size_t id) const;
    bool overlayDirty(size_t id) const;
    const std::string& overlaySource(size_t id) const;
    void clearOverlayDirty(size_t id);
    void setOverlaySource(size_t id, std::string path);
    void markOverlayDirty(size_t id);
    size_t registerOverlay(std::string name, std::shared_ptr<DirNode> overlayRoot);
    void unregisterOverlay(size_t overlayId);

    std::vector<size_t> overlaysForPath(const std::string& path) const;
    std::vector<OverlayHit> resolveMulti(const std::string& path) const;
    std::vector<OverlayHit> resolveMulti(const std::string& path, const std::vector<size_t>& allowed) const;
    std::shared_ptr<VfsNode> resolve(const std::string& path);
    std::shared_ptr<VfsNode> resolveForOverlay(const std::string& path, size_t overlayId);
    std::shared_ptr<VfsNode> tryResolveForOverlay(const std::string& path, size_t overlayId) const;
    std::shared_ptr<DirNode> ensureDir(const std::string& path, size_t overlayId = 0);
    std::shared_ptr<DirNode> ensureDirForOverlay(const std::string& path, size_t overlayId);

    void mkdir(const std::string& p, size_t overlayId = 0);
    void touch(const std::string& p, size_t overlayId = 0);
    void write(const std::string& p, const std::string& data, size_t overlayId = 0);
    std::string read(const std::string& p, std::optional<size_t> overlayId = std::nullopt) const;
    void addNode(const std::string& dirpath, std::shared_ptr<VfsNode> n, size_t overlayId = 0);
    void rm(const std::string& p, size_t overlayId = 0);
    void mv(const std::string& src, const std::string& dst, size_t overlayId = 0);
    void link(const std::string& src, const std::string& dst, size_t overlayId = 0);

    DirListing listDir(const std::string& p, const std::vector<size_t>& overlays) const;
    void ls(const std::string& p);
    void tree(std::shared_ptr<VfsNode> n = nullptr, std::string pref = "");

    // Advanced tree visualization options
    struct TreeOptions {
        bool use_box_chars = true;      // Use box-drawing characters (├─, └─, │)
        bool show_sizes = false;         // Show token/size estimates
        bool show_tags = false;          // Show tags inline
        bool use_colors = false;         // ANSI color coding by type
        int max_depth = -1;              // -1 = unlimited
        std::string filter_pattern;      // Only show matching paths
        bool sort_entries = false;       // Sort children alphabetically
        bool show_node_kind = false;     // Show kind indicator (D/F/A/M/etc)
    };

    void treeAdvanced(std::shared_ptr<VfsNode> n, const std::string& path,
                     const TreeOptions& opts, int depth = 0, bool is_last = true);
    void treeAdvanced(const std::string& path, const TreeOptions& opts);
    std::string formatTreeNode(VfsNode* node, const std::string& path, const TreeOptions& opts);

    // Mount management
    enum class MountType { Filesystem, Library, Remote };
    struct MountInfo {
        std::string vfs_path;
        std::string host_path;  // For filesystem/library, or "host:port" for remote
        std::shared_ptr<VfsNode> mount_node;
        MountType type;
    };
    std::vector<MountInfo> mounts;
    bool mount_allowed = true;

    void mountFilesystem(const std::string& host_path, const std::string& vfs_path, size_t overlayId = 0);
    void mountLibrary(const std::string& lib_path, const std::string& vfs_path, size_t overlayId = 0);
    void mountRemote(const std::string& host, int port, const std::string& remote_path, const std::string& vfs_path, size_t overlayId = 0);
    void unmount(const std::string& vfs_path);
    std::vector<MountInfo> listMounts() const;
    void setMountAllowed(bool allowed);
    bool isMountAllowed() const;

    // Remote filesystem server
    void serveRemoteFilesystem(int port, const std::string& vfs_root = "/");
    void stopRemoteServer();

    // Tag management (helpers that delegate to tag_registry and tag_storage)
    TagId registerTag(const std::string& name);
    TagId getTagId(const std::string& name) const;
    std::string getTagName(TagId id) const;
    bool hasTagRegistered(const std::string& name) const;
    std::vector<std::string> allRegisteredTags() const;

    void addTag(const std::string& vfs_path, const std::string& tag_name);
    void removeTag(const std::string& vfs_path, const std::string& tag_name);
    bool nodeHasTag(const std::string& vfs_path, const std::string& tag_name) const;
    std::vector<std::string> getNodeTags(const std::string& vfs_path) const;
    void clearNodeTags(const std::string& vfs_path);
    std::vector<std::string> findNodesByTag(const std::string& tag_name) const;
    std::vector<std::string> findNodesByTags(const std::vector<std::string>& tag_names, bool match_all) const;
};

extern Vfs* G_VFS; // glob aputinta varten

//
// Stateful VFS Visitor
//
struct VfsVisitor {
    Vfs* vfs;
    std::vector<std::shared_ptr<VfsNode>> results;
    size_t current_index;

    VfsVisitor(Vfs* v) : vfs(v), current_index(0) {}

    // Commands
    void find(const std::string& start_path, const std::string& pattern);
    void findext(const std::string& start_path, const std::string& extension);
    std::string name() const;
    bool isend() const;
    void next();
    void reset() { current_index = 0; }
    size_t count() const { return results.size(); }

private:
    void findRecursive(std::shared_ptr<VfsNode> node, const std::string& path,
                      const std::string& pattern, bool is_extension);
    bool matchesPattern(const std::string& name, const std::string& pattern) const;
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

//
// Exec util
//
std::string exec_capture(const std::string& cmd, const std::string& desc = {});
bool has_cmd(const std::string& c);

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

// helpers
std::shared_ptr<CppTranslationUnit> expect_tu(std::shared_ptr<VfsNode> n);
std::shared_ptr<CppFunction>        expect_fn(std::shared_ptr<VfsNode> n);
std::shared_ptr<CppCompound>        expect_block(std::shared_ptr<VfsNode> n);
void vfs_add(Vfs& vfs, const std::string& path, std::shared_ptr<VfsNode> node, size_t overlayId = 0);
void cpp_dump_to_vfs(Vfs& vfs, size_t overlayId, const std::string& tuPath, const std::string& filePath);

//
// Planner AST nodes (hierarchical planning system)
//
struct PlanNode : AstNode {
    std::string content;  // Text content for the plan node
    std::map<std::string, std::shared_ptr<VfsNode>> ch;

    PlanNode(std::string n, std::string c = "") : AstNode(std::move(n)), content(std::move(c)) {}
    bool isDir() const override { return true; }
    std::map<std::string, std::shared_ptr<VfsNode>>& children() override { return ch; }
    Value eval(std::shared_ptr<Env>) override { return Value::S(content); }
    std::string read() const override { return content; }
    void write(const std::string& s) override { content = s; }
};

// Specific plan node types
struct PlanRoot : PlanNode {
    PlanRoot(std::string n, std::string c = "") : PlanNode(std::move(n), std::move(c)) {}
};

struct PlanSubPlan : PlanNode {
    PlanSubPlan(std::string n, std::string c = "") : PlanNode(std::move(n), std::move(c)) {}
};

struct PlanGoals : PlanNode {
    std::vector<std::string> goals;
    PlanGoals(std::string n) : PlanNode(std::move(n)) {}
    std::string read() const override;
    void write(const std::string& s) override;
};

struct PlanIdeas : PlanNode {
    std::vector<std::string> ideas;
    PlanIdeas(std::string n) : PlanNode(std::move(n)) {}
    std::string read() const override;
    void write(const std::string& s) override;
};

struct PlanStrategy : PlanNode {
    PlanStrategy(std::string n, std::string c = "") : PlanNode(std::move(n), std::move(c)) {}
};

struct PlanJob {
    std::string description;
    int priority;  // Lower number = higher priority
    bool completed;
    std::string assignee;  // "user" or "agent" or specific agent name
};

struct PlanJobs : PlanNode {
    std::vector<PlanJob> jobs;
    PlanJobs(std::string n) : PlanNode(std::move(n)) {}
    std::string read() const override;
    void write(const std::string& s) override;
    void addJob(const std::string& desc, int priority = 100, const std::string& assignee = "");
    void completeJob(size_t index);
    std::vector<size_t> getSortedJobIndices() const;
};

struct PlanDeps : PlanNode {
    std::vector<std::string> dependencies;
    PlanDeps(std::string n) : PlanNode(std::move(n)) {}
    std::string read() const override;
    void write(const std::string& s) override;
};

struct PlanImplemented : PlanNode {
    std::vector<std::string> items;
    PlanImplemented(std::string n) : PlanNode(std::move(n)) {}
    std::string read() const override;
    void write(const std::string& s) override;
};

struct PlanResearch : PlanNode {
    std::vector<std::string> topics;
    PlanResearch(std::string n) : PlanNode(std::move(n)) {}
    std::string read() const override;
    void write(const std::string& s) override;
};

struct PlanNotes : PlanNode {
    PlanNotes(std::string n, std::string c = "") : PlanNode(std::move(n), std::move(c)) {}
};

//
// Planner Context (navigator state)
//
struct PlannerContext {
    std::string current_path;  // Current location in /plan tree
    std::vector<std::string> navigation_history;  // Breadcrumbs for backtracking
    std::set<std::string> visible_nodes;  // Paths of nodes currently "in view" for AI context

    enum class Mode { Forward, Backward };
    Mode mode = Mode::Forward;

    void navigateTo(const std::string& path);
    void forward();   // Move towards leaves (add details)
    void backward();  // Move towards root (revise higher-level plans)
    void addToContext(const std::string& vfs_path);
    void removeFromContext(const std::string& vfs_path);
    void clearContext();
};

//
// Discuss Session (conversation state management)
//
struct DiscussSession {
    std::string session_id;  // Named or random hex session identifier
    std::vector<std::string> conversation_history;  // User + AI messages
    std::string current_plan_path;  // Path to active plan in /plan tree (if any)

    enum class Mode {
        Simple,      // Direct AI queries without planning
        Planning,    // Plan-based discussion with breakdown
        Execution    // Working on pre-planned features
    };
    Mode mode = Mode::Simple;

    bool is_active() const { return !session_id.empty(); }
    void clear();
    void add_message(const std::string& role, const std::string& content);
    std::string generate_session_id();
};

//
// Hash utilities (BLAKE3)
//
std::string compute_file_hash(const std::string& filepath);
std::string compute_string_hash(const std::string& data);

//
// AI helpers (OpenAI + llama.cpp bridge)
//
std::string json_escape(const std::string& s);
std::string build_responses_payload(const std::string& model, const std::string& user_prompt);
std::string call_openai(const std::string& prompt);
std::string call_llama(const std::string& prompt);
std::string call_ai(const std::string& prompt);

//
// Action Planner Context Builder (AI context offloader)
//

// Filter predicates for selecting VFS nodes
struct ContextFilter {
    enum class Type {
        TagAny,        // Node has any of the specified tags
        TagAll,        // Node has all of the specified tags
        TagNone,       // Node has none of the specified tags
        PathPrefix,    // Node path starts with prefix
        PathPattern,   // Node path matches glob pattern
        ContentMatch,  // Node content contains string
        ContentRegex,  // Node content matches regex
        NodeKind,      // Node is specific kind (Dir, File, Ast, etc.)
        Custom         // User-defined lambda predicate
    };

    Type type;
    TagSet tags;                              // For tag-based filters
    std::string pattern;                      // For path/content filters
    VfsNode::Kind kind;                       // For node kind filter
    std::function<bool(VfsNode*)> predicate;  // For custom filter

    // Factory methods
    static ContextFilter tagAny(const TagSet& t);
    static ContextFilter tagAll(const TagSet& t);
    static ContextFilter tagNone(const TagSet& t);
    static ContextFilter pathPrefix(const std::string& prefix);
    static ContextFilter pathPattern(const std::string& pattern);
    static ContextFilter contentMatch(const std::string& substr);
    static ContextFilter contentRegex(const std::string& regex);
    static ContextFilter nodeKind(VfsNode::Kind k);
    static ContextFilter custom(std::function<bool(VfsNode*)> pred);

    // Evaluate filter against node
    bool matches(VfsNode* node, const std::string& path, Vfs& vfs) const;
};

// Context entry with metadata
struct ContextEntry {
    std::string vfs_path;
    VfsNode* node;
    std::string content;
    size_t token_estimate;  // Rough estimate of tokens
    int priority;           // Higher priority = more important
    TagSet tags;            // Tags associated with this node

    ContextEntry(std::string path, VfsNode* n, std::string c, int prio = 100)
        : vfs_path(std::move(path)), node(n), content(std::move(c)),
          token_estimate(estimate_tokens(content)), priority(prio) {}

    static size_t estimate_tokens(const std::string& text);
};

// Context builder for AI calls with token budgets and smart selection
struct ContextBuilder {
    Vfs& vfs;
    TagStorage& tag_storage;
    TagRegistry& tag_registry;

    size_t max_tokens;
    std::vector<ContextEntry> entries;
    std::vector<ContextFilter> filters;

    ContextBuilder(Vfs& v, TagStorage& ts, TagRegistry& tr, size_t max_tok = 4000)
        : vfs(v), tag_storage(ts), tag_registry(tr), max_tokens(max_tok) {}

    // Add filters
    void addFilter(const ContextFilter& filter);
    void addFilter(ContextFilter&& filter);

    // Collect nodes matching filters
    void collect();
    void collectFromPath(const std::string& root_path);

    // Build final context string within token budget
    std::string build();
    std::string buildWithPriority();  // Sort by priority before building

    // Statistics
    size_t totalTokens() const;
    size_t entryCount() const { return entries.size(); }
    void clear();

    // Advanced context building features
    struct ContextOptions {
        bool include_dependencies = false;   // Follow links and include related nodes
        bool semantic_chunking = true;       // Keep related content together
        bool adaptive_budget = false;        // Adjust token budget based on complexity
        bool deduplicate = true;             // Remove redundant content
        int summary_threshold = -1;          // Summarize nodes > N tokens (-1=no summary)
        bool hierarchical = false;           // Build overview + detail sections
        std::string cache_key;               // Optional cache key for reuse
    };

    std::string buildWithOptions(const ContextOptions& opts);
    std::vector<ContextEntry> getDependencies(const ContextEntry& entry);
    std::string summarizeEntry(const ContextEntry& entry, size_t target_tokens);
    void deduplicateEntries();
    std::pair<std::string, std::string> buildHierarchical();  // Returns (overview, details)

    // Compound filter support
    enum class FilterLogic { And, Or, Not };
    void addCompoundFilter(FilterLogic logic, const std::vector<ContextFilter>& subfilters);

private:
    void visitNode(const std::string& path, VfsNode* node);
    bool matchesAnyFilter(const std::string& path, VfsNode* node) const;
    std::unordered_set<std::string> seen_content;  // For deduplication
};

// Code replacement strategy for determining what statements to remove/modify
struct ReplacementStrategy {
    enum class Type {
        ReplaceAll,        // Replace entire node content
        ReplaceRange,      // Replace specific line range
        ReplaceFunction,   // Replace specific function definition
        ReplaceBlock,      // Replace code block (if/for/while)
        InsertBefore,      // Insert before matching location
        InsertAfter,       // Insert after matching location
        DeleteMatching,    // Delete statements matching criteria
        CommentOut         // Comment out instead of deleting
    };

    Type type;
    std::string target_path;    // VFS path of target node
    std::string identifier;     // Function name, variable name, etc.
    size_t start_line;          // For range operations
    size_t end_line;            // For range operations
    std::string match_pattern;  // Regex or literal for matching
    std::string replacement;    // New content to insert

    static ReplacementStrategy replaceAll(const std::string& path, const std::string& content);
    static ReplacementStrategy replaceRange(const std::string& path, size_t start, size_t end, const std::string& content);
    static ReplacementStrategy replaceFunction(const std::string& path, const std::string& func_name, const std::string& content);
    static ReplacementStrategy insertBefore(const std::string& path, const std::string& pattern, const std::string& content);
    static ReplacementStrategy insertAfter(const std::string& path, const std::string& pattern, const std::string& content);
    static ReplacementStrategy deleteMatching(const std::string& path, const std::string& pattern);
    static ReplacementStrategy commentOut(const std::string& path, const std::string& pattern);

    // Apply strategy to VFS
    bool apply(Vfs& vfs) const;
};

// Test suite for action planner hypothesis validation
struct ActionPlannerTest {
    std::string name;
    std::string description;
    std::function<bool()> test_fn;
    bool passed;
    std::string error_message;

    ActionPlannerTest(std::string n, std::string desc, std::function<bool()> fn)
        : name(std::move(n)), description(std::move(desc)), test_fn(std::move(fn)), passed(false) {}

    bool run();
};

struct ActionPlannerTestSuite {
    std::vector<ActionPlannerTest> tests;
    Vfs& vfs;
    TagStorage& tag_storage;
    TagRegistry& tag_registry;

    ActionPlannerTestSuite(Vfs& v, TagStorage& ts, TagRegistry& tr)
        : vfs(v), tag_storage(ts), tag_registry(tr) {}

    void addTest(const std::string& name, const std::string& desc, std::function<bool()> fn);
    void runAll();
    void printResults() const;
    size_t passedCount() const;
    size_t failedCount() const;
};

//
// Hypothesis Testing System (5 progressive complexity levels)
//

// Hypothesis represents a testable proposition about code structure or behavior
struct Hypothesis {
    enum class Level {
        SimpleQuery = 1,       // Find functions/patterns in VFS
        CodeModification = 2,  // Add error handling to existing code
        Refactoring = 3,       // Extract duplicated code
        FeatureAddition = 4,   // Add logging/instrumentation
        Architecture = 5       // Implement design patterns
    };

    Level level;
    std::string description;
    std::string goal;
    std::vector<std::string> assumptions;
    std::vector<std::string> validation_criteria;
    bool tested;
    bool valid;
    std::string result;

    Hypothesis(Level lvl, std::string desc, std::string g)
        : level(lvl), description(std::move(desc)), goal(std::move(g)),
          tested(false), valid(false) {}

    void addAssumption(const std::string& assumption);
    void addValidation(const std::string& criterion);
    std::string levelName() const;
};

// Hypothesis test result with detailed diagnostics
struct HypothesisResult {
    bool success;
    std::string message;
    std::vector<std::string> findings;  // What was found
    std::vector<std::string> actions;   // What would be done
    size_t nodes_examined;
    size_t nodes_matched;

    HypothesisResult() : success(false), nodes_examined(0), nodes_matched(0) {}

    void addFinding(const std::string& finding);
    void addAction(const std::string& action);
    std::string summary() const;
};

// Hypothesis tester performs validation without calling AI
struct HypothesisTester {
    Vfs& vfs;
    TagStorage& tag_storage;
    TagRegistry& tag_registry;
    ContextBuilder context_builder;

    HypothesisTester(Vfs& v, TagStorage& ts, TagRegistry& tr)
        : vfs(v), tag_storage(ts), tag_registry(tr),
          context_builder(v, ts, tr, 4000) {}

    // Level 1: Find function/pattern in VFS
    HypothesisResult testSimpleQuery(const std::string& target, const std::string& search_path = "/");

    // Level 2: Validate error handling addition strategy
    HypothesisResult testErrorHandlingAddition(const std::string& function_name,
                                                const std::string& error_handling_style);

    // Level 3: Identify duplicate code blocks for extraction
    HypothesisResult testDuplicateExtraction(const std::string& search_path,
                                              size_t min_similarity_lines = 3);

    // Level 4: Plan logging instrumentation for error paths
    HypothesisResult testLoggingInstrumentation(const std::string& search_path);

    // Level 5: Evaluate architectural pattern applicability
    HypothesisResult testArchitecturePattern(const std::string& pattern_name,
                                              const std::string& target_path);

    // General hypothesis testing interface
    HypothesisResult test(Hypothesis& hypothesis);

private:
    // Helper methods
    std::vector<std::string> findFunctionDefinitions(const std::string& path);
    std::vector<std::string> findReturnPaths(const std::string& function_content);
    std::vector<std::pair<std::string, std::string>> findDuplicateBlocks(
        const std::string& path, size_t min_lines);
    std::vector<std::string> findErrorPaths(const std::string& path);
    bool contentSimilar(const std::string& a, const std::string& b, size_t min_lines);
};

// Hypothesis test suite for all 5 levels
struct HypothesisTestSuite {
    HypothesisTester tester;
    std::vector<Hypothesis> hypotheses;

    HypothesisTestSuite(Vfs& v, TagStorage& ts, TagRegistry& tr)
        : tester(v, ts, tr) {}

    void addHypothesis(Hypothesis h);
    void runAll();
    void printResults() const;
    size_t validCount() const;
    size_t invalidCount() const;
    size_t untestedCount() const;

    // Create standard test suite with examples for all 5 levels
    void createStandardSuite();
};

#endif
