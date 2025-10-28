#include "VfsShell.h"

// ============================================================================
// Scope Store Implementation - Binary diffs, feature masks, deterministic context
// ============================================================================

// BinaryDiff::SvnDeltaContext implementation
BinaryDiff::SvnDeltaContext::SvnDeltaContext() : pool(nullptr), stream(nullptr), window(nullptr) {
    apr_pool_create(&pool, nullptr);
}

BinaryDiff::SvnDeltaContext::~SvnDeltaContext() {
    if (pool) {
        apr_pool_destroy(pool);
    }
}

// Compute binary diff using SVN delta algorithm
std::vector<uint8_t> BinaryDiff::compute(const std::string& old_content,
                                          const std::string& new_content) {
    TRACE_FN("old_size=", old_content.size(), " new_size=", new_content.size());

    SvnDeltaContext ctx;
    std::vector<uint8_t> diff_data;

    // Create SVN string buffers
    svn_string_t old_str = {old_content.data(), old_content.size()};
    svn_string_t new_str = {new_content.data(), new_content.size()};

    // Create text delta stream
    svn_txdelta_stream_t* stream;
    svn_txdelta2(&stream,
                 svn_stream_from_string(&old_str, ctx.pool),
                 svn_stream_from_string(&new_str, ctx.pool),
                 TRUE,  // calculate_checksums
                 ctx.pool);

    // Collect delta windows
    while (true) {
        svn_txdelta_window_t* window;
        svn_error_t* err = svn_txdelta_next_window(&window, stream, ctx.pool);
        if (err) {
            throw std::runtime_error(std::string("SVN delta error: ") + err->message);
        }
        if (!window) break;

        // Serialize window to binary format
        // For simplicity, we'll use a compact format:
        // [sview_offset:8][sview_len:8][tview_len:8][num_ops:4][ops...][new_data_len:4][new_data...]
        auto write_uint64 = [&](uint64_t val) {
            for (int i = 0; i < 8; i++) {
                diff_data.push_back(static_cast<uint8_t>((val >> (i * 8)) & 0xFF));
            }
        };
        auto write_uint32 = [&](uint32_t val) {
            for (int i = 0; i < 4; i++) {
                diff_data.push_back(static_cast<uint8_t>((val >> (i * 8)) & 0xFF));
            }
        };

        write_uint64(window->sview_offset);
        write_uint64(window->sview_len);
        write_uint64(window->tview_len);
        write_uint32(window->num_ops);

        // Write operations
        for (int i = 0; i < window->num_ops; i++) {
            diff_data.push_back(static_cast<uint8_t>(window->ops[i].action_code));
            write_uint64(window->ops[i].offset);
            write_uint64(window->ops[i].length);
        }

        // Write new data
        write_uint32(window->new_data->len);
        diff_data.insert(diff_data.end(),
                        window->new_data->data,
                        window->new_data->data + window->new_data->len);
    }

    return diff_data;
}

// Apply binary diff to reconstruct content
std::string BinaryDiff::apply(const std::string& base_content,
                              const std::vector<uint8_t>& diff) {
    TRACE_FN("base_size=", base_content.size(), " diff_size=", diff.size());

    std::string result;
    size_t offset = 0;

    auto read_uint64 = [&]() -> uint64_t {
        uint64_t val = 0;
        for (int i = 0; i < 8; i++) {
            val |= static_cast<uint64_t>(diff[offset++]) << (i * 8);
        }
        return val;
    };
    auto read_uint32 = [&]() -> uint32_t {
        uint32_t val = 0;
        for (int i = 0; i < 4; i++) {
            val |= static_cast<uint32_t>(diff[offset++]) << (i * 8);
        }
        return val;
    };

    // Process windows
    while (offset < diff.size()) {
        uint64_t sview_offset = read_uint64();
        uint64_t sview_len = read_uint64();
        uint64_t tview_len = read_uint64();
        uint32_t num_ops = read_uint32();

        std::string target_view;
        target_view.reserve(tview_len);

        // Process operations
        for (uint32_t i = 0; i < num_ops; i++) {
            uint8_t action = diff[offset++];
            uint64_t op_offset = read_uint64();
            uint64_t op_length = read_uint64();

            if (action == svn_txdelta_source) {
                // Copy from source (base_content)
                target_view.append(base_content.substr(sview_offset + op_offset, op_length));
            } else if (action == svn_txdelta_target) {
                // Copy from target (already constructed)
                target_view.append(target_view.substr(op_offset, op_length));
            } else if (action == svn_txdelta_new) {
                // Will be added from new_data below
            }
        }

        // Add new data
        uint32_t new_data_len = read_uint32();
        if (new_data_len > 0) {
            target_view.append(reinterpret_cast<const char*>(&diff[offset]), new_data_len);
            offset += new_data_len;
        }

        result.append(target_view);
    }

    return result;
}

// FeatureMask implementation
void FeatureMask::enableAll(const std::vector<Feature>& features) {
    for (auto f : features) {
        enable(f);
    }
}

void FeatureMask::disableAll(const std::vector<Feature>& features) {
    for (auto f : features) {
        disable(f);
    }
}

std::string FeatureMask::toString() const {
    return mask.toString();  // Uses BitVector's toString()
}

FeatureMask FeatureMask::fromString(const std::string& s) {
    FeatureMask fm;
    fm.mask = BitVector::fromString(s);
    return fm;
}

// ScopeStore implementation
uint64_t ScopeStore::createSnapshot(Vfs& vfs, const std::string& description) {
    TRACE_FN("desc=", description);

    ScopeSnapshot snapshot;
    snapshot.snapshot_id = next_snapshot_id++;
    snapshot.timestamp = std::time(nullptr);
    snapshot.description = description;
    snapshot.parent_snapshot_id = current_snapshot_id;
    snapshot.feature_mask = active_features;

    // Serialize current VFS state
    std::string current_state = serializeVfs(vfs);

    // Compute diff from parent if exists
    if (current_snapshot_id != 0 && snapshots.count(current_snapshot_id)) {
        const auto& parent = snapshots[current_snapshot_id];
        std::string parent_state = parent.diff_data.empty() ? "" :
            BinaryDiff::apply("", parent.diff_data);  // Reconstruct parent state

        // For root snapshot, store full state
        if (snapshot.parent_snapshot_id == 0) {
            parent_state = "";
        }

        snapshot.diff_data = BinaryDiff::compute(parent_state, current_state);
        snapshot.uncompressed_size = current_state.size();
    } else {
        // Root snapshot - store full state as diff from empty
        snapshot.diff_data = BinaryDiff::compute("", current_state);
        snapshot.uncompressed_size = current_state.size();
    }

    // Track affected paths (simplified - all paths for now)
    // TODO: Compute actual diff and track only changed paths
    // TODO: Implement VFS traversal using actual Vfs API (listDir, etc.)
    snapshot.affected_paths.push_back("/");

    snapshots[snapshot.snapshot_id] = std::move(snapshot);
    current_snapshot_id = snapshot.snapshot_id;

    return snapshot.snapshot_id;
}

bool ScopeStore::restoreSnapshot(Vfs& vfs, uint64_t snapshot_id) {
    TRACE_FN("snapshot_id=", snapshot_id);

    if (!snapshots.count(snapshot_id)) {
        return false;
    }

    // Reconstruct VFS state by applying diffs from root
    std::vector<uint64_t> path_to_snapshot;
    uint64_t curr = snapshot_id;
    while (curr != 0) {
        path_to_snapshot.push_back(curr);
        curr = snapshots[curr].parent_snapshot_id;
    }

    std::string state = "";
    for (auto it = path_to_snapshot.rbegin(); it != path_to_snapshot.rend(); ++it) {
        state = BinaryDiff::apply(state, snapshots[*it].diff_data);
    }

    deserializeVfs(vfs, state);
    current_snapshot_id = snapshot_id;
    return true;
}

bool ScopeStore::applyDiff(Vfs& vfs, uint64_t snapshot_id) {
    TRACE_FN("snapshot_id=", snapshot_id);

    if (!snapshots.count(snapshot_id)) {
        return false;
    }

    // Apply diff incrementally from current state
    std::string current_state = serializeVfs(vfs);
    const auto& snapshot = snapshots[snapshot_id];

    std::string new_state = BinaryDiff::apply(current_state, snapshot.diff_data);
    deserializeVfs(vfs, new_state);

    return true;
}

void ScopeStore::enableFeature(uint32_t feature_id) {
    if (feature_id < 512) {
        active_features.mask.set(feature_id);
    }
}

void ScopeStore::disableFeature(uint32_t feature_id) {
    if (feature_id < 512) {
        active_features.mask.clear(feature_id);
    }
}

bool ScopeStore::isFeatureActive(uint32_t feature_id) const {
    return feature_id < 512 && active_features.mask.test(feature_id);
}

std::vector<uint8_t> ScopeStore::computeDiff(uint64_t from_id, uint64_t to_id) {
    TRACE_FN("from=", from_id, " to=", to_id);

    if (!snapshots.count(from_id) || !snapshots.count(to_id)) {
        return {};
    }

    // Reconstruct both states
    Vfs temp_vfs;

    restoreSnapshot(temp_vfs, from_id);
    std::string from_state = serializeVfs(temp_vfs);

    restoreSnapshot(temp_vfs, to_id);
    std::string to_state = serializeVfs(temp_vfs);

    return BinaryDiff::compute(from_state, to_state);
}

void ScopeStore::save(const std::string& path) {
    TRACE_FN("path=", path);

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Failed to open scope store file for writing");
    }

    // Header
    out << "SCOPE_STORE_V1\n";
    out << "snapshot_count: " << snapshots.size() << "\n";
    out << "feature_count: " << feature_names.size() << "\n\n";

    // Features
    out << "[FEATURES]\n";
    for (const auto& [id, name] : feature_names) {
        out << id << ": " << name << "\n";
    }
    out << "\n";

    // Snapshots
    out << "[SNAPSHOTS]\n";
    for (const auto& [id, snap] : snapshots) {
        out << "id: " << snap.snapshot_id << "\n";
        out << "timestamp: " << snap.timestamp << "\n";
        out << "parent: " << snap.parent_snapshot_id << "\n";
        out << "description: " << snap.description << "\n";
        out << "feature_mask: " << snap.feature_mask.toString() << "\n";
        out << "affected_paths: " << snap.affected_paths.size() << "\n";
        for (const auto& p : snap.affected_paths) {
            out << "  " << p << "\n";
        }
        out << "diff_size: " << snap.diff_data.size() << "\n";
        out.write(reinterpret_cast<const char*>(snap.diff_data.data()), snap.diff_data.size());
        out << "\n";
    }
}

void ScopeStore::load(const std::string& path) {
    TRACE_FN("path=", path);

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Failed to open scope store file for reading");
    }

    std::string line;
    std::getline(in, line);
    if (line != "SCOPE_STORE_V1") {
        throw std::runtime_error("Invalid scope store file format");
    }

    // Parse header
    size_t snapshot_count = 0;
    size_t feature_count = 0;

    std::getline(in, line);  // snapshot_count
    sscanf(line.c_str(), "snapshot_count: %zu", &snapshot_count);

    std::getline(in, line);  // feature_count
    sscanf(line.c_str(), "feature_count: %zu", &feature_count);

    std::getline(in, line);  // empty line

    // Load features
    std::getline(in, line);  // [FEATURES]
    for (size_t i = 0; i < feature_count; i++) {
        std::getline(in, line);
        uint32_t id;
        char name[256];
        sscanf(line.c_str(), "%u: %s", &id, name);
        feature_names[id] = name;
    }

    std::getline(in, line);  // empty line
    std::getline(in, line);  // [SNAPSHOTS]

    // Load snapshots
    for (size_t i = 0; i < snapshot_count; i++) {
        ScopeSnapshot snap;

        std::getline(in, line);  // id
        sscanf(line.c_str(), "id: %lu", &snap.snapshot_id);

        std::getline(in, line);  // timestamp
        sscanf(line.c_str(), "timestamp: %lu", &snap.timestamp);

        std::getline(in, line);  // parent
        sscanf(line.c_str(), "parent: %lu", &snap.parent_snapshot_id);

        std::getline(in, line);  // description
        snap.description = line.substr(13);  // Skip "description: "

        std::getline(in, line);  // feature_mask
        snap.feature_mask = FeatureMask::fromString(line.substr(14));

        std::getline(in, line);  // affected_paths count
        size_t path_count;
        sscanf(line.c_str(), "affected_paths: %zu", &path_count);

        for (size_t j = 0; j < path_count; j++) {
            std::getline(in, line);
            snap.affected_paths.push_back(line.substr(2));  // Skip "  "
        }

        std::getline(in, line);  // diff_size
        size_t diff_size;
        sscanf(line.c_str(), "diff_size: %zu", &diff_size);

        snap.diff_data.resize(diff_size);
        in.read(reinterpret_cast<char*>(snap.diff_data.data()), diff_size);

        std::getline(in, line);  // newline after binary data

        snapshots[snap.snapshot_id] = std::move(snap);
        if (snap.snapshot_id >= next_snapshot_id) {
            next_snapshot_id = snap.snapshot_id + 1;
        }
    }
}

std::string ScopeStore::serializeVfs(Vfs& vfs) {
    TRACE_FN();

    std::ostringstream out;

    // Simple VFS serialization format
    // TODO: Implement proper VFS traversal using actual Vfs API
    // Stub implementation for now
    (void)vfs;  // Suppress unused warning
    out << "VFS_STUB\n";

    return out.str();
}

void ScopeStore::deserializeVfs(Vfs& vfs, const std::string& data) {
    TRACE_FN();

    // Clear VFS
    vfs = Vfs();

    // TODO: Implement proper VFS deserialization using actual Vfs API
    // Stub implementation for now
    (void)data;  // Suppress unused warning
}

// DeterministicContextBuilder implementation
std::string DeterministicContextBuilder::build(const BuildOptions& opts) {
    TRACE_FN("snapshot_id=", opts.snapshot_id);

    // TODO: Implement full context building with VFS traversal
    // Stub implementation for now
    std::ostringstream context;

    if (opts.include_metadata && opts.snapshot_id != 0 &&
        scope_store.snapshots.count(opts.snapshot_id)) {
        const auto& snap = scope_store.snapshots[opts.snapshot_id];
        context << "=== Snapshot " << snap.snapshot_id << " ===\n";
        context << "Description: " << snap.description << "\n";
        context << "Timestamp: " << snap.timestamp << "\n";
        context << "Features: " << snap.feature_mask.toString() << "\n\n";
    }

    context << "CONTEXT_STUB\n";
    return context.str();
}

uint64_t DeterministicContextBuilder::contextHash(const BuildOptions& opts) {
    TRACE_FN();

    std::string context = build(opts);
    return ContextBuilder::contentFingerprint(context);
}

DeterministicContextBuilder::ContextDiff DeterministicContextBuilder::compareContexts(
    uint64_t snapshot1, uint64_t snapshot2) {
    TRACE_FN("snap1=", snapshot1, " snap2=", snapshot2);

    ContextDiff diff;

    // TODO: Implement proper snapshot comparison using actual Vfs API
    // Stub implementation for now
    (void)snapshot1;
    (void)snapshot2;

    diff.token_delta = 0;

    return diff;
}

DeterministicContextBuilder::ReplayResult DeterministicContextBuilder::replay(
    const std::vector<uint64_t>& snapshot_ids) {
    TRACE_FN("snapshot_count=", snapshot_ids.size());

    ReplayResult result;

    for (uint64_t snap_id : snapshot_ids) {
        BuildOptions opts;
        opts.snapshot_id = snap_id;
        opts.stable_sort = true;

        std::string context = build(opts);
        uint64_t hash = contextHash(opts);

        result.contexts.push_back(context);
        result.hashes.push_back(hash);
        result.token_counts.push_back(context.size() / 4);  // Rough token estimate
    }

    return result;
}

