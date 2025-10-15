#pragma once


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
