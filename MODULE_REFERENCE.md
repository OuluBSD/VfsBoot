# VfsBoot Module Quick Reference

**Quick lookup guide for the 20-module refactoring**

## Module Index

| Module | Header Lines | Impl Lines | Key Classes | Depends On |
|--------|--------------|------------|-------------|------------|
| [vfs_common](#vfs_common) | 200 | 250 | - | - |
| [tag_system](#tag_system) | 450 | 250 | TagSet, TagRegistry, TagStorage | vfs_common |
| [logic_engine](#logic_engine) | 250 | 650 | LogicFormula, LogicEngine | tag_system |
| [vfs_core](#vfs_core) | 400 | 700 | VfsNode, Vfs | tag_system, logic_engine |
| [vfs_mount](#vfs_mount) | 200 | 500 | MountNode, RemoteNode | vfs_core |
| [sexp](#sexp) | 250 | 500 | Value, Env, AstNode | vfs_core |
| [cpp_ast](#cpp_ast) | 250 | 500 | CppNode, CppFunction | sexp |
| [clang_parser](#clang_parser) | 550 | 600 | ClangParser, ClangAstNode | sexp, vfs_core |
| [planner](#planner) | 250 | 350 | PlanNode, PlannerContext | sexp, vfs_core |
| [ai_bridge](#ai_bridge) | 50 | 450 | - | vfs_common |
| [context_builder](#context_builder) | 300 | 650 | ContextBuilder, ContextFilter | tag_system, vfs_core |
| [make](#make) | 100 | 400 | MakeFile | vfs_core |
| [hypothesis](#hypothesis) | 250 | 700 | Hypothesis, HypothesisTester | context_builder |
| [scope_store](#scope_store) | 350 | 650 | ScopeStore, BinaryDiff | vfs_core, context_builder |
| [feedback](#feedback) | 350 | 800 | FeedbackLoop, RulePatch | logic_engine, vfs_core |
| [web_server](#web_server) | 50 | TBD | WebServer namespace | vfs_common |
| [shell_commands](#shell_commands) | 100 | 3000+ | - | (all above) |
| [repl](#repl) | 50 | 400 | - | shell_commands |
| [main](#main) | 0 | 300 | - | repl, web_server |
| [ui_backend](#ui_backend) | (existing) | (existing) | UIBackend | - |

---

## vfs_common

**Purpose:** Foundation layer with includes, tracing, i18n, hash utilities

**Source:**
- Header: codex.h lines 1-97
- Impl: codex.cpp lines 76-162, 163-200

**Exports:**
```cpp
// Includes: <algorithm>, <vector>, <map>, <memory>, etc.
// Tracing
#define TRACE_FN(...)
#define TRACE_MSG(...)
#define TRACE_LOOP(...)

// i18n
namespace i18n {
    enum class MsgId { WELCOME, UNKNOWN_COMMAND, ... };
    const char* get(MsgId id);
    void init();
    void set_english_only();
}

// Hash
std::string compute_file_hash(const std::string& filepath);
std::string compute_string_hash(const std::string& data);

// Forward declarations
struct Vfs;
```

---

## tag_system

**Purpose:** Tag registry and bit vector-based tag sets

**Source:**
- Header: codex.h lines 107-422, 673-686
- Impl: codex.cpp lines 3533-3634, 4247-4256

**Exports:**
```cpp
using TagId = uint32_t;
constexpr TagId TAG_INVALID = 0;

// 64-bit chunk-based bit vector
class BitVector {
    void set(size_t bit);
    void clear(size_t bit);
    bool test(size_t bit) const;
    uint64_t hash() const;
};

// Set operations on TagIds
class TagSet {
    void insert(TagId tag);
    void erase(TagId tag);
    bool contains(TagId tag) const;
    size_t size() const;
    // Set ops: |, &, -, ^
    // Subset: isSubsetOf()
};

// Tag name registry
struct TagRegistry {
    TagId registerTag(const std::string& name);
    TagId getTagId(const std::string& name) const;
    std::string getTagName(TagId id) const;
    bool hasTag(const std::string& name) const;
};

// VfsNode -> TagSet mapping
struct TagStorage {
    void addTag(VfsNode* node, TagId tag);
    void removeTag(VfsNode* node, TagId tag);
    bool hasTag(VfsNode* node, TagId tag) const;
    const TagSet* getTags(VfsNode* node) const;
};

// Interactive tag mining
struct TagMiningSession {
    TagSet user_provided_tags;
    TagSet inferred_tags;
    void addUserTag(TagId tag);
    void recordFeedback(const std::string& tag_name, bool confirmed);
};
```

---

## logic_engine

**Purpose:** Theorem proving and rule-based inference

**Source:**
- Header: codex.h lines 424-506
- Impl: codex.cpp lines 3635-4246

**Exports:**
```cpp
enum class LogicOp { VAR, NOT, AND, OR, IMPLIES };

struct LogicFormula {
    static std::shared_ptr<LogicFormula> makeVar(TagId id);
    static std::shared_ptr<LogicFormula> makeNot(std::shared_ptr<LogicFormula> f);
    static std::shared_ptr<LogicFormula> makeAnd(...);
    static std::shared_ptr<LogicFormula> makeOr(...);
    static std::shared_ptr<LogicFormula> makeImplies(...);

    bool evaluate(const TagSet& tags) const;
    std::string toString(const TagRegistry& reg) const;
};

struct ImplicationRule {
    std::string name;
    std::shared_ptr<LogicFormula> premise;
    std::shared_ptr<LogicFormula> conclusion;
    float confidence;  // 0.0 to 1.0
    std::string source;  // "hardcoded", "learned", etc.
};

struct LogicEngine {
    void addRule(const ImplicationRule& rule);
    void addHardcodedRules();

    // Forward chaining
    TagSet inferTags(const TagSet& initial_tags, float min_confidence = 0.8f) const;

    // Consistency
    std::optional<ConflictInfo> checkConsistency(const TagSet& tags) const;

    // Explanation
    std::vector<std::string> explainInference(TagId tag, const TagSet& initial_tags) const;

    // Persistence
    void saveRulesToVfs(Vfs& vfs, const std::string& base_path = "/plan/rules") const;
    void loadRulesFromVfs(Vfs& vfs, const std::string& base_path = "/plan/rules");
};
```

---

## vfs_core

**Purpose:** Core virtual filesystem with overlay system

**Source:**
- Header: codex.h lines 507-541, 687-812, 814-839
- Impl: codex.cpp lines 2671-3176, 4257-4344

**Exports:**
```cpp
struct VfsNode : std::enable_shared_from_this<VfsNode> {
    enum class Kind { Dir, File, Ast, Mount, Library };
    std::string name;
    std::weak_ptr<VfsNode> parent;
    Kind kind;

    virtual bool isDir() const;
    virtual std::string read() const;
    virtual void write(const std::string&);
    virtual std::map<std::string, std::shared_ptr<VfsNode>>& children();
};

struct DirNode : VfsNode {
    std::map<std::string, std::shared_ptr<VfsNode>> ch;
};

struct FileNode : VfsNode {
    std::string content;
};

struct Vfs {
    std::shared_ptr<DirNode> root;
    std::vector<Overlay> overlay_stack;
    TagRegistry tag_registry;
    TagStorage tag_storage;
    LogicEngine logic_engine;

    // Path operations
    static std::vector<std::string> splitPath(const std::string& p);
    std::shared_ptr<VfsNode> resolve(const std::string& path);

    // File operations
    void mkdir(const std::string& p, size_t overlayId = 0);
    void touch(const std::string& p, size_t overlayId = 0);
    void write(const std::string& p, const std::string& data, size_t overlayId = 0);
    std::string read(const std::string& p, std::optional<size_t> overlayId = std::nullopt) const;
    void rm(const std::string& p, size_t overlayId = 0);
    void mv(const std::string& src, const std::string& dst, size_t overlayId = 0);

    // Directory operations
    void ls(const std::string& p);
    void tree(std::shared_ptr<VfsNode> n = nullptr, std::string pref = "");

    // Overlay management
    size_t registerOverlay(std::string name, std::shared_ptr<DirNode> overlayRoot);
    void unregisterOverlay(size_t overlayId);

    // Tag helpers
    void addTag(const std::string& vfs_path, const std::string& tag_name);
    std::vector<std::string> findNodesByTag(const std::string& tag_name) const;
};

struct VfsVisitor {
    Vfs* vfs;
    void find(const std::string& start_path, const std::string& pattern);
    std::string name() const;
    void next();
};

extern Vfs* G_VFS;
```

---

## vfs_mount

**Purpose:** Mount points for filesystem, libraries, remote VFS

**Source:**
- Header: codex.h lines 542-593
- Impl: codex.cpp lines 3177-3532

**Exports:**
```cpp
struct MountNode : VfsNode {
    std::string host_path;
    mutable std::map<std::string, std::shared_ptr<VfsNode>> cache;

    bool isDir() const override;
    std::string read() const override;
    void write(const std::string& s) override;
};

struct LibraryNode : VfsNode {
    std::string lib_path;
    void* handle;  // dlopen handle
    std::map<std::string, std::shared_ptr<VfsNode>> symbols;
};

struct RemoteNode : VfsNode {
    std::string host;
    int port;
    std::string remote_path;

    bool isDir() const override;
    std::string read() const override;
    void write(const std::string& s) override;
};

// In Vfs class:
void mountFilesystem(const std::string& host_path, const std::string& vfs_path, size_t overlayId = 0);
void mountLibrary(const std::string& lib_path, const std::string& vfs_path, size_t overlayId = 0);
void mountRemote(const std::string& host, int port, const std::string& remote_path, const std::string& vfs_path, size_t overlayId = 0);
void unmount(const std::string& vfs_path);
```

---

## sexp

**Purpose:** S-expression parser and evaluator

**Source:**
- Header: codex.h lines 594-671, 840-851
- Impl: codex.cpp lines 2606-2670, 4345-4428, 4429-4571

**Exports:**
```cpp
struct Value {
    using Builtin = std::function<Value(std::vector<Value>&, std::shared_ptr<Env>)>;
    using List = std::vector<Value>;

    std::variant<int64_t, bool, std::string, Builtin, Closure, List> v;

    static Value I(int64_t x);
    static Value B(bool b);
    static Value S(std::string s);
    static Value Built(Builtin f);
    static Value Clo(Closure c);
    static Value L(List xs);

    std::string show() const;
};

struct Env : std::enable_shared_from_this<Env> {
    std::map<std::string, Value> tbl;
    std::shared_ptr<Env> up;

    void set(const std::string& k, const Value& v);
    std::optional<Value> get(const std::string& k);
};

struct AstNode : VfsNode {
    virtual Value eval(std::shared_ptr<Env>) = 0;
};

struct AstInt : AstNode { int64_t val; };
struct AstBool : AstNode { bool val; };
struct AstStr : AstNode { std::string val; };
struct AstSym : AstNode { std::string id; };
struct AstIf : AstNode { /* c, a, b */ };
struct AstLambda : AstNode { /* params, body */ };
struct AstCall : AstNode { /* fn, args */ };

// Parser
struct Token { std::string s; };
std::vector<Token> lex(const std::string& src);
std::shared_ptr<AstNode> parse(const std::string& src);

// Builtins
void install_builtins(std::shared_ptr<Env> g);
```

---

## cpp_ast

**Purpose:** C++ AST builder for code generation

**Source:**
- Header: codex.h lines 852-963
- Impl: codex.cpp lines 4572-4786

**Exports:**
```cpp
struct CppNode : AstNode {
    virtual std::string dump(int indent = 0) const = 0;
};

struct CppInclude : CppNode { std::string header; bool angled; };
struct CppExpr : CppNode {};
struct CppId : CppExpr { std::string id; };
struct CppString : CppExpr {
    std::string s;
    static std::string esc(const std::string& x);
};
struct CppInt : CppExpr { long long v; };
struct CppCall : CppExpr { /* fn, args */ };
struct CppBinOp : CppExpr { std::string op; /* a, b */ };

struct CppStmt : CppNode {};
struct CppExprStmt : CppStmt { /* e */ };
struct CppReturn : CppStmt { /* e */ };
struct CppVarDecl : CppStmt { std::string type, name, init; };
struct CppCompound : CppStmt { std::vector<std::shared_ptr<CppStmt>> stmts; };

struct CppFunction : CppNode {
    std::string retType, name;
    std::vector<CppParam> params;
    std::shared_ptr<CppCompound> body;
};

struct CppTranslationUnit : CppNode {
    std::vector<std::shared_ptr<CppInclude>> includes;
    std::vector<std::shared_ptr<CppFunction>> funcs;
};

// Helpers
std::shared_ptr<CppTranslationUnit> expect_tu(std::shared_ptr<VfsNode> n);
void cpp_dump_to_vfs(Vfs& vfs, size_t overlayId, const std::string& tuPath, const std::string& filePath);
```

---

## clang_parser

**Purpose:** libclang integration for parsing C++ code

**Source:**
- Header: codex.h lines 974-1400
- Impl: codex.cpp lines 569-1031

**Exports:**
```cpp
struct SourceLocation {
    std::string file;
    unsigned int line, column, offset, length;
    std::string toString() const;
};

struct ClangAstNode : AstNode {
    SourceLocation location;
    std::string spelling;
    std::map<std::string, std::shared_ptr<VfsNode>> ch;

    virtual std::string dump(int indent = 0) const = 0;
};

struct ClangType : ClangAstNode { std::string type_name; };
struct ClangDecl : ClangAstNode {};
struct ClangFunctionDecl : ClangDecl { /* return_type, params, body */ };
struct ClangVarDecl : ClangDecl { /* type_str, var_name, initializer */ };

struct ClangStmt : ClangAstNode {};
struct ClangIfStmt : ClangStmt { /* condition, then_branch, else_branch */ };
struct ClangForStmt : ClangStmt { /* init, condition, increment, body */ };

struct ClangExpr : ClangAstNode {};
struct ClangBinaryOperator : ClangExpr { std::string opcode; /* lhs, rhs */ };
struct ClangCallExpr : ClangExpr { /* callee, arguments */ };

struct ClangParser {
    Vfs& vfs;
    CXTranslationUnit tu;

    bool parseFile(const std::string& filepath, const std::string& vfs_target_path);
    std::shared_ptr<ClangAstNode> convertCursor(CXCursor cursor);
    static SourceLocation getLocation(CXCursor cursor);
};
```

---

## planner

**Purpose:** Planning system with hierarchical goals

**Source:**
- Header: codex.h lines 1401-1523
- Impl: codex.cpp lines 4787-5049

**Exports:**
```cpp
struct PlanNode : AstNode {
    std::string content;
    std::map<std::string, std::shared_ptr<VfsNode>> ch;
};

struct PlanRoot : PlanNode {};
struct PlanSubPlan : PlanNode {};
struct PlanGoals : PlanNode { std::vector<std::string> goals; };
struct PlanIdeas : PlanNode { std::vector<std::string> ideas; };
struct PlanStrategy : PlanNode {};
struct PlanJobs : PlanNode {
    std::vector<PlanJob> jobs;
    void addJob(const std::string& desc, int priority = 100, const std::string& assignee = "");
    void completeJob(size_t index);
};

struct PlannerContext {
    std::string current_path;
    std::vector<std::string> navigation_history;
    std::set<std::string> visible_nodes;

    void navigateTo(const std::string& path);
    void forward();
    void backward();
    void addToContext(const std::string& vfs_path);
};

struct DiscussSession {
    std::string session_id;
    std::vector<std::string> conversation_history;
    enum class Mode { Simple, Planning, Execution };
    Mode mode;

    void clear();
    void add_message(const std::string& role, const std::string& content);
};
```

---

## ai_bridge

**Purpose:** OpenAI and Llama.cpp integration

**Source:**
- Header: codex.h lines 1524-1537
- Impl: codex.cpp lines 5050-5458

**Exports:**
```cpp
std::string build_responses_payload(const std::string& model, const std::string& user_prompt);
std::string call_openai(const std::string& prompt);
std::string call_llama(const std::string& prompt);
std::string call_ai(const std::string& prompt);  // Dispatch to OpenAI or Llama
```

**Environment Variables:**
- `OPENAI_API_KEY` or `~/openai-key.txt`
- `LLAMA_BASE_URL` or `LLAMA_SERVER` (default: http://192.168.1.169:8080)
- `CODEX_AI_PROVIDER` (force "openai" or "llama")

---

## context_builder

**Purpose:** AI context management with token budgets

**Source:**
- Header: codex.h lines 1538-1714
- Impl: codex.cpp lines 5459-6067

**Exports:**
```cpp
struct ContextFilter {
    enum class Type { TagAny, TagAll, PathPrefix, ContentMatch, ... };

    static ContextFilter tagAny(const TagSet& t);
    static ContextFilter pathPrefix(const std::string& prefix);
    bool matches(VfsNode* node, const std::string& path, Vfs& vfs) const;
};

struct ContextEntry {
    std::string vfs_path;
    VfsNode* node;
    std::string content;
    size_t token_estimate;
    int priority;
    TagSet tags;
};

struct ContextBuilder {
    Vfs& vfs;
    size_t max_tokens;
    std::vector<ContextEntry> entries;

    void addFilter(const ContextFilter& filter);
    void collect();
    std::string build();
    std::string buildWithPriority();
};

struct ReplacementStrategy {
    enum class Type { ReplaceAll, ReplaceRange, InsertBefore, ... };
    bool apply(Vfs& vfs) const;
};
```

---

## make

**Purpose:** Minimal GNU Make subset

**Source:**
- Header: codex.h lines 2252-2311
- Impl: codex.cpp lines 7943-8267

**Exports:**
```cpp
struct MakeRule {
    std::string target;
    std::vector<std::string> dependencies;
    std::vector<std::string> commands;
    bool is_phony;
};

struct MakeFile {
    std::map<std::string, std::string> variables;
    std::map<std::string, MakeRule> rules;

    void parse(const std::string& content);
    std::string expandVariables(const std::string& text) const;
    bool needsRebuild(const std::string& target, Vfs& vfs) const;

    struct BuildResult {
        bool success;
        std::string output;
        std::vector<std::string> targets_built;
    };
    BuildResult build(const std::string& target, Vfs& vfs, bool verbose = false);
};
```

---

## hypothesis

**Purpose:** 5-level hypothesis testing system

**Source:**
- Header: codex.h lines 1715-1853
- Impl: codex.cpp lines 6068-6667

**Exports:**
```cpp
struct Hypothesis {
    enum class Level {
        SimpleQuery = 1,
        CodeModification = 2,
        Refactoring = 3,
        FeatureAddition = 4,
        Architecture = 5
    };

    Level level;
    std::string description, goal;
    std::vector<std::string> assumptions, validation_criteria;
    bool tested, valid;
};

struct HypothesisResult {
    bool success;
    std::string message;
    std::vector<std::string> findings, actions;
    size_t nodes_examined, nodes_matched;
};

struct HypothesisTester {
    Vfs& vfs;
    ContextBuilder context_builder;

    HypothesisResult testSimpleQuery(const std::string& target, const std::string& search_path = "/");
    HypothesisResult testErrorHandlingAddition(const std::string& function_name, const std::string& style);
    HypothesisResult testDuplicateExtraction(const std::string& path, size_t min_lines = 3);
    HypothesisResult testLoggingInstrumentation(const std::string& path);
    HypothesisResult testArchitecturePattern(const std::string& pattern, const std::string& path);
};
```

---

## scope_store

**Purpose:** Binary diffs, snapshots, feature masks

**Source:**
- Header: codex.h lines 1855-2040
- Impl: codex.cpp lines 6668-7166

**Exports:**
```cpp
struct BinaryDiff {
    static std::vector<uint8_t> compute(const std::string& old_content, const std::string& new_content);
    static std::string apply(const std::string& base_content, const std::vector<uint8_t>& diff);
};

struct FeatureMask {
    BitVector mask;  // 512-bit vector
    enum class Feature : uint32_t {
        VFS_PERSISTENCE = 0,
        AST_BUILDER = 1,
        AI_BRIDGE = 2,
        // ... up to 511
    };

    void enable(Feature f);
    bool isEnabled(Feature f) const;
};

struct ScopeSnapshot {
    uint64_t snapshot_id, timestamp, parent_snapshot_id;
    std::vector<uint8_t> diff_data;
    FeatureMask feature_mask;
    std::vector<std::string> affected_paths;
};

struct ScopeStore {
    std::unordered_map<uint64_t, ScopeSnapshot> snapshots;
    FeatureMask active_features;

    uint64_t createSnapshot(Vfs& vfs, const std::string& description);
    bool restoreSnapshot(Vfs& vfs, uint64_t snapshot_id);
    void save(const std::string& path);
    void load(const std::string& path);
};
```

---

## feedback

**Purpose:** Feedback pipeline for learning from execution

**Source:**
- Header: codex.h lines 2041-2251
- Impl: codex.cpp lines 7167-7799

**Exports:**
```cpp
struct PlannerMetrics {
    std::string scenario_name;
    bool success;
    size_t iterations, rules_applied;
    std::vector<std::string> rules_triggered, rules_failed;
    double execution_time_ms;
};

struct MetricsCollector {
    std::vector<PlannerMetrics> history;

    void startRun(const std::string& scenario_name);
    void recordRuleTrigger(const std::string& rule_name);
    void finishRun();
    std::vector<std::string> getMostTriggeredRules(size_t top_n = 10) const;
};

struct RulePatch {
    enum class Operation { Add, Modify, Remove, AdjustConfidence };
    Operation operation;
    std::string rule_name;
    float new_confidence;
    std::string rationale;
};

struct RulePatchStaging {
    std::vector<RulePatch> pending_patches;

    void stagePatch(const RulePatch& patch);
    bool applyPatch(size_t index);
    void rejectPatch(size_t index, const std::string& reason = "");
};

struct FeedbackLoop {
    MetricsCollector& metrics_collector;
    RulePatchStaging& patch_staging;

    std::vector<RulePatch> analyzeMetrics(size_t min_evidence_count = 3);
    FeedbackCycleResult runCycle(bool auto_apply = false, size_t min_evidence = 3);
};
```

---

## web_server

**Purpose:** HTTP/WebSocket server for browser terminal

**Source:**
- Header: codex.h lines 2312-2335
- Impl: codex.cpp (embedded in main section)

**Exports:**
```cpp
namespace WebServer {
    bool start(int port = 8080);
    void stop();
    bool is_running();
    void send_output(const std::string& output);

    using CommandCallback = std::function<std::pair<bool, std::string>(const std::string&)>;
    void set_command_callback(CommandCallback callback);
}
```

---

## shell_commands

**Purpose:** All shell command implementations

**Source:**
- Impl: codex.cpp lines 1833-2605, 8268-11526

**Exports:**
```cpp
// ~50+ cmd_* functions

// VFS commands
void cmd_ls(Vfs& vfs, const std::vector<std::string>& args);
void cmd_tree(Vfs& vfs, const std::vector<std::string>& args);
void cmd_mkdir(Vfs& vfs, const std::vector<std::string>& args);
void cmd_touch(Vfs& vfs, const std::vector<std::string>& args);
void cmd_cat(Vfs& vfs, const std::vector<std::string>& args);
void cmd_rm(Vfs& vfs, const std::vector<std::string>& args);
void cmd_mv(Vfs& vfs, const std::vector<std::string>& args);

// S-expression commands
void cmd_parse(Vfs& vfs, const std::vector<std::string>& args);
void cmd_eval(Vfs& vfs, const std::vector<std::string>& args);

// C++ AST commands
void cmd_cpp_tu(Vfs& vfs, const std::vector<std::string>& args);
void cmd_cpp_func(Vfs& vfs, const std::vector<std::string>& args);

// AI commands
void cmd_ai(Vfs& vfs, const std::vector<std::string>& args);
void cmd_tools(Vfs& vfs, const std::vector<std::string>& args);
void cmd_discuss(Vfs& vfs, const std::vector<std::string>& args);

// Planning commands
void cmd_plan_create(Vfs& vfs, const std::vector<std::string>& args);
void cmd_plan_goto(Vfs& vfs, const std::vector<std::string>& args);

// ... and many more

// Command dispatch
using CommandFunc = std::function<void(Vfs&, const std::vector<std::string>&)>;
std::map<std::string, CommandFunc> buildCommandMap();

// Tab completion
std::vector<std::string> get_all_command_names();
std::vector<std::string> get_vfs_path_completions(Vfs& vfs, const std::string& prefix);
```

---

## repl

**Purpose:** Main REPL loop with line editing

**Source:**
- Impl: codex.cpp lines 7800-7942

**Exports:**
```cpp
void run_repl(Vfs& vfs, std::shared_ptr<Env> env);

// Line editing utilities (internal)
std::string read_line_with_editing(const std::string& prompt);
std::string tab_complete(const std::string& buffer, size_t cursor_pos, Vfs& vfs);
```

---

## main

**Purpose:** Entry point and initialization

**Source:**
- Impl: codex.cpp (main function section)

**Exports:**
```cpp
int main(int argc, char** argv);

// Handles:
// - Command-line argument parsing (--web-server, --port, --english-only)
// - VFS initialization
// - Overlay loading
// - Builtin installation
// - i18n initialization
// - Mode selection (REPL vs web server)
```

---

## ui_backend

**Purpose:** UI abstraction (existing file)

**Source:** VfsShell/ui_backend.h (already exists)

**Exports:**
```cpp
bool run_ncurses_editor(Vfs& vfs, const std::string& vfs_path,
                        std::vector<std::string>& lines,
                        bool file_exists, size_t overlay_id);
```

---

## Quick Extraction Guide

For each module:

1. **Create files:**
   ```bash
   touch VfsShell/<module>.h VfsShell/<module>.cpp
   ```

2. **Header template:**
   ```cpp
   #ifndef VFSSHELL_<MODULE>_H_
   #define VFSSHELL_<MODULE>_H_

   #include "<dependency1>.h"
   #include "<dependency2>.h"

   // Declarations from codex.h lines X-Y

   #endif
   ```

3. **Implementation template:**
   ```cpp
   #include "<module>.h"

   // Additional includes if needed

   // Implementations from codex.cpp lines X-Y
   ```

4. **Update codex.h:**
   ```cpp
   #include "<module>.h"
   ```

5. **Test:**
   ```bash
   make clean && make
   ```

---

## Common Patterns

### Forward Declarations
```cpp
// In header, if only using pointers:
struct Vfs;  // Forward declaration
void func(Vfs* vfs);

// In cpp, include full header:
#include "vfs_core.h"
void func(Vfs* vfs) { /* implementation */ }
```

### Include Guards
```cpp
#ifndef VFSSHELL_MODULE_H_
#define VFSSHELL_MODULE_H_
// ... content ...
#endif

// OR (preferred):
#pragma once
// ... content ...
```

### Inline vs Regular Functions
```cpp
// Small functions in header (inline):
struct Foo {
    int get() const { return x; }  // Implicitly inline
private:
    int x;
};

// Large functions in cpp:
// foo.h
struct Foo {
    void process();
};

// foo.cpp
void Foo::process() {
    // Implementation
}
```

---

## Troubleshooting

| Error | Solution |
|-------|----------|
| Undefined reference | Check .cpp file is in Makefile, function is not `static` |
| Incomplete type | Include full header instead of forward declaration |
| Multiple definition | Check for missing include guards, duplicate definitions |
| Circular dependency | Use forward declarations, check dependency graph |

---

**See also:**
- MODULAR_STRUCTURE.md (detailed analysis)
- DEPENDENCY_GRAPH.txt (visual dependencies)
- REFACTORING_PLAN.md (step-by-step guide)
