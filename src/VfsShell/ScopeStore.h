#pragma once


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
