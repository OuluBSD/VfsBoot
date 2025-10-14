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
#include <sstream>
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
#include <unordered_map>

// SVN diff library for binary deltas
#include <svn_delta.h>
#include <svn_pools.h>
#include <apr_pools.h>

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
    void set_english_only();  // Force English (for scripting)
}

#define _(msg_id) ::i18n::get(::i18n::MsgId::msg_id)

//
// Forward declarations
//
struct Vfs;

//
// Tag Registry (enumerated tags for memory efficiency)
//
using TagId = uint32_t;
constexpr TagId TAG_INVALID = 0;

//
// BitVector-based TagSet for O(1) operations with 64-bit chunks
//
// Simple BitVector class for feature masks
class BitVector {
    static constexpr size_t BITS_PER_CHUNK = 64;
    std::vector<uint64_t> chunks;
    size_t num_bits;

    static constexpr size_t chunkIndex(size_t bit) { return bit / BITS_PER_CHUNK; }
    static constexpr size_t bitOffset(size_t bit) { return bit % BITS_PER_CHUNK; }

public:
    explicit BitVector(size_t bits = 512) : num_bits(bits) {
        chunks.resize((bits + BITS_PER_CHUNK - 1) / BITS_PER_CHUNK, 0);
    }

    void set(size_t bit) {
        if (bit < num_bits) {
            size_t idx = chunkIndex(bit);
            if (idx >= chunks.size()) chunks.resize(idx + 1, 0);
            chunks[idx] |= (1ULL << bitOffset(bit));
        }
    }

    void clear(size_t bit) {
        if (bit < num_bits && bit < chunks.size() * BITS_PER_CHUNK) {
            chunks[chunkIndex(bit)] &= ~(1ULL << bitOffset(bit));
        }
    }

    bool test(size_t bit) const {
        if (bit >= num_bits) return false;
        size_t idx = chunkIndex(bit);
        return idx < chunks.size() && (chunks[idx] & (1ULL << bitOffset(bit)));
    }

    uint64_t hash() const {
        uint64_t h = 0;
        for (auto chunk : chunks) h ^= chunk;
        return h;
    }

    std::string toString() const {
        std::ostringstream out;
        out << std::hex;
        for (size_t i = 0; i < chunks.size(); i++) {
            if (i > 0) out << ":";
            out << chunks[i];
        }
        return out.str();
    }

    static BitVector fromString(const std::string& s) {
        BitVector bv(512);
        std::istringstream in(s);
        in >> std::hex;
        size_t idx = 0;
        std::string chunk_str;
        while (std::getline(in, chunk_str, ':')) {
            if (idx >= bv.chunks.size()) bv.chunks.resize(idx + 1, 0);
            bv.chunks[idx++] = std::stoull(chunk_str, nullptr, 16);
        }
        return bv;
    }
};

class TagSet {
    static constexpr size_t BITS_PER_CHUNK = 64;
    std::vector<uint64_t> chunks;  // Bit vector in 64-bit chunks

    // Get chunk index and bit position for a tag
    static constexpr size_t chunkIndex(TagId tag) { return tag / BITS_PER_CHUNK; }
    static constexpr size_t bitPosition(TagId tag) { return tag % BITS_PER_CHUNK; }

    void ensureCapacity(TagId tag) {
        size_t needed = chunkIndex(tag) + 1;
        if(chunks.size() < needed) {
            chunks.resize(needed, 0);
        }
    }

public:
    TagSet() = default;
    TagSet(std::initializer_list<TagId> tags) {
        for(TagId tag : tags) insert(tag);
    }

    // Insert tag - O(1) amortized
    void insert(TagId tag) {
        if(tag == TAG_INVALID) return;
        ensureCapacity(tag);
        chunks[chunkIndex(tag)] |= (1ULL << bitPosition(tag));
    }

    // Erase tag - O(1)
    void erase(TagId tag) {
        if(tag == TAG_INVALID) return;
        size_t idx = chunkIndex(tag);
        if(idx < chunks.size()) {
            chunks[idx] &= ~(1ULL << bitPosition(tag));
        }
    }

    // Check membership - O(1)
    size_t count(TagId tag) const {
        if(tag == TAG_INVALID) return 0;
        size_t idx = chunkIndex(tag);
        if(idx >= chunks.size()) return 0;
        return (chunks[idx] & (1ULL << bitPosition(tag))) ? 1 : 0;
    }

    bool contains(TagId tag) const { return count(tag) > 0; }

    // Cardinality using popcount - O(chunks)
    size_t size() const {
        size_t total = 0;
        for(uint64_t chunk : chunks) {
            total += __builtin_popcountll(chunk);
        }
        return total;
    }

    bool empty() const {
        for(uint64_t chunk : chunks) {
            if(chunk != 0) return false;
        }
        return true;
    }

    void clear() { chunks.clear(); }

    // Set operations using bitwise ops
    TagSet operator|(const TagSet& other) const {  // Union
        TagSet result;
        size_t max_size = std::max(chunks.size(), other.chunks.size());
        result.chunks.resize(max_size, 0);
        for(size_t i = 0; i < chunks.size(); ++i) {
            result.chunks[i] = chunks[i];
        }
        for(size_t i = 0; i < other.chunks.size(); ++i) {
            result.chunks[i] |= other.chunks[i];
        }
        return result;
    }

    TagSet operator&(const TagSet& other) const {  // Intersection
        TagSet result;
        size_t min_size = std::min(chunks.size(), other.chunks.size());
        result.chunks.resize(min_size, 0);
        for(size_t i = 0; i < min_size; ++i) {
            result.chunks[i] = chunks[i] & other.chunks[i];
        }
        return result;
    }

    TagSet operator-(const TagSet& other) const {  // Difference
        TagSet result;
        result.chunks.resize(chunks.size(), 0);
        for(size_t i = 0; i < chunks.size(); ++i) {
            uint64_t other_chunk = i < other.chunks.size() ? other.chunks[i] : 0;
            result.chunks[i] = chunks[i] & ~other_chunk;
        }
        return result;
    }

    TagSet operator^(const TagSet& other) const {  // Symmetric difference (XOR)
        TagSet result;
        size_t max_size = std::max(chunks.size(), other.chunks.size());
        result.chunks.resize(max_size, 0);
        for(size_t i = 0; i < max_size; ++i) {
            uint64_t a = i < chunks.size() ? chunks[i] : 0;
            uint64_t b = i < other.chunks.size() ? other.chunks[i] : 0;
            result.chunks[i] = a ^ b;
        }
        return result;
    }

    // In-place operations
    TagSet& operator|=(const TagSet& other) {
        if(other.chunks.size() > chunks.size()) {
            chunks.resize(other.chunks.size(), 0);
        }
        for(size_t i = 0; i < other.chunks.size(); ++i) {
            chunks[i] |= other.chunks[i];
        }
        return *this;
    }

    TagSet& operator&=(const TagSet& other) {
        for(size_t i = 0; i < chunks.size(); ++i) {
            chunks[i] &= i < other.chunks.size() ? other.chunks[i] : 0;
        }
        return *this;
    }

    TagSet& operator-=(const TagSet& other) {
        for(size_t i = 0; i < std::min(chunks.size(), other.chunks.size()); ++i) {
            chunks[i] &= ~other.chunks[i];
        }
        return *this;
    }

    // Fast subset checking
    bool isSubsetOf(const TagSet& other) const {
        for(size_t i = 0; i < chunks.size(); ++i) {
            uint64_t other_chunk = i < other.chunks.size() ? other.chunks[i] : 0;
            if((chunks[i] & ~other_chunk) != 0) return false;
        }
        return true;
    }

    bool isSupersetOf(const TagSet& other) const {
        return other.isSubsetOf(*this);
    }

    // Iterator support for range-based for loops
    class iterator {
        const std::vector<uint64_t>* chunks_ptr;
        size_t chunk_idx;
        size_t bit_idx;

        void advance() {
            if(!chunks_ptr || chunk_idx >= chunks_ptr->size()) return;

            // Find next set bit
            while(chunk_idx < chunks_ptr->size()) {
                uint64_t chunk = (*chunks_ptr)[chunk_idx];
                if(chunk == 0) {
                    chunk_idx++;
                    bit_idx = 0;
                    continue;
                }

                // Skip to next set bit in current chunk
                chunk >>= bit_idx;
                while(bit_idx < BITS_PER_CHUNK && (chunk & 1) == 0) {
                    chunk >>= 1;
                    bit_idx++;
                }

                if(bit_idx < BITS_PER_CHUNK) return;  // Found
                chunk_idx++;
                bit_idx = 0;
            }
        }

    public:
        iterator(const std::vector<uint64_t>* ptr, bool is_end)
            : chunks_ptr(ptr), chunk_idx(is_end ? ptr->size() : 0), bit_idx(0) {
            if(!is_end && ptr && !ptr->empty()) advance();
        }

        TagId operator*() const {
            return static_cast<TagId>(chunk_idx * BITS_PER_CHUNK + bit_idx);
        }

        iterator& operator++() {
            bit_idx++;
            advance();
            return *this;
        }

        bool operator!=(const iterator& other) const {
            return chunk_idx != other.chunk_idx || bit_idx != other.bit_idx;
        }

        bool operator==(const iterator& other) const {
            return !(*this != other);
        }
    };

    iterator begin() const { return iterator(&chunks, false); }
    iterator end() const { return iterator(&chunks, true); }

    // XOR-based hash for fast fingerprinting
    uint64_t hash() const {
        uint64_t h = 0;
        for(uint64_t chunk : chunks) {
            h ^= chunk;
        }
        return h;
    }

    // Comparison operators
    bool operator==(const TagSet& other) const {
        size_t max_size = std::max(chunks.size(), other.chunks.size());
        for(size_t i = 0; i < max_size; ++i) {
            uint64_t a = i < chunks.size() ? chunks[i] : 0;
            uint64_t b = i < other.chunks.size() ? other.chunks[i] : 0;
            if(a != b) return false;
        }
        return true;
    }

    bool operator!=(const TagSet& other) const {
        return !(*this == other);
    }
};

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

    // Rule persistence
    void saveRulesToVfs(Vfs& vfs, const std::string& base_path = "/plan/rules") const;
    void loadRulesFromVfs(Vfs& vfs, const std::string& base_path = "/plan/rules");
    std::string serializeRule(const ImplicationRule& rule) const;
    ImplicationRule deserializeRule(const std::string& serialized) const;

    // Dynamic rule creation
    void addSimpleRule(const std::string& name, const std::string& premise_tag,
                       const std::string& conclusion_tag, float confidence, const std::string& source);
    void addExclusionRule(const std::string& name, const std::string& tag1,
                          const std::string& tag2, const std::string& source = "user");
    void removeRule(const std::string& name);
    bool hasRule(const std::string& name) const;
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
// ============================================================================
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

    // Fast XOR-based content fingerprinting for deduplication
    static uint64_t contentFingerprint(const std::string& content) {
        uint64_t hash = 0xcbf29ce484222325ULL;  // FNV-1a basis
        const uint64_t prime = 0x100000001b3ULL;

        // Process 8 bytes at a time for speed
        size_t i = 0;
        const char* data = content.data();
        size_t len = content.size();

        while(i + 8 <= len) {
            uint64_t chunk;
            std::memcpy(&chunk, data + i, 8);
            hash = (hash ^ chunk) * prime;
            i += 8;
        }

        // Process remaining bytes
        while(i < len) {
            hash = (hash ^ static_cast<uint64_t>(data[i])) * prime;
            i++;
        }

        return hash;
    }

    // Fast TagSet fingerprint for quick tag comparison
    static uint64_t tagFingerprint(const TagSet& tags) {
        return tags.hash();  // Uses XOR of all chunks
    }

private:
    void visitNode(const std::string& path, VfsNode* node);
    bool matchesAnyFilter(const std::string& path, VfsNode* node) const;
    std::unordered_set<std::string> seen_content;  // For deduplication (fallback)
    std::unordered_set<uint64_t> seen_fingerprints;  // Fast hash-based deduplication
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

// ============================================================================
// Scope Store System - Binary diffs, feature masks, deterministic context
// ============================================================================

// Binary diff encoding using SVN delta algorithm
struct BinaryDiff {
    // Compute binary diff between old and new content
    static std::vector<uint8_t> compute(const std::string& old_content,
                                         const std::string& new_content);

    // Apply binary diff to base content
    static std::string apply(const std::string& base_content,
                            const std::vector<uint8_t>& diff);

    // SVN delta implementation details
    struct SvnDeltaContext {
        apr_pool_t* pool;
        svn_txdelta_stream_t* stream;
        svn_txdelta_window_t* window;

        SvnDeltaContext();
        ~SvnDeltaContext();
    };
};

// Feature mask system using BitVector
struct FeatureMask {
    BitVector mask;  // 512-bit vector for 512 features

    // Core feature IDs (0-31)
    enum class Feature : uint32_t {
        VFS_PERSISTENCE = 0,
        AST_BUILDER = 1,
        AI_BRIDGE = 2,
        OVERLAY_SYSTEM = 3,

        // Planner features (10-19)
        ACTION_PLANNER = 10,
        HYPOTHESIS_TESTING = 11,
        LOGIC_SOLVER = 12,
        TAG_SYSTEM = 13,

        // Code generation (20-29)
        CPP_CODEGEN = 20,
        JAVA_CODEGEN = 21,
        CSHARP_CODEGEN = 22,

        // Experimental (30-39)
        REMOTE_VFS = 30,
        LIBRARY_MOUNT = 31,
        AUTOSAVE = 32,
        SCOPE_STORE = 33,

        // Reserved: 40-511 for future use
    };

    FeatureMask() : mask(512) {}

    void enable(Feature f) { mask.set(static_cast<uint32_t>(f)); }
    void disable(Feature f) { mask.clear(static_cast<uint32_t>(f)); }
    bool isEnabled(Feature f) const { return mask.test(static_cast<uint32_t>(f)); }

    void enableAll(const std::vector<Feature>& features);
    void disableAll(const std::vector<Feature>& features);

    // Serialization
    std::string toString() const;
    static FeatureMask fromString(const std::string& s);
};

// Snapshot of VFS state with binary diff from parent
struct ScopeSnapshot {
    uint64_t snapshot_id;
    uint64_t timestamp;           // Unix epoch
    std::string description;
    uint64_t parent_snapshot_id;  // 0 = root snapshot

    // Binary diff from parent
    std::vector<uint8_t> diff_data;
    size_t uncompressed_size;

    // Feature mask at snapshot time
    FeatureMask feature_mask;

    // Metadata
    std::unordered_map<std::string, std::string> metadata;

    // Paths affected by this snapshot
    std::vector<std::string> affected_paths;

    ScopeSnapshot() : snapshot_id(0), timestamp(0), parent_snapshot_id(0),
                      uncompressed_size(0) {}
};

// Scope store manages snapshots and feature-gated development
struct ScopeStore {
    std::unordered_map<uint64_t, ScopeSnapshot> snapshots;
    uint64_t current_snapshot_id;
    uint64_t next_snapshot_id;

    // Feature registry
    std::unordered_map<uint32_t, std::string> feature_names;
    FeatureMask active_features;

    ScopeStore() : current_snapshot_id(0), next_snapshot_id(1) {
        // Initialize feature names
        feature_names[0] = "VFS_PERSISTENCE";
        feature_names[1] = "AST_BUILDER";
        feature_names[2] = "AI_BRIDGE";
        feature_names[3] = "OVERLAY_SYSTEM";
        feature_names[10] = "ACTION_PLANNER";
        feature_names[11] = "HYPOTHESIS_TESTING";
        feature_names[12] = "LOGIC_SOLVER";
        feature_names[13] = "TAG_SYSTEM";
        feature_names[20] = "CPP_CODEGEN";
        feature_names[21] = "JAVA_CODEGEN";
        feature_names[22] = "CSHARP_CODEGEN";
        feature_names[30] = "REMOTE_VFS";
        feature_names[31] = "LIBRARY_MOUNT";
        feature_names[32] = "AUTOSAVE";
        feature_names[33] = "SCOPE_STORE";
    }

    // Snapshot operations
    uint64_t createSnapshot(Vfs& vfs, const std::string& description);
    bool restoreSnapshot(Vfs& vfs, uint64_t snapshot_id);
    bool applyDiff(Vfs& vfs, uint64_t snapshot_id);

    // Feature operations
    void enableFeature(uint32_t feature_id);
    void disableFeature(uint32_t feature_id);
    bool isFeatureActive(uint32_t feature_id) const;

    // Diff between snapshots
    std::vector<uint8_t> computeDiff(uint64_t from_id, uint64_t to_id);

    // Persistence
    void save(const std::string& path);
    void load(const std::string& path);

    // Helper: serialize VFS to string
    std::string serializeVfs(Vfs& vfs);
    void deserializeVfs(Vfs& vfs, const std::string& data);
};

// Deterministic context builder with reproducible ordering
struct DeterministicContextBuilder {
    ScopeStore& scope_store;
    ContextBuilder& context_builder;

    struct BuildOptions {
        uint64_t snapshot_id = 0;       // 0 = current VFS state
        BitVector feature_filter;       // Only include these features
        bool stable_sort = true;        // Lexicographic sorting
        bool include_metadata = true;   // Include snapshot metadata
        uint64_t seed = 0;              // Random seed (0 = no sampling)
        double sample_rate = 1.0;       // Sampling rate (1.0 = all)

        BuildOptions() : feature_filter(512) {}
    };

    DeterministicContextBuilder(ScopeStore& ss, ContextBuilder& cb)
        : scope_store(ss), context_builder(cb) {}

    std::string build(const BuildOptions& opts);
    uint64_t contextHash(const BuildOptions& opts);

    // Context diff between snapshots
    struct ContextDiff {
        std::vector<std::string> added_paths;
        std::vector<std::string> removed_paths;
        std::vector<std::string> modified_paths;
        size_t token_delta;
    };

    ContextDiff compareContexts(uint64_t snapshot1, uint64_t snapshot2);

    // Replay context building
    struct ReplayResult {
        std::vector<std::string> contexts;
        std::vector<uint64_t> hashes;
        std::vector<size_t> token_counts;
    };

    ReplayResult replay(const std::vector<uint64_t>& snapshot_ids);
};

// ============================================================================
// Feedback Pipeline for Planner Rule Evolution
// ============================================================================

// Execution metrics captured during planner runs
struct PlannerMetrics {
    std::string scenario_name;
    uint64_t timestamp;
    bool success;
    size_t iterations;
    size_t rules_applied;
    std::vector<std::string> rules_triggered;  // Names of rules that fired
    std::vector<std::string> rules_failed;     // Names of rules that were expected but didn't fire
    std::string error_message;

    // Performance metrics
    double execution_time_ms;
    size_t context_tokens_used;
    size_t vfs_nodes_examined;

    // Outcome metrics
    bool plan_matched_expected;
    bool actions_completed;
    bool verification_passed;

    PlannerMetrics()
        : timestamp(0), success(false), iterations(0), rules_applied(0),
          execution_time_ms(0.0), context_tokens_used(0), vfs_nodes_examined(0),
          plan_matched_expected(false), actions_completed(false), verification_passed(false) {}
};

// Metrics collector for capturing execution data
struct MetricsCollector {
    std::vector<PlannerMetrics> history;
    PlannerMetrics* current_metrics;

    MetricsCollector() : current_metrics(nullptr) {}

    // Start collecting metrics for a new run
    void startRun(const std::string& scenario_name);

    // Record rule trigger
    void recordRuleTrigger(const std::string& rule_name);
    void recordRuleFailed(const std::string& rule_name);

    // Record outcome
    void recordSuccess(bool success, const std::string& error = "");
    void recordIterations(size_t count);
    void recordPerformance(double exec_time_ms, size_t tokens, size_t nodes);
    void recordOutcome(bool plan_matched, bool actions_completed, bool verification_passed);

    // Finish current run and add to history
    void finishRun();

    // Analysis
    std::vector<std::string> getMostTriggeredRules(size_t top_n = 10) const;
    std::vector<std::string> getMostFailedRules(size_t top_n = 10) const;
    double getAverageSuccessRate() const;
    double getAverageIterations() const;

    // Persistence
    void saveToVfs(Vfs& vfs, const std::string& path = "/plan/metrics") const;
    void loadFromVfs(Vfs& vfs, const std::string& path = "/plan/metrics");
};

// Rule patch represents a proposed modification to an implication rule
struct RulePatch {
    enum class Operation {
        Add,            // Add new rule
        Modify,         // Modify existing rule (change confidence/formulas)
        Remove,         // Remove rule
        AdjustConfidence // Only adjust confidence value
    };

    Operation operation;
    std::string rule_name;

    // For Add/Modify operations
    std::shared_ptr<LogicFormula> new_premise;
    std::shared_ptr<LogicFormula> new_conclusion;
    float new_confidence;
    std::string source;  // "ai-generated", "learned", "user"

    // Rationale for the patch
    std::string rationale;

    // Supporting evidence from metrics
    std::vector<std::string> supporting_scenarios;
    size_t evidence_count;

    RulePatch()
        : operation(Operation::Add), new_confidence(1.0f), evidence_count(0) {}

    // Create patches
    static RulePatch addRule(const std::string& name,
                             std::shared_ptr<LogicFormula> premise,
                             std::shared_ptr<LogicFormula> conclusion,
                             float confidence,
                             const std::string& source,
                             const std::string& rationale);

    static RulePatch modifyRule(const std::string& name,
                                std::shared_ptr<LogicFormula> new_premise,
                                std::shared_ptr<LogicFormula> new_conclusion,
                                float new_confidence,
                                const std::string& rationale);

    static RulePatch removeRule(const std::string& name, const std::string& rationale);

    static RulePatch adjustConfidence(const std::string& name,
                                      float new_confidence,
                                      const std::string& rationale);

    // Serialization
    std::string serialize(const TagRegistry& reg) const;
    static RulePatch deserialize(const std::string& data, TagRegistry& reg);
};

// Rule patch staging area for managing proposed changes
struct RulePatchStaging {
    std::vector<RulePatch> pending_patches;
    std::vector<RulePatch> applied_patches;
    std::vector<RulePatch> rejected_patches;

    LogicEngine* logic_engine;

    explicit RulePatchStaging(LogicEngine* engine) : logic_engine(engine) {}

    // Add patch to staging
    void stagePatch(const RulePatch& patch);
    void stagePatch(RulePatch&& patch);

    // Review patches
    size_t pendingCount() const { return pending_patches.size(); }
    const RulePatch& getPatch(size_t index) const { return pending_patches.at(index); }
    std::vector<RulePatch> listPending() const { return pending_patches; }

    // Apply or reject patches
    bool applyPatch(size_t index);
    bool rejectPatch(size_t index, const std::string& reason = "");
    bool applyAll();
    void rejectAll();

    // Clear staging area
    void clearPending();
    void clearApplied();
    void clearRejected();
    void clearAll();

    // Persistence
    void savePendingToVfs(Vfs& vfs, const std::string& path = "/plan/patches/pending") const;
    void saveAppliedToVfs(Vfs& vfs, const std::string& path = "/plan/patches/applied") const;
    void loadPendingFromVfs(Vfs& vfs, const std::string& path = "/plan/patches/pending");
};

// Feedback loop orchestrator
struct FeedbackLoop {
    MetricsCollector& metrics_collector;
    RulePatchStaging& patch_staging;
    LogicEngine& logic_engine;
    TagRegistry& tag_registry;

    // AI integration (optional)
    bool use_ai_assistance;
    std::string ai_provider;  // "openai" or "llama"

    FeedbackLoop(MetricsCollector& mc, RulePatchStaging& ps, LogicEngine& le, TagRegistry& tr)
        : metrics_collector(mc), patch_staging(ps), logic_engine(le), tag_registry(tr),
          use_ai_assistance(false), ai_provider("openai") {}

    // Analyze metrics and generate rule patches
    std::vector<RulePatch> analyzeMetrics(size_t min_evidence_count = 3);

    // Generate patches from failure patterns
    std::vector<RulePatch> generatePatchesFromFailures();

    // Generate patches from success patterns
    std::vector<RulePatch> generatePatchesFromSuccesses();

    // Generate patches with AI assistance
    std::vector<RulePatch> generatePatchesWithAI(const std::string& context_prompt);

    // Run full feedback cycle
    struct FeedbackCycleResult {
        size_t patches_generated;
        size_t patches_staged;
        size_t patches_applied;
        std::string summary;
    };

    FeedbackCycleResult runCycle(bool auto_apply = false, size_t min_evidence = 3);

    // Interactive review mode
    void interactiveReview();

private:
    // Pattern detection helpers
    struct RulePattern {
        std::string rule_name;
        size_t trigger_count;
        size_t success_count;
        size_t failure_count;
        double success_rate;
    };

    std::vector<RulePattern> detectPatterns() const;
    bool shouldIncreaseConfidence(const RulePattern& pattern) const;
    bool shouldDecreaseConfidence(const RulePattern& pattern) const;
    bool shouldRemoveRule(const RulePattern& pattern) const;
};

// ============================================================================
// Minimal GNU Make Implementation
// ============================================================================

// Makefile rule representation
struct MakeRule {
    std::string target;                      // Target name
    std::vector<std::string> dependencies;   // Prerequisites
    std::vector<std::string> commands;       // Shell commands to execute
    bool is_phony;                           // .PHONY target (always rebuild)

    MakeRule() : is_phony(false) {}
    MakeRule(std::string t) : target(std::move(t)), is_phony(false) {}
};

// Makefile parser and executor
struct MakeFile {
    std::map<std::string, std::string> variables;  // Variable definitions
    std::map<std::string, MakeRule> rules;         // All rules indexed by target
    std::set<std::string> phony_targets;           // .PHONY targets

    // Parse Makefile content from string
    void parse(const std::string& content);

    // Variable substitution $(VAR) and ${VAR}
    std::string expandVariables(const std::string& text) const;

    // Expand automatic variables ($@, $<, $^)
    std::string expandAutomaticVars(const std::string& text, const MakeRule& rule) const;

    // Check if target needs rebuild (timestamp comparison)
    bool needsRebuild(const std::string& target, Vfs& vfs) const;

    // Get file modification time from VFS or filesystem
    std::optional<uint64_t> getModTime(const std::string& path, Vfs& vfs) const;

    // Build dependency graph and execute rules
    struct BuildResult {
        bool success;
        std::string output;
        std::vector<std::string> targets_built;
        std::vector<std::string> errors;
    };

    BuildResult build(const std::string& target, Vfs& vfs, bool verbose = false);

private:
    // Recursive build with cycle detection
    bool buildTarget(const std::string& target, Vfs& vfs,
                    std::set<std::string>& building,  // Cycle detection
                    std::set<std::string>& built,     // Already built
                    BuildResult& result, bool verbose);

    // Execute shell commands for a rule
    bool executeCommands(const MakeRule& rule, BuildResult& result, bool verbose);

    // Parse a single line
    void parseLine(const std::string& line, std::string& current_target);
};

//
// Web Server (libwebsockets-based HTTP/WebSocket server)
//
namespace WebServer {
    // Start web server on specified port
    bool start(int port = 8080);

    // Stop web server
    void stop();

    // Check if server is running
    bool is_running();

    // Send output to all connected terminals
    void send_output(const std::string& output);

    // Set command execution callback for WebSocket terminals
    // Callback receives: command string, returns: (success, output)
    using CommandCallback = std::function<std::pair<bool, std::string>(const std::string&)>;
    void set_command_callback(CommandCallback callback);
}

#endif
