#pragma once

#include <set>

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
    std::optional<std::string> mapToHostPath(const std::string& vfs_path) const;
    std::optional<std::string> mapFromHostPath(const std::string& host_path) const;

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

char type_char(const std::shared_ptr<VfsNode>& node);
