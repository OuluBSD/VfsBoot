# Scope Store System Design

## Overview

The Scope Store is a context management system that captures VFS state changes as binary diffs with feature masks, enabling deterministic context building for AI interactions and hypothesis testing.

## Architecture

### 1. Scope Store Core

```cpp
struct ScopeSnapshot {
    uint64_t snapshot_id;              // Unique identifier
    uint64_t timestamp;                // Unix epoch timestamp
    std::string description;           // Human-readable description
    uint64_t parent_snapshot_id;       // Previous snapshot (0 = root)

    // Binary diff from parent
    std::vector<uint8_t> diff_data;    // Binary delta encoding
    size_t uncompressed_size;          // Original size

    // Feature masks - which features are active
    BitVector feature_mask;            // 512-bit mask for features

    // Metadata
    std::unordered_map<std::string, std::string> metadata;

    // VFS paths affected by this snapshot
    std::vector<std::string> affected_paths;
};

struct ScopeStore {
    std::unordered_map<uint64_t, ScopeSnapshot> snapshots;
    uint64_t current_snapshot_id;
    uint64_t next_snapshot_id;

    // Feature registry
    std::unordered_map<uint32_t, std::string> feature_names;  // bit_index -> name
    BitVector active_features;  // Currently active features

    // Create snapshot from current VFS state
    uint64_t createSnapshot(Vfs& vfs, const std::string& description);

    // Restore VFS to snapshot state
    bool restoreSnapshot(Vfs& vfs, uint64_t snapshot_id);

    // Apply diff to current state
    bool applyDiff(Vfs& vfs, uint64_t snapshot_id);

    // Feature mask operations
    void enableFeature(uint32_t feature_id);
    void disableFeature(uint32_t feature_id);
    bool isFeatureActive(uint32_t feature_id) const;

    // Get diff between two snapshots
    std::vector<uint8_t> computeDiff(uint64_t from_id, uint64_t to_id);

    // Persistence
    void save(const std::string& path);
    void load(const std::string& path);
};
```

### 2. Binary Diff Encoding

Use SVN's libsvn_diff for efficient binary delta compression:

```cpp
struct BinaryDiff {
    // Compute binary diff between two VFS states
    static std::vector<uint8_t> compute(
        const std::string& old_content,
        const std::string& new_content
    );

    // Apply binary diff to reconstruct content
    static std::string apply(
        const std::string& base_content,
        const std::vector<uint8_t>& diff
    );

    // SVN delta format wrapper
    struct SvnDelta {
        // SVN diff uses a custom binary format optimized for version control
        // We'll wrap svn_txdelta_* functions for:
        // 1. Window-based delta streams
        // 2. Instruction encoding (copy from source, copy from target, new data)
        // 3. zlib compression of delta windows
    };
};
```

### 3. Feature Mask System

```cpp
struct FeatureMask {
    BitVector mask;  // 512-bit vector for 512 features

    // Feature IDs
    enum class Feature : uint32_t {
        // Core features
        VFS_PERSISTENCE = 0,
        AST_BUILDER = 1,
        AI_BRIDGE = 2,
        OVERLAY_SYSTEM = 3,

        // Planner features
        ACTION_PLANNER = 10,
        HYPOTHESIS_TESTING = 11,
        LOGIC_SOLVER = 12,
        TAG_SYSTEM = 13,

        // Code generation features
        CPP_CODEGEN = 20,
        JAVA_CODEGEN = 21,
        CSHARP_CODEGEN = 22,

        // Experimental features
        REMOTE_VFS = 30,
        LIBRARY_MOUNT = 31,
        AUTOSAVE = 32,

        // Reserved for future use: 33-511
    };

    void enable(Feature f) { mask.set(static_cast<uint32_t>(f)); }
    void disable(Feature f) { mask.clear(static_cast<uint32_t>(f)); }
    bool isEnabled(Feature f) const { return mask.test(static_cast<uint32_t>(f)); }

    // Bulk operations
    void enableAll(const std::vector<Feature>& features);
    void disableAll(const std::vector<Feature>& features);

    // Serialization
    std::string toString() const;
    static FeatureMask fromString(const std::string& s);
};
```

### 4. Deterministic Context Builder

```cpp
struct DeterministicContextBuilder {
    ScopeStore& scope_store;
    ContextBuilder& context_builder;

    // Build context with deterministic ordering
    struct BuildOptions {
        uint64_t snapshot_id = 0;           // Use specific snapshot (0 = current)
        BitVector feature_filter;           // Only include nodes from these features
        bool stable_sort = true;            // Lexicographic path sorting
        bool include_metadata = true;       // Include snapshot metadata
        uint64_t seed = 0;                  // Random seed for sampling (0 = no sampling)
        double sample_rate = 1.0;           // Sampling rate (1.0 = all nodes)
    };

    std::string build(const BuildOptions& opts);

    // Hash of context for verification
    uint64_t contextHash(const BuildOptions& opts);

    // Compare contexts between snapshots
    struct ContextDiff {
        std::vector<std::string> added_paths;
        std::vector<std::string> removed_paths;
        std::vector<std::string> modified_paths;
        size_t token_delta;  // Change in token count
    };

    ContextDiff compareContexts(uint64_t snapshot1, uint64_t snapshot2);

    // Replay context building across snapshots
    struct ReplayResult {
        std::vector<std::string> contexts;  // Context at each snapshot
        std::vector<uint64_t> hashes;       // Hash at each snapshot
        std::vector<size_t> token_counts;   // Token count at each snapshot
    };

    ReplayResult replay(const std::vector<uint64_t>& snapshot_ids);
};
```

## Usage Examples

### 1. Creating Snapshots During Development

```cpp
// Initial state
scope_store.createSnapshot(vfs, "Initial VFS state");

// After adding hello world
scope_store.createSnapshot(vfs, "Added hello world program");

// After adding error handling
scope_store.createSnapshot(vfs, "Added error handling to main");
```

### 2. Feature-Gated Development

```cpp
// Enable experimental features
scope_store.enableFeature(Feature::REMOTE_VFS);
scope_store.enableFeature(Feature::LIBRARY_MOUNT);

// Create snapshot with active features
uint64_t snap_id = scope_store.createSnapshot(vfs, "Experimental features");

// Later: build context only for stable features
BuildOptions opts;
opts.feature_filter.set(Feature::AST_BUILDER);
opts.feature_filter.set(Feature::AI_BRIDGE);
std::string context = det_builder.build(opts);
```

### 3. Deterministic Hypothesis Testing

```cpp
// Test hypothesis across multiple snapshots
auto results = det_builder.replay({snap1, snap2, snap3});

for (size_t i = 0; i < results.contexts.size(); i++) {
    std::cout << "Snapshot " << i << " hash: " << results.hashes[i] << "\n";
    std::cout << "Token count: " << results.token_counts[i] << "\n";

    // Run hypothesis test with deterministic context
    hypothesis_tester.test(results.contexts[i]);
}
```

### 4. Training Data Generation

```cpp
// Generate training scenarios
for (uint64_t snap_id : training_snapshots) {
    BuildOptions opts;
    opts.snapshot_id = snap_id;
    opts.stable_sort = true;
    opts.seed = 42;  // Fixed seed for reproducibility

    std::string context = det_builder.build(opts);
    uint64_t hash = det_builder.contextHash(opts);

    // Save to training set
    training_data.push_back({snap_id, context, hash});
}
```

## Shell Commands

```
scope.create <description>              - Create snapshot of current VFS
scope.restore <snapshot-id>             - Restore VFS to snapshot state
scope.list                              - List all snapshots
scope.diff <snap1> <snap2>              - Show diff between snapshots
scope.feature.enable <feature-name>     - Enable feature
scope.feature.disable <feature-name>    - Disable feature
scope.feature.list                      - List all features and status

context.build.det <snapshot-id> [opts]  - Build deterministic context
context.hash <snapshot-id>              - Get context hash
context.compare <snap1> <snap2>         - Compare contexts
context.replay <snap1> <snap2> ...      - Replay context building
```

## Persistence Format

### Scope Store File (.scope)

```
SCOPE_STORE_V1
snapshot_count: <n>
feature_count: <m>

[FEATURES]
0: VFS_PERSISTENCE
1: AST_BUILDER
...

[SNAPSHOTS]
id: <uint64>
timestamp: <uint64>
parent: <uint64>
description: <string>
feature_mask: <hex>
affected_paths: <count>
  /path/to/file1
  /path/to/file2
diff_size: <size>
<binary_diff_data>

[METADATA]
<key>: <value>
...
```

## Implementation Plan

1. **Phase 1**: Basic snapshot system with full VFS serialization (no diffs)
2. **Phase 2**: Binary diff integration using libsvn_diff
3. **Phase 3**: Feature mask system
4. **Phase 4**: Deterministic context builder
5. **Phase 5**: Shell commands and CLI integration
6. **Phase 6**: Training data generation and replay system

## Dependencies

- `libsvn_diff` - Binary delta encoding
- `BitVector` - Already implemented for feature masks
- `ContextBuilder` - Existing context builder
- `Vfs` - Virtual file system
- `BLAKE3` - For content hashing and verification

## Performance Considerations

- Snapshots stored with zlib compression (~70% reduction)
- Binary diffs use SVN's window-based algorithm (efficient for code)
- Feature masks use 512-bit vectors (64 bytes) for O(1) operations
- Context hashing uses BLAKE3 for speed
- Replay system uses lazy loading (only load snapshots when needed)
