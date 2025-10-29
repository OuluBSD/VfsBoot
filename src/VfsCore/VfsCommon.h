#ifndef _VfsCore_VfsCommon_h_
#define _VfsCore_VfsCommon_h_

#include <Core/Core.h>

using namespace Upp;

#include <string>
#include <vector>
#include <memory>
#include <string>
#include <map>
#include <vector>
#include <functional>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <memory>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <future>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <list>
#include <deque>
#include <array>
#include <forward_list>
#include <stack>
#include <utility>
#include <tuple>
#include <iterator>
#include <type_traits>
#include <stdexcept>
#include <exception>
#include <system_error>
#include <cerrno>
#include <cstring>
#include <cstddef>
#include <cstdint>
#include <climits>
#include <cfloat>
#include <cmath>
#include <cassert>
#include <cctype>
#include <cwctype>
#include <cstdarg>
#include <ctime>
#include <cstdlib>
#include <cstdio>
#include <csignal>
#include <csetjmp>
#include <new>
#include <limits>
#include <ratio>
#include <cfenv>
#include <locale>
#include <clocale>
#include <codecvt>
#include <random>
#include <chrono>
#include <ratio>
#include <regex>
#include <filesystem>
#include <optional>
#include <variant>
#include <any>
#include <typeinfo>
#include <typeindex>
#include <atomic>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <future>
#include <chrono>
#include <ratio>
#include <ctime>
#include <cinttypes>

// Forward declarations for types defined elsewhere
struct Vfs;
struct WorkingDirectory;

// VFS Node base class
struct VfsNode {
    enum class Kind { Dir, File, Ast, Mount, Library, Remote };
    Kind kind;
    String name;
    std::weak_ptr<VfsNode> parent;  // Parent directory

    VfsNode() : kind(Kind::File) {}
    explicit VfsNode(Kind k) : kind(k) {}
    VfsNode(Kind k, String n) : kind(k), name(n) {}
    virtual ~VfsNode() = default;

    virtual bool isDir() const { return kind == Kind::Dir; }
    virtual String read() const { return String(); }
    virtual void write(const String& s) { (void)s; }
    virtual std::unordered_map<std::string, std::shared_ptr<VfsNode>>& children() {
        static std::unordered_map<std::string, std::shared_ptr<VfsNode>> empty;
        return empty;
    }
};

// Tracing (optional debug feature)
// Only define TRACE macros if not already defined to avoid redefinition warnings
#ifndef TRACE_FN
#define TRACE_FN(...) ((void)0)
#endif

#ifndef TRACE_MSG
#define TRACE_MSG(...) ((void)0)
#endif
#define TRACE_LOOP(...)

// i18n (internationalization)
namespace i18n {
    enum class MsgId {
        WELCOME, UNKNOWN_COMMAND, DISCUSS_HINT,
        FILE_NOT_FOUND,
        DIR_NOT_FOUND, NOT_A_FILE, NOT_A_DIR,
        PARSE_ERROR, EVAL_ERROR, HELP_TEXT
    };
    
    const char* get(MsgId id);
    void init();
    void set_english_only();
}

// Hash utilities
std::string compute_file_hash(const std::string& filepath);
std::string compute_string_hash(const std::string& data);

// Directory and File node types
struct DirNode : VfsNode {
    std::unordered_map<std::string, std::shared_ptr<VfsNode>> ch;

    DirNode() : VfsNode(Kind::Dir) {}
    explicit DirNode(String n) : VfsNode(Kind::Dir, n) {}

    bool isDir() const override { return true; }
    std::unordered_map<std::string, std::shared_ptr<VfsNode>>& children() override { return ch; }
};

struct FileNode : VfsNode {
    String content;

    FileNode() : VfsNode(Kind::File) {}
    FileNode(String n, String c) : VfsNode(Kind::File, n), content(c) {}

    String read() const override { return content; }
    void write(const String& s) override { content = s; }
};

// Forward declarations for other types
using TagId = size_t;
struct TagRegistry;
struct TagStorage;
struct LogicEngine;
struct MetricsCollector;
struct RulePatchStaging;
struct FeedbackLoop;

struct WorkingDirectory {
    enum class ConflictPolicy { Manual, Oldest, Newest };

    std::string path;
    std::vector<size_t> overlays;
    ConflictPolicy conflict_policy = ConflictPolicy::Manual;
    size_t primary_overlay = 0;

    std::vector<std::string> cwd_stack;
    std::vector<size_t> overlay_stack;
};

// VFS Overlay structure
struct Overlay {
    std::string name;
    std::shared_ptr<VfsNode> root;
    std::string policy;
    std::string mount_path;
};

// Tree options for advanced tree display
struct TreeOptions {
    bool show_sizes = false;
    bool show_tags = false;
    bool use_colors = false;
    bool show_node_kind = false;
    bool use_box_chars = false;
    bool sort_entries = false;
    int max_depth = -1;
    std::string filter_pattern;
};

// Main VFS structure
struct Vfs {
    // Nested types
    struct OverlayHit {
        size_t overlay_id;
        std::shared_ptr<VfsNode> node;
    };

    struct DirListing {
        std::vector<std::string> entries;
        std::vector<char> types;
    };

    std::shared_ptr<VfsNode> root;
    std::vector<Overlay> overlay_stack;
    std::vector<bool> overlay_dirty;
    std::vector<std::string> overlay_source;

    TagRegistry* tag_registry = nullptr;
    TagStorage* tag_storage = nullptr;
    LogicEngine* logic_engine = nullptr;

    Vfs();
    std::vector<std::string> splitPath(const std::string& p) const;
    size_t overlayCount() const;
    const std::string& overlayName(size_t id) const;
    std::shared_ptr<VfsNode> overlayRoot(size_t id) const;
    bool overlayDirty(size_t id) const;
    const std::string& overlaySource(size_t id) const;
    void clearOverlayDirty(size_t id);
    void setOverlaySource(size_t id, std::string path);
    size_t findOverlay(const std::string& name) const;
    std::optional<size_t> findOverlayByName(const std::string& name) const;
    void markOverlayDirty(size_t id);
    size_t registerOverlay(std::string name, std::shared_ptr<VfsNode> overlayRoot = nullptr);
    void unregisterOverlay(size_t overlayId);
    std::vector<size_t> allOverlays() const;
    std::shared_ptr<VfsNode> find(const std::string& path, const std::vector<size_t>& overlays) const;
    std::shared_ptr<VfsNode> ensureDir(const std::string& path, size_t overlayId);
    std::shared_ptr<VfsNode> ensureDirForOverlay(const std::string& path, size_t overlayId);
    void add(const std::string& path, std::shared_ptr<VfsNode> node, size_t overlayId);
    std::string read(const std::string& path, const std::vector<size_t>& overlays);
    void write(const std::string& path, const std::string& content, size_t overlayId);
    void remove(const std::string& path, size_t overlayId);
    void move(const std::string& from, const std::string& to, size_t overlayId);
    void addNode(const std::string& dirpath, std::shared_ptr<VfsNode> n, size_t overlayId);
    std::shared_ptr<VfsNode> resolveForOverlay(const std::string& path, size_t overlayId);

    // Additional methods used in VfsCore.cpp
    std::vector<size_t> overlaysForPath(const std::string& path) const;
    std::vector<OverlayHit> resolveMulti(const std::string& path) const;
    std::vector<OverlayHit> resolveMulti(const std::string& path, const std::vector<size_t>& allowed) const;
    std::shared_ptr<VfsNode> resolve(const std::string& path);
    std::shared_ptr<VfsNode> tryResolveForOverlay(const std::string& path, size_t overlayId) const;
    void mkdir(const std::string& path, size_t overlayId);
    void touch(const std::string& path, size_t overlayId);
    std::string read(const std::string& path, std::optional<size_t> overlayId) const;
    void rm(const std::string& path, size_t overlayId);
    void mv(const std::string& src, const std::string& dst, size_t overlayId);
    void link(const std::string& src, const std::string& dst, size_t overlayId);
    DirListing listDir(const std::string& p, const std::vector<size_t>& overlays) const;

    // Tree display methods
    void tree(std::shared_ptr<VfsNode> n, std::string pref);
    std::string formatTreeNode(VfsNode* node, const std::string& path, const TreeOptions& opts);
    void treeAdvanced(std::shared_ptr<VfsNode> n, const std::string& path,
                     const TreeOptions& opts, int depth = 0, bool is_last = false);
    void treeAdvanced(const std::string& path, const TreeOptions& opts);
    void ls(const std::string& p);

    // Tag registry methods
    TagId registerTag(const std::string& name);
    TagId getTagId(const std::string& name) const;
    std::string getTagName(TagId id) const;
    bool hasTagRegistered(const std::string& name) const;
    std::vector<std::string> allRegisteredTags() const;

    // Tag node methods
    void addTag(const std::string& vfs_path, const std::string& tag_name);
    void removeTag(const std::string& vfs_path, const std::string& tag_name);
    bool nodeHasTag(const std::string& vfs_path, const std::string& tag_name) const;
    std::vector<std::string> getNodeTags(const std::string& vfs_path) const;
};

bool run_ncurses_editor(Vfs& vfs, const std::string& vfs_path,
                        std::vector<std::string>& lines,
                        bool file_exists, size_t overlay_id);

void vfs_add(Vfs& vfs, const std::string& path, std::shared_ptr<VfsNode> node, size_t overlayId);

#endif
