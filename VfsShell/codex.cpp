#include "codex.h"
#include "snippet_catalog.h"
#include <fstream>
#include <sstream>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <mutex>
#include <optional>
#include <random>
#include <regex>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <limits>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <blake3.h>
#include <dlfcn.h>

#ifdef CODEX_TRACE
#include <fstream>

namespace codex_trace {
    namespace {
        std::mutex& trace_mutex(){ static std::mutex m; return m; }
        std::ofstream& trace_stream(){
            static std::ofstream s("codex_trace.log", std::ios::app);
            return s;
        }
        void write_line(const std::string& line){
            auto& os = trace_stream();
            os << line << '\n';
            os.flush();
        }
    }

    void log_line(const std::string& line){
        std::lock_guard<std::mutex> lock(trace_mutex());
        write_line(line);
    }

    Scope::Scope(const char* fn, const std::string& details) : name(fn ? fn : "?"){
        if(!name.empty()){
            std::string msg = std::string("enter ") + name;
            if(!details.empty()) msg += " | " + details;
            log_line(msg);
        }
    }

    Scope::~Scope(){
        if(!name.empty()){
            log_line(std::string("exit ") + name);
        }
    }

    void log_loop(const char* tag, const std::string& details){
        std::lock_guard<std::mutex> lock(trace_mutex());
        std::string msg = std::string("loop ") + (tag ? tag : "?");
        if(!details.empty()) msg += " | " + details;
        write_line(msg);
    }
}
#endif

//
// Internationalization implementation
//
namespace i18n {
    namespace {
        enum class Lang { EN, FI };
        Lang current_lang = Lang::EN;

        struct MsgTable {
            const char* en;
#ifdef CODEX_I18N_ENABLED
            const char* fi;
#endif
        };

        const MsgTable messages[] = {
            // WELCOME
            { "VfsShell 🌲 VFS+AST+AI — type 'help' for available commands."
#ifdef CODEX_I18N_ENABLED
            , "VfsShell 🌲 VFS+AST+AI — 'help' kertoo karun totuuden."
#endif
            },
            // UNKNOWN_COMMAND
            { "error: unknown command. Type 'help' for available commands."
#ifdef CODEX_I18N_ENABLED
            , "virhe: tuntematon komento. 'help' kertoo karun totuuden."
#endif
            },
        };

        Lang detect_language() {
            const char* lang_env = std::getenv("LANG");
            if(!lang_env) lang_env = std::getenv("LC_MESSAGES");
            if(!lang_env) lang_env = std::getenv("LC_ALL");

            if(lang_env) {
                std::string lang_str(lang_env);
                if(lang_str.find("fi_") == 0 || lang_str.find("fi.") == 0 ||
                   lang_str.find("finnish") != std::string::npos ||
                   lang_str.find("Finnish") != std::string::npos) {
                    return Lang::FI;
                }
            }
            return Lang::EN;
        }
    }

    void init() {
#ifdef CODEX_I18N_ENABLED
        current_lang = detect_language();
#else
        current_lang = Lang::EN;
#endif
    }

    const char* get(MsgId id) {
        size_t idx = static_cast<size_t>(id);
        if(idx >= sizeof(messages) / sizeof(messages[0])) {
            return "??? missing translation ???";
        }
#ifdef CODEX_I18N_ENABLED
        if(current_lang == Lang::FI) {
            return messages[idx].fi;
        }
#endif
        return messages[idx].en;
    }
}

static std::string trim_copy(const std::string& s){
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b-1]))) --b;
    return s.substr(a, b - a);
}
static std::string join_path(const std::string& base, const std::string& leaf){
    if (base.empty() || base == "/") return std::string("/") + leaf;
    if (!leaf.empty() && leaf[0]=='/') return leaf;
    if (base.back()=='/') return base + leaf;
    return base + "/" + leaf;
}
static std::string normalize_path(const std::string& cwd, const std::string& operand){
    auto stack = (operand.empty() || operand[0]!='/') ? Vfs::splitPath(cwd.empty() ? "/" : cwd)
                                                    : std::vector<std::string>{};
    auto apply = [&stack](const std::string& part){
        if(part.empty() || part==".") return;
        if(part==".."){ if(!stack.empty()) stack.pop_back(); return; }
        stack.push_back(part);
    };
    if(!operand.empty()){
        for(auto& part : Vfs::splitPath(operand)) apply(part);
    }
    if(stack.empty()) return "/";
    std::string out;
    for(const auto& part : stack){
        out.push_back('/');
        out += part;
    }
    return out.empty() ? std::string("/") : out;
}
static std::string path_basename(const std::string& path){
    if(path.empty() || path=="/") return "/";
    auto pos = path.find_last_of('/');
    if(pos==std::string::npos) return path;
    return path.substr(pos+1);
}
static std::string path_dirname(const std::string& path){
    if(path.empty() || path=="/") return "/";
    auto pos = path.find_last_of('/');
    if(pos==std::string::npos) return "";
    if(pos==0) return "/";
    return path.substr(0, pos);
}

//
// BLAKE3 hash functions
//
std::string compute_string_hash(const std::string& data){
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, data.data(), data.size());
    uint8_t output[BLAKE3_OUT_LEN];
    blake3_hasher_finalize(&hasher, output, BLAKE3_OUT_LEN);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for(size_t i = 0; i < BLAKE3_OUT_LEN; ++i){
        oss << std::setw(2) << static_cast<unsigned>(output[i]);
    }
    return oss.str();
}

std::string compute_file_hash(const std::string& filepath){
    std::ifstream file(filepath, std::ios::binary);
    if(!file) throw std::runtime_error("cannot open file for hashing: " + filepath);

    blake3_hasher hasher;
    blake3_hasher_init(&hasher);

    constexpr size_t BUFFER_SIZE = 65536;
    std::vector<char> buffer(BUFFER_SIZE);
    while(file.read(buffer.data(), BUFFER_SIZE) || file.gcount() > 0){
        blake3_hasher_update(&hasher, buffer.data(), static_cast<size_t>(file.gcount()));
    }

    uint8_t output[BLAKE3_OUT_LEN];
    blake3_hasher_finalize(&hasher, output, BLAKE3_OUT_LEN);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for(size_t i = 0; i < BLAKE3_OUT_LEN; ++i){
        oss << std::setw(2) << static_cast<unsigned>(output[i]);
    }
    return oss.str();
}

static std::pair<std::string, std::string> serialize_ast_node(const std::shared_ptr<AstNode>& node);
static std::shared_ptr<AstNode> deserialize_ast_node(const std::string& type,
                                                     const std::string& payload,
                                                     const std::string& path,
                                                     std::vector<std::function<void()>>& fixups,
                                                     std::unordered_map<std::string, std::shared_ptr<VfsNode>>& path_map);

struct WorkingDirectory {
    std::string path = "/";
    std::vector<size_t> overlays{0};
    size_t primary_overlay = 0;
    enum class ConflictPolicy { Manual, Oldest, Newest };
    ConflictPolicy conflict_policy = ConflictPolicy::Manual;
};

struct SolutionContext {
    bool active = false;
    bool auto_detected = false;
    size_t overlay_id = 0;
    std::string title;
    std::string file_path;
};

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

static std::function<void()> g_on_save_shortcut;
static constexpr const char* kPackageExtension = ".cxpkg";
static constexpr const char* kAssemblyExtension = ".cxasm";

static void sort_unique(std::vector<size_t>& ids){
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
}

static const char* policy_label(WorkingDirectory::ConflictPolicy policy){
    switch(policy){
        case WorkingDirectory::ConflictPolicy::Manual: return "manual";
        case WorkingDirectory::ConflictPolicy::Oldest: return "oldest";
        case WorkingDirectory::ConflictPolicy::Newest: return "newest";
    }
    return "?";
}

static std::optional<WorkingDirectory::ConflictPolicy> parse_policy(const std::string& name){
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    if(lower == "manual" || lower == "default") return WorkingDirectory::ConflictPolicy::Manual;
    if(lower == "oldest" || lower == "first") return WorkingDirectory::ConflictPolicy::Oldest;
    if(lower == "newest" || lower == "last") return WorkingDirectory::ConflictPolicy::Newest;
    return std::nullopt;
}

static size_t select_overlay(const Vfs& vfs, const WorkingDirectory& cwd, const std::vector<size_t>& overlays){
    if(overlays.empty()) throw std::runtime_error("overlay selection: no candidates");
    auto contains_primary = std::find(overlays.begin(), overlays.end(), cwd.primary_overlay) != overlays.end();
    switch(cwd.conflict_policy){
        case WorkingDirectory::ConflictPolicy::Manual:
            if(contains_primary) return cwd.primary_overlay;
            break;
        case WorkingDirectory::ConflictPolicy::Newest:
            return *std::max_element(overlays.begin(), overlays.end());
        case WorkingDirectory::ConflictPolicy::Oldest:
            return *std::min_element(overlays.begin(), overlays.end());
    }
    std::ostringstream oss;
    oss << "ambiguous overlays: ";
    for(size_t i = 0; i < overlays.size(); ++i){
        if(i) oss << ", ";
        oss << vfs.overlayName(overlays[i]);
    }
    oss << ". use overlay.use or overlay.policy";
    throw std::runtime_error(oss.str());
}

static std::string overlay_suffix(const Vfs& vfs, const std::vector<size_t>& overlays, size_t primary){
    if(overlays.empty()) return {};
    std::ostringstream oss;
    oss << " [";
    for(size_t i = 0; i < overlays.size(); ++i){
        if(i) oss << ", ";
        size_t id = overlays[i];
        oss << vfs.overlayName(id);
        if(id == primary) oss << "*";
    }
    oss << "]";
    return oss.str();
}

static void update_directory_context(Vfs& vfs, WorkingDirectory& cwd, const std::string& absPath){
    auto candidates = vfs.overlaysForPath(absPath);
    if(candidates.empty()) throw std::runtime_error("cd: not a directory");
    sort_unique(candidates);
    cwd.path = absPath;
    cwd.overlays = candidates;
    auto pick_primary = [&]() -> size_t {
        switch(cwd.conflict_policy){
            case WorkingDirectory::ConflictPolicy::Manual:
                if(std::find(candidates.begin(), candidates.end(), cwd.primary_overlay) != candidates.end())
                    return cwd.primary_overlay;
                return candidates.front();
            case WorkingDirectory::ConflictPolicy::Oldest:
                return *std::min_element(candidates.begin(), candidates.end());
            case WorkingDirectory::ConflictPolicy::Newest:
                return *std::max_element(candidates.begin(), candidates.end());
        }
        return candidates.front();
    };
    cwd.primary_overlay = pick_primary();
}

static void adjust_context_after_unmount(Vfs& vfs, WorkingDirectory& cwd, size_t removedId){
    auto adjust = [&](std::vector<size_t>& ids){
        ids.erase(std::remove(ids.begin(), ids.end(), removedId), ids.end());
        for(auto& id : ids){
            if(id > removedId) --id;
        }
        if(ids.empty()) ids.push_back(0);
        sort_unique(ids);
    };

    adjust(cwd.overlays);
    if(cwd.primary_overlay == removedId){
        cwd.primary_overlay = cwd.overlays.front();
    } else if(cwd.primary_overlay > removedId){
        --cwd.primary_overlay;
    }

    try{
        update_directory_context(vfs, cwd, cwd.path);
    } catch(const std::exception&){
        cwd.path = "/";
        update_directory_context(vfs, cwd, cwd.path);
    }
}

static void maybe_extend_context(Vfs& vfs, WorkingDirectory& cwd){
    try{
        update_directory_context(vfs, cwd, cwd.path);
    } catch(const std::exception&){
        // keep existing overlays if path vanished; caller should handle
    }
}

static size_t mount_overlay_from_file(Vfs& vfs, const std::string& name, const std::string& hostPath){
    TRACE_FN("name=", name, ", file=", hostPath);
    if(name.empty()) throw std::runtime_error("overlay: name required");
    std::ifstream in(hostPath, std::ios::binary);
    if(!in) throw std::runtime_error("overlay: cannot open file");

    std::string header;
    if(!std::getline(in, header)) throw std::runtime_error("overlay: empty file");
    auto trimmed = trim_copy(header);
    int version = 0;
    if(trimmed == "# codex-vfs-overlay 1") version = 1;
    if(trimmed == "# codex-vfs-overlay 2") version = 2;
    if(trimmed == "# codex-vfs-overlay 3") version = 3;
    if(version == 0) throw std::runtime_error("overlay: invalid header");

    std::string source_file;
    std::string source_hash;

    // Version 3 adds source file hash tracking
    if(version >= 3){
        std::string hash_line;
        if(std::getline(in, hash_line)){
            auto hash_trimmed = trim_copy(hash_line);
            if(!hash_trimmed.empty() && hash_trimmed[0] == 'H'){
                std::istringstream iss(hash_trimmed);
                char tag;
                if(iss >> tag >> source_file >> source_hash){
                    // Successfully parsed hash line
                } else {
                    // Not a valid hash line, rewind if possible (though we can't easily)
                    // For now, just proceed without hash
                }
            }
        }
    }

    auto root = std::make_shared<DirNode>("/");
    root->name = "/";
    root->parent.reset();

    std::unordered_map<std::string, std::shared_ptr<VfsNode>> path_map;
    path_map["/"] = root;
    std::vector<std::function<void()>> ast_fixups;

    auto ensure_dir = [&](const std::string& path) -> std::shared_ptr<DirNode> {
        if(path.empty() || path == "/") return root;
        auto parts = Vfs::splitPath(path);
        std::shared_ptr<VfsNode> cur = root;
        std::string curPath = "/";
        for(const auto& part : parts){
            if(!cur->isDir()) throw std::runtime_error("overlay: conflicting node at " + path);
            auto& ch = cur->children();
            auto it = ch.find(part);
            if(it == ch.end()){
                auto dir = std::make_shared<DirNode>(part);
                dir->parent = cur;
                ch[part] = dir;
                cur = dir;
            } else {
                cur = it->second;
            }
            curPath = join_path(curPath, part);
            path_map[curPath] = cur;
        }
        if(!cur->isDir()) throw std::runtime_error("overlay: conflicting node at " + path);
        return std::static_pointer_cast<DirNode>(cur);
    };

    auto create_file = [&](const std::string& path, std::string content){
        auto parts = Vfs::splitPath(path);
        if(parts.empty()) throw std::runtime_error("overlay: invalid file path");
        std::string namePart = parts.back();
        parts.pop_back();
        std::shared_ptr<DirNode> dir = root;
        if(!parts.empty()){
            std::string dirPath = "/";
            for(const auto& part : parts) dirPath = join_path(dirPath, part);
            dir = ensure_dir(dirPath);
        }
        auto& ch = dir->children();
        auto file = std::make_shared<FileNode>(namePart, std::move(content));
        file->parent = dir;
        ch[namePart] = file;
        path_map[path] = file;
    };

    auto create_ast = [&](const std::string& path, const std::string& type, std::string payload){
        auto parts = Vfs::splitPath(path);
        if(parts.empty()) throw std::runtime_error("overlay: invalid ast path");
        std::string namePart = parts.back();
        parts.pop_back();
        std::shared_ptr<DirNode> dir = root;
        if(!parts.empty()){
            std::string dirPath = "/";
            for(const auto& part : parts) dirPath = join_path(dirPath, part);
            dir = ensure_dir(dirPath);
        }
        auto node = deserialize_ast_node(type, payload, path, ast_fixups, path_map);
        node->name = namePart;
        node->parent = dir;
        dir->children()[namePart] = node;
        path_map[path] = node;
    };

    while(true){
        std::streampos entry_pos = in.tellg();
        if(entry_pos == std::streampos(-1)) break;
        int peeked = in.peek();
        if(peeked == EOF) break;
        std::string line;
        if(!std::getline(in, line)) break;
        if(line.empty()) continue;

        if(line[0] == 'D' && line.size() > 1 && std::isspace(static_cast<unsigned char>(line[1]))){
            std::string path = trim_copy(line.substr(2));
            if(path.empty() || path[0] != '/') throw std::runtime_error("overlay: invalid dir path");
            ensure_dir(path);
            continue;
        }

        if(line[0] == 'F' && line.size() > 1 && std::isspace(static_cast<unsigned char>(line[1]))){
            std::istringstream meta(line);
            char tag;
            std::string path;
            size_t size = 0;
            if(!(meta >> tag >> path >> size))
                throw std::runtime_error("overlay: malformed file entry");
            if(path.empty() || path[0] != '/')
                throw std::runtime_error("overlay: invalid file path");
            std::string content(size, '\0');
            in.read(content.data(), static_cast<std::streamsize>(size));
            if(static_cast<size_t>(in.gcount()) != size)
                throw std::runtime_error("overlay: truncated file content");
            int term = in.peek();
            if(term == '\r'){
                in.get();
                if(in.peek() == '\n') in.get();
            } else if(term == '\n'){
                in.get();
            }
            create_file(path, std::move(content));
            continue;
        }

        if(line[0] == 'A' && line.size() > 1 && std::isspace(static_cast<unsigned char>(line[1]))){
            if(version < 2) throw std::runtime_error("overlay: AST entry not supported in version 1 snapshot");
            std::istringstream meta(line);
            char tag;
            std::string path;
            std::string type;
            size_t size = 0;
            if(!(meta >> tag >> path >> type >> size))
                throw std::runtime_error("overlay: malformed ast entry");
            if(path.empty() || path[0] != '/')
                throw std::runtime_error("overlay: invalid ast path");
            std::string payload(size, '\0');
            in.read(payload.data(), static_cast<std::streamsize>(size));
            if(static_cast<size_t>(in.gcount()) != size)
                throw std::runtime_error("overlay: truncated ast payload");
            int term = in.peek();
            if(term == '\r'){
                in.get();
                if(in.peek() == '\n') in.get();
            } else if(term == '\n'){
                in.get();
            }
            create_ast(path, type, std::move(payload));
            continue;
        }

        throw std::runtime_error("overlay: unknown entry near byte " + std::to_string(static_cast<long long>(entry_pos)));
    }

    for(auto& fix : ast_fixups) fix();

    auto id = vfs.registerOverlay(name, root);
    vfs.setOverlaySource(id, hostPath);

    // Set source file and hash for version 3
    if(version >= 3 && !source_file.empty()){
        vfs.overlay_stack[id].source_file = source_file;
        vfs.overlay_stack[id].source_hash = source_hash;

        // Verify hash if source file exists
        if(!source_hash.empty()){
            try{
                std::filesystem::path src_path = source_file;
                if(src_path.is_relative()){
                    // Try to resolve relative to the .vfs file location
                    std::filesystem::path vfs_dir = std::filesystem::path(hostPath).parent_path();
                    if(!vfs_dir.empty()){
                        src_path = vfs_dir / src_path;
                    }
                }

                if(std::filesystem::exists(src_path)){
                    std::string current_hash = compute_file_hash(src_path.string());
                    if(current_hash != source_hash){
                        std::cout << "warning: source file hash mismatch for " << source_file << "\n";
                        std::cout << "  expected: " << source_hash << "\n";
                        std::cout << "  current:  " << current_hash << "\n";
                        std::cout << "  VFS may be out of sync with source. Consider re-parsing.\n";
                    }
                }
            } catch(const std::exception& e){
                std::cout << "note: could not verify source hash: " << e.what() << "\n";
            }
        }
    }

    return id;
}

// Forward declaration
static void save_overlay_to_file(Vfs& vfs, size_t overlayId, const std::string& hostPath);

static std::string get_timestamp_string(){
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
    localtime_r(&time_t, &tm_buf);
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d-%H%M%S");
    return oss.str();
}

static void create_timestamped_backup(const std::string& filepath){
    std::filesystem::path src(filepath);
    if(!std::filesystem::exists(src)) return;

    auto parent = src.parent_path();
    if(parent.empty()) parent = ".";
    std::filesystem::path backup_dir = parent / ".vfsh";

    std::error_code ec;
    std::filesystem::create_directories(backup_dir, ec);
    if(ec) throw std::runtime_error("failed to create .vfsh directory: " + ec.message());

    std::string timestamp = get_timestamp_string();
    std::string backup_name = src.filename().string() + "." + timestamp + ".bak";
    std::filesystem::path backup_path = backup_dir / backup_name;

    std::filesystem::copy_file(src, backup_path, std::filesystem::copy_options::overwrite_existing, ec);
    if(ec) throw std::runtime_error("failed to create backup: " + ec.message());
}

static void save_crash_recovery(Vfs& vfs, const AutosaveContext& autosave_ctx){
    try{
        namespace fs = std::filesystem;
        fs::path cwd = fs::current_path();
        fs::path recovery_dir = cwd / ".vfsh";
        fs::create_directories(recovery_dir);

        fs::path recovery_path = recovery_dir / "recovery.vfs";

        // Save all VFS overlays that are tracked for autosave
        std::ofstream out(recovery_path, std::ios::binary | std::ios::trunc);
        if(!out) return;

        out << "# codex-vfs-overlay 3\n";
        out << "# crash recovery snapshot\n";

        // For now, just save the base overlay as crash recovery
        // TODO: extend to save multiple overlays if needed
        if(vfs.overlayCount() > 0){
            auto root = vfs.overlayRoot(0);
            if(root){
                std::function<void(const std::shared_ptr<VfsNode>&, const std::string&)> dump;
                dump = [&](const std::shared_ptr<VfsNode>& node, const std::string& path){
                    if(!node) return;
                    bool traverse = node->isDir();
                    if(node->kind == VfsNode::Kind::Dir){
                        if(path != "/"){
                            out << "D " << path << "\n";
                        }
                    } else if(node->kind == VfsNode::Kind::File){
                        auto data = node->read();
                        out << "F " << path << " " << data.size() << "\n";
                        if(!data.empty()){
                            out.write(data.data(), static_cast<std::streamsize>(data.size()));
                        }
                        out << "\n";
                        return;
                    }

                    if(traverse){
                        const auto& children = node->children();
                        for(const auto& [name, child] : children){
                            std::string childPath = join_path(path, name);
                            dump(child, childPath);
                        }
                    }
                };
                dump(root, "/");
            }
        }
    } catch(const std::exception&){
        // Silently fail - don't disrupt normal operation
    }
}

static void autosave_thread_func(Vfs* vfs_ptr, AutosaveContext* autosave_ctx){
    while(!autosave_ctx->should_stop.load()){
        std::this_thread::sleep_for(std::chrono::seconds(1));

        if(!autosave_ctx->enabled) continue;

        auto now = std::chrono::steady_clock::now();

        // Check for autosave of solution files (.cxpkg/.cxasm and their chained .vfs)
        {
            std::lock_guard<std::mutex> lock(autosave_ctx->mtx);
            auto since_modification = std::chrono::duration_cast<std::chrono::seconds>(
                now - autosave_ctx->last_modification).count();

            if(since_modification >= autosave_ctx->delay_seconds){
                // Check if any tracked overlays are dirty
                bool any_dirty = false;
                for(size_t id : autosave_ctx->solution_overlay_ids){
                    if(id < vfs_ptr->overlayCount() && vfs_ptr->overlayDirty(id)){
                        any_dirty = true;
                        break;
                    }
                }

                if(any_dirty){
                    // Save each dirty overlay
                    for(size_t id : autosave_ctx->solution_overlay_ids){
                        if(id < vfs_ptr->overlayCount() && vfs_ptr->overlayDirty(id)){
                            const auto& source = vfs_ptr->overlaySource(id);
                            if(!source.empty()){
                                try{
                                    save_overlay_to_file(*vfs_ptr, id, source);
                                } catch(const std::exception&){
                                    // Silently continue
                                }
                            }
                        }
                    }
                    // Reset modification time
                    autosave_ctx->last_modification = now;
                }
            }
        }

        // Check for crash recovery snapshot
        {
            std::lock_guard<std::mutex> lock(autosave_ctx->mtx);
            auto since_recovery = std::chrono::duration_cast<std::chrono::seconds>(
                now - autosave_ctx->last_crash_recovery).count();

            if(since_recovery >= autosave_ctx->crash_recovery_interval_seconds){
                save_crash_recovery(*vfs_ptr, *autosave_ctx);
                autosave_ctx->last_crash_recovery = now;
            }
        }
    }
}

static void save_overlay_to_file(Vfs& vfs, size_t overlayId, const std::string& hostPath){
    TRACE_FN("overlayId=", overlayId, ", file=", hostPath);
    auto root = vfs.overlayRoot(overlayId);
    if(!root) throw std::runtime_error("overlay.save: overlay missing root");

    std::filesystem::path outPath(hostPath);
    if(auto parent = outPath.parent_path(); !parent.empty()){
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if(ec){
            throw std::runtime_error(std::string("overlay.save: failed to create directories: ") + ec.message());
        }
    }

    // Create timestamped backup before overwriting
    try{
        create_timestamped_backup(hostPath);
    } catch(const std::exception& e){
        std::cout << "note: backup creation failed: " << e.what() << "\n";
    }

    std::ofstream out(hostPath, std::ios::binary | std::ios::trunc);
    if(!out) throw std::runtime_error("overlay.save: cannot open file for writing");

    // Write version 3 header with optional hash
    out << "# codex-vfs-overlay 3\n";

    // Write source file hash if available
    if(overlayId < vfs.overlay_stack.size()){
        const auto& overlay = vfs.overlay_stack[overlayId];
        if(!overlay.source_file.empty() && !overlay.source_hash.empty()){
            out << "H " << overlay.source_file << " " << overlay.source_hash << "\n";
        }
    }

    std::function<void(const std::shared_ptr<VfsNode>&, const std::string&)> dump;
    dump = [&](const std::shared_ptr<VfsNode>& node, const std::string& path){
        if(!node) return;
        bool traverse = node->isDir();
        if(node->kind == VfsNode::Kind::Dir){
            if(path != "/"){
                out << "D " << path << "\n";
            }
        } else if(node->kind == VfsNode::Kind::File){
            auto data = node->read();
            out << "F " << path << " " << data.size() << "\n";
            if(!data.empty()){
                out.write(data.data(), static_cast<std::streamsize>(data.size()));
            }
            out << "\n";
            return;
        } else if(node->kind == VfsNode::Kind::Ast){
            auto ast = std::dynamic_pointer_cast<AstNode>(node);
            if(!ast) throw std::runtime_error("overlay.save: ast node cast failed at " + path);
            auto payload = serialize_ast_node(ast);
            out << "A " << path << " " << payload.first << " " << payload.second.size() << "\n";
            if(!payload.second.empty()){
                out.write(payload.second.data(), static_cast<std::streamsize>(payload.second.size()));
            }
            out << "\n";
        } else {
            throw std::runtime_error("overlay.save: unsupported node type at " + path);
        }

        if(traverse){
            const auto& children = node->children();
            for(const auto& [name, child] : children){
                std::string childPath = join_path(path, name);
                dump(child, childPath);
            }
        }
    };

    dump(root, "/");
    vfs.setOverlaySource(overlayId, hostPath);
    vfs.clearOverlayDirty(overlayId);
}

static bool is_solution_file(const std::filesystem::path& p){
    auto ext = p.extension().string();
    std::string lowered = ext;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return lowered == kPackageExtension || lowered == kAssemblyExtension;
}

static std::optional<std::filesystem::path> auto_detect_vfs_path(){
    namespace fs = std::filesystem;
    fs::path cwd;
    try{
        cwd = fs::current_path();
    } catch(const std::exception&){
        return std::nullopt;
    }
    if(cwd.empty()) return std::nullopt;
    auto title = cwd.filename().string();
    if(title.empty()) return std::nullopt;
    fs::path vfs_file = cwd / (title + ".vfs");
    if(fs::exists(vfs_file) && fs::is_regular_file(vfs_file)) return vfs_file;
    return std::nullopt;
}

static std::optional<std::filesystem::path> auto_detect_solution_path(){
    namespace fs = std::filesystem;
    fs::path cwd;
    try{
        cwd = fs::current_path();
    } catch(const std::exception&){
        return std::nullopt;
    }
    if(cwd.empty()) return std::nullopt;
    auto title = cwd.filename().string();
    if(title.empty()) return std::nullopt;
    fs::path pkg = cwd / (title + kPackageExtension);
    if(fs::exists(pkg) && fs::is_regular_file(pkg)) return pkg;
    fs::path asm_path = cwd / (title + kAssemblyExtension);
    if(fs::exists(asm_path) && fs::is_regular_file(asm_path)) return asm_path;
    return std::nullopt;
}

static std::string make_unique_overlay_name(Vfs& vfs, std::string base){
    if(base.empty()) base = "solution";
    std::string candidate = base;
    int counter = 2;
    while(vfs.findOverlayByName(candidate)){
        candidate = base + "_" + std::to_string(counter++);
    }
    return candidate;
}

static bool solution_save(Vfs& vfs, SolutionContext& sol, bool quiet){
    if(!sol.active){
        if(!quiet) std::cout << "(no solution loaded)\n";
        return false;
    }
    if(sol.file_path.empty()){
        if(!quiet) std::cout << "solution '" << sol.title << "' has no destination file\n";
        return false;
    }
    try{
        save_overlay_to_file(vfs, sol.overlay_id, sol.file_path);
        if(!quiet){
            std::cout << "saved solution '" << sol.title << "' -> " << sol.file_path << "\n";
        }
        return true;
    } catch(const std::exception& e){
        if(!quiet) std::cout << "error: solution save failed: " << e.what() << "\n";
        return false;
    }
}

static void attach_solution_shortcut(Vfs& vfs, SolutionContext& sol){
    g_on_save_shortcut = [&vfs, &sol](){
        solution_save(vfs, sol, false);
    };
}

static bool load_solution_from_file(Vfs& vfs, WorkingDirectory& cwd, SolutionContext& sol, const std::filesystem::path& file, bool auto_detected){
    if(file.empty()) return false;
    if(!std::filesystem::exists(file)){
        std::cout << "note: solution file '" << file.string() << "' not found\n";
        return false;
    }
    if(!std::filesystem::is_regular_file(file)){
        std::cout << "note: solution path '" << file.string() << "' is not a regular file\n";
        return false;
    }
    std::string overlayName = make_unique_overlay_name(vfs, file.stem().string());
    size_t id = mount_overlay_from_file(vfs, overlayName, file.string());
    maybe_extend_context(vfs, cwd);
    if(std::find(cwd.overlays.begin(), cwd.overlays.end(), id) == cwd.overlays.end()){
        cwd.overlays.push_back(id);
        sort_unique(cwd.overlays);
    }
    cwd.primary_overlay = id;
    sol.active = true;
    sol.auto_detected = auto_detected;
    sol.overlay_id = id;
    sol.title = file.stem().string();
    sol.file_path = file.string();
    attach_solution_shortcut(vfs, sol);
    std::cout << "loaded solution '" << sol.title << "' (#" << id << ") from " << sol.file_path << "\n";
    return true;
}

static size_t mount_overlay_from_file(Vfs& vfs, const std::string& name, const std::string& hostPath);
static std::string unescape_meta(const std::string& s){
    std::string out; out.reserve(s.size());
    for(size_t i=0;i<s.size();++i){
        char c = s[i];
        if(c=='\\' && i+1<s.size()){
            char n = s[++i];
            switch(n){
                case 'n': out.push_back('\n'); break;
                case 't': out.push_back('\t'); break;
                case 'r': out.push_back('\r'); break;
                case '\\': out.push_back('\\'); break;
                case '"': out.push_back('"'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'v': out.push_back('\v'); break;
                case 'a': out.push_back('\a'); break;
                default: out.push_back(n); break;
            }
        } else {
            out.push_back(c);
        }
    }
    return out;
}

static std::string sanitize_component(const std::string& s){
    std::string out;
    out.reserve(s.size());
    for(char c : s){
        unsigned char uc = static_cast<unsigned char>(c);
        if(std::isalnum(uc) || c=='-' || c=='_'){
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('_');
        }
    }
    if(out.empty()) out.push_back('_');
    return out;
}

namespace {
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
} // namespace

static std::pair<std::string, std::string> serialize_s_ast_node(const std::shared_ptr<AstNode>& node);
static std::shared_ptr<AstNode> deserialize_s_ast_node(const std::string& type, const std::string& payload);
static bool is_s_ast_type(const std::string& type);
static void serialize_cpp_expr(BinaryWriter& w, const std::shared_ptr<CppExpr>& expr);
static std::shared_ptr<CppExpr> deserialize_cpp_expr(BinaryReader& r);
static void deserialize_cpp_compound_into(const std::string& payload,
                                          const std::string& node_path,
                                          const std::shared_ptr<CppCompound>& compound,
                                          std::vector<std::function<void()>>& fixups,
                                          std::unordered_map<std::string, std::shared_ptr<VfsNode>>& path_map);
static std::string serialize_cpp_compound_payload(const std::shared_ptr<CppCompound>& compound);

static uint64_t fnv1a64(const std::string& data){
    const uint64_t offset = 1469598103934665603ull;
    const uint64_t prime  = 1099511628211ull;
    uint64_t h = offset;
    for(unsigned char c : data){
        h ^= c;
        h *= prime;
    }
    return h;
}

static std::string hash_hex(uint64_t value){
    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << value;
    return oss.str();
}

// ====== S-expression AST serialization ======
static std::pair<std::string, std::string> serialize_s_ast_node(const std::shared_ptr<AstNode>& node){
    if(!node) throw std::runtime_error("cannot serialize null AST node");

    if(auto n = std::dynamic_pointer_cast<AstInt>(node)){
        BinaryWriter w;
        w.i64(n->val);
        return {"AstInt", std::move(w.data)};
    }
    if(auto n = std::dynamic_pointer_cast<AstBool>(node)){
        BinaryWriter w;
        w.u8(n->val ? 1 : 0);
        return {"AstBool", std::move(w.data)};
    }
    if(auto n = std::dynamic_pointer_cast<AstStr>(node)){
        BinaryWriter w;
        w.str(n->val);
        return {"AstStr", std::move(w.data)};
    }
    if(auto n = std::dynamic_pointer_cast<AstSym>(node)){
        BinaryWriter w;
        w.str(n->id);
        return {"AstSym", std::move(w.data)};
    }
    if(auto n = std::dynamic_pointer_cast<AstIf>(node)){
        BinaryWriter w;
        auto c = serialize_s_ast_node(n->c);
        auto a = serialize_s_ast_node(n->a);
        auto b = serialize_s_ast_node(n->b);
        w.str(c.first); w.str(c.second);
        w.str(a.first); w.str(a.second);
        w.str(b.first); w.str(b.second);
        return {"AstIf", std::move(w.data)};
    }
    if(auto n = std::dynamic_pointer_cast<AstLambda>(node)){
        BinaryWriter w;
        if(n->params.size() > std::numeric_limits<uint32_t>::max())
            throw std::runtime_error("lambda parameter list too large to serialize");
        w.u32(static_cast<uint32_t>(n->params.size()));
        for(const auto& p : n->params) w.str(p);
        auto body = serialize_s_ast_node(n->body);
        w.str(body.first);
        w.str(body.second);
        return {"AstLambda", std::move(w.data)};
    }
    if(auto n = std::dynamic_pointer_cast<AstCall>(node)){
        BinaryWriter w;
        auto fn = serialize_s_ast_node(n->fn);
        w.str(fn.first);
        w.str(fn.second);
        if(n->args.size() > std::numeric_limits<uint32_t>::max())
            throw std::runtime_error("call argument list too large to serialize");
        w.u32(static_cast<uint32_t>(n->args.size()));
        for(const auto& arg : n->args){
            auto ap = serialize_s_ast_node(arg);
            w.str(ap.first);
            w.str(ap.second);
        }
        return {"AstCall", std::move(w.data)};
    }

    throw std::runtime_error("serialize_s_ast_node: unsupported node type");
}

static std::shared_ptr<AstNode> deserialize_s_ast_node(const std::string& type, const std::string& payload){
    BinaryReader r(payload);
    std::shared_ptr<AstNode> node;
    if(type == "AstInt"){
        node = std::make_shared<AstInt>("<i>", r.i64());
    } else if(type == "AstBool"){
        node = std::make_shared<AstBool>("<b>", r.u8() != 0);
    } else if(type == "AstStr"){
        node = std::make_shared<AstStr>("<s>", r.str());
    } else if(type == "AstSym"){
        node = std::make_shared<AstSym>("<sym>", r.str());
    } else if(type == "AstIf"){
        auto cType = r.str(); auto cData = r.str();
        auto aType = r.str(); auto aData = r.str();
        auto bType = r.str(); auto bData = r.str();
        auto c = deserialize_s_ast_node(cType, cData);
        auto a = deserialize_s_ast_node(aType, aData);
        auto b = deserialize_s_ast_node(bType, bData);
        node = std::make_shared<AstIf>("<if>", c, a, b);
    } else if(type == "AstLambda"){
        uint32_t count = r.u32();
        std::vector<std::string> params;
        params.reserve(count);
        for(uint32_t i = 0; i < count; ++i) params.push_back(r.str());
        auto bodyType = r.str();
        auto bodyData = r.str();
        auto body = deserialize_s_ast_node(bodyType, bodyData);
        node = std::make_shared<AstLambda>("<lam>", params, body);
    } else if(type == "AstCall"){
        auto fnType = r.str();
        auto fnData = r.str();
        auto fn = deserialize_s_ast_node(fnType, fnData);
        uint32_t argc = r.u32();
        std::vector<std::shared_ptr<AstNode>> args;
        args.reserve(argc);
        for(uint32_t i = 0; i < argc; ++i){
            auto aType = r.str();
            auto aData = r.str();
            args.push_back(deserialize_s_ast_node(aType, aData));
        }
        node = std::make_shared<AstCall>("<call>", fn, args);
    } else {
        throw std::runtime_error("deserialize_s_ast_node: unsupported node type '" + type + "'");
    }
    r.expect_eof();
    return node;
}

static bool is_s_ast_type(const std::string& type){
    return type == "AstInt" || type == "AstBool" || type == "AstStr" ||
           type == "AstSym" || type == "AstIf" || type == "AstLambda" ||
           type == "AstCall";
}

static bool is_s_ast_instance(const std::shared_ptr<AstNode>& node){
    return std::dynamic_pointer_cast<AstInt>(node) ||
           std::dynamic_pointer_cast<AstBool>(node) ||
           std::dynamic_pointer_cast<AstStr>(node) ||
           std::dynamic_pointer_cast<AstSym>(node) ||
           std::dynamic_pointer_cast<AstIf>(node) ||
           std::dynamic_pointer_cast<AstLambda>(node) ||
           std::dynamic_pointer_cast<AstCall>(node);
}

// ====== C++ AST serialization ======
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

static void serialize_cpp_expr(BinaryWriter& w, const std::shared_ptr<CppExpr>& expr){
    if(!expr) throw std::runtime_error("serialize_cpp_expr: null expression");

    if(auto id = std::dynamic_pointer_cast<CppId>(expr)){
        w.u8(static_cast<uint8_t>(CppExprTag::Id));
        w.str(id->id);
        return;
    }
    if(auto s = std::dynamic_pointer_cast<CppString>(expr)){
        w.u8(static_cast<uint8_t>(CppExprTag::String));
        w.str(s->s);
        return;
    }
    if(auto i = std::dynamic_pointer_cast<CppInt>(expr)){
        w.u8(static_cast<uint8_t>(CppExprTag::Int));
        w.i64(i->v);
        return;
    }
    if(auto call = std::dynamic_pointer_cast<CppCall>(expr)){
        w.u8(static_cast<uint8_t>(CppExprTag::Call));
        serialize_cpp_expr(w, call->fn);
        if(call->args.size() > std::numeric_limits<uint32_t>::max())
            throw std::runtime_error("serialize_cpp_expr: argument list too large");
        w.u32(static_cast<uint32_t>(call->args.size()));
        for(const auto& a : call->args) serialize_cpp_expr(w, a);
        return;
    }
    if(auto bin = std::dynamic_pointer_cast<CppBinOp>(expr)){
        w.u8(static_cast<uint8_t>(CppExprTag::BinOp));
        w.str(bin->op);
        serialize_cpp_expr(w, bin->a);
        serialize_cpp_expr(w, bin->b);
        return;
    }
    if(auto stream = std::dynamic_pointer_cast<CppStreamOut>(expr)){
        w.u8(static_cast<uint8_t>(CppExprTag::StreamOut));
        if(stream->chain.size() > std::numeric_limits<uint32_t>::max())
            throw std::runtime_error("serialize_cpp_expr: stream chain too large");
        w.u32(static_cast<uint32_t>(stream->chain.size()));
        for(const auto& part : stream->chain) serialize_cpp_expr(w, part);
        return;
    }
    if(auto raw = std::dynamic_pointer_cast<CppRawExpr>(expr)){
        w.u8(static_cast<uint8_t>(CppExprTag::Raw));
        w.str(raw->text);
        return;
    }

    throw std::runtime_error("serialize_cpp_expr: unsupported expression type");
}

static std::shared_ptr<CppExpr> deserialize_cpp_expr(BinaryReader& r){
    if(r.eof()) throw std::runtime_error("deserialize_cpp_expr: unexpected EOF");
    auto tag = static_cast<CppExprTag>(r.u8());
    switch(tag){
        case CppExprTag::Id:
            return std::make_shared<CppId>("id", r.str());
        case CppExprTag::String:
            return std::make_shared<CppString>("s", r.str());
        case CppExprTag::Int:
            return std::make_shared<CppInt>("i", r.i64());
        case CppExprTag::Call: {
            auto fn = deserialize_cpp_expr(r);
            uint32_t argc = r.u32();
            std::vector<std::shared_ptr<CppExpr>> args;
            args.reserve(argc);
            for(uint32_t i = 0; i < argc; ++i){
                args.push_back(deserialize_cpp_expr(r));
            }
            return std::make_shared<CppCall>("call", fn, args);
        }
        case CppExprTag::BinOp: {
            auto op = r.str();
            auto a = deserialize_cpp_expr(r);
            auto b = deserialize_cpp_expr(r);
            return std::make_shared<CppBinOp>("binop", op, a, b);
        }
        case CppExprTag::StreamOut: {
            uint32_t count = r.u32();
            std::vector<std::shared_ptr<CppExpr>> chain;
            chain.reserve(count);
            for(uint32_t i = 0; i < count; ++i) chain.push_back(deserialize_cpp_expr(r));
            return std::make_shared<CppStreamOut>("cout", chain);
        }
        case CppExprTag::Raw:
            return std::make_shared<CppRawExpr>("rexpr", r.str());
    }
    throw std::runtime_error("deserialize_cpp_expr: unknown tag");
}

static std::string serialize_cpp_compound_payload(const std::shared_ptr<CppCompound>& compound){
    if(!compound) throw std::runtime_error("serialize_cpp_compound_payload: null compound");
    BinaryWriter w;
    if(compound->stmts.size() > std::numeric_limits<uint32_t>::max())
        throw std::runtime_error("serialize_cpp_compound_payload: too many statements");
    w.u32(static_cast<uint32_t>(compound->stmts.size()));
    for(const auto& stmt : compound->stmts){
        if(!stmt) throw std::runtime_error("serialize_cpp_compound_payload: null statement");
        if(auto es = std::dynamic_pointer_cast<CppExprStmt>(stmt)){
            w.u8(static_cast<uint8_t>(CppStmtTag::ExprStmt));
            serialize_cpp_expr(w, es->e);
        } else if(auto ret = std::dynamic_pointer_cast<CppReturn>(stmt)){
            w.u8(static_cast<uint8_t>(CppStmtTag::Return));
            w.u8(ret->e ? 1 : 0);
            if(ret->e) serialize_cpp_expr(w, ret->e);
        } else if(auto raw = std::dynamic_pointer_cast<CppRawStmt>(stmt)){
            w.u8(static_cast<uint8_t>(CppStmtTag::Raw));
            w.str(raw->text);
        } else if(auto var = std::dynamic_pointer_cast<CppVarDecl>(stmt)){
            w.u8(static_cast<uint8_t>(CppStmtTag::VarDecl));
            w.str(var->type);
            w.str(var->name);
            w.u8(var->hasInit ? 1 : 0);
            if(var->hasInit) w.str(var->init);
        } else if(auto loop = std::dynamic_pointer_cast<CppRangeFor>(stmt)){
            w.u8(static_cast<uint8_t>(CppStmtTag::RangeForRef));
            w.str(loop->name);
        } else {
            throw std::runtime_error("serialize_cpp_compound_payload: unsupported statement type");
        }
    }
    return w.data;
}

static void deserialize_cpp_compound_into(const std::string& payload,
                                          const std::string& node_path,
                                          const std::shared_ptr<CppCompound>& compound,
                                          std::vector<std::function<void()>>& fixups,
                                          std::unordered_map<std::string, std::shared_ptr<VfsNode>>& path_map){
    if(!compound) throw std::runtime_error("deserialize_cpp_compound_into: null compound");
    BinaryReader r(payload);
    uint32_t count = r.u32();
    std::vector<std::shared_ptr<CppStmt>> parsed;
    parsed.reserve(count);
    std::vector<std::pair<size_t, std::string>> pending_rangefor;

    for(uint32_t idx = 0; idx < count; ++idx){
        auto tag = static_cast<CppStmtTag>(r.u8());
        switch(tag){
            case CppStmtTag::ExprStmt: {
                auto expr = deserialize_cpp_expr(r);
                parsed.push_back(std::make_shared<CppExprStmt>("expr", expr));
                break;
            }
            case CppStmtTag::Return: {
                bool hasExpr = r.u8() != 0;
                std::shared_ptr<CppExpr> expr;
                if(hasExpr) expr = deserialize_cpp_expr(r);
                parsed.push_back(std::make_shared<CppReturn>("ret", expr));
                break;
            }
            case CppStmtTag::Raw: {
                parsed.push_back(std::make_shared<CppRawStmt>("stmt", r.str()));
                break;
            }
            case CppStmtTag::VarDecl: {
                auto type = r.str();
                auto name = r.str();
                bool hasInit = r.u8() != 0;
                std::string init;
                if(hasInit) init = r.str();
                parsed.push_back(std::make_shared<CppVarDecl>("var", type, name, init, hasInit));
                break;
            }
            case CppStmtTag::RangeForRef: {
                auto childName = r.str();
                pending_rangefor.emplace_back(parsed.size(), childName);
                parsed.push_back(nullptr);
                break;
            }
            default:
                throw std::runtime_error("deserialize_cpp_compound_into: unknown statement tag");
        }
    }
    r.expect_eof();
    compound->stmts = std::move(parsed);

    if(!pending_rangefor.empty()){
        auto compoundWeak = std::weak_ptr<CppCompound>(compound);
        fixups.push_back([compoundWeak, node_path, pending_rangefor, &path_map](){
            auto locked = compoundWeak.lock();
            if(!locked) return;
            for(const auto& pair : pending_rangefor){
                const auto& child = pair.second;
                auto full = join_path(node_path, child);
                auto it = path_map.find(full);
                if(it == path_map.end())
                    throw std::runtime_error("compound fixup missing child node: " + full);
                auto loop = std::dynamic_pointer_cast<CppRangeFor>(it->second);
                if(!loop)
                    throw std::runtime_error("compound fixup expected CppRangeFor at: " + full);
                locked->stmts[pair.first] = loop;
            }
        });
    }
}

static std::pair<std::string, std::string> serialize_ast_node(const std::shared_ptr<AstNode>& node){
    if(!node) throw std::runtime_error("serialize_ast_node: null node");

    if(auto holder = std::dynamic_pointer_cast<AstHolder>(node)){
        if(!holder->inner) throw std::runtime_error("AstHolder missing inner node");
        BinaryWriter w;
        auto inner = serialize_s_ast_node(holder->inner);
        w.str(inner.first);
        w.str(inner.second);
        return {"AstHolder", std::move(w.data)};
    }

    if(is_s_ast_instance(node)){
        return serialize_s_ast_node(node);
    }

    if(auto tu = std::dynamic_pointer_cast<CppTranslationUnit>(node)){
        BinaryWriter w;
        if(tu->includes.size() > std::numeric_limits<uint32_t>::max())
            throw std::runtime_error("serialize_ast_node: too many includes");
        w.u32(static_cast<uint32_t>(tu->includes.size()));
        for(const auto& inc : tu->includes){
            if(!inc) throw std::runtime_error("serialize_ast_node: null include");
            w.str(inc->header);
            w.u8(inc->angled ? 1 : 0);
        }
        if(tu->funcs.size() > std::numeric_limits<uint32_t>::max())
            throw std::runtime_error("serialize_ast_node: too many functions");
        w.u32(static_cast<uint32_t>(tu->funcs.size()));
        for(const auto& fn : tu->funcs){
            if(!fn) throw std::runtime_error("serialize_ast_node: null function pointer");
            w.str(fn->name);
        }
        return {"CppTranslationUnit", std::move(w.data)};
    }

    if(auto fn = std::dynamic_pointer_cast<CppFunction>(node)){
        BinaryWriter w;
        w.str(fn->retType);
        w.str(fn->name);
        if(fn->params.size() > std::numeric_limits<uint32_t>::max())
            throw std::runtime_error("serialize_ast_node: function parameter list too large");
        w.u32(static_cast<uint32_t>(fn->params.size()));
        for(const auto& p : fn->params){
            w.str(p.type);
            w.str(p.name);
        }
        std::string bodyName = (fn->body ? fn->body->name : std::string("body"));
        w.str(bodyName);
        return {"CppFunction", std::move(w.data)};
    }

    if(auto compound = std::dynamic_pointer_cast<CppCompound>(node)){
        auto payload = serialize_cpp_compound_payload(compound);
        return {"CppCompound", std::move(payload)};
    }

    if(auto loop = std::dynamic_pointer_cast<CppRangeFor>(node)){
        BinaryWriter w;
        w.str(loop->decl);
        w.str(loop->range);
        std::string bodyName = (loop->body ? loop->body->name : std::string("body"));
        w.str(bodyName);
        return {"CppRangeFor", std::move(w.data)};
    }

    // PlanNode serialization
    if(auto jobs = std::dynamic_pointer_cast<PlanJobs>(node)){
        BinaryWriter w;
        w.u32(static_cast<uint32_t>(jobs->jobs.size()));
        for(const auto& job : jobs->jobs){
            w.str(job.description);
            w.u32(static_cast<uint32_t>(job.priority));
            w.u8(job.completed ? 1 : 0);
            w.str(job.assignee);
        }
        return {"PlanJobs", std::move(w.data)};
    }

    if(auto goals = std::dynamic_pointer_cast<PlanGoals>(node)){
        BinaryWriter w;
        w.u32(static_cast<uint32_t>(goals->goals.size()));
        for(const auto& g : goals->goals) w.str(g);
        return {"PlanGoals", std::move(w.data)};
    }

    if(auto ideas = std::dynamic_pointer_cast<PlanIdeas>(node)){
        BinaryWriter w;
        w.u32(static_cast<uint32_t>(ideas->ideas.size()));
        for(const auto& i : ideas->ideas) w.str(i);
        return {"PlanIdeas", std::move(w.data)};
    }

    if(auto deps = std::dynamic_pointer_cast<PlanDeps>(node)){
        BinaryWriter w;
        w.u32(static_cast<uint32_t>(deps->dependencies.size()));
        for(const auto& d : deps->dependencies) w.str(d);
        return {"PlanDeps", std::move(w.data)};
    }

    if(auto impl = std::dynamic_pointer_cast<PlanImplemented>(node)){
        BinaryWriter w;
        w.u32(static_cast<uint32_t>(impl->items.size()));
        for(const auto& item : impl->items) w.str(item);
        return {"PlanImplemented", std::move(w.data)};
    }

    if(auto research = std::dynamic_pointer_cast<PlanResearch>(node)){
        BinaryWriter w;
        w.u32(static_cast<uint32_t>(research->topics.size()));
        for(const auto& t : research->topics) w.str(t);
        return {"PlanResearch", std::move(w.data)};
    }

    if(std::dynamic_pointer_cast<PlanRoot>(node)){
        BinaryWriter w;
        w.str(node->read());
        return {"PlanRoot", std::move(w.data)};
    }

    if(std::dynamic_pointer_cast<PlanSubPlan>(node)){
        BinaryWriter w;
        w.str(node->read());
        return {"PlanSubPlan", std::move(w.data)};
    }

    if(std::dynamic_pointer_cast<PlanStrategy>(node)){
        BinaryWriter w;
        w.str(node->read());
        return {"PlanStrategy", std::move(w.data)};
    }

    if(std::dynamic_pointer_cast<PlanNotes>(node)){
        BinaryWriter w;
        w.str(node->read());
        return {"PlanNotes", std::move(w.data)};
    }

    throw std::runtime_error("serialize_ast_node: unsupported node type");
}

static std::shared_ptr<AstNode> deserialize_ast_node(const std::string& type,
                                                     const std::string& payload,
                                                     const std::string& path,
                                                     std::vector<std::function<void()>>& fixups,
                                                     std::unordered_map<std::string, std::shared_ptr<VfsNode>>& path_map){
    auto basename = path_basename(path);

    if(type == "AstHolder"){
        BinaryReader r(payload);
        auto innerType = r.str();
        auto innerPayload = r.str();
        r.expect_eof();
        auto inner = deserialize_s_ast_node(innerType, innerPayload);
        auto holder = std::make_shared<AstHolder>(basename, inner);
        return holder;
    }

    if(is_s_ast_type(type)){
        auto node = deserialize_s_ast_node(type, payload);
        node->name = basename;
        return node;
    }

    if(type == "CppTranslationUnit"){
        BinaryReader r(payload);
        uint32_t includeCount = r.u32();
        auto tu = std::make_shared<CppTranslationUnit>(basename);
        tu->includes.clear();
        for(uint32_t i = 0; i < includeCount; ++i){
            auto header = r.str();
            bool angled = r.u8() != 0;
            tu->includes.push_back(std::make_shared<CppInclude>("include", header, angled));
        }
        uint32_t funcCount = r.u32();
        std::vector<std::string> funcNames;
        funcNames.reserve(funcCount);
        for(uint32_t i = 0; i < funcCount; ++i){
            funcNames.push_back(r.str());
        }
        r.expect_eof();
        auto weakTu = std::weak_ptr<CppTranslationUnit>(tu);
        fixups.push_back([weakTu, path, funcNames, &path_map](){
            auto locked = weakTu.lock();
            if(!locked) return;
            locked->funcs.clear();
            for(const auto& name : funcNames){
                auto full = join_path(path, name);
                auto it = path_map.find(full);
                if(it == path_map.end())
                    throw std::runtime_error("translation unit fixup missing function node: " + full);
                auto fn = std::dynamic_pointer_cast<CppFunction>(it->second);
                if(!fn)
                    throw std::runtime_error("translation unit fixup expected CppFunction at: " + full);
                locked->funcs.push_back(fn);
            }
        });
        return tu;
    }

    if(type == "CppFunction"){
        BinaryReader r(payload);
        auto retType = r.str();
        auto fnName = r.str();
        uint32_t paramCount = r.u32();
        std::vector<CppParam> params;
        params.reserve(paramCount);
        for(uint32_t i = 0; i < paramCount; ++i){
            CppParam p;
            p.type = r.str();
            p.name = r.str();
            params.push_back(std::move(p));
        }
        auto bodyName = r.str();
        r.expect_eof();
        auto fn = std::make_shared<CppFunction>(basename, retType, fnName);
        fn->retType = retType;
        fn->name = fnName;
        fn->params = std::move(params);
        fn->body.reset();
        auto weakFn = std::weak_ptr<CppFunction>(fn);
        fixups.push_back([weakFn, path, bodyName, &path_map](){
            auto locked = weakFn.lock();
            if(!locked) return;
            auto bodyPath = join_path(path, bodyName);
            auto it = path_map.find(bodyPath);
            if(it == path_map.end())
                throw std::runtime_error("function fixup missing body node: " + bodyPath);
            auto body = std::dynamic_pointer_cast<CppCompound>(it->second);
            if(!body)
                throw std::runtime_error("function fixup expected CppCompound at: " + bodyPath);
            locked->body = body;
        });
        return fn;
    }

    if(type == "CppCompound"){
        auto compound = std::make_shared<CppCompound>(basename);
        deserialize_cpp_compound_into(payload, path, compound, fixups, path_map);
        return compound;
    }

    if(type == "CppRangeFor"){
        BinaryReader r(payload);
        auto decl = r.str();
        auto range = r.str();
        auto bodyName = r.str();
        r.expect_eof();
        auto loop = std::make_shared<CppRangeFor>(basename, decl, range);
        loop->body.reset();
        auto weakLoop = std::weak_ptr<CppRangeFor>(loop);
        fixups.push_back([weakLoop, path, bodyName, &path_map](){
            auto locked = weakLoop.lock();
            if(!locked) return;
            auto bodyPath = join_path(path, bodyName);
            auto it = path_map.find(bodyPath);
            if(it == path_map.end())
                throw std::runtime_error("rangefor fixup missing body node: " + bodyPath);
            auto body = std::dynamic_pointer_cast<CppCompound>(it->second);
            if(!body)
                throw std::runtime_error("rangefor fixup expected CppCompound at: " + bodyPath);
            locked->body = body;
        });
        return loop;
    }

    // PlanNode deserialization
    if(type == "PlanJobs"){
        BinaryReader r(payload);
        uint32_t count = r.u32();
        auto jobs = std::make_shared<PlanJobs>(basename);
        for(uint32_t i = 0; i < count; ++i){
            PlanJob job;
            job.description = r.str();
            job.priority = static_cast<int>(r.u32());
            job.completed = r.u8() != 0;
            job.assignee = r.str();
            jobs->jobs.push_back(std::move(job));
        }
        r.expect_eof();
        return jobs;
    }

    if(type == "PlanGoals"){
        BinaryReader r(payload);
        uint32_t count = r.u32();
        auto goals = std::make_shared<PlanGoals>(basename);
        for(uint32_t i = 0; i < count; ++i){
            goals->goals.push_back(r.str());
        }
        r.expect_eof();
        return goals;
    }

    if(type == "PlanIdeas"){
        BinaryReader r(payload);
        uint32_t count = r.u32();
        auto ideas = std::make_shared<PlanIdeas>(basename);
        for(uint32_t i = 0; i < count; ++i){
            ideas->ideas.push_back(r.str());
        }
        r.expect_eof();
        return ideas;
    }

    if(type == "PlanDeps"){
        BinaryReader r(payload);
        uint32_t count = r.u32();
        auto deps = std::make_shared<PlanDeps>(basename);
        for(uint32_t i = 0; i < count; ++i){
            deps->dependencies.push_back(r.str());
        }
        r.expect_eof();
        return deps;
    }

    if(type == "PlanImplemented"){
        BinaryReader r(payload);
        uint32_t count = r.u32();
        auto impl = std::make_shared<PlanImplemented>(basename);
        for(uint32_t i = 0; i < count; ++i){
            impl->items.push_back(r.str());
        }
        r.expect_eof();
        return impl;
    }

    if(type == "PlanResearch"){
        BinaryReader r(payload);
        uint32_t count = r.u32();
        auto research = std::make_shared<PlanResearch>(basename);
        for(uint32_t i = 0; i < count; ++i){
            research->topics.push_back(r.str());
        }
        r.expect_eof();
        return research;
    }

    if(type == "PlanRoot"){
        BinaryReader r(payload);
        auto content = r.str();
        r.expect_eof();
        return std::make_shared<PlanRoot>(basename, content);
    }

    if(type == "PlanSubPlan"){
        BinaryReader r(payload);
        auto content = r.str();
        r.expect_eof();
        return std::make_shared<PlanSubPlan>(basename, content);
    }

    if(type == "PlanStrategy"){
        BinaryReader r(payload);
        auto content = r.str();
        r.expect_eof();
        return std::make_shared<PlanStrategy>(basename, content);
    }

    if(type == "PlanNotes"){
        BinaryReader r(payload);
        auto content = r.str();
        r.expect_eof();
        return std::make_shared<PlanNotes>(basename, content);
    }

    throw std::runtime_error("deserialize_ast_node: unsupported node type '" + type + "'");
}

static std::string join_args(const std::vector<std::string>& args, size_t start = 0){
    std::string out;
    for(size_t i = start; i < args.size(); ++i){
        if(i != start) out.push_back(' ');
        out += args[i];
    }
    return out;
}

static std::optional<std::filesystem::path> history_file_path(){
    if(const char* env = std::getenv("CODEX_HISTORY_FILE"); env && *env){
        return std::filesystem::path(env);
    }
    if(const char* home = std::getenv("HOME"); home && *home){
        return std::filesystem::path(home) / ".codex_history";
    }
    return std::nullopt;
}

static void load_history(std::vector<std::string>& history){
    auto path_opt = history_file_path();
    if(!path_opt) return;
    std::ifstream in(*path_opt);
    if(!in) return;
    std::string line;
    while(std::getline(in, line)){
        if(trim_copy(line).empty()) continue;
        history.push_back(line);
    }
}

static void save_history(const std::vector<std::string>& history){
    auto path_opt = history_file_path();
    if(!path_opt) return;
    const auto& path = *path_opt;
    std::error_code ec;
    auto parent = path.parent_path();
    if(!parent.empty()) std::filesystem::create_directories(parent, ec);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if(!out){
        TRACE_MSG("history write failed: ", path.string());
        return;
    }
    for(const auto& entry : history){
        out << entry << '\n';
    }
}

static bool terminal_available(){
    return ::isatty(STDIN_FILENO) == 1 && ::isatty(STDOUT_FILENO) == 1;
}

struct RawTerminalMode {
    termios original{};
    bool active = false;

    RawTerminalMode(){
        if(::isatty(STDIN_FILENO) != 1) return;
        if(tcgetattr(STDIN_FILENO, &original) != 0) return;
        termios raw = original;
        raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
        raw.c_iflag &= static_cast<tcflag_t>(~(IXON | ICRNL));
        raw.c_oflag &= static_cast<tcflag_t>(~OPOST);
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == 0){
            active = true;
        }
    }

    ~RawTerminalMode(){
        if(active){
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &original);
        }
    }

    bool ok() const { return active; }
};

static void redraw_prompt_line(const std::string& prompt, const std::string& buffer, size_t cursor){
    std::cout << '\r' << prompt << buffer << "\x1b[K";
    if(cursor < buffer.size()){
        size_t tail = buffer.size() - cursor;
        std::cout << "\x1b[" << tail << 'D';
    }
    std::cout.flush();
}

static bool read_line_with_history(const std::string& prompt, std::string& out, const std::vector<std::string>& history){
    std::cout << prompt;
    std::cout.flush();

    if(!terminal_available()){
        if(!std::getline(std::cin, out)){
            return false;
        }
        return true;
    }

    RawTerminalMode guard;
    if(!guard.ok()){
        if(!std::getline(std::cin, out)){
            return false;
        }
        return true;
    }

    std::string buffer;
    size_t cursor = 0;
    size_t history_pos = history.size();
    std::string saved_new_entry;
    bool saved_valid = false;

    auto redraw_current = [&](){
        redraw_prompt_line(prompt, buffer, cursor);
    };

    auto trigger_save_shortcut = [&](){
        if(!g_on_save_shortcut) return;
        std::cout << '\r';
        std::cout.flush();
        std::cout << '\n';
        g_on_save_shortcut();
        redraw_current();
    };

    while(true){
        unsigned char ch = 0;
        ssize_t n = ::read(STDIN_FILENO, &ch, 1);
        if(n <= 0){
            std::cout << '\n';
            return false;
        }

        if(ch == '\r' || ch == '\n'){
            std::cout << '\n';
            out = buffer;
            return true;
        }

        if(ch == 3){ // Ctrl-C
            std::cout << "^C\n";
            buffer.clear();
            cursor = 0;
            history_pos = history.size();
            saved_valid = false;
            std::cout << prompt;
            std::cout.flush();
            continue;
        }

        if(ch == 4){ // Ctrl-D
            if(buffer.empty()){
                std::cout << '\n';
                return false;
            }
            if(cursor < buffer.size()){
                auto pos = buffer.begin() + static_cast<std::string::difference_type>(cursor);
                buffer.erase(pos);
                redraw_current();
                if(history_pos != history.size()){
                    history_pos = history.size();
                    saved_valid = false;
                }
            }
            continue;
        }

        if(ch == 127 || ch == 8){ // backspace
            if(cursor > 0){
                auto pos = buffer.begin() + static_cast<std::string::difference_type>(cursor) - 1;
                buffer.erase(pos);
                --cursor;
                redraw_current();
                if(history_pos != history.size()){
                    history_pos = history.size();
                    saved_valid = false;
                }
            }
            continue;
        }

        if(ch == 1){ // Ctrl-A
            if(cursor != 0){
                cursor = 0;
                redraw_current();
            }
            continue;
        }

        if(ch == 5){ // Ctrl-E
            if(cursor != buffer.size()){
                cursor = buffer.size();
                redraw_current();
            }
            continue;
        }

        if(ch == 21){ // Ctrl-U
            if(cursor > 0){
                buffer.erase(0, cursor);
                cursor = 0;
                redraw_current();
                if(history_pos != history.size()){
                    history_pos = history.size();
                    saved_valid = false;
                }
            }
            continue;
        }

        if(ch == 11){ // Ctrl-K
            if(cursor < buffer.size()){
                buffer.erase(cursor);
                redraw_current();
                if(history_pos != history.size()){
                    history_pos = history.size();
                    saved_valid = false;
                }
            }
            continue;
        }

        if(ch == 27){ // escape sequences
            unsigned char seq1 = 0;
            if(::read(STDIN_FILENO, &seq1, 1) <= 0) continue;
            if(seq1 == 'O'){
                unsigned char seq2 = 0;
                if(::read(STDIN_FILENO, &seq2, 1) <= 0) continue;
                if(seq2 == 'R'){ // F3 shortcut
                    trigger_save_shortcut();
                }
                continue;
            }
            if(seq1 != '['){
                continue;
            }
            unsigned char seq2 = 0;
            if(::read(STDIN_FILENO, &seq2, 1) <= 0) continue;

            if(seq2 >= '0' && seq2 <= '9'){
                unsigned char seq3 = 0;
                if(::read(STDIN_FILENO, &seq3, 1) <= 0) continue;
                if(seq2 == '1' && seq3 == '3'){ // expect ~ for F3
                    unsigned char seq4 = 0;
                    if(::read(STDIN_FILENO, &seq4, 1) <= 0) continue;
                    if(seq4 == '~'){
                        trigger_save_shortcut();
                    }
                    continue;
                }
                if(seq2 == '3' && seq3 == '~'){ // delete key
                    if(cursor < buffer.size()){
                        auto pos = buffer.begin() + static_cast<std::string::difference_type>(cursor);
                        buffer.erase(pos);
                        redraw_current();
                        if(history_pos != history.size()){
                            history_pos = history.size();
                            saved_valid = false;
                        }
                    }
                }
                continue;
            }

            if(seq2 == 'A'){ // up
                if(history.empty()){
                    std::cout << '\a' << std::flush;
                    continue;
                }
                if(history_pos == history.size()){
                    if(!saved_valid){
                        saved_new_entry = buffer;
                        saved_valid = true;
                    }
                    history_pos = history.empty() ? 0 : history.size() - 1;
                } else if(history_pos > 0){
                    --history_pos;
                } else {
                    std::cout << '\a' << std::flush;
                    continue;
                }
                buffer = history[history_pos];
                cursor = buffer.size();
                redraw_current();
                continue;
            }

            if(seq2 == 'B'){ // down
                if(history_pos == history.size()){
                    if(saved_valid){
                        buffer = saved_new_entry;
                        cursor = buffer.size();
                        redraw_current();
                        saved_valid = false;
                    } else {
                        std::cout << '\a' << std::flush;
                    }
                    continue;
                }
                ++history_pos;
                if(history_pos == history.size()){
                    buffer = saved_valid ? saved_new_entry : std::string{};
                    cursor = buffer.size();
                    redraw_current();
                saved_valid = false;
                } else {
                    buffer = history[history_pos];
                    cursor = buffer.size();
                    redraw_current();
                }
                continue;
            }

            if(seq2 == 'C'){ // right
                if(cursor < buffer.size()){
                    ++cursor;
                    redraw_current();
                }
                continue;
            }

            if(seq2 == 'D'){ // left
                if(cursor > 0){
                    --cursor;
                    redraw_current();
                }
                continue;
            }

            continue;
        }

        if(ch >= 32 && ch <= 126){
            auto pos = buffer.begin() + static_cast<std::string::difference_type>(cursor);
            buffer.insert(pos, static_cast<char>(ch));
            ++cursor;
            redraw_current();
            if(history_pos != history.size()){
                history_pos = history.size();
                saved_valid = false;
            }
            continue;
        }
    }
}

struct CommandInvocation {
    std::string name;
    std::vector<std::string> args;
};

struct CommandPipeline {
    std::vector<CommandInvocation> commands;
    std::string output_redirect;  // file path for > or >>
    bool redirect_append = false; // true for >>, false for >
};

struct CommandChainEntry {
    std::string logical; // "", "&&", "||"
    CommandPipeline pipeline;
};

struct CommandResult {
    bool success = true;
    bool exit_requested = false;
    std::string output;
};

struct ScopedCoutCapture {
    std::ostringstream buffer;
    std::streambuf* old = nullptr;
    ScopedCoutCapture(){
        old = std::cout.rdbuf(buffer.rdbuf());
    }
    ~ScopedCoutCapture(){
        std::cout.rdbuf(old);
    }
    std::string str() const { return buffer.str(); }
};

static std::vector<std::string> tokenize_command_line(const std::string& line){
    std::vector<std::string> tokens;
    std::string cur;
    bool in_single = false;
    bool in_double = false;
    bool escape = false;
    auto flush = [&]{
        if(!cur.empty()){
            tokens.push_back(cur);
            cur.clear();
        }
    };
    for(size_t i = 0; i < line.size(); ++i){
        char c = line[i];
        if(escape){
            cur.push_back(c);
            escape = false;
            continue;
        }
        if(!in_single && c == '\\'){
            escape = true;
            continue;
        }
        if(c == '"' && !in_single){
            in_double = !in_double;
            continue;
        }
        if(c == '\'' && !in_double){
            in_single = !in_single;
            continue;
        }
        if(!in_single && !in_double){
            if(std::isspace(static_cast<unsigned char>(c))){
                flush();
                continue;
            }
            if(c == '|'){
                flush();
                if(i + 1 < line.size() && line[i+1] == '|'){
                    tokens.emplace_back("||");
                    ++i;
                } else {
                    tokens.emplace_back("|");
                }
                continue;
            }
            if(c == '&' && i + 1 < line.size() && line[i+1] == '&'){
                flush();
                tokens.emplace_back("&&");
                ++i;
                continue;
            }
            if(c == '>'){
                flush();
                if(i + 1 < line.size() && line[i+1] == '>'){
                    tokens.emplace_back(">>");
                    ++i;
                } else {
                    tokens.emplace_back(">");
                }
                continue;
            }
        }
        cur.push_back(c);
    }
    if(escape) throw std::runtime_error("line ended with unfinished escape");
    if(in_single || in_double) throw std::runtime_error("unterminated quote");
    flush();
    return tokens;
}

static std::vector<CommandChainEntry> parse_command_chain(const std::vector<std::string>& tokens){
    std::vector<CommandChainEntry> chain;
    CommandPipeline current_pipe;
    CommandInvocation current_cmd;
    std::string next_logic;

    auto flush_command = [&](){
        if(current_cmd.name.empty()) throw std::runtime_error("expected command before operator");
        current_pipe.commands.push_back(current_cmd);
        current_cmd = CommandInvocation{};
    };

    auto flush_pipeline = [&](){
        if(current_pipe.commands.empty()) throw std::runtime_error("missing command sequence");
        chain.push_back(CommandChainEntry{next_logic, current_pipe});
        current_pipe = CommandPipeline{};
        next_logic.clear();
    };

    for(size_t idx = 0; idx < tokens.size(); ++idx){
        const auto& tok = tokens[idx];
        if(tok == "|"){
            flush_command();
            continue;
        }
        if(tok == "&&" || tok == "||"){
            flush_command();
            flush_pipeline();
            next_logic = tok;
            continue;
        }
        if(tok == ">" || tok == ">>"){
            flush_command();
            if(idx + 1 >= tokens.size()) throw std::runtime_error("missing redirect target after " + tok);
            current_pipe.output_redirect = tokens[idx + 1];
            current_pipe.redirect_append = (tok == ">>");
            ++idx;
            continue;
        }
        if(current_cmd.name.empty()){
            current_cmd.name = tok;
        } else {
            current_cmd.args.push_back(tok);
        }
    }

    if(!current_cmd.name.empty()) flush_command();
    if(!current_pipe.commands.empty()){
        chain.push_back(CommandChainEntry{next_logic, current_pipe});
        next_logic.clear();
    }
    if(!next_logic.empty()) throw std::runtime_error("dangling logical operator");
    return chain;
}

static size_t count_lines(const std::string& s){
    if(s.empty()) return 0;
    size_t n = std::count(s.begin(), s.end(), '\n');
    if(s.back() != '\n') ++n;
    return n;
}

struct LineSplit {
    std::vector<std::string> lines;
    bool trailing_newline = false;
};

static LineSplit split_lines(const std::string& s){
    LineSplit result;
    std::string current;
    bool last_was_newline = false;
    for(char c : s){
        if(c == '\n'){
            result.lines.push_back(current);
            current.clear();
            last_was_newline = true;
        } else {
            current.push_back(c);
            last_was_newline = false;
        }
    }
    if(!current.empty()){
        result.lines.push_back(current);
    }
    result.trailing_newline = last_was_newline;
    return result;
}

static std::string join_line_range(const LineSplit& split, size_t begin, size_t end){
    if(begin >= end || begin >= split.lines.size()) return {};
    end = std::min(end, split.lines.size());
    std::ostringstream oss;
    for(size_t idx = begin; idx < end; ++idx){
        oss << split.lines[idx];
        bool had_newline = (idx < split.lines.size() - 1) || split.trailing_newline;
        if(had_newline) oss << '\n';
    }
    return oss.str();
}

static size_t parse_size_arg(const std::string& s, const char* ctx){
    if(s.empty()) throw std::runtime_error(std::string(ctx) + " must be non-negative integer");
    size_t idx = 0;
    while(idx < s.size()){
        if(!std::isdigit(static_cast<unsigned char>(s[idx])))
            throw std::runtime_error(std::string(ctx) + " must be non-negative integer");
        ++idx;
    }
    try{
        return static_cast<size_t>(std::stoull(s));
    } catch(const std::exception&){
        throw std::runtime_error(std::string(ctx) + " out of range");
    }
}

static long long parse_int_arg(const std::string& s, const char* ctx){
    if(s.empty()) throw std::runtime_error(std::string(ctx) + " must be integer");
    size_t idx = 0;
    if(s[0] == '+' || s[0] == '-'){
        idx = 1;
        if(idx == s.size()) throw std::runtime_error(std::string(ctx) + " must be integer");
    }
    while(idx < s.size()){
        if(!std::isdigit(static_cast<unsigned char>(s[idx])))
            throw std::runtime_error(std::string(ctx) + " must be integer");
        ++idx;
    }
    try{
        return std::stoll(s);
    } catch(const std::exception&){
        throw std::runtime_error(std::string(ctx) + " out of range");
    }
}

static std::mt19937& rng(){
    static std::mt19937 gen(std::random_device{}());
    return gen;
}

static std::filesystem::path ai_cache_root(){
    if(const char* env = std::getenv("CODEX_AI_CACHE_DIR"); env && *env){
        return std::filesystem::path(env);
    }
    return std::filesystem::path("cache") / "ai";
}

static std::filesystem::path ai_cache_base_path(const std::string& providerLabel, const std::string& key_material){
    std::filesystem::path dir = ai_cache_root() / sanitize_component(providerLabel);
    std::string hash = hash_hex(fnv1a64(key_material));
    return dir / hash;
}

static std::filesystem::path ai_cache_output_path(const std::string& providerLabel, const std::string& key_material){
    auto base = ai_cache_base_path(providerLabel, key_material);
    base += "-out.txt";
    return base;
}

static std::filesystem::path ai_cache_input_path(const std::string& providerLabel, const std::string& key_material){
    auto base = ai_cache_base_path(providerLabel, key_material);
    base += "-in.txt";
    return base;
}

static std::filesystem::path ai_cache_legacy_output_path(const std::string& providerLabel, const std::string& key_material){
    std::filesystem::path dir = ai_cache_root() / sanitize_component(providerLabel);
    std::string hash = hash_hex(fnv1a64(key_material));
    return dir / (hash + ".txt");
}

static std::string make_cache_key_material(const std::string& providerSignature, const std::string& prompt){
    return providerSignature + '\x1f' + prompt;
}

static std::optional<std::string> ai_cache_read(const std::string& providerLabel, const std::string& key_material){
    auto out_path = ai_cache_output_path(providerLabel, key_material);
    std::ifstream in(out_path, std::ios::binary);
    if(!in){
        auto legacy = ai_cache_legacy_output_path(providerLabel, key_material);
        in.open(legacy, std::ios::binary);
        if(!in) return std::nullopt;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

static void ai_cache_write(const std::string& providerLabel, const std::string& key_material, const std::string& prompt, const std::string& payload){
    auto out_path = ai_cache_output_path(providerLabel, key_material);
    auto in_path = ai_cache_input_path(providerLabel, key_material);
    std::error_code ec;
    std::filesystem::create_directories(out_path.parent_path(), ec);
    {
        std::ofstream in(in_path, std::ios::binary);
        if(in) in.write(prompt.data(), static_cast<std::streamsize>(prompt.size()));
    }
    std::ofstream out(out_path, std::ios::binary);
    if(!out) return;
    out.write(payload.data(), static_cast<std::streamsize>(payload.size()));
}

// ====== Value::show ======
std::string Value::show() const {
    if (std::holds_alternative<int64_t>(v)) return std::to_string(std::get<int64_t>(v));
    if (std::holds_alternative<bool>(v))    return std::get<bool>(v) ? "#t" : "#f";
    if (std::holds_alternative<std::string>(v))  return std::string("\"")+std::get<std::string>(v)+"\"";
    if (std::holds_alternative<Builtin>(v)) return "<builtin>";
    if (std::holds_alternative<Closure>(v)) return "<closure>";
    const auto& xs = std::get<List>(v);
    std::string s="("; bool first=true; for(const auto& e: xs){ if(!first) s+=' '; first=false; s+=e.show(); } s+=')'; return s;
}

// ====== AST node ctors ======
AstInt::AstInt(std::string n, int64_t v) : AstNode(std::move(n)), val(v) { kind=Kind::Ast; }
AstBool::AstBool(std::string n, bool v)  : AstNode(std::move(n)), val(v) { kind=Kind::Ast; }
AstStr::AstStr(std::string n, std::string v): AstNode(std::move(n)), val(std::move(v)) { kind=Kind::Ast; }
AstSym::AstSym(std::string n, std::string s): AstNode(std::move(n)), id(std::move(s)) { kind=Kind::Ast; }
AstIf::AstIf(std::string n, std::shared_ptr<AstNode> C, std::shared_ptr<AstNode> A, std::shared_ptr<AstNode> B)
    : AstNode(std::move(n)), c(std::move(C)), a(std::move(A)), b(std::move(B)) { kind=Kind::Ast; }
AstLambda::AstLambda(std::string n, std::vector<std::string> ps, std::shared_ptr<AstNode> b)
    : AstNode(std::move(n)), params(std::move(ps)), body(std::move(b)){ kind=Kind::Ast; }
AstCall::AstCall(std::string n, std::shared_ptr<AstNode> f, std::vector<std::shared_ptr<AstNode>> a)
    : AstNode(std::move(n)), fn(std::move(f)), args(std::move(a)){ kind=Kind::Ast; }
AstHolder::AstHolder(std::string n, std::shared_ptr<AstNode> in)
    : AstNode(std::move(n)), inner(std::move(in)){ kind=Kind::Ast; }

// ====== AST eval ======
Value AstInt::eval(std::shared_ptr<Env>) { return Value::I(val); }
Value AstBool::eval(std::shared_ptr<Env>) { return Value::B(val); }
Value AstStr::eval(std::shared_ptr<Env>) { return Value::S(val); }
Value AstSym::eval(std::shared_ptr<Env> e) {
    auto v = e->get(id); if(!v) throw std::runtime_error("unbound "+id); return *v;
}
Value AstIf::eval(std::shared_ptr<Env> e){
    auto cv=c->eval(e);
    bool t = std::visit([](auto&& x)->bool{
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<T,bool>)        return x;
        if constexpr (std::is_same_v<T,int64_t>)     return x!=0;
        if constexpr (std::is_same_v<T,std::string>) return !x.empty();
        if constexpr (std::is_same_v<T,Value::List>) return !x.empty();
        else return true;
    }, cv.v);
    return (t? a: b)->eval(e);
}
Value AstLambda::eval(std::shared_ptr<Env> e){
    return Value::Clo(Value::Closure{params, body, e});
}
Value AstCall::eval(std::shared_ptr<Env> e){
    Value f = fn->eval(e);
    std::vector<Value> av; av.reserve(args.size());
    for (auto& a: args) av.push_back(a->eval(e));
    if (std::holds_alternative<Value::Builtin>(f.v)) {
        auto& b = std::get<Value::Builtin>(f.v);
        return b(av, e);
    } else if (std::holds_alternative<Value::Closure>(f.v)) {
        auto clo = std::get<Value::Closure>(f.v);
        if (clo.params.size()!=av.size()) throw std::runtime_error("arity mismatch");
        auto child = std::make_shared<Env>(clo.env);
        for (size_t i=0;i<av.size();++i) child->set(clo.params[i], av[i]);
        return clo.body->eval(child);
    }
    throw std::runtime_error("call of non-function");
}
Value AstHolder::eval(std::shared_ptr<Env> e){ return inner->eval(e); }

// ====== VFS ======
Vfs* G_VFS = nullptr;

namespace {
std::shared_ptr<VfsNode> traverse_optional(const Vfs::Overlay& overlay, const std::vector<std::string>& parts){
    std::shared_ptr<VfsNode> cur = overlay.root;
    if(parts.empty()) return cur;
    for(const auto& part : parts){
        if(!cur->isDir()) return nullptr;
        auto& ch = cur->children();
        auto it = ch.find(part);
        if(it == ch.end()) return nullptr;
        cur = it->second;
    }
    return cur;
}

char type_char(const std::shared_ptr<VfsNode>& node){
    if(!node) return '?';
    if(node->kind == VfsNode::Kind::Dir) return 'd';
    if(node->kind == VfsNode::Kind::File) return 'f';
    if(node->kind == VfsNode::Kind::Mount) return 'm';
    if(node->kind == VfsNode::Kind::Library) return 'l';
    return 'a';
}
}

Vfs::Vfs(){
    TRACE_FN();
    overlay_stack.push_back(Overlay{ "base", root, "", "" });
    overlay_dirty.push_back(false);
    overlay_source.emplace_back();
    G_VFS = this;
}

std::vector<std::string> Vfs::splitPath(const std::string& p){
    TRACE_FN("p=", p);
    std::vector<std::string> parts; std::string cur;
    for(char c: p){ if(c=='/'){ if(!cur.empty()){ parts.push_back(cur); cur.clear(); } } else cur.push_back(c); }
    if(!cur.empty()) parts.push_back(cur);
    return parts;
}
size_t Vfs::overlayCount() const {
    return overlay_stack.size();
}

const std::string& Vfs::overlayName(size_t id) const {
    if(id >= overlay_stack.size()) throw std::out_of_range("overlay id");
    return overlay_stack[id].name;
}

std::shared_ptr<DirNode> Vfs::overlayRoot(size_t id) const {
    if(id >= overlay_stack.size()) throw std::out_of_range("overlay id");
    return overlay_stack[id].root;
}

bool Vfs::overlayDirty(size_t id) const {
    if(id >= overlay_dirty.size()) throw std::out_of_range("overlay id");
    return overlay_dirty[id];
}

const std::string& Vfs::overlaySource(size_t id) const {
    if(id >= overlay_source.size()) throw std::out_of_range("overlay id");
    return overlay_source[id];
}

void Vfs::clearOverlayDirty(size_t id){
    if(id >= overlay_dirty.size()) throw std::out_of_range("overlay id");
    overlay_dirty[id] = false;
}

void Vfs::setOverlaySource(size_t id, std::string path){
    if(id >= overlay_source.size()) throw std::out_of_range("overlay id");
    overlay_source[id] = std::move(path);
}

void Vfs::markOverlayDirty(size_t id){
    if(id >= overlay_dirty.size()) throw std::out_of_range("overlay id");
    if(id == 0) return; // base overlay does not participate in auto-saving
    overlay_dirty[id] = true;
}

std::optional<size_t> Vfs::findOverlayByName(const std::string& name) const {
    for(size_t i = 0; i < overlay_stack.size(); ++i){
        if(overlay_stack[i].name == name) return i;
    }
    return std::nullopt;
}

size_t Vfs::registerOverlay(std::string name, std::shared_ptr<DirNode> overlayRoot){
    TRACE_FN("name=", name);
    if(name.empty()) throw std::runtime_error("overlay name required");
    if(findOverlayByName(name)) throw std::runtime_error("overlay name already in use");
    if(!overlayRoot) overlayRoot = std::make_shared<DirNode>("/");
    overlayRoot->name = "/";
    overlayRoot->parent.reset();
    overlay_stack.push_back(Overlay{std::move(name), overlayRoot, "", ""});
    overlay_dirty.push_back(false);
    overlay_source.emplace_back();
    return overlay_stack.size() - 1;
}

void Vfs::unregisterOverlay(size_t overlayId){
    TRACE_FN("overlayId=", overlayId);
    if(overlayId == 0) throw std::runtime_error("cannot remove base overlay");
    if(overlayId >= overlay_stack.size()) throw std::out_of_range("overlay id");
    overlay_stack.erase(overlay_stack.begin() + static_cast<std::ptrdiff_t>(overlayId));
    overlay_dirty.erase(overlay_dirty.begin() + static_cast<std::ptrdiff_t>(overlayId));
    overlay_source.erase(overlay_source.begin() + static_cast<std::ptrdiff_t>(overlayId));
}

std::vector<size_t> Vfs::overlaysForPath(const std::string& path) const {
    TRACE_FN("path=", path);
    auto hits = resolveMulti(path);
    std::vector<size_t> ids;
    ids.reserve(hits.size());
    for(const auto& hit : hits){
        if(hit.node && hit.node->isDir()) ids.push_back(hit.overlay_id);
    }
    return ids;
}

std::vector<Vfs::OverlayHit> Vfs::resolveMulti(const std::string& path) const {
    std::vector<size_t> all;
    all.reserve(overlay_stack.size());
    for(size_t i = 0; i < overlay_stack.size(); ++i) all.push_back(i);
    return resolveMulti(path, all);
}

std::vector<Vfs::OverlayHit> Vfs::resolveMulti(const std::string& path, const std::vector<size_t>& allowed) const {
    TRACE_FN("path=", path);
    if(path.empty() || path[0] != '/') throw std::runtime_error("abs path required");
    auto parts = splitPath(path);
    std::vector<OverlayHit> hits;
    auto visit = [&](size_t idx){
        if(idx >= overlay_stack.size()) return;
        auto node = traverse_optional(overlay_stack[idx], parts);
        if(node) hits.push_back(OverlayHit{idx, node});
    };
    if(allowed.empty()){
        for(size_t i = 0; i < overlay_stack.size(); ++i) visit(i);
    } else {
        for(size_t idx : allowed) visit(idx);
    }
    return hits;
}

std::shared_ptr<VfsNode> Vfs::resolve(const std::string& path){
    TRACE_FN("path=", path);
    auto hits = resolveMulti(path);
    if(hits.empty()) throw std::runtime_error("not found: " + path);
    if(hits.size() > 1){
        std::ostringstream oss;
        oss << "path '" << path << "' present in overlays: ";
        for(size_t i = 0; i < hits.size(); ++i){
            if(i) oss << ", ";
            oss << overlay_stack[hits[i].overlay_id].name;
        }
        throw std::runtime_error(oss.str());
    }
    return hits.front().node;
}

std::shared_ptr<VfsNode> Vfs::resolveForOverlay(const std::string& path, size_t overlayId){
    TRACE_FN("path=", path, ", overlay=", overlayId);
    if(path.empty() || path[0] != '/') throw std::runtime_error("abs path required");
    if(overlayId >= overlay_stack.size()) throw std::out_of_range("overlay id");
    auto parts = splitPath(path);
    auto node = traverse_optional(overlay_stack[overlayId], parts);
    if(!node) throw std::runtime_error("not found in overlay");
    return node;
}

std::shared_ptr<VfsNode> Vfs::tryResolveForOverlay(const std::string& path, size_t overlayId) const {
    if(path.empty() || path[0] != '/') return nullptr;
    if(overlayId >= overlay_stack.size()) return nullptr;
    auto parts = splitPath(path);
    return traverse_optional(overlay_stack[overlayId], parts);
}

std::shared_ptr<DirNode> Vfs::ensureDir(const std::string& path, size_t overlayId){
    return ensureDirForOverlay(path, overlayId);
}

std::shared_ptr<DirNode> Vfs::ensureDirForOverlay(const std::string& path, size_t overlayId){
    TRACE_FN("path=", path, ", overlay=", overlayId);
    if(overlayId >= overlay_stack.size()) throw std::out_of_range("overlay id");
    if(path.empty() || path[0] != '/') throw std::runtime_error("abs path required");
    if(path == "/") return overlay_stack[overlayId].root;
    auto parts = splitPath(path);
    std::shared_ptr<VfsNode> cur = overlay_stack[overlayId].root;
    for(const auto& part : parts){
        if(!cur->isDir()) throw std::runtime_error("not dir: " + part);
        auto& ch = cur->children();
        auto it = ch.find(part);
        if(it == ch.end()){
            auto dir = std::make_shared<DirNode>(part);
            dir->parent = cur;
            ch[part] = dir;
            markOverlayDirty(overlayId);
            cur = dir;
        } else {
            cur = it->second;
        }
    }
    if(!cur->isDir()) throw std::runtime_error("exists but not dir");
    return std::static_pointer_cast<DirNode>(cur);
}

void Vfs::mkdir(const std::string& path, size_t overlayId){
    TRACE_FN("path=", path, ", overlay=", overlayId);
    ensureDirForOverlay(path, overlayId);
}

void Vfs::touch(const std::string& path, size_t overlayId){
    TRACE_FN("path=", path, ", overlay=", overlayId);
    auto parts = splitPath(path);
    if(parts.empty()) throw std::runtime_error("bad path");
    std::string fname = parts.back();
    parts.pop_back();
    std::string dir = "/";
    for(const auto& part : parts) dir = join_path(dir, part);
    auto dirNode = ensureDirForOverlay(dir, overlayId);
    auto& ch = dirNode->children();
    auto it = ch.find(fname);
    if(it == ch.end()){
        auto file = std::make_shared<FileNode>(fname, "");
        file->parent = dirNode;
        ch[fname] = file;
        markOverlayDirty(overlayId);
    } else if(it->second->kind != VfsNode::Kind::File){
        throw std::runtime_error("touch non-file");
    }
}

void Vfs::write(const std::string& path, const std::string& data, size_t overlayId){
    TRACE_FN("path=", path, ", overlay=", overlayId, ", size=", data.size());
    auto parts = splitPath(path);
    if(parts.empty()) throw std::runtime_error("bad path");
    std::string fname = parts.back();
    parts.pop_back();
    std::string dir = "/";
    for(const auto& part : parts) dir = join_path(dir, part);
    auto dirNode = ensureDirForOverlay(dir, overlayId);
    auto& ch = dirNode->children();
    std::shared_ptr<VfsNode> node;
    auto it = ch.find(fname);
    if(it == ch.end()){
        auto file = std::make_shared<FileNode>(fname, "");
        file->parent = dirNode;
        ch[fname] = file;
        node = file;
        markOverlayDirty(overlayId);
    } else {
        node = it->second;
    }
    if(node->kind != VfsNode::Kind::File && node->kind != VfsNode::Kind::Ast)
        throw std::runtime_error("write non-file");
    node->write(data);
    markOverlayDirty(overlayId);
}

std::string Vfs::read(const std::string& path, std::optional<size_t> overlayId) const {
    TRACE_FN("path=", path);
    if(overlayId){
        auto node = tryResolveForOverlay(path, *overlayId);
        if(!node) throw std::runtime_error("not found: " + path);
        if(node->kind != VfsNode::Kind::File) throw std::runtime_error("read non-file");
        return node->read();
    }
    auto hits = resolveMulti(path);
    if(hits.empty()) throw std::runtime_error("not found: " + path);
    std::shared_ptr<VfsNode> target;
    for(const auto& hit : hits){
        if(hit.node->kind == VfsNode::Kind::File){
            if(target) throw std::runtime_error("multiple overlays contain file at " + path);
            target = hit.node;
        } else if(hit.node->kind == VfsNode::Kind::Ast){
            if(target) throw std::runtime_error("multiple overlays contain node at " + path);
            target = hit.node;
        }
    }
    if(!target) throw std::runtime_error("read non-file");
    return target->read();
}

void Vfs::addNode(const std::string& dirpath, std::shared_ptr<VfsNode> n, size_t overlayId){
    TRACE_FN("dirpath=", dirpath, ", overlay=", overlayId);
    if(!n) throw std::runtime_error("null node");
    auto dirNode = ensureDirForOverlay(dirpath.empty() ? std::string("/") : dirpath, overlayId);
    n->parent = dirNode;
    dirNode->children()[n->name] = n;
    markOverlayDirty(overlayId);
}

void Vfs::rm(const std::string& path, size_t overlayId){
    TRACE_FN("path=", path, ", overlay=", overlayId);
    if(path == "/") throw std::runtime_error("rm / not allowed");
    auto node = resolveForOverlay(path, overlayId);
    auto parent = node->parent.lock();
    if(!parent) throw std::runtime_error("parent missing");
    parent->children().erase(node->name);
    markOverlayDirty(overlayId);
}

void Vfs::mv(const std::string& src, const std::string& dst, size_t overlayId){
    TRACE_FN("src=", src, ", dst=", dst, ", overlay=", overlayId);
    auto node = resolveForOverlay(src, overlayId);
    auto parent = node->parent.lock();
    if(!parent) throw std::runtime_error("parent missing");
    parent->children().erase(node->name);

    auto parts = splitPath(dst);
    if(parts.empty()) throw std::runtime_error("bad path");
    std::string name = parts.back();
    parts.pop_back();
    std::string dir = "/";
    for(const auto& part : parts) dir = join_path(dir, part);
    auto dirNode = ensureDirForOverlay(dir, overlayId);
    node->name = name;
    node->parent = dirNode;
    dirNode->children()[name] = node;
    markOverlayDirty(overlayId);
}

void Vfs::link(const std::string& src, const std::string& dst, size_t overlayId){
    TRACE_FN("src=", src, ", dst=", dst, ", overlay=", overlayId);
    auto node = resolveForOverlay(src, overlayId);
    auto parts = splitPath(dst);
    if(parts.empty()) throw std::runtime_error("bad path");
    std::string name = parts.back();
    parts.pop_back();
    std::string dir = "/";
    for(const auto& part : parts) dir = join_path(dir, part);
    auto dirNode = ensureDirForOverlay(dir, overlayId);
    dirNode->children()[name] = node;
    markOverlayDirty(overlayId);
}

Vfs::DirListing Vfs::listDir(const std::string& p, const std::vector<size_t>& overlays) const {
    TRACE_FN("path=", p);
    DirListing listing;
    std::vector<size_t> allowed = overlays;
    if(allowed.empty()) allowed.push_back(0);
    for(size_t overlayId : allowed){
        if(overlayId >= overlay_stack.size()) continue;
        auto node = tryResolveForOverlay(p, overlayId);
        if(!node || !node->isDir()) continue;
        for(auto& kv : node->children()){
            auto child = kv.second;
            auto& entry = listing[kv.first];
            entry.overlays.push_back(overlayId);
            entry.nodes.push_back(child);
            entry.types.insert(type_char(child));
        }
    }
    return listing;
}

void Vfs::ls(const std::string& p){
    TRACE_FN("p=", p);
    auto node = resolveForOverlay(p, 0);
    if(!node->isDir()){
        std::cout << p << "\n";
        return;
    }
    for(auto& kv : node->children()){
        auto& child = kv.second;
        std::cout << type_char(child) << " " << kv.first << "\n";
    }
}

void Vfs::tree(std::shared_ptr<VfsNode> n, std::string pref){
    TRACE_FN("node=", n ? n->name : std::string("<root>"), ", pref=", pref);
    if(!n) n = root;
    std::cout << pref << type_char(n) << " " << n->name << "\n";
    if(n->isDir()){
        for(auto& kv : n->children()){
            tree(kv.second, pref + "  ");
        }
    }
}

// ====== Mount Nodes ======

MountNode::MountNode(std::string n, std::string hp)
    : VfsNode(std::move(n), Kind::Mount), host_path(std::move(hp)) {}

bool MountNode::isDir() const {
    namespace fs = std::filesystem;
    try {
        return fs::is_directory(host_path);
    } catch(...) {
        return false;
    }
}

std::string MountNode::read() const {
    namespace fs = std::filesystem;
    if(fs::is_directory(host_path)){
        return "";
    }
    std::ifstream ifs(host_path, std::ios::binary);
    if(!ifs) throw std::runtime_error("mount: cannot read file " + host_path);
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

void MountNode::write(const std::string& s) {
    namespace fs = std::filesystem;
    if(fs::is_directory(host_path)){
        throw std::runtime_error("mount: cannot write to directory");
    }
    std::ofstream ofs(host_path, std::ios::binary | std::ios::trunc);
    if(!ofs) throw std::runtime_error("mount: cannot write file " + host_path);
    ofs << s;
}

void MountNode::populateCache() const {
    namespace fs = std::filesystem;
    if(!fs::is_directory(host_path)) return;

    cache.clear();
    try {
        for(const auto& entry : fs::directory_iterator(host_path)){
            auto filename = entry.path().filename().string();
            auto node = std::make_shared<MountNode>(filename, entry.path().string());
            cache[filename] = node;
        }
    } catch(const std::exception& e){
        throw std::runtime_error(std::string("mount: directory iteration failed: ") + e.what());
    }
}

std::map<std::string, std::shared_ptr<VfsNode>>& MountNode::children() {
    populateCache();
    return cache;
}

LibrarySymbolNode::LibrarySymbolNode(std::string n, void* ptr, std::string sig)
    : VfsNode(std::move(n), Kind::File), func_ptr(ptr), signature(std::move(sig)) {}

LibraryNode::LibraryNode(std::string n, std::string lp)
    : VfsNode(std::move(n), Kind::Library), lib_path(std::move(lp)), handle(nullptr) {

    handle = dlopen(lib_path.c_str(), RTLD_LAZY);
    if(!handle){
        throw std::runtime_error(std::string("mount.lib: dlopen failed: ") + dlerror());
    }

    // Note: Automatic symbol enumeration is platform-specific and complex.
    // For now, we create a placeholder. Users can query symbols manually or
    // we can extend this with platform-specific code (e.g., parsing .so with libelf)
    auto placeholder = std::make_shared<FileNode>("_info",
        "Library loaded: " + lib_path + "\nUse dlsym or add symbol discovery");
    symbols["_info"] = placeholder;
}

LibraryNode::~LibraryNode() {
    if(handle){
        dlclose(handle);
        handle = nullptr;
    }
}

// RemoteNode implementation
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

RemoteNode::RemoteNode(std::string n, std::string h, int p, std::string rp)
    : VfsNode(std::move(n), Kind::Mount), host(std::move(h)), port(p),
      remote_path(std::move(rp)), sock_fd(-1), cache_valid(false) {}

RemoteNode::~RemoteNode() {
    disconnect();
}

void RemoteNode::ensureConnected() const {
    std::lock_guard<std::mutex> lock(conn_mutex);
    if(sock_fd >= 0) return;

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(sock_fd < 0){
        throw std::runtime_error("remote: failed to create socket");
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    // Try to resolve hostname
    struct hostent* he = gethostbyname(host.c_str());
    if(he == nullptr){
        close(sock_fd);
        sock_fd = -1;
        throw std::runtime_error("remote: cannot resolve host " + host);
    }

    memcpy(&server_addr.sin_addr, he->h_addr_list[0], he->h_length);

    if(connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        close(sock_fd);
        sock_fd = -1;
        throw std::runtime_error("remote: failed to connect to " + host + ":" + std::to_string(port));
    }

    TRACE_MSG("RemoteNode connected to ", host, ":", port);
}

void RemoteNode::disconnect() const {
    std::lock_guard<std::mutex> lock(conn_mutex);
    if(sock_fd >= 0){
        close(sock_fd);
        sock_fd = -1;
    }
}

std::string RemoteNode::execRemote(const std::string& command) const {
    ensureConnected();

    std::lock_guard<std::mutex> lock(conn_mutex);

    // Send: EXEC <command>\n
    std::string request = "EXEC " + command + "\n";
    ssize_t sent = send(sock_fd, request.c_str(), request.size(), 0);
    if(sent < 0 || static_cast<size_t>(sent) != request.size()){
        disconnect();
        throw std::runtime_error("remote: failed to send command");
    }

    // Receive response: OK <output>\n or ERR <message>\n
    std::string response;
    char buf[4096];
    while(true){
        ssize_t n = recv(sock_fd, buf, sizeof(buf)-1, 0);
        if(n <= 0){
            disconnect();
            throw std::runtime_error("remote: connection closed");
        }
        buf[n] = '\0';
        response += buf;
        if(response.find('\n') != std::string::npos) break;
    }

    // Parse response
    if(response.substr(0, 3) == "OK "){
        return response.substr(3, response.size() - 4); // strip "OK " and trailing \n
    } else if(response.substr(0, 4) == "ERR "){
        throw std::runtime_error("remote error: " + response.substr(4, response.size() - 5));
    } else {
        throw std::runtime_error("remote: invalid response format");
    }
}

bool RemoteNode::isDir() const {
    try {
        std::string cmd = "test -d " + remote_path + " && echo yes || echo no";
        std::string result = execRemote(cmd);
        return result == "yes";
    } catch(...) {
        return false;
    }
}

std::string RemoteNode::read() const {
    std::string cmd = "cat " + remote_path;
    return execRemote(cmd);
}

void RemoteNode::write(const std::string& s) {
    // Escape single quotes in content
    std::string escaped = s;
    size_t pos = 0;
    while((pos = escaped.find('\'', pos)) != std::string::npos){
        escaped.replace(pos, 1, "'\\''");
        pos += 4;
    }
    std::string cmd = "echo '" + escaped + "' > " + remote_path;
    execRemote(cmd);
    cache_valid = false;
}

void RemoteNode::populateCache() const {
    cache.clear();
    std::string cmd = "ls " + remote_path;
    std::string output = execRemote(cmd);

    std::istringstream iss(output);
    std::string line;
    while(std::getline(iss, line)){
        if(line.empty()) continue;
        auto child_path = remote_path;
        if(child_path.back() != '/') child_path += '/';
        child_path += line;
        auto child = std::make_shared<RemoteNode>(line, host, port, child_path);
        cache[line] = child;
    }
}

std::map<std::string, std::shared_ptr<VfsNode>>& RemoteNode::children() {
    if(!cache_valid){
        populateCache();
        cache_valid = true;
    }
    return cache;
}

// ====== Mount Management ======

void Vfs::mountFilesystem(const std::string& host_path, const std::string& vfs_path, size_t overlayId) {
    TRACE_FN("host=", host_path, ", vfs=", vfs_path, ", overlay=", overlayId);

    if(!mount_allowed){
        throw std::runtime_error("mount: mounting is currently disabled (use mount.allow)");
    }

    namespace fs = std::filesystem;
    if(!fs::exists(host_path)){
        throw std::runtime_error("mount: host path does not exist: " + host_path);
    }

    auto abs_host = fs::absolute(host_path).string();

    // Check if already mounted
    for(const auto& m : mounts){
        if(m.vfs_path == vfs_path){
            throw std::runtime_error("mount: path already has a mount: " + vfs_path);
        }
    }

    auto mount_node = std::make_shared<MountNode>(path_basename(vfs_path), abs_host);
    // addNode expects parent directory path, so we need to get the parent
    auto parent_path = path_dirname(vfs_path);
    if(parent_path.empty()) parent_path = "/";
    addNode(parent_path, mount_node, overlayId);

    MountInfo info;
    info.vfs_path = vfs_path;
    info.host_path = abs_host;
    info.mount_node = mount_node;
    info.type = MountType::Filesystem;
    mounts.push_back(info);
}

void Vfs::mountLibrary(const std::string& lib_path, const std::string& vfs_path, size_t overlayId) {
    TRACE_FN("lib=", lib_path, ", vfs=", vfs_path, ", overlay=", overlayId);

    if(!mount_allowed){
        throw std::runtime_error("mount.lib: mounting is currently disabled (use mount.allow)");
    }

    namespace fs = std::filesystem;
    if(!fs::exists(lib_path)){
        throw std::runtime_error("mount.lib: library does not exist: " + lib_path);
    }

    auto abs_lib = fs::absolute(lib_path).string();

    // Check if already mounted
    for(const auto& m : mounts){
        if(m.vfs_path == vfs_path){
            throw std::runtime_error("mount.lib: path already has a mount: " + vfs_path);
        }
    }

    auto lib_node = std::make_shared<LibraryNode>(path_basename(vfs_path), abs_lib);
    // addNode expects parent directory path, so we need to get the parent
    auto parent_path = path_dirname(vfs_path);
    if(parent_path.empty()) parent_path = "/";
    addNode(parent_path, lib_node, overlayId);

    MountInfo info;
    info.vfs_path = vfs_path;
    info.host_path = abs_lib;
    info.mount_node = lib_node;
    info.type = MountType::Library;
    mounts.push_back(info);
}

void Vfs::mountRemote(const std::string& host, int port, const std::string& remote_path, const std::string& vfs_path, size_t overlayId) {
    TRACE_FN("host=", host, ", port=", port, ", remote=", remote_path, ", vfs=", vfs_path, ", overlay=", overlayId);

    if(!mount_allowed){
        throw std::runtime_error("mount.remote: mounting is currently disabled (use mount.allow)");
    }

    // Check if already mounted
    for(const auto& m : mounts){
        if(m.vfs_path == vfs_path){
            throw std::runtime_error("mount.remote: path already has a mount: " + vfs_path);
        }
    }

    auto remote_node = std::make_shared<RemoteNode>(path_basename(vfs_path), host, port, remote_path);
    auto parent_path = path_dirname(vfs_path);
    if(parent_path.empty()) parent_path = "/";
    addNode(parent_path, remote_node, overlayId);

    MountInfo info;
    info.vfs_path = vfs_path;
    info.host_path = host + ":" + std::to_string(port) + ":" + remote_path;
    info.mount_node = remote_node;
    info.type = MountType::Remote;
    mounts.push_back(info);
}

void Vfs::unmount(const std::string& vfs_path) {
    TRACE_FN("vfs=", vfs_path);

    auto it = std::find_if(mounts.begin(), mounts.end(),
        [&](const MountInfo& m){ return m.vfs_path == vfs_path; });

    if(it == mounts.end()){
        throw std::runtime_error("unmount: no mount at path: " + vfs_path);
    }

    // Remove from VFS (base overlay for now)
    rm(vfs_path, 0);

    mounts.erase(it);
}

std::vector<Vfs::MountInfo> Vfs::listMounts() const {
    return mounts;
}

void Vfs::setMountAllowed(bool allowed) {
    mount_allowed = allowed;
}

bool Vfs::isMountAllowed() const {
    return mount_allowed;
}

// ====== Tag Registry & Storage ======
TagId TagRegistry::registerTag(const std::string& name){
    auto it = name_to_id.find(name);
    if(it != name_to_id.end()) return it->second;
    TagId id = next_id++;
    name_to_id[name] = id;
    id_to_name[id] = name;
    return id;
}

TagId TagRegistry::getTagId(const std::string& name) const {
    auto it = name_to_id.find(name);
    return it != name_to_id.end() ? it->second : TAG_INVALID;
}

std::string TagRegistry::getTagName(TagId id) const {
    auto it = id_to_name.find(id);
    return it != id_to_name.end() ? it->second : "";
}

bool TagRegistry::hasTag(const std::string& name) const {
    return name_to_id.find(name) != name_to_id.end();
}

std::vector<std::string> TagRegistry::allTags() const {
    std::vector<std::string> tags;
    tags.reserve(name_to_id.size());
    for(const auto& pair : name_to_id){
        tags.push_back(pair.first);
    }
    return tags;
}

void TagStorage::addTag(VfsNode* node, TagId tag){
    if(!node || tag == TAG_INVALID) return;
    node_tags[node].insert(tag);
}

void TagStorage::removeTag(VfsNode* node, TagId tag){
    if(!node) return;
    auto it = node_tags.find(node);
    if(it != node_tags.end()){
        it->second.erase(tag);
        if(it->second.empty()) node_tags.erase(it);
    }
}

bool TagStorage::hasTag(VfsNode* node, TagId tag) const {
    if(!node) return false;
    auto it = node_tags.find(node);
    if(it == node_tags.end()) return false;
    return it->second.count(tag) > 0;
}

const TagSet* TagStorage::getTags(VfsNode* node) const {
    if(!node) return nullptr;
    auto it = node_tags.find(node);
    return it != node_tags.end() ? &it->second : nullptr;
}

void TagStorage::clearTags(VfsNode* node){
    if(node) node_tags.erase(node);
}

std::vector<VfsNode*> TagStorage::findByTag(TagId tag) const {
    std::vector<VfsNode*> result;
    for(const auto& pair : node_tags){
        if(pair.second.count(tag) > 0){
            result.push_back(pair.first);
        }
    }
    return result;
}

std::vector<VfsNode*> TagStorage::findByTags(const TagSet& tags, bool match_all) const {
    std::vector<VfsNode*> result;
    for(const auto& pair : node_tags){
        if(match_all){
            // All tags must be present
            bool has_all = true;
            for(TagId tag : tags){
                if(pair.second.count(tag) == 0){
                    has_all = false;
                    break;
                }
            }
            if(has_all) result.push_back(pair.first);
        } else {
            // Any tag can be present
            bool has_any = false;
            for(TagId tag : tags){
                if(pair.second.count(tag) > 0){
                    has_any = true;
                    break;
                }
            }
            if(has_any) result.push_back(pair.first);
        }
    }
    return result;
}

// VFS tag helpers
TagId Vfs::registerTag(const std::string& name){
    return tag_registry.registerTag(name);
}

TagId Vfs::getTagId(const std::string& name) const {
    return tag_registry.getTagId(name);
}

std::string Vfs::getTagName(TagId id) const {
    return tag_registry.getTagName(id);
}

bool Vfs::hasTagRegistered(const std::string& name) const {
    return tag_registry.hasTag(name);
}

std::vector<std::string> Vfs::allRegisteredTags() const {
    return tag_registry.allTags();
}

void Vfs::addTag(const std::string& vfs_path, const std::string& tag_name){
    auto node = resolve(vfs_path);
    if(!node) throw std::runtime_error("tag.add: path not found: " + vfs_path);
    TagId tag_id = tag_registry.registerTag(tag_name);
    tag_storage.addTag(node.get(), tag_id);
}

void Vfs::removeTag(const std::string& vfs_path, const std::string& tag_name){
    auto node = resolve(vfs_path);
    if(!node) throw std::runtime_error("tag.remove: path not found: " + vfs_path);
    TagId tag_id = tag_registry.getTagId(tag_name);
    if(tag_id == TAG_INVALID) return;  // Tag doesn't exist, nothing to remove
    tag_storage.removeTag(node.get(), tag_id);
}

bool Vfs::nodeHasTag(const std::string& vfs_path, const std::string& tag_name) const {
    auto node = const_cast<Vfs*>(this)->resolve(vfs_path);
    if(!node) return false;
    TagId tag_id = tag_registry.getTagId(tag_name);
    if(tag_id == TAG_INVALID) return false;
    return tag_storage.hasTag(node.get(), tag_id);
}

std::vector<std::string> Vfs::getNodeTags(const std::string& vfs_path) const {
    auto node = const_cast<Vfs*>(this)->resolve(vfs_path);
    if(!node) return {};
    const TagSet* tags = tag_storage.getTags(node.get());
    if(!tags) return {};

    std::vector<std::string> result;
    result.reserve(tags->size());
    for(TagId tag_id : *tags){
        result.push_back(tag_registry.getTagName(tag_id));
    }
    return result;
}

void Vfs::clearNodeTags(const std::string& vfs_path){
    auto node = resolve(vfs_path);
    if(!node) throw std::runtime_error("tag.clear: path not found: " + vfs_path);
    tag_storage.clearTags(node.get());
}

std::vector<std::string> Vfs::findNodesByTag(const std::string& tag_name) const {
    TagId tag_id = tag_registry.getTagId(tag_name);
    if(tag_id == TAG_INVALID) return {};

    auto nodes = tag_storage.findByTag(tag_id);
    std::vector<std::string> result;
    // TODO: Need reverse path lookup - this is a known limitation for now
    // For MVP, we'll need to implement path tracking or traverse the VFS tree
    return result;
}

std::vector<std::string> Vfs::findNodesByTags(const std::vector<std::string>& tag_names, bool match_all) const {
    TagSet tag_ids;
    for(const auto& name : tag_names){
        TagId id = tag_registry.getTagId(name);
        if(id != TAG_INVALID) tag_ids.insert(id);
    }
    if(tag_ids.empty()) return {};

    auto nodes = tag_storage.findByTags(tag_ids, match_all);
    std::vector<std::string> result;
    // TODO: Need reverse path lookup - this is a known limitation for now
    return result;
}

// ====== Parser ======
static size_t POS;
static std::shared_ptr<AstNode> parseExpr(const std::vector<Token>& T);

std::vector<Token> lex(const std::string& src){
    std::vector<Token> t; std::string cur;
    auto push=[&]{ if(!cur.empty()){ t.push_back({cur}); cur.clear(); } };
    for (size_t i=0;i<src.size();++i){
        char c=src[i];
        if (std::isspace(static_cast<unsigned char>(c))) { push(); continue; }
        if (c=='('||c==')'){ push(); t.push_back({std::string(1,c)}); continue; }
        if (c=='"'){ push(); std::string s; ++i;
            while(i<src.size() && src[i]!='"'){
                if(src[i]=='\\' && i+1<src.size()){ s.push_back(src[i+1]); i+=2; }
                else s.push_back(src[i++]);
            }
            t.push_back({std::string("\"")+s+"\""}); continue;
        }
        cur.push_back(c);
    }
    push(); return t;
}

static bool isInt(const std::string& s){
    if (s.empty()) return false;
    size_t i=(s[0]=='-'?1:0); if (i==s.size()) return false;
    for (; i<s.size(); ++i) if(!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
    return true;
}

static std::shared_ptr<AstNode> atom(const std::string& s){
    if (s=="#t") return std::make_shared<AstBool>("<b>", true);
    if (s=="#f") return std::make_shared<AstBool>("<b>", false);
    if (s.size()>=2 && s.front()=='"' && s.back()=='"')
        return std::make_shared<AstStr>("<s>", s.substr(1, s.size()-2));
    if (isInt(s)) return std::make_shared<AstInt>("<i>", std::stoll(s));
    return std::make_shared<AstSym>("<sym>", s);
}

static std::shared_ptr<AstNode> parseList(const std::vector<Token>& T){
    if (POS>=T.size() || T[POS].s!="(") throw std::runtime_error("expected (");
    ++POS;
    if (POS<T.size() && T[POS].s==")"){ ++POS; return std::make_shared<AstStr>("<s>",""); }
    auto head = parseExpr(T);
    auto sym  = std::dynamic_pointer_cast<AstSym>(head);
    std::vector<std::shared_ptr<AstNode>> items;
    while (POS<T.size() && T[POS].s!=")") items.push_back(parseExpr(T));
    if (POS>=T.size()) throw std::runtime_error("missing )");
    ++POS;

    std::string H = sym? sym->id : std::string();
    if (H=="if"){
        if (items.size()!=3) throw std::runtime_error("if needs 3 args");
        return std::make_shared<AstIf>("<if>", items[0], items[1], items[2]);
    }
    if (H=="lambda"){
        if (items.size()<2) throw std::runtime_error("lambda needs params and body");
        // proto: single param only: (lambda x body)
        std::vector<std::string> ps;
        if (auto sp=std::dynamic_pointer_cast<AstSym>(items[0])) ps.push_back(sp->id);
        else throw std::runtime_error("lambda single param only");
        auto body=items.back();
        return std::make_shared<AstLambda>("<lam>", ps, body);
    }
    // generic call
    return std::make_shared<AstCall>("<call>", head, items);
}

static std::shared_ptr<AstNode> parseExpr(const std::vector<Token>& T){
    if (POS>=T.size()) throw std::runtime_error("unexpected EOF");
    auto s = T[POS].s;
    if (s=="(") return parseList(T);
    if (s==")") throw std::runtime_error("unexpected )");
    ++POS;
    return atom(s);
}

std::shared_ptr<AstNode> parse(const std::string& src){
    POS=0; auto T = lex(src);
    auto n = parseExpr(T);
    if (POS!=T.size()) throw std::runtime_error("extra tokens");
    return n;
}

// ====== Builtins ======
void install_builtins(std::shared_ptr<Env> g){
    auto wrap = [&](auto op){
        return Value::Built([op](std::vector<Value>& av, std::shared_ptr<Env>){
            if (av.size()<2) throw std::runtime_error("need at least 2 args");
            auto gi=[](const Value& v)->int64_t{
                if (!std::holds_alternative<int64_t>(v.v)) throw std::runtime_error("int expected");
                return std::get<int64_t>(v.v);
            };
            int64_t acc = gi(av[0]);
            for (size_t i=1;i<av.size();++i) acc = op(acc, gi(av[i]));
            return Value::I(acc);
        });
    };
    g->set("+", wrap(std::plus<int64_t>()));
    g->set("-", wrap(std::minus<int64_t>()));
    g->set("*", wrap(std::multiplies<int64_t>()));

    g->set("=", Value::Built([](std::vector<Value>& av, std::shared_ptr<Env>){
        if (av.size()!=2) throw std::runtime_error("= needs 2 args");
        return Value::B(av[0].show()==av[1].show());
    }));
    g->set("<", Value::Built([](std::vector<Value>& av, std::shared_ptr<Env>){
        if (av.size()!=2) throw std::runtime_error("< needs 2 args");
        if (!std::holds_alternative<int64_t>(av[0].v) || !std::holds_alternative<int64_t>(av[1].v))
            throw std::runtime_error("int expected");
        return Value::B(std::get<int64_t>(av[0].v) < std::get<int64_t>(av[1].v));
    }));
    g->set("print", Value::Built([](std::vector<Value>& av, std::shared_ptr<Env>){
        for (size_t i=0;i<av.size();++i){ if(i) std::cout<<" "; std::cout<<av[i].show(); }
        std::cout<<"\n"; return av.empty()? Value() : av.back();
    }));

    // lists
    g->set("list", Value::Built([](std::vector<Value>& av, std::shared_ptr<Env>){ return Value::L(av); }));
    g->set("cons", Value::Built([](std::vector<Value>& av, std::shared_ptr<Env>){
        if (av.size()!=2) throw std::runtime_error("cons x xs");
        if (!std::holds_alternative<Value::List>(av[1].v)) throw std::runtime_error("cons expects list");
        Value::List out; const auto& xs = std::get<Value::List>(av[1].v);
        out.reserve(xs.size()+1); out.push_back(av[0]); out.insert(out.end(), xs.begin(), xs.end());
        return Value::L(out);
    }));
    g->set("head", Value::Built([](std::vector<Value>& av, std::shared_ptr<Env>){
        if (av.size()!=1 || !std::holds_alternative<Value::List>(av[0].v)) throw std::runtime_error("head xs");
        const auto& xs = std::get<Value::List>(av[0].v); if(xs.empty()) throw std::runtime_error("head of empty");
        return xs.front();
    }));
    g->set("tail", Value::Built([](std::vector<Value>& av, std::shared_ptr<Env>){
        if (av.size()!=1 || !std::holds_alternative<Value::List>(av[0].v)) throw std::runtime_error("tail xs");
        const auto& xs = std::get<Value::List>(av[0].v); if(xs.empty()) throw std::runtime_error("tail of empty");
        return Value::L(Value::List(xs.begin()+1, xs.end()));
    }));
    g->set("null?", Value::Built([](std::vector<Value>& av, std::shared_ptr<Env>){
        if (av.size()!=1) throw std::runtime_error("null? xs");
        return Value::B(std::holds_alternative<Value::List>(av[0].v) && std::get<Value::List>(av[0].v).empty());
    }));

    // strings
    g->set("str.cat", Value::Built([](std::vector<Value>& av, std::shared_ptr<Env>){
        std::string s; for(auto& v: av){ if(!std::holds_alternative<std::string>(v.v)) throw std::runtime_error("str.cat expects strings"); s += std::get<std::string>(v.v); }
        return Value::S(s);
    }));
    g->set("str.sub", Value::Built([](std::vector<Value>& av, std::shared_ptr<Env>){
        if(av.size()!=3) throw std::runtime_error("str.sub s start len");
        if(!std::holds_alternative<std::string>(av[0].v) || !std::holds_alternative<int64_t>(av[1].v) || !std::holds_alternative<int64_t>(av[2].v))
            throw std::runtime_error("str.sub types");
        const std::string& s = std::get<std::string>(av[0].v);
        size_t st = static_cast<size_t>(std::max<int64_t>(0, std::get<int64_t>(av[1].v)));
        size_t ln = static_cast<size_t>(std::max<int64_t>(0, std::get<int64_t>(av[2].v)));
        if (st > s.size()) return Value::S("");
        return Value::S(s.substr(st, ln));
    }));
    g->set("str.find", Value::Built([](std::vector<Value>& av, std::shared_ptr<Env>){
        if(av.size()!=2 || !std::holds_alternative<std::string>(av[0].v) || !std::holds_alternative<std::string>(av[1].v))
            throw std::runtime_error("str.find s sub");
        auto pos = std::get<std::string>(av[0].v).find(std::get<std::string>(av[1].v));
        return Value::I(pos==std::string::npos? -1 : static_cast<int64_t>(pos));
    }));

    // VFS helpers
    g->set("vfs-write", Value::Built([](std::vector<Value>& av, std::shared_ptr<Env>){
        if(!G_VFS) throw std::runtime_error("no vfs");
        if(av.size()!=2 || !std::holds_alternative<std::string>(av[0].v) || !std::holds_alternative<std::string>(av[1].v))
            throw std::runtime_error("vfs-write path string");
        G_VFS->write(std::get<std::string>(av[0].v), std::get<std::string>(av[1].v), 0);
        return av[0];
    }));
    g->set("vfs-read", Value::Built([](std::vector<Value>& av, std::shared_ptr<Env>){
        if(!G_VFS) throw std::runtime_error("no vfs");
        if(av.size()!=1 || !std::holds_alternative<std::string>(av[0].v))
            throw std::runtime_error("vfs-read path");
        return Value::S(G_VFS->read(std::get<std::string>(av[0].v), 0));
    }));
    g->set("vfs-ls", Value::Built([](std::vector<Value>& av, std::shared_ptr<Env>){
        if(!G_VFS) throw std::runtime_error("no vfs");
        if(av.size()!=1 || !std::holds_alternative<std::string>(av[0].v))
            throw std::runtime_error("vfs-ls \"/path\"");
        auto n = G_VFS->resolveForOverlay(std::get<std::string>(av[0].v), 0);
        if(!n->isDir()) throw std::runtime_error("vfs-ls: not dir");
        Value::List entries;
        for(auto& kv : n->children()){
            const std::string& name = kv.first;
            auto& node = kv.second;
            std::string t = node->kind==VfsNode::Kind::Dir? "dir" : (node->kind==VfsNode::Kind::File? "file" : "ast");
            entries.push_back(Value::L(Value::List{ Value::S(name), Value::S(t) }));
        }
        return Value::L(entries);
    }));

    // export & sys
    g->set("export", Value::Built([](std::vector<Value>& av, std::shared_ptr<Env>){
        if(!G_VFS) throw std::runtime_error("no vfs");
        if(av.size()!=2 || !std::holds_alternative<std::string>(av[0].v) || !std::holds_alternative<std::string>(av[1].v))
            throw std::runtime_error("export vfs host");
        std::string data = G_VFS->read(std::get<std::string>(av[0].v), 0);
        std::string host = std::get<std::string>(av[1].v);
        std::ofstream f(host, std::ios::binary);
        if(!f) throw std::runtime_error("export: cannot open host file");
        f.write(data.data(), static_cast<std::streamsize>(data.size()));
        return Value::S(host);
    }));
    g->set("sys", Value::Built([](std::vector<Value>& av, std::shared_ptr<Env>){
        if(av.size()!=1 || !std::holds_alternative<std::string>(av[0].v))
            throw std::runtime_error("sys \"cmd\"");
        std::string cmd = std::get<std::string>(av[0].v);
        // kevyt saneeraus
        for(char c: cmd){
            if(!(std::isalnum(static_cast<unsigned char>(c)) ||
                 std::isspace(static_cast<unsigned char>(c)) ||
                 std::strchr("/._-+:*\"'()=", c)))
                 throw std::runtime_error("sys: kielletty merkki");
        }
        std::string out = exec_capture(cmd + " 2>&1");
        return Value::S(out);
    }));

    // C++ apuri: hello-koodi
    g->set("cpp:hello", Value::Built([](std::vector<Value>&, std::shared_ptr<Env>){
        std::string code = "#include <iostream>\nint main(){ std::cout<<\"Hello, world!\\n\"; return 0; }\n";
        return Value::S(code);
    }));
}

// ====== Exec utils ======
std::string exec_capture(const std::string& cmd, const std::string& desc){
    TRACE_FN("cmd=", cmd, ", desc=", desc);
    std::array<char, 4096> buf{};
    std::string out;
    FILE* pipe = popen(cmd.c_str(), "r");
    if(!pipe) return out;

    static std::mutex output_mutex;
    std::atomic<bool> done{false};
    auto start_time = std::chrono::steady_clock::now();
    std::string label = desc.empty() ? std::string("external command") : desc;

    std::thread keep_alive([&](){
        using namespace std::chrono_literals;
        bool warned=false;
        auto next_report = std::chrono::steady_clock::now() + 10s;
        while(!done.load(std::memory_order_relaxed)){
            std::this_thread::sleep_for(200ms);
            if(done.load(std::memory_order_relaxed)) break;
            auto now = std::chrono::steady_clock::now();
            if(now < next_report) continue;
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
            {
                std::lock_guard<std::mutex> lock(output_mutex);
                std::cout << "[keepalive] " << label << " running for " << elapsed << "s...\n";
                if(!warned && elapsed >= 300){
                    std::cout << "[keepalive] " << label << " exceeded 300s; check connectivity or abort if needed.\n";
                    warned=true;
                }
                std::cout.flush();
            }
            next_report = now + 10s;
        }
    });

    while(true){
        size_t n = fread(buf.data(), 1, buf.size(), pipe);
        TRACE_LOOP("exec_capture.read", std::string("bytes=") + std::to_string(n));
        if(n>0) out.append(buf.data(), n);
        if(n<buf.size()) break;
    }
    done.store(true, std::memory_order_relaxed);
    if(keep_alive.joinable()) keep_alive.join();
    pclose(pipe);
    return out;
}
bool has_cmd(const std::string& c){
    std::string cmd = "command -v "+c+" >/dev/null 2>&1";
    int r = system(cmd.c_str());
    return r==0;
}

// ====== C++ AST nodes ======
CppInclude::CppInclude(std::string n, std::string h, bool a)
    : CppNode(std::move(n)), header(std::move(h)), angled(a) { kind=Kind::Ast; }
std::string CppInclude::dump(int) const {
    return std::string("#include ") + (angled?"<":"\"") + header + (angled?">":"\"") + "\n";
}
CppId::CppId(std::string n, std::string i) : CppExpr(std::move(n)), id(std::move(i)) { kind=Kind::Ast; }
std::string CppId::dump(int) const { return id; }
namespace {
bool is_octal_digit(char c){ return c>='0' && c<='7'; }
bool is_hex_digit(char c){ return std::isxdigit(static_cast<unsigned char>(c)) != 0; }

void verify_cpp_string_literal(const std::string& lit){
    for(size_t i=0;i<lit.size();++i){
        unsigned char uc = static_cast<unsigned char>(lit[i]);
        if(uc=='\n' || uc=='\r') throw std::runtime_error("cpp string literal contains raw newline");
        if(uc=='\\'){
            if(++i>=lit.size()) throw std::runtime_error("unterminated escape in cpp string literal");
            unsigned char esc = static_cast<unsigned char>(lit[i]);
            switch(esc){
                case '"': case '\\': case 'n': case 'r': case 't':
                case 'b': case 'f': case 'v': case 'a': case '?':
                    break;
                case 'x': {
                    size_t digits=0;
                    while(i+1<lit.size() && is_hex_digit(lit[i+1]) && digits<2){ ++i; ++digits; }
                    if(digits==0) throw std::runtime_error("\\x escape missing hex digits");
                    break;
                }
                case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': {
                    size_t digits=0;
                    while(i+1<lit.size() && is_octal_digit(lit[i+1]) && digits<2){ ++i; ++digits; }
                    break;
                }
                default:
                    throw std::runtime_error("unsupported escape sequence in cpp string literal");
            }
        } else if(uc < 0x20 || uc == 0x7f){
            throw std::runtime_error("cpp string literal contains unescaped control byte");
        }
    }
}
}

CppString::CppString(std::string n, std::string v) : CppExpr(std::move(n)), s(std::move(v)) { kind=Kind::Ast; }
std::string CppString::esc(const std::string& x){
    std::string out;
    out.reserve(x.size()+8);
    auto append_octal = [&out](unsigned char uc){
        out.push_back('\\');
        out.push_back(static_cast<char>('0' + ((uc >> 6) & 0x7)));
        out.push_back(static_cast<char>('0' + ((uc >> 3) & 0x7)));
        out.push_back(static_cast<char>('0' + (uc & 0x7)));
    };

    bool escape_next_question = false;
    for(size_t i=0;i<x.size();++i){
        unsigned char uc = static_cast<unsigned char>(x[i]);
        if(uc=='?'){
            if(escape_next_question || (i+1<x.size() && x[i+1]=='?')){
                out += "\\?";
                escape_next_question = (i+1<x.size() && x[i+1]=='?');
            } else {
                out.push_back('?');
                escape_next_question = false;
            }
            continue;
        }

        escape_next_question = false;
        switch(uc){
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\v': out += "\\v"; break;
            case '\a': out += "\\a"; break;
            default:
                if(uc < 0x20 || uc == 0x7f){
                    append_octal(uc);
                } else if(uc >= 0x80){
                    append_octal(uc);
                } else {
                    out.push_back(static_cast<char>(uc));
                }
        }
    }
    return out;
}
std::string CppString::dump(int) const {
    std::string escaped = esc(s);
    verify_cpp_string_literal(escaped);
    return std::string("\"") + escaped + "\"";
}
CppInt::CppInt(std::string n, long long x): CppExpr(std::move(n)), v(x) { kind=Kind::Ast; }
std::string CppInt::dump(int) const { return std::to_string(v); }
CppCall::CppCall(std::string n, std::shared_ptr<CppExpr> f, std::vector<std::shared_ptr<CppExpr>> a)
    : CppExpr(std::move(n)), fn(std::move(f)), args(std::move(a)) { kind=Kind::Ast; }
std::string CppCall::dump(int) const {
    std::string s = fn->dump(); s+='(';
    bool first=true; for(auto& a: args){ if(!first) s+=", "; first=false; s+=a->dump(); } s+=')';
    return s;
}
CppBinOp::CppBinOp(std::string n, std::string o, std::shared_ptr<CppExpr> A, std::shared_ptr<CppExpr> B)
    : CppExpr(std::move(n)), op(std::move(o)), a(std::move(A)), b(std::move(B)) { kind=Kind::Ast; }
std::string CppBinOp::dump(int) const { return a->dump()+" "+op+" "+b->dump(); }
CppStreamOut::CppStreamOut(std::string n, std::vector<std::shared_ptr<CppExpr>> xs)
    : CppExpr(std::move(n)), chain(std::move(xs)) { kind=Kind::Ast; }
std::string CppStreamOut::dump(int) const {
    std::string s="std::cout"; for(auto& e: chain){ s+=" << "; s+=e->dump(); } return s;
}
CppRawExpr::CppRawExpr(std::string n, std::string t)
    : CppExpr(std::move(n)), text(std::move(t)) { kind=Kind::Ast; }
std::string CppRawExpr::dump(int) const { return text; }
CppExprStmt::CppExprStmt(std::string n, std::shared_ptr<CppExpr> E)
    : CppStmt(std::move(n)), e(std::move(E)) { kind=Kind::Ast; }
std::string CppExprStmt::dump(int indent) const { return ind(indent)+e->dump()+";\n"; }
CppReturn::CppReturn(std::string n, std::shared_ptr<CppExpr> E)
    : CppStmt(std::move(n)), e(std::move(E)) { kind=Kind::Ast; }
std::string CppReturn::dump(int indent) const {
    std::string s = ind(indent) + "return";
    if (e) s += " " + e->dump();
    s += ";\n";
    return s;
}
CppRawStmt::CppRawStmt(std::string n, std::string t)
    : CppStmt(std::move(n)), text(std::move(t)) { kind=Kind::Ast; }
std::string CppRawStmt::dump(int indent) const {
    std::string pad = ind(indent);
    std::string out;
    size_t start = 0;
    while (start <= text.size()){
        size_t end = text.find('\n', start);
        std::string line = end==std::string::npos ? text.substr(start) : text.substr(start, end-start);
        if(!line.empty() || end!=std::string::npos) out += pad + line + "\n";
        if (end==std::string::npos) break;
        start = end + 1;
    }
    if(out.empty()) out = pad + "\n";
    return out;
}
CppVarDecl::CppVarDecl(std::string n, std::string ty, std::string nm, std::string in, bool has)
    : CppStmt(std::move(n)), type(std::move(ty)), name(std::move(nm)), init(std::move(in)), hasInit(has) { kind=Kind::Ast; }
std::string CppVarDecl::dump(int indent) const {
    std::string s = ind(indent) + type + " " + name;
    if (hasInit){
        if (!init.empty() && (init[0]=='{' || init[0]=='(')) s += init;
        else if (!init.empty() && init[0]=='=') s += " " + init;
        else if (!init.empty()) s += " = " + init;
    }
    s += ";\n";
    return s;
}
CppCompound::CppCompound(std::string n) : CppStmt(std::move(n)) { kind=Kind::Ast; }
std::string CppCompound::dump(int indent) const {
    std::string s = ind(indent) + "{\n";
    for(auto& st: stmts) s += st->dump(indent+2);
    s += ind(indent) + "}\n"; return s;
}
CppFunction::CppFunction(std::string n, std::string rt, std::string nm)
    : CppNode(std::move(n)), retType(std::move(rt)), name(std::move(nm)) { kind=Kind::Ast; body=std::make_shared<CppCompound>("body"); }
std::string CppFunction::dump(int indent) const {
    std::string s; s += retType + " " + name + "(";
    for(size_t i=0;i<params.size();++i){ if(i) s += ", "; s += params[i].type + " " + params[i].name; }
    s += ")\n"; s += body->dump(indent); return s;
}
CppRangeFor::CppRangeFor(std::string n, std::string d, std::string r)
    : CppStmt(std::move(n)), decl(std::move(d)), range(std::move(r)), body(std::make_shared<CppCompound>("body")) { kind=Kind::Ast; }
std::string CppRangeFor::dump(int indent) const {
    std::string s = ind(indent) + "for (" + decl + " : " + range + ")\n";
    s += body->dump(indent);
    return s;
}
CppTranslationUnit::CppTranslationUnit(std::string n) : CppNode(std::move(n)) { kind=Kind::Ast; }
std::string CppTranslationUnit::dump(int) const {
    std::string s;
    for(auto& i: includes) s += i->dump(0);
    s += "\n";
    for(auto& f: funcs){ s += f->dump(0); s += "\n"; }
    return s;
}

std::shared_ptr<CppTranslationUnit> expect_tu(std::shared_ptr<VfsNode> n){
    auto tu = std::dynamic_pointer_cast<CppTranslationUnit>(n);
    if(!tu) throw std::runtime_error("not a CppTranslationUnit node");
    return tu;
}
std::shared_ptr<CppFunction> expect_fn(std::shared_ptr<VfsNode> n){
    auto fn = std::dynamic_pointer_cast<CppFunction>(n);
    if(!fn) throw std::runtime_error("not a CppFunction node");
    return fn;
}
std::shared_ptr<CppCompound> expect_block(std::shared_ptr<VfsNode> n){
    if(auto fn = std::dynamic_pointer_cast<CppFunction>(n)) return fn->body;
    if(auto block = std::dynamic_pointer_cast<CppCompound>(n)) return block;
    if(auto loop = std::dynamic_pointer_cast<CppRangeFor>(n)) return loop->body;
    throw std::runtime_error("node does not own a compound body");
}
void vfs_add(Vfs& vfs, const std::string& path, std::shared_ptr<VfsNode> node, size_t overlayId){
    std::string dir = path.substr(0, path.find_last_of('/'));
    if(dir.empty()) dir = "/";
    std::string name = path.substr(path.find_last_of('/')+1);
    node->name = name;
    vfs.addNode(dir, node, overlayId);
}
void cpp_dump_to_vfs(Vfs& vfs, size_t overlayId, const std::string& tuPath, const std::string& filePath){
    auto n = vfs.resolveForOverlay(tuPath, overlayId);
    auto tu = expect_tu(n);
    std::string code = tu->dump(0);
    vfs.write(filePath, code, overlayId);
}

// ====== Planner Nodes ======
std::string PlanGoals::read() const {
    std::string result;
    for(size_t i = 0; i < goals.size(); ++i){
        result += "- " + goals[i] + "\n";
    }
    return result;
}

void PlanGoals::write(const std::string& s){
    goals.clear();
    std::istringstream iss(s);
    std::string line;
    while(std::getline(iss, line)){
        auto trimmed = trim_copy(line);
        if(trimmed.empty()) continue;
        if(trimmed.size() >= 2 && trimmed[0] == '-' && std::isspace(static_cast<unsigned char>(trimmed[1]))){
            goals.push_back(trim_copy(trimmed.substr(2)));
        } else {
            goals.push_back(trimmed);
        }
    }
}

std::string PlanIdeas::read() const {
    std::string result;
    for(size_t i = 0; i < ideas.size(); ++i){
        result += "- " + ideas[i] + "\n";
    }
    return result;
}

void PlanIdeas::write(const std::string& s){
    ideas.clear();
    std::istringstream iss(s);
    std::string line;
    while(std::getline(iss, line)){
        auto trimmed = trim_copy(line);
        if(trimmed.empty()) continue;
        if(trimmed.size() >= 2 && trimmed[0] == '-' && std::isspace(static_cast<unsigned char>(trimmed[1]))){
            ideas.push_back(trim_copy(trimmed.substr(2)));
        } else {
            ideas.push_back(trimmed);
        }
    }
}

std::string PlanJobs::read() const {
    std::string result;
    auto sorted = getSortedJobIndices();
    for(size_t idx : sorted){
        const auto& job = jobs[idx];
        result += (job.completed ? "[x] " : "[ ] ");
        result += "P" + std::to_string(job.priority) + " ";
        result += job.description;
        if(!job.assignee.empty()){
            result += " (@" + job.assignee + ")";
        }
        result += "\n";
    }
    return result;
}

void PlanJobs::write(const std::string& s){
    jobs.clear();
    std::istringstream iss(s);
    std::string line;
    while(std::getline(iss, line)){
        auto trimmed = trim_copy(line);
        if(trimmed.empty()) continue;

        PlanJob job;
        job.completed = false;
        job.priority = 100;
        job.assignee = "";

        // Parse [x] or [ ]
        if(trimmed.size() >= 3 && trimmed[0] == '['){
            if(trimmed[1] == 'x' || trimmed[1] == 'X'){
                job.completed = true;
            }
            size_t close = trimmed.find(']');
            if(close != std::string::npos && close < trimmed.size() - 1){
                trimmed = trim_copy(trimmed.substr(close + 1));
            }
        }

        // Parse priority P<num>
        if(trimmed.size() >= 2 && trimmed[0] == 'P' && std::isdigit(static_cast<unsigned char>(trimmed[1]))){
            size_t end = 1;
            while(end < trimmed.size() && std::isdigit(static_cast<unsigned char>(trimmed[end]))){
                ++end;
            }
            job.priority = std::stoi(trimmed.substr(1, end - 1));
            trimmed = trim_copy(trimmed.substr(end));
        }

        // Parse assignee (@name)
        size_t at_pos = trimmed.find(" (@");
        if(at_pos != std::string::npos){
            size_t close_paren = trimmed.find(')', at_pos);
            if(close_paren != std::string::npos){
                job.assignee = trimmed.substr(at_pos + 3, close_paren - at_pos - 3);
                trimmed = trim_copy(trimmed.substr(0, at_pos));
            }
        }

        job.description = trimmed;
        if(!job.description.empty()){
            jobs.push_back(job);
        }
    }
}

void PlanJobs::addJob(const std::string& desc, int priority, const std::string& assignee){
    PlanJob job;
    job.description = desc;
    job.priority = priority;
    job.completed = false;
    job.assignee = assignee;
    jobs.push_back(job);
}

void PlanJobs::completeJob(size_t index){
    if(index < jobs.size()){
        jobs[index].completed = true;
    }
}

std::vector<size_t> PlanJobs::getSortedJobIndices() const {
    std::vector<size_t> indices(jobs.size());
    for(size_t i = 0; i < jobs.size(); ++i) indices[i] = i;
    std::sort(indices.begin(), indices.end(), [this](size_t a, size_t b){
        if(jobs[a].completed != jobs[b].completed) return !jobs[a].completed;  // Incomplete first
        if(jobs[a].priority != jobs[b].priority) return jobs[a].priority < jobs[b].priority;
        return a < b;
    });
    return indices;
}

std::string PlanDeps::read() const {
    std::string result;
    for(const auto& dep : dependencies){
        result += "- " + dep + "\n";
    }
    return result;
}

void PlanDeps::write(const std::string& s){
    dependencies.clear();
    std::istringstream iss(s);
    std::string line;
    while(std::getline(iss, line)){
        auto trimmed = trim_copy(line);
        if(trimmed.empty()) continue;
        if(trimmed.size() >= 2 && trimmed[0] == '-' && std::isspace(static_cast<unsigned char>(trimmed[1]))){
            dependencies.push_back(trim_copy(trimmed.substr(2)));
        } else {
            dependencies.push_back(trimmed);
        }
    }
}

std::string PlanImplemented::read() const {
    std::string result;
    for(const auto& item : items){
        result += "- " + item + "\n";
    }
    return result;
}

void PlanImplemented::write(const std::string& s){
    items.clear();
    std::istringstream iss(s);
    std::string line;
    while(std::getline(iss, line)){
        auto trimmed = trim_copy(line);
        if(trimmed.empty()) continue;
        if(trimmed.size() >= 2 && trimmed[0] == '-' && std::isspace(static_cast<unsigned char>(trimmed[1]))){
            items.push_back(trim_copy(trimmed.substr(2)));
        } else {
            items.push_back(trimmed);
        }
    }
}

std::string PlanResearch::read() const {
    std::string result;
    for(const auto& topic : topics){
        result += "- " + topic + "\n";
    }
    return result;
}

void PlanResearch::write(const std::string& s){
    topics.clear();
    std::istringstream iss(s);
    std::string line;
    while(std::getline(iss, line)){
        auto trimmed = trim_copy(line);
        if(trimmed.empty()) continue;
        if(trimmed.size() >= 2 && trimmed[0] == '-' && std::isspace(static_cast<unsigned char>(trimmed[1]))){
            topics.push_back(trim_copy(trimmed.substr(2)));
        } else {
            topics.push_back(trimmed);
        }
    }
}

// Planner Context
void PlannerContext::navigateTo(const std::string& path){
    if(!current_path.empty()){
        navigation_history.push_back(current_path);
    }
    current_path = path;
}

void PlannerContext::forward(){
    mode = Mode::Forward;
}

void PlannerContext::backward(){
    mode = Mode::Backward;
    if(!navigation_history.empty()){
        current_path = navigation_history.back();
        navigation_history.pop_back();
    }
}

void PlannerContext::addToContext(const std::string& vfs_path){
    visible_nodes.insert(vfs_path);
}

void PlannerContext::removeFromContext(const std::string& vfs_path){
    visible_nodes.erase(vfs_path);
}

void PlannerContext::clearContext(){
    visible_nodes.clear();
}

// ====== OpenAI helpers ======
static std::string system_prompt_text(){
    return std::string("You are a codex-like assistant embedded in a tiny single-binary IDE.\n") +
           snippets::tool_list() + "\nRespond concisely in Finnish.";
}
std::string json_escape(const std::string& s){
    std::string o; o.reserve(s.size()+8);
    for(char c: s){
        switch(c){
            case '"': o+="\\\""; break;
            case '\\': o+="\\\\"; break;
            case '\n': o+="\\n"; break;
            case '\r': o+="\\r"; break;
            case '\t': o+="\\t"; break;
            default: o+=c;
        }
    }
    return o;
}
std::string build_responses_payload(const std::string& model, const std::string& user_prompt){
    std::string sys = system_prompt_text();
    const char* content_type = "input_text";
    std::string js = std::string("{")+
        "\"model\":\""+json_escape(model)+"\","+
        "\"input\":[{\"role\":\"system\",\"content\":[{\"type\":\""+content_type+"\",\"text\":\""+json_escape(sys)+"\"}]},"+
        "{\"role\":\"user\",\"content\":[{\"type\":\""+content_type+"\",\"text\":\""+json_escape(user_prompt)+"\"}]}]}";
    return js;
}
static std::string build_chat_payload(const std::string& model, const std::string& system_prompt, const std::string& user_prompt){
    return std::string("{") +
        "\"model\":\"" + json_escape(model) + "\"," +
        "\"messages\":[" +
            "{\"role\":\"system\",\"content\":\"" + json_escape(system_prompt) + "\"}," +
            "{\"role\":\"user\",\"content\":\"" + json_escape(user_prompt) + "\"}" +
        "]," +
        "\"temperature\":0.0" +
    "}";
}
static int hex_value(char c){
    if(c>='0' && c<='9') return c - '0';
    if(c>='a' && c<='f') return 10 + (c - 'a');
    if(c>='A' && c<='F') return 10 + (c - 'A');
    return -1;
}
static void append_utf8(std::string& out, uint32_t codepoint){
    auto append_replacement = [&](){ out.append("\xEF\xBF\xBD"); };
    if(codepoint <= 0x7F){
        out.push_back(static_cast<char>(codepoint));
        return;
    }
    if(codepoint <= 0x7FF){
        out.push_back(static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        return;
    }
    if(codepoint <= 0xFFFF){
        if(codepoint >= 0xD800 && codepoint <= 0xDFFF){
            append_replacement();
            return;
        }
        out.push_back(static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        return;
    }
    if(codepoint <= 0x10FFFF){
        out.push_back(static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        return;
    }
    append_replacement();
}
static bool decode_unicode_escape_sequence(const std::string& raw, size_t u_pos, size_t& consumed, uint32_t& codepoint){
    if(u_pos >= raw.size()) return false;
    if(u_pos + 4 >= raw.size()) return false;
    uint32_t code = 0;
    for(int k = 0; k < 4; ++k){
        int v = hex_value(raw[u_pos + 1 + k]);
        if(v < 0) return false;
        code = (code << 4) | static_cast<uint32_t>(v);
    }

    size_t total_consumed = 5; // 'u' + 4 hex digits
    size_t last_digit_pos = u_pos + 4;

    if(code >= 0xD800 && code <= 0xDBFF){
        size_t next_slash = last_digit_pos + 1;
        if(next_slash + 5 < raw.size() && raw[next_slash] == '\\' && raw[next_slash + 1] == 'u'){
            uint32_t low = 0;
            for(int k = 0; k < 4; ++k){
                int v = hex_value(raw[next_slash + 2 + k]);
                if(v < 0) return false;
                low = (low << 4) | static_cast<uint32_t>(v);
            }
            if(low >= 0xDC00 && low <= 0xDFFF){
                code = 0x10000u + ((code - 0xD800u) << 10) + (low - 0xDC00u);
                total_consumed += 6; // '\' 'u' + 4 hex digits
            } else {
                code = 0xFFFD;
            }
        } else {
            code = 0xFFFD;
        }
    } else if(code >= 0xDC00 && code <= 0xDFFF){
        code = 0xFFFD;
    }

    consumed = total_consumed;
    codepoint = code;
    return true;
}
static std::optional<std::string> decode_json_string(const std::string& raw, size_t quote_pos){
    if(quote_pos>=raw.size() || raw[quote_pos] != '"') return std::nullopt;
    std::string out;
    bool escape=false;
    for(size_t i = quote_pos + 1; i < raw.size(); ++i){
        char c = raw[i];
        if(escape){
            switch(c){
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'v': out.push_back('\v'); break;
                case 'a': out.push_back('\a'); break;
                case '\\': out.push_back('\\'); break;
                case '"': out.push_back('"'); break;
                case 'u':{
                    size_t consumed = 0;
                    uint32_t codepoint = 0;
                    if(decode_unicode_escape_sequence(raw, i, consumed, codepoint)){
                        append_utf8(out, codepoint);
                        if(consumed > 0) i += consumed - 1;
                    } else {
                        out.push_back('\\');
                        out.push_back('u');
                    }
                    break;
                }
                default:
                    out.push_back(c);
                    break;
            }
            escape=false;
            continue;
        }
        if(c=='\\'){
            escape=true;
            continue;
        }
        if(c=='"') return out;
        out.push_back(c);
    }
    return std::nullopt;
}
static std::optional<std::string> json_string_value_after_colon(const std::string& raw, size_t colon_pos){
    if(colon_pos==std::string::npos) return std::nullopt;
    size_t value_pos = raw.find_first_not_of(" \t\r\n", colon_pos + 1);
    if(value_pos==std::string::npos || raw[value_pos] != '"') return std::nullopt;
    return decode_json_string(raw, value_pos);
}
static std::optional<std::string> find_json_string_field(const std::string& raw, const std::string& field, size_t startPos=0){
    std::string marker = std::string("\"") + field + "\"";
    size_t pos = raw.find(marker, startPos);
    if(pos==std::string::npos) return std::nullopt;
    size_t colon = raw.find(':', pos + marker.size());
    if(colon==std::string::npos) return std::nullopt;
    size_t quote = raw.find('"', colon+1);
    if(quote==std::string::npos) return std::nullopt;
    return decode_json_string(raw, quote);
}
static std::optional<std::string> openai_extract_output_text(const std::string& raw){
    size_t search_pos = 0;
    while(true){
        size_t type_pos = raw.find("\"type\"", search_pos);
        if(type_pos==std::string::npos) break;
        size_t colon = raw.find(':', type_pos);
        if(colon==std::string::npos) break;
        auto type_val = json_string_value_after_colon(raw, colon);
        if(type_val && *type_val == "output_text"){
            size_t text_pos = raw.find("\"text\"", colon);
            while(text_pos!=std::string::npos){
                size_t text_colon = raw.find(':', text_pos);
                if(text_colon==std::string::npos) break;
                if(auto text_val = json_string_value_after_colon(raw, text_colon)) return text_val;
                text_pos = raw.find("\"text\"", text_pos + 6);
            }
        }
        search_pos = colon + 1;
    }

    std::string legacy_marker = "\"output_text\"";
    if(size_t legacy_pos = raw.find(legacy_marker); legacy_pos!=std::string::npos){
        size_t colon = raw.find(':', legacy_pos);
        if(auto legacy_val = json_string_value_after_colon(raw, colon)) return legacy_val;
        if(colon!=std::string::npos){
            size_t quote = raw.find('"', colon);
            if(auto legacy_val = decode_json_string(raw, quote)) return legacy_val;
        }
    }
    return std::nullopt;
}
static std::string build_llama_completion_payload(const std::string& system_prompt, const std::string& user_prompt){
    std::string prompt = std::string("<|system|>\n") + system_prompt + "\n<|user|>\n" + user_prompt + "\n<|assistant|>";
    return std::string("{") +
        "\"prompt\":\"" + json_escape(prompt) + "\"," +
        "\"temperature\":0.0,\"stream\":false" +
    "}";
}
static std::optional<std::string> load_openai_key(){
    if(const char* envKey = std::getenv("OPENAI_API_KEY")){ if(*envKey) return std::string(envKey); }
    const char* home = std::getenv("HOME");
    if(!home || !*home) return std::nullopt;
    std::string path = std::string(home) + "/openai-key.txt";
    std::ifstream f(path, std::ios::binary);
    if(!f) return std::nullopt;
    std::string contents((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    while(!contents.empty() && (contents.back()=='\n' || contents.back()=='\r')) contents.pop_back();
    if(contents.empty()) return std::nullopt;
    return contents;
}

std::string call_openai(const std::string& prompt){
    auto keyOpt = load_openai_key();
    if(!keyOpt) return "error: OPENAI_API_KEY puuttuu ympäristöstä tai ~/openai-key.txt-tiedostosta";
    const std::string& key = *keyOpt;
    std::string base = std::getenv("OPENAI_BASE_URL") ? std::getenv("OPENAI_BASE_URL") : std::string("https://api.openai.com/v1");
    if(base.size() && base.back()=='/') base.pop_back();
    std::string model = std::getenv("OPENAI_MODEL") ? std::getenv("OPENAI_MODEL") : std::string("gpt-4o-mini");

    std::string payload = build_responses_payload(model, prompt);

    bool curl_ok = has_cmd("curl"); bool wget_ok = has_cmd("wget");
    if(!curl_ok && !wget_ok) return "error: curl tai wget ei löydy PATHista";

    std::string tmp = "/tmp/oai_req_" + std::to_string(::getpid()) + ".json";
    { std::ofstream f(tmp, std::ios::binary); if(!f) return "error: ei voi avata temp-tiedostoa"; f.write(payload.data(), (std::streamsize)payload.size()); }

    std::string cmd;
    if(curl_ok){
        cmd = std::string("curl -sS -X POST ")+base+"/responses -H 'Content-Type: application/json' -H 'Authorization: Bearer "+key+"' --data-binary @"+tmp;
    } else {
        cmd = std::string("wget -qO- --method=POST --header=Content-Type:application/json ")+
              std::string("--header=Authorization:'Bearer ")+key+"' "+base+"/responses --body-file="+tmp;
    }

    std::string raw = exec_capture(cmd, "ai:openai");
    std::remove(tmp.c_str());
    if(raw.empty()) return "error: tyhjä vastaus OpenAI:lta\n";

    // very crude output_text extraction
    if(auto text = openai_extract_output_text(raw)){
        return std::string("AI: ") + *text + "\n";
    }
    return raw + "\n";
}

std::string call_llama(const std::string& prompt){
    auto env_or_empty = [](const char* name) -> std::string {
        if(const char* val = std::getenv(name); val && *val) return std::string(val);
        return {};
    };
    std::string base = env_or_empty("LLAMA_BASE_URL");
    if(base.empty()) base = env_or_empty("LLAMA_SERVER");
    if(base.empty()) base = env_or_empty("LLAMA_URL");
    if(base.empty()) base = "http://192.168.1.169:8080";
    if(!base.empty() && base.back()=='/') base.pop_back();

    std::string model = env_or_empty("LLAMA_MODEL");
    if(model.empty()) model = "coder";

    bool curl_ok = has_cmd("curl"); bool wget_ok = has_cmd("wget");
    if(!curl_ok && !wget_ok) return "error: curl tai wget ei löydy PATHista";

    std::string system_prompt = system_prompt_text();

    static unsigned long long llama_req_counter = 0;
    auto send_request = [&](const std::string& endpoint, const std::string& payload) -> std::string {
        std::string tmp = "/tmp/llama_req_" + std::to_string(::getpid()) + "_" + std::to_string(++llama_req_counter) + ".json";
        {
            std::ofstream f(tmp, std::ios::binary);
            if(!f) return std::string();
            f.write(payload.data(), static_cast<std::streamsize>(payload.size()));
        }
        std::string url = base + endpoint;
        std::string cmd;
        if(curl_ok){
            cmd = std::string("curl -sS -X POST \"") + url + "\" -H \"Content-Type: application/json\" --data-binary @" + tmp;
        } else {
            cmd = std::string("wget -qO- --method=POST --header=Content-Type:application/json --body-file=") + tmp + " \"" + url + "\"";
        }
        std::string raw = exec_capture(cmd, std::string("ai:llama ")+endpoint);
        std::remove(tmp.c_str());
        return raw;
    };

    auto parse_chat_response = [&](const std::string& raw) -> std::optional<std::string> {
        if(raw.empty()) return std::nullopt;
        if(auto err = find_json_string_field(raw, "error")) return std::string("error: llama: ") + *err;
        size_t assistantPos = raw.find("\"role\":\"assistant\"");
        size_t searchPos = assistantPos==std::string::npos ? 0 : assistantPos;
        if(auto content = find_json_string_field(raw, "content", searchPos)) return std::string("AI: ") + *content;
        if(auto text = find_json_string_field(raw, "text", searchPos)) return std::string("AI: ") + *text;
        if(auto generic = find_json_string_field(raw, "result")) return std::string("AI: ") + *generic;
        return std::nullopt;
    };

    std::string chat_payload = build_chat_payload(model, system_prompt, prompt);
    std::string chat_raw = send_request("/v1/chat/completions", chat_payload);
    if(auto parsed = parse_chat_response(chat_raw)){
        if(parsed->rfind("error:", 0)==0) return *parsed + "\n";
        return *parsed + "\n";
    }

    std::string comp_payload = build_llama_completion_payload(system_prompt, prompt);
    std::string comp_raw = send_request("/completion", comp_payload);
    if(comp_raw.empty()){
        if(!chat_raw.empty()) return std::string("error: llama: unexpected response: ") + chat_raw + "\n";
        return "error: tyhjä vastaus llama-palvelimelta\n";
    }
    if(auto err = find_json_string_field(comp_raw, "error")) return std::string("error: llama: ") + *err + "\n";
    if(auto completion = find_json_string_field(comp_raw, "completion")) return std::string("AI: ") + *completion + "\n";
    size_t choicesPos = comp_raw.find("\"choices\"");
    if(auto text = find_json_string_field(comp_raw, "text", choicesPos==std::string::npos ? 0 : choicesPos))
        return std::string("AI: ") + *text + "\n";
    return std::string("error: llama: unexpected response: ") + comp_raw + "\n";
}

static bool env_truthy(const char* name){
    if(const char* val = std::getenv(name)) return *val != '\0';
    return false;
}

static std::string env_string(const char* name){
    if(const char* val = std::getenv(name); val && *val) return std::string(val);
    return {};
}

static std::string openai_cache_signature(){
    std::string base = env_string("OPENAI_BASE_URL");
    if(base.empty()) base = "https://api.openai.com/v1";
    if(!base.empty() && base.back()=='/') base.pop_back();
    std::string model = env_string("OPENAI_MODEL");
    if(model.empty()) model = "gpt-4o-mini";
    return std::string("openai|") + model + '|' + base;
}

static std::string llama_cache_signature(){
    std::string base = env_string("LLAMA_BASE_URL");
    if(base.empty()) base = env_string("LLAMA_SERVER");
    if(base.empty()) base = env_string("LLAMA_URL");
    if(base.empty()) base = "http://192.168.1.169:8080";
    if(!base.empty() && base.back()=='/') base.pop_back();
    std::string model = env_string("LLAMA_MODEL");
    if(model.empty()) model = "coder";
    return std::string("llama|") + model + '|' + base;
}

std::string call_ai(const std::string& prompt){
    auto dispatch_with_cache = [&](const std::string& providerLabel, const std::string& signature, auto&& fn) -> std::string {
        std::string key_material = make_cache_key_material(signature, prompt);
        if(auto cached = ai_cache_read(providerLabel, key_material)){
            return *cached;
        }
        std::string response = fn();
        ai_cache_write(providerLabel, key_material, prompt, response);
        return response;
    };

    auto use_llama = [&]() -> std::string {
        const std::string signature = llama_cache_signature();
        return dispatch_with_cache("llama", signature, [&](){ return call_llama(prompt); });
    };

    auto use_openai = [&]() -> std::string {
        const std::string signature = openai_cache_signature();
        return dispatch_with_cache("openai", signature, [&](){ return call_openai(prompt); });
    };

    std::string provider;
    if(const char* p = std::getenv("CODEX_AI_PROVIDER")) provider = p;
    std::transform(provider.begin(), provider.end(), provider.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });

    if(provider == "llama") return use_llama();
    if(provider == "openai") return use_openai();

    bool llama_hint = env_truthy("LLAMA_BASE_URL") || env_truthy("LLAMA_SERVER") || env_truthy("LLAMA_URL");
    auto keyOpt = load_openai_key();

    if(!keyOpt){
        return use_llama();
    }

    if(llama_hint) return use_llama();
    return use_openai();
}

// ====== REPL ======
static void help(){
    TRACE_FN();
    std::cout <<
R"(Commands:
  pwd
  cd [path]
  ls [path]
  tree [path]
  mkdir <path>
  touch <path>
  rm <path>
  mv <src> <dst>
  link <src> <dst>
  export <vfs> <host>
  cat [paths...] (tai stdin jos ei polkuja)
  grep [-i] <pattern> [path]
  rg [-i] <pattern> [path]
  head [-n N] [path]
  tail [-n N] [path]
  uniq [path]
  count [path]
  history [-a | -n N]
  random [min [max]]
  true / false
  echo <path> <data...>
  parse <src-file> <dst-ast>
  eval <ast-path>
  putkita komentoja: a | b | c, a && b, a || b
  # AI
  ai <prompt...>
  ai.brief <key> [extra...]
  tools
  overlay.list
  overlay.mount <name> <file>
  overlay.save <name> <file>
  overlay.unmount <name>
  overlay.policy [manual|oldest|newest]
  overlay.use <name>
  solution.save [file]
  # Filesystem mounts
  mount <host-path> <vfs-path>
  mount.lib <lib-path> <vfs-path>
  mount.remote <host> <port> <remote-vfs-path> <local-vfs-path>
  mount.list
  mount.allow
  mount.disallow
  unmount <vfs-path>
  # Tags (metadata for nodes)
  tag.add <vfs-path> <tag-name> [tag-name...]
  tag.remove <vfs-path> <tag-name> [tag-name...]
  tag.list [vfs-path]
  tag.clear <vfs-path>
  tag.has <vfs-path> <tag-name>
  # Planner (hierarchical planning system)
  plan.create <path> <type> [content]
  plan.goto <path>
  plan.forward
  plan.backward
  plan.context.add <vfs-path> [vfs-path...]
  plan.context.remove <vfs-path> [vfs-path...]
  plan.context.clear
  plan.context.list
  plan.status
  plan.jobs.add <jobs-path> <description> [priority] [assignee]
  plan.jobs.complete <jobs-path> <index>
  plan.save [file]
  # C++ builder
  cpp.tu <ast-path>
  cpp.include <tu-path> <header> [angled0/1]
  cpp.func <tu-path> <name> <ret>
  cpp.param <fn-path> <type> <name>
  cpp.print <scope-path> <text>
  cpp.vardecl <scope-path> <type> <name> [init]
  cpp.expr <scope-path> <expression>
  cpp.stmt <scope-path> <raw>
  cpp.return <scope-path> [expression]
  cpp.returni <scope-path> <int>
  cpp.rangefor <scope-path> <loop-name> <decl> | <range>
  cpp.dump <tu-path> <vfs-file-path>
Notes:
  - Polut voivat olla suhteellisia nykyiseen VFS-hakemistoon (cd).
  - ./codex <skripti> suorittaa komennot tiedostosta ilman REPL-kehotetta.
  - ./codex <skripti> - suorittaa skriptin ja palaa interaktiiviseen tilaan.
  - F3 tallentaa aktiivisen solutionin (sama kuin solution.save).
  - ai.brief lukee promptit snippets/-hakemistosta (CODEX_SNIPPET_DIR ylikirjoittaa polun).
  - OPENAI_API_KEY pakollinen 'ai' komentoon OpenAI-tilassa. OPENAI_MODEL (oletus gpt-4o-mini), OPENAI_BASE_URL (oletus https://api.openai.com/v1).
  - Llama-palvelin: LLAMA_BASE_URL / LLAMA_SERVER (oletus http://192.168.1.169:8080), LLAMA_MODEL (oletus coder), CODEX_AI_PROVIDER=llama pakottaa käyttöön.
)"<<std::endl;
}

// ====== Daemon Server Mode ======

static void run_daemon_server(int port, Vfs&, std::shared_ptr<Env>, WorkingDirectory&) {
    TRACE_FN("port=", port);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0){
        throw std::runtime_error("daemon: failed to create socket");
    }

    int opt = 1;
    if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0){
        close(server_fd);
        throw std::runtime_error("daemon: setsockopt failed");
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if(bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        close(server_fd);
        throw std::runtime_error("daemon: bind failed on port " + std::to_string(port));
    }

    if(listen(server_fd, 5) < 0){
        close(server_fd);
        throw std::runtime_error("daemon: listen failed");
    }

    std::cout << "daemon: listening on port " << port << "\n";
    std::cout << "daemon: ready to accept VFS remote mount connections\n";

    while(true){
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);

        if(client_fd < 0){
            std::cerr << "daemon: accept failed\n";
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        std::cout << "daemon: connection from " << client_ip << ":" << ntohs(client_addr.sin_port) << "\n";

        // Handle client connection in separate thread
        std::thread([client_fd](){
            auto handle_request = [](const std::string& request) -> std::string {
                try {
                    // Parse: EXEC <command>\n
                    if(request.substr(0, 5) != "EXEC "){
                        return "ERR invalid command format\n";
                    }

                    std::string command = request.substr(5);
                    if(!command.empty() && command.back() == '\n'){
                        command.pop_back();
                    }

                    // Execute command as shell command and capture output
                    FILE* pipe = popen(command.c_str(), "r");
                    if(!pipe){
                        return "ERR failed to execute command\n";
                    }

                    std::string output;
                    char buf[4096];
                    while(fgets(buf, sizeof(buf), pipe) != nullptr){
                        output += buf;
                    }

                    int status = pclose(pipe);
                    if(status != 0){
                        return "ERR command failed with status " + std::to_string(status) + "\n";
                    }

                    return "OK " + output + "\n";
                } catch(const std::exception& e){
                    return "ERR " + std::string(e.what()) + "\n";
                } catch(...){
                    return "ERR internal error\n";
                }
            };

            try {
                while(true){
                    char buf[4096];
                    ssize_t n = recv(client_fd, buf, sizeof(buf)-1, 0);
                    if(n <= 0) break;

                    buf[n] = '\0';
                    std::string request(buf);
                    std::string response = handle_request(request);

                    send(client_fd, response.c_str(), response.size(), 0);
                }
            } catch(...){
                // Connection closed or error
            }

            close(client_fd);
        }).detach();
    }

    close(server_fd);
}

int main(int argc, char** argv){
    TRACE_FN();
    using std::string; using std::shared_ptr;
    std::ios::sync_with_stdio(false); std::cin.tie(nullptr);
    i18n::init();
    snippets::initialize(argc > 0 ? argv[0] : nullptr);

    auto usage = [&](const std::string& msg){
        std::cerr << msg << "\n";
        return 1;
    };

    const string usage_text = string("usage: ") + argv[0] + " [--solution <pkg|asm>] [--daemon <port>] [script [-]]";

    std::string script_path;
    std::string solution_arg;
    bool fallback_after_script = false;
    int daemon_port = -1;

    auto looks_like_solution_hint = [](const std::string& arg){
        return is_solution_file(std::filesystem::path(arg));
    };

    for(int i = 1; i < argc; ++i){
        std::string arg = argv[i];
        if(arg == "--solution" || arg == "-S"){
            if(i + 1 >= argc) return usage("--solution requires a file path");
            solution_arg = argv[++i];
            continue;
        }
        if(arg == "--daemon" || arg == "-d"){
            if(i + 1 >= argc) return usage("--daemon requires a port number");
            daemon_port = std::stoi(argv[++i]);
            continue;
        }
        if(arg == "--script"){
            if(i + 1 >= argc) return usage("--script requires a file path");
            script_path = argv[++i];
            if(i + 1 < argc && std::string(argv[i + 1]) == "-"){
                fallback_after_script = true;
                ++i;
            }
            continue;
        }
        if(arg == "-"){
            if(script_path.empty()) return usage("'-' requires a preceding script path");
            fallback_after_script = true;
            continue;
        }
        if(solution_arg.empty() && looks_like_solution_hint(arg)){
            solution_arg = arg;
            continue;
        }
        if(script_path.empty()){
            script_path = arg;
            continue;
        }
        return usage(usage_text);
    }

    bool interactive = script_path.empty();
    bool script_active = !interactive;
    std::unique_ptr<std::ifstream> scriptStream;
    std::istream* input = &std::cin;

    if(!script_path.empty()){
        scriptStream = std::make_unique<std::ifstream>(script_path);
        if(!*scriptStream){
            std::cerr << "failed to open script '" << script_path << "'\n";
            return 1;
        }
        input = scriptStream.get();
    }

    Vfs vfs; auto env = std::make_shared<Env>(); install_builtins(env);
    vfs.mkdir("/src"); vfs.mkdir("/ast"); vfs.mkdir("/env"); vfs.mkdir("/astcpp"); vfs.mkdir("/cpp"); vfs.mkdir("/plan");
    WorkingDirectory cwd;
    update_directory_context(vfs, cwd, cwd.path);
    PlannerContext planner;
    planner.current_path = "/";

    // Auto-load .vfs file if present
    try{
        if(auto vfs_path = auto_detect_vfs_path()){
            auto abs_vfs_path = std::filesystem::absolute(*vfs_path);
            auto title = abs_vfs_path.parent_path().filename().string();
            if(title.empty()) title = "autoload";
            auto overlay_name = make_unique_overlay_name(vfs, title);
            mount_overlay_from_file(vfs, overlay_name, abs_vfs_path.string());
            std::cout << "auto-loaded " << abs_vfs_path.filename().string() << " as overlay '" << overlay_name << "'\n";
            maybe_extend_context(vfs, cwd);
        }
    } catch(const std::exception& e){
        std::cout << "note: auto-load .vfs failed: " << e.what() << "\n";
    }

    SolutionContext solution;
    std::optional<std::filesystem::path> solution_path_fs;
    try{
        if(!solution_arg.empty()){
            std::filesystem::path p = solution_arg;
            if(p.is_relative()) p = std::filesystem::absolute(p);
            solution_path_fs = p;
        } else if(auto auto_path = auto_detect_solution_path()){
            solution_path_fs = std::filesystem::absolute(*auto_path);
        }
    } catch(const std::exception& e){
        std::cout << "note: unable to resolve solution path: " << e.what() << "\n";
    }
    bool solution_loaded = false;
    if(solution_path_fs){
        if(!is_solution_file(*solution_path_fs)){
            std::cout << "note: '" << solution_path_fs->string() << "' does not use expected "
                      << kPackageExtension << " or " << kAssemblyExtension << " extension\n";
        }
        solution_loaded = load_solution_from_file(vfs, cwd, solution, *solution_path_fs, solution_arg.empty());
    }
    if(!solution_loaded){
        g_on_save_shortcut = nullptr;
    }

    // Auto-load plan.vfs if present (planner state persistence)
    try{
        std::filesystem::path plan_path = "plan.vfs";
        if(std::filesystem::exists(plan_path)){
            auto abs_plan_path = std::filesystem::absolute(plan_path);
            mount_overlay_from_file(vfs, "plan", abs_plan_path.string());
            std::cout << "auto-loaded plan.vfs into /plan tree\n";
            // Initialize planner to /plan if it exists
            if(auto plan_root = vfs.tryResolveForOverlay("/plan", 0)){
                if(plan_root->isDir()){
                    planner.current_path = "/plan";
                }
            }
        }
    } catch(const std::exception& e){
        std::cout << "note: auto-load plan.vfs failed: " << e.what() << "\n";
    }

    std::cout << _(WELCOME) << "\n";
    string line;
    // Daemon mode: run server and exit
    if(daemon_port > 0){
        try {
            run_daemon_server(daemon_port, vfs, env, cwd);
        } catch(const std::exception& e){
            std::cerr << "daemon error: " << e.what() << "\n";
            return 1;
        }
        return 0;
    }

    size_t repl_iter = 0;
    std::vector<std::string> history;
    load_history(history);
    history.reserve(history.size() + 256);
    bool history_dirty = false;

    auto execute_single = [&](const CommandInvocation& inv, const std::string& stdin_data) -> CommandResult {
        ScopedCoutCapture capture;
        CommandResult result;
        const std::string& cmd = inv.name;

        auto read_path = [&](const std::string& operand) -> std::string {
            std::string abs = normalize_path(cwd.path, operand);
            if(auto node = vfs.tryResolveForOverlay(abs, cwd.primary_overlay)){
                if(node->kind == VfsNode::Kind::Dir)
                    throw std::runtime_error("cannot read directory: " + operand);
                return node->read();
            }
            auto hits = vfs.resolveMulti(abs);
            if(hits.empty()) throw std::runtime_error("path not found: " + operand);
            std::vector<size_t> overlays;
            for(const auto& hit : hits){
                if(hit.node->kind != VfsNode::Kind::Dir)
                    overlays.push_back(hit.overlay_id);
            }
            if(overlays.empty()) throw std::runtime_error("cannot read directory: " + operand);
            sort_unique(overlays);
            size_t chosen = select_overlay(vfs, cwd, overlays);
            auto node = vfs.resolveForOverlay(abs, chosen);
            if(node->kind == VfsNode::Kind::Dir) throw std::runtime_error("cannot read directory: " + operand);
            return node->read();
        };

        if(cmd == "pwd"){
            std::ostringstream oss;
            oss << cwd.path << overlay_suffix(vfs, cwd.overlays, cwd.primary_overlay) << "\n";
            result.output = oss.str();

        } else if(cmd == "cd"){
            std::string target = inv.args.empty() ? std::string("/") : inv.args[0];
            std::string abs = normalize_path(cwd.path, target);
            auto dirOverlays = vfs.overlaysForPath(abs);
            if(dirOverlays.empty()){
                auto hits = vfs.resolveMulti(abs);
                if(hits.empty()) throw std::runtime_error("cd: no such path");
                throw std::runtime_error("cd: not a directory");
            }
            update_directory_context(vfs, cwd, abs);

        } else if(cmd == "ls"){
            std::string abs = inv.args.empty() ? cwd.path : normalize_path(cwd.path, inv.args[0]);
            auto hits = vfs.resolveMulti(abs);
            if(hits.empty()) throw std::runtime_error("ls: path not found");

            bool anyDir = false;
            std::vector<size_t> listingOverlays;
            for(const auto& hit : hits){
                if(hit.node->isDir()){
                    anyDir = true;
                    listingOverlays.push_back(hit.overlay_id);
                } else {
                    listingOverlays.push_back(hit.overlay_id);
                }
            }
            sort_unique(listingOverlays);

            if(anyDir){
                auto listing = vfs.listDir(abs, listingOverlays);
                for(const auto& [name, entry] : listing){
                    std::vector<size_t> ids = entry.overlays;
                    sort_unique(ids);
                    char type = entry.types.size() == 1 ? *entry.types.begin() : '!';
                    std::cout << type << " " << name;
                    if(ids.size() > 1 || (ids.size() == 1 && ids[0] != cwd.primary_overlay)){
                        std::cout << overlay_suffix(vfs, ids, cwd.primary_overlay);
                    }
                    std::cout << "\n";
                }
            } else {
                size_t fileCount = 0;
                std::shared_ptr<VfsNode> node;
                std::vector<size_t> ids;
                for(const auto& hit : hits){
                    if(hit.node->kind != VfsNode::Kind::Dir){
                        ++fileCount;
                        node = hit.node;
                        ids.push_back(hit.overlay_id);
                    }
                }
                if(!node) throw std::runtime_error("ls: unsupported node type");
                sort_unique(ids);
                char type = fileCount > 1 ? '!' : type_char(node);
                std::cout << type << " " << path_basename(abs);
                if(ids.size() > 1 || (ids.size() == 1 && ids[0] != cwd.primary_overlay)){
                    std::cout << overlay_suffix(vfs, ids, cwd.primary_overlay);
                }
                std::cout << "\n";
            }

        } else if(cmd == "tree"){
            std::string abs = inv.args.empty() ? cwd.path : normalize_path(cwd.path, inv.args[0]);
            auto hits = vfs.resolveMulti(abs);
            if(hits.empty()) throw std::runtime_error("tree: path not found");
            std::vector<size_t> ids;
            for(const auto& hit : hits){
                if(hit.node->isDir()) ids.push_back(hit.overlay_id);
            }
            if(ids.empty()) throw std::runtime_error("tree: not a directory");
            sort_unique(ids);

            std::function<void(const std::string&, const std::string&, const std::vector<size_t>&)> dump;
            dump = [&](const std::string& path, const std::string& prefix, const std::vector<size_t>& overlays){
                auto currentHits = vfs.resolveMulti(path, overlays);
                char type = 'd';
                if(!currentHits.empty()){
                    std::set<char> types;
                    for(const auto& h : currentHits) types.insert(type_char(h.node));
                    type = types.size()==1 ? *types.begin() : '!';
                }
                std::cout << prefix << type << " " << path_basename(path) << overlay_suffix(vfs, overlays, cwd.primary_overlay) << "\n";
                auto listing = vfs.listDir(path, overlays);
                for(const auto& [name, entry] : listing){
                    std::string childPath = path == "/" ? join_path(path, name) : join_path(path, name);
                    std::vector<size_t> childIds = entry.overlays;
                    sort_unique(childIds);
                    dump(childPath, prefix + "  ", childIds);
                }
            };

            dump(abs, "", ids);

        } else if(cmd == "mkdir"){
            if(inv.args.empty()) throw std::runtime_error("mkdir <path>");
            std::string abs = normalize_path(cwd.path, inv.args[0]);
            vfs.mkdir(abs, cwd.primary_overlay);

        } else if(cmd == "touch"){
            if(inv.args.empty()) throw std::runtime_error("touch <path>");
            std::string abs = normalize_path(cwd.path, inv.args[0]);
            vfs.touch(abs, cwd.primary_overlay);

        } else if(cmd == "cat"){
            if(inv.args.empty()){
                result.output = stdin_data;
            } else {
                std::ostringstream oss;
                for(size_t i = 0; i < inv.args.size(); ++i){
                    std::string data = read_path(inv.args[i]);
                    oss << data;
                    if(data.empty() || data.back() != '\n') oss << '\n';
                }
                result.output = oss.str();
            }

        } else if(cmd == "grep"){
            if(inv.args.empty()) throw std::runtime_error("grep [-i] <pattern> [path]");
            size_t idx = 0;
            bool ignore_case = false;
            if(inv.args[idx] == "-i"){
                ignore_case = true;
                ++idx;
                if(idx >= inv.args.size()) throw std::runtime_error("grep [-i] <pattern> [path]");
            }
            std::string pattern = inv.args[idx++];
            std::string data = idx < inv.args.size() ? read_path(inv.args[idx]) : stdin_data;
            auto lines = split_lines(data);
            std::ostringstream oss;
            bool matched = false;
            std::string needle = pattern;
            if(ignore_case){
                std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
            }
            for(size_t i = 0; i < lines.lines.size(); ++i){
                std::string hay = lines.lines[i];
                if(ignore_case){
                    std::transform(hay.begin(), hay.end(), hay.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
                }
                if(hay.find(needle) != std::string::npos){
                    matched = true;
                    oss << lines.lines[i];
                    bool had_newline = (i < lines.lines.size() - 1) || lines.trailing_newline;
                    if(had_newline) oss << '\n';
                }
            }
            result.output = oss.str();
            result.success = matched;

        } else if(cmd == "rg"){
            if(inv.args.empty()) throw std::runtime_error("rg [-i] <pattern> [path]");
            size_t idx = 0;
            bool ignore_case = false;
            if(inv.args[idx] == "-i"){
                ignore_case = true;
                ++idx;
                if(idx >= inv.args.size()) throw std::runtime_error("rg [-i] <pattern> [path]");
            }
            std::string pattern = inv.args[idx++];
            std::regex_constants::syntax_option_type flags = std::regex_constants::ECMAScript;
            if(ignore_case) flags = static_cast<std::regex_constants::syntax_option_type>(flags | std::regex_constants::icase);
            std::regex re;
            try{
                re = std::regex(pattern, flags);
            } catch(const std::regex_error& e){
                throw std::runtime_error(std::string("rg regex error: ") + e.what());
            }
            std::string data = idx < inv.args.size() ? read_path(inv.args[idx]) : stdin_data;
            auto lines = split_lines(data);
            std::ostringstream oss;
            bool matched = false;
            for(size_t i = 0; i < lines.lines.size(); ++i){
                if(std::regex_search(lines.lines[i], re)){
                    matched = true;
                    oss << lines.lines[i];
                    bool had_newline = (i < lines.lines.size() - 1) || lines.trailing_newline;
                    if(had_newline) oss << '\n';
                }
            }
            result.output = oss.str();
            result.success = matched;

        } else if(cmd == "count"){
            std::string data = inv.args.empty() ? stdin_data : read_path(inv.args[0]);
            size_t lines = count_lines(data);
            result.output = std::to_string(lines) + "\n";

        } else if(cmd == "history"){
            bool show_all = false;
            size_t requested = 10;
            size_t idx = 0;
            while(idx < inv.args.size()){
                const std::string& opt = inv.args[idx];
                if(opt == "-a"){
                    show_all = true;
                    ++idx;
                } else if(opt == "-n"){
                    if(idx + 1 >= inv.args.size()) throw std::runtime_error("history -n <count>");
                    requested = parse_size_arg(inv.args[idx + 1], "history count");
                    show_all = false;
                    idx += 2;
                } else {
                    throw std::runtime_error("history [-a | -n <count>]");
                }
            }

            size_t total = history.size();
            size_t start = 0;
            if(!show_all){
                if(requested < total){
                    start = total - requested;
                }
            }
            for(size_t i = start; i < total; ++i){
                std::cout << (i + 1) << "  " << history[i] << "\n";
            }

        } else if(cmd == "true"){
            result.success = true;

        } else if(cmd == "false"){
            result.success = false;

        } else if(cmd == "tail"){
            size_t idx = 0;
            size_t take = 10;
            auto is_number = [](const std::string& s){
                return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c){ return std::isdigit(c); });
            };
            if(idx < inv.args.size()){
                if(inv.args[idx] == "-n"){
                    if(idx + 1 >= inv.args.size()) throw std::runtime_error("tail -n <count> [path]");
                    take = parse_size_arg(inv.args[idx + 1], "tail count");
                    idx += 2;
                } else if(inv.args.size() - idx > 1 && is_number(inv.args[idx])){
                    take = parse_size_arg(inv.args[idx], "tail count");
                    ++idx;
                }
            }
            std::string data = idx < inv.args.size() ? read_path(inv.args[idx]) : stdin_data;
            auto lines = split_lines(data);
            size_t total = lines.lines.size();
            size_t begin = take >= total ? 0 : total - take;
            result.output = join_line_range(lines, begin, total);

        } else if(cmd == "head"){
            size_t idx = 0;
            size_t take = 10;
            auto is_number = [](const std::string& s){
                return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c){ return std::isdigit(c); });
            };
            if(idx < inv.args.size()){
                if(inv.args[idx] == "-n"){
                    if(idx + 1 >= inv.args.size()) throw std::runtime_error("head -n <count> [path]");
                    take = parse_size_arg(inv.args[idx + 1], "head count");
                    idx += 2;
                } else if(inv.args.size() - idx > 1 && is_number(inv.args[idx])){
                    take = parse_size_arg(inv.args[idx], "head count");
                    ++idx;
                }
            }
            std::string data = idx < inv.args.size() ? read_path(inv.args[idx]) : stdin_data;
            auto lines = split_lines(data);
            size_t end = std::min(take, lines.lines.size());
            result.output = join_line_range(lines, 0, end);

        } else if(cmd == "uniq"){
            std::string data = inv.args.empty() ? stdin_data : read_path(inv.args[0]);
            auto lines = split_lines(data);
            std::ostringstream oss;
            std::string prev;
            bool have_prev = false;
            for(size_t i = 0; i < lines.lines.size(); ++i){
                const auto& line = lines.lines[i];
                if(!have_prev || line != prev){
                    oss << line;
                    bool had_newline = (i < lines.lines.size() - 1) || lines.trailing_newline;
                    if(had_newline) oss << '\n';
                    prev = line;
                    have_prev = true;
                }
            }
            result.output = oss.str();

        } else if(cmd == "random"){
            long long lo = 0;
            long long hi = 1000000;
            if(inv.args.size() > 2) throw std::runtime_error("random [min [max]]");
            if(inv.args.size() == 1){
                hi = parse_int_arg(inv.args[0], "random max");
            } else if(inv.args.size() >= 2){
                lo = parse_int_arg(inv.args[0], "random min");
                hi = parse_int_arg(inv.args[1], "random max");
            }
            if(lo > hi) throw std::runtime_error("random range invalid (min > max)");
            std::uniform_int_distribution<long long> dist(lo, hi);
            long long value = dist(rng());
            result.output = std::to_string(value) + "\n";

        } else if(cmd == "echo"){
            // Bourne shell compatible echo - prints to stdout
            std::string text = join_args(inv.args, 0);
            result.output = text + "\n";

        } else if(cmd == "rm"){
            if(inv.args.empty()) throw std::runtime_error("rm <path>");
            std::string abs = normalize_path(cwd.path, inv.args[0]);
            vfs.rm(abs, cwd.primary_overlay);

        } else if(cmd == "mv"){
            if(inv.args.size() < 2) throw std::runtime_error("mv <src> <dst>");
            std::string absSrc = normalize_path(cwd.path, inv.args[0]);
            std::string absDst = normalize_path(cwd.path, inv.args[1]);
            vfs.mv(absSrc, absDst, cwd.primary_overlay);

        } else if(cmd == "link"){
            if(inv.args.size() < 2) throw std::runtime_error("link <src> <dst>");
            std::string absSrc = normalize_path(cwd.path, inv.args[0]);
            std::string absDst = normalize_path(cwd.path, inv.args[1]);
            vfs.link(absSrc, absDst, cwd.primary_overlay);

        } else if(cmd == "export"){
            if(inv.args.size() < 2) throw std::runtime_error("export <vfs> <host>");
            std::string data = read_path(inv.args[0]);
            std::ofstream out(inv.args[1], std::ios::binary);
            if(!out) throw std::runtime_error("export: cannot open host file");
            out.write(data.data(), static_cast<std::streamsize>(data.size()));
            std::cout << "export -> " << inv.args[1] << "\n";

        } else if(cmd == "parse"){
            if(inv.args.size() < 2) throw std::runtime_error("parse <src> <dst>");
            std::string absDst = normalize_path(cwd.path, inv.args[1]);
            auto text = read_path(inv.args[0]);
            auto ast = parse(text);
            auto holder = std::make_shared<AstHolder>(path_basename(absDst), ast);
            std::string dir = absDst.substr(0, absDst.find_last_of('/'));
            if(dir.empty()) dir = "/";
            vfs.addNode(dir, holder, cwd.primary_overlay);
            std::cout << "AST @ " << absDst << " valmis.\n";

        } else if(cmd == "eval"){
            if(inv.args.empty()) throw std::runtime_error("eval <path>");
            std::string abs = normalize_path(cwd.path, inv.args[0]);
            std::shared_ptr<VfsNode> n;
            try{
                n = vfs.resolveForOverlay(abs, cwd.primary_overlay);
            } catch(const std::exception&){
                auto hits = vfs.resolveMulti(abs);
                if(hits.empty()) throw;
                std::vector<size_t> overlays;
                for(const auto& h : hits){
                    if(h.node->kind == VfsNode::Kind::Ast || h.node->kind == VfsNode::Kind::File)
                        overlays.push_back(h.overlay_id);
                }
                sort_unique(overlays);
                size_t chosen = select_overlay(vfs, cwd, overlays);
                n = vfs.resolveForOverlay(abs, chosen);
            }
            if(n->kind != VfsNode::Kind::Ast) throw std::runtime_error("not AST");
            auto a = std::dynamic_pointer_cast<AstNode>(n);
            auto val = a->eval(env);
            std::cout << val.show() << "\n";

        } else if(cmd == "ai"){
            std::string prompt = join_args(inv.args);
            if(prompt.empty()){
                std::cout << "anna promptti.\n";
                result.success = false;
            } else {
                result.output = call_ai(prompt);
            }

        } else if(cmd == "ai.brief"){
            if(inv.args.empty()) throw std::runtime_error("ai.brief <key> [extra...]");
            auto key = inv.args[0];
            std::optional<std::string> prompt;
            if(key == "ai-bridge-hello" || key == "bridge.hello" || key == "bridge-hello"){
                prompt = snippets::ai_bridge_hello_briefing();
            }

            if(!prompt || prompt->empty()){
                std::cout << "unknown briefing key\n";
                result.success = false;
            } else {
                if(inv.args.size() > 1){
                    auto extra = join_args(inv.args, 1);
                    if(!extra.empty()){
                        if(!prompt->empty() && prompt->back() != '\n') prompt->push_back(' ');
                        prompt->append(extra);
                    }
                }
                result.output = call_ai(*prompt);
            }

        } else if(cmd == "tools"){
            auto tools = snippets::tool_list();
            std::cout << tools;
            if(tools.empty() || tools.back() != '\n') std::cout << '\n';

        } else if(cmd == "overlay.list"){
            for(size_t i = 0; i < vfs.overlayCount(); ++i){
                bool in_scope = std::find(cwd.overlays.begin(), cwd.overlays.end(), i) != cwd.overlays.end();
                bool primary = (i == cwd.primary_overlay);
                std::cout << (primary ? '*' : ' ') << (in_scope ? '+' : ' ') << " [" << i << "] " << vfs.overlayName(i) << "\n";
            }
            std::cout << "policy: " << policy_label(cwd.conflict_policy) << "\n";

        } else if(cmd == "overlay.use"){
            if(inv.args.empty()) throw std::runtime_error("overlay.use <name>");
            auto name = inv.args[0];
            auto idOpt = vfs.findOverlayByName(name);
            if(!idOpt) throw std::runtime_error("overlay: unknown overlay");
            size_t id = *idOpt;
            if(std::find(cwd.overlays.begin(), cwd.overlays.end(), id) == cwd.overlays.end())
                throw std::runtime_error("overlay not active in current directory");
            cwd.primary_overlay = id;

        } else if(cmd == "overlay.policy"){
            if(inv.args.empty()){
                std::cout << "overlay policy: " << policy_label(cwd.conflict_policy) << " (manual|oldest|newest)\n";
            } else {
                auto parsed = parse_policy(inv.args[0]);
                if(!parsed) throw std::runtime_error("overlay.policy manual|oldest|newest");
                cwd.conflict_policy = *parsed;
                update_directory_context(vfs, cwd, cwd.path);
                std::cout << "overlay policy set to " << policy_label(cwd.conflict_policy) << "\n";
            }

        } else if(cmd == "overlay.mount"){
            if(inv.args.size() < 2) throw std::runtime_error("overlay.mount <name> <file>");
            size_t id = mount_overlay_from_file(vfs, inv.args[0], inv.args[1]);
            maybe_extend_context(vfs, cwd);
            std::cout << "mounted overlay " << inv.args[0] << " (#" << id << ")\n";

        } else if(cmd == "overlay.save"){
            if(inv.args.size() < 2) throw std::runtime_error("overlay.save <name> <file>");
            auto idOpt = vfs.findOverlayByName(inv.args[0]);
            if(!idOpt) throw std::runtime_error("overlay: unknown overlay");
            save_overlay_to_file(vfs, *idOpt, inv.args[1]);
            if(solution.active && *idOpt == solution.overlay_id){
                std::filesystem::path p = inv.args[1];
                try{
                    if(p.is_relative()) p = std::filesystem::absolute(p);
                } catch(const std::exception&){ }
                solution.file_path = p.string();
            }
            std::cout << "overlay " << inv.args[0] << " (#" << *idOpt << ") -> " << inv.args[1] << "\n";

        } else if(cmd == "overlay.unmount"){
            if(inv.args.empty()) throw std::runtime_error("overlay.unmount <name>");
            auto idOpt = vfs.findOverlayByName(inv.args[0]);
            if(!idOpt) throw std::runtime_error("overlay: unknown overlay");
            if(*idOpt == 0) throw std::runtime_error("cannot unmount base overlay");
            vfs.unregisterOverlay(*idOpt);
            adjust_context_after_unmount(vfs, cwd, *idOpt);

        } else if(cmd == "mount"){
            if(inv.args.size() < 2) throw std::runtime_error("mount <host-path> <vfs-path>");
            std::string host_path = inv.args[0];
            std::string vfs_path = normalize_path(cwd.path, inv.args[1]);
            vfs.mountFilesystem(host_path, vfs_path, cwd.primary_overlay);
            std::cout << "mounted " << host_path << " -> " << vfs_path << "\n";

        } else if(cmd == "mount.lib"){
            if(inv.args.size() < 2) throw std::runtime_error("mount.lib <lib-path> <vfs-path>");
            std::string lib_path = inv.args[0];
            std::string vfs_path = normalize_path(cwd.path, inv.args[1]);
            vfs.mountLibrary(lib_path, vfs_path, cwd.primary_overlay);
            std::cout << "mounted library " << lib_path << " -> " << vfs_path << "\n";

        } else if(cmd == "mount.remote"){
            if(inv.args.size() < 4) throw std::runtime_error("mount.remote <host> <port> <remote-vfs-path> <local-vfs-path>");
            std::string host = inv.args[0];
            int port = std::stoi(inv.args[1]);
            std::string remote_path = inv.args[2];
            std::string vfs_path = normalize_path(cwd.path, inv.args[3]);
            vfs.mountRemote(host, port, remote_path, vfs_path, cwd.primary_overlay);
            std::cout << "mounted remote " << host << ":" << port << ":" << remote_path << " -> " << vfs_path << "\n";

        } else if(cmd == "mount.list"){
            auto mounts = vfs.listMounts();
            if(mounts.empty()){
                std::cout << "no mounts\n";
            } else {
                for(const auto& m : mounts){
                    std::string type_marker;
                    switch(m.type){
                        case Vfs::MountType::Filesystem: type_marker = "m "; break;
                        case Vfs::MountType::Library: type_marker = "l "; break;
                        case Vfs::MountType::Remote: type_marker = "r "; break;
                    }
                    std::cout << type_marker << m.vfs_path << " <- " << m.host_path << "\n";
                }
            }
            std::cout << "mounting " << (vfs.isMountAllowed() ? "allowed" : "disabled") << "\n";

        } else if(cmd == "mount.allow"){
            vfs.setMountAllowed(true);
            std::cout << "mounting enabled\n";

        } else if(cmd == "mount.disallow"){
            vfs.setMountAllowed(false);
            std::cout << "mounting disabled (existing mounts remain active)\n";

        } else if(cmd == "unmount"){
            if(inv.args.empty()) throw std::runtime_error("unmount <vfs-path>");
            std::string vfs_path = normalize_path(cwd.path, inv.args[0]);
            vfs.unmount(vfs_path);
            std::cout << "unmounted " << vfs_path << "\n";

        } else if(cmd == "tag.add"){
            if(inv.args.size() < 2) throw std::runtime_error("tag.add <vfs-path> <tag-name> [tag-name...]");
            std::string vfs_path = normalize_path(cwd.path, inv.args[0]);
            for(size_t i = 1; i < inv.args.size(); ++i){
                vfs.addTag(vfs_path, inv.args[i]);
            }
            std::cout << "tagged " << vfs_path << " with " << (inv.args.size() - 1) << " tag(s)\n";

        } else if(cmd == "tag.remove"){
            if(inv.args.size() < 2) throw std::runtime_error("tag.remove <vfs-path> <tag-name> [tag-name...]");
            std::string vfs_path = normalize_path(cwd.path, inv.args[0]);
            for(size_t i = 1; i < inv.args.size(); ++i){
                vfs.removeTag(vfs_path, inv.args[i]);
            }
            std::cout << "removed " << (inv.args.size() - 1) << " tag(s) from " << vfs_path << "\n";

        } else if(cmd == "tag.list"){
            if(inv.args.empty()){
                // List all registered tags
                auto tags = vfs.allRegisteredTags();
                if(tags.empty()){
                    std::cout << "no tags registered\n";
                } else {
                    std::cout << "registered tags (" << tags.size() << "):\n";
                    for(const auto& tag : tags){
                        std::cout << "  " << tag << "\n";
                    }
                }
            } else {
                // List tags for specific path
                std::string vfs_path = normalize_path(cwd.path, inv.args[0]);
                auto tags = vfs.getNodeTags(vfs_path);
                if(tags.empty()){
                    std::cout << vfs_path << ": no tags\n";
                } else {
                    std::cout << vfs_path << ": ";
                    for(size_t i = 0; i < tags.size(); ++i){
                        if(i > 0) std::cout << ", ";
                        std::cout << tags[i];
                    }
                    std::cout << "\n";
                }
            }

        } else if(cmd == "tag.clear"){
            if(inv.args.empty()) throw std::runtime_error("tag.clear <vfs-path>");
            std::string vfs_path = normalize_path(cwd.path, inv.args[0]);
            vfs.clearNodeTags(vfs_path);
            std::cout << "cleared all tags from " << vfs_path << "\n";

        } else if(cmd == "tag.has"){
            if(inv.args.size() < 2) throw std::runtime_error("tag.has <vfs-path> <tag-name>");
            std::string vfs_path = normalize_path(cwd.path, inv.args[0]);
            bool has = vfs.nodeHasTag(vfs_path, inv.args[1]);
            std::cout << vfs_path << (has ? " has " : " does not have ") << "tag '" << inv.args[1] << "'\n";

        } else if(cmd == "plan.create"){
            if(inv.args.size() < 2) throw std::runtime_error("plan.create <path> <type> [content]");
            std::string vfs_path = normalize_path(cwd.path, inv.args[0]);
            std::string type = inv.args[1];
            std::string content;
            if(inv.args.size() > 2){
                for(size_t i = 2; i < inv.args.size(); ++i){
                    if(i > 2) content += " ";
                    content += inv.args[i];
                }
            }

            std::shared_ptr<PlanNode> node;
            if(type == "root"){
                node = std::make_shared<PlanRoot>(path_basename(vfs_path), content);
            } else if(type == "subplan"){
                node = std::make_shared<PlanSubPlan>(path_basename(vfs_path), content);
            } else if(type == "goals"){
                node = std::make_shared<PlanGoals>(path_basename(vfs_path));
            } else if(type == "ideas"){
                node = std::make_shared<PlanIdeas>(path_basename(vfs_path));
            } else if(type == "strategy"){
                node = std::make_shared<PlanStrategy>(path_basename(vfs_path), content);
            } else if(type == "jobs"){
                node = std::make_shared<PlanJobs>(path_basename(vfs_path));
            } else if(type == "deps"){
                node = std::make_shared<PlanDeps>(path_basename(vfs_path));
            } else if(type == "implemented"){
                node = std::make_shared<PlanImplemented>(path_basename(vfs_path));
            } else if(type == "research"){
                node = std::make_shared<PlanResearch>(path_basename(vfs_path));
            } else if(type == "notes"){
                node = std::make_shared<PlanNotes>(path_basename(vfs_path), content);
            } else {
                throw std::runtime_error("plan.create: unknown type '" + type + "' (valid: root, subplan, goals, ideas, strategy, jobs, deps, implemented, research, notes)");
            }

            vfs_add(vfs, vfs_path, node, cwd.primary_overlay);
            std::cout << "created plan node (" << type << ") @ " << vfs_path << "\n";

        } else if(cmd == "plan.goto"){
            if(inv.args.empty()) throw std::runtime_error("plan.goto <path>");
            std::string vfs_path = normalize_path(cwd.path, inv.args[0]);
            auto node = vfs.tryResolveForOverlay(vfs_path, cwd.primary_overlay);
            if(!node) throw std::runtime_error("plan.goto: path not found: " + vfs_path);
            planner.navigateTo(vfs_path);
            std::cout << "planner now at: " << planner.current_path << "\n";

        } else if(cmd == "plan.forward"){
            planner.forward();
            std::cout << "planner moved forward (towards details)\n";
            std::cout << "mode: " << (planner.mode == PlannerContext::Mode::Forward ? "forward" : "backward") << "\n";

        } else if(cmd == "plan.backward"){
            planner.backward();
            std::cout << "planner moved backward (towards high-level)\n";
            std::cout << "mode: " << (planner.mode == PlannerContext::Mode::Forward ? "forward" : "backward") << "\n";

        } else if(cmd == "plan.context.add"){
            if(inv.args.empty()) throw std::runtime_error("plan.context.add <vfs-path> [vfs-path...]");
            for(const auto& arg : inv.args){
                std::string vfs_path = normalize_path(cwd.path, arg);
                planner.addToContext(vfs_path);
            }
            std::cout << "added " << inv.args.size() << " path(s) to planner context\n";

        } else if(cmd == "plan.context.remove"){
            if(inv.args.empty()) throw std::runtime_error("plan.context.remove <vfs-path> [vfs-path...]");
            for(const auto& arg : inv.args){
                std::string vfs_path = normalize_path(cwd.path, arg);
                planner.removeFromContext(vfs_path);
            }
            std::cout << "removed " << inv.args.size() << " path(s) from planner context\n";

        } else if(cmd == "plan.context.clear"){
            planner.clearContext();
            std::cout << "cleared planner context\n";

        } else if(cmd == "plan.context.list"){
            if(planner.visible_nodes.empty()){
                std::cout << "planner context is empty\n";
            } else {
                std::cout << "planner context (" << planner.visible_nodes.size() << " paths):\n";
                for(const auto& path : planner.visible_nodes){
                    std::cout << "  " << path << "\n";
                }
            }

        } else if(cmd == "plan.status"){
            std::cout << "planner status:\n";
            std::cout << "  current: " << planner.current_path << "\n";
            std::cout << "  mode: " << (planner.mode == PlannerContext::Mode::Forward ? "forward" : "backward") << "\n";
            std::cout << "  context size: " << planner.visible_nodes.size() << "\n";
            std::cout << "  history depth: " << planner.navigation_history.size() << "\n";

        } else if(cmd == "plan.jobs.add"){
            if(inv.args.size() < 2) throw std::runtime_error("plan.jobs.add <jobs-path> <description> [priority] [assignee]");
            std::string vfs_path = normalize_path(cwd.path, inv.args[0]);
            auto node = vfs.tryResolveForOverlay(vfs_path, cwd.primary_overlay);
            if(!node) throw std::runtime_error("plan.jobs.add: path not found: " + vfs_path);
            auto jobs_node = std::dynamic_pointer_cast<PlanJobs>(node);
            if(!jobs_node) throw std::runtime_error("plan.jobs.add: not a jobs node: " + vfs_path);

            std::string description = inv.args[1];
            int priority = 100;
            std::string assignee = "";
            if(inv.args.size() > 2){
                priority = std::stoi(inv.args[2]);
            }
            if(inv.args.size() > 3){
                assignee = inv.args[3];
            }

            jobs_node->addJob(description, priority, assignee);
            std::cout << "added job to " << vfs_path << "\n";

        } else if(cmd == "plan.jobs.complete"){
            if(inv.args.size() < 2) throw std::runtime_error("plan.jobs.complete <jobs-path> <index>");
            std::string vfs_path = normalize_path(cwd.path, inv.args[0]);
            auto node = vfs.tryResolveForOverlay(vfs_path, cwd.primary_overlay);
            if(!node) throw std::runtime_error("plan.jobs.complete: path not found: " + vfs_path);
            auto jobs_node = std::dynamic_pointer_cast<PlanJobs>(node);
            if(!jobs_node) throw std::runtime_error("plan.jobs.complete: not a jobs node: " + vfs_path);

            size_t index = std::stoul(inv.args[1]);
            jobs_node->completeJob(index);
            std::cout << "marked job " << index << " as completed in " << vfs_path << "\n";

        } else if(cmd == "plan.save"){
            std::filesystem::path plan_file = "plan.vfs";
            if(!inv.args.empty()) plan_file = inv.args[0];

            try {
                if(plan_file.is_relative()) plan_file = std::filesystem::absolute(plan_file);

                // Create a temporary overlay to hold the /plan tree
                auto temp_root = std::make_shared<DirNode>("/");
                auto temp_overlay_id = vfs.registerOverlay("_plan_temp", temp_root);

                // Copy /plan tree content to the temporary overlay
                auto hits = vfs.resolveMulti("/plan");
                if(!hits.empty() && hits[0].node->isDir()){
                    // Clone the /plan directory structure by directly copying node references
                    std::function<void(const std::string&, std::shared_ptr<VfsNode>&)> clone_tree;
                    clone_tree = [&](const std::string& src_path, std::shared_ptr<VfsNode>& dst_parent){
                        auto listing = vfs.listDir(src_path, vfs.overlaysForPath(src_path));
                        for(const auto& [name, entry] : listing){
                            std::string child_path = src_path == "/" ? "/" + name : src_path + "/" + name;
                            if(!entry.nodes.empty()){
                                auto src_node = entry.nodes[0];
                                // Directly reference the source node (preserves type)
                                dst_parent->children()[name] = src_node;
                                // If it has children, continue cloning
                                if(src_node->isDir()){
                                    clone_tree(child_path, src_node);
                                }
                            }
                        }
                    };

                    auto plan_dir = std::make_shared<DirNode>("plan");
                    temp_root->children()["plan"] = plan_dir;
                    std::shared_ptr<VfsNode> plan_node = plan_dir;
                    clone_tree("/plan", plan_node);
                }

                save_overlay_to_file(vfs, temp_overlay_id, plan_file.string());
                vfs.unregisterOverlay(temp_overlay_id);
                std::cout << "saved plan tree to " << plan_file.string() << "\n";
            } catch(const std::exception& e){
                std::cout << "error saving plan: " << e.what() << "\n";
                result.success = false;
            }

        } else if(cmd == "solution.save"){
            std::filesystem::path target = solution.file_path;
            if(!inv.args.empty()) target = inv.args[0];
            if(!solution.active){
                std::cout << "no solution loaded\n";
                result.success = false;
            } else {
                if(target.empty()){
                    std::cout << "solution.save requires a file path\n";
                    result.success = false;
                } else {
                    try{
                        if(target.is_relative()) target = std::filesystem::absolute(target);
                    } catch(const std::exception& e){
                        std::cout << "error: solution.save: " << e.what() << "\n";
                        result.success = false;
                        target.clear();
                    }
                    if(!target.empty()){
                        solution.file_path = target.string();
                        if(!solution_save(vfs, solution, false)){
                            result.success = false;
                        }
                    }
                }
            }

        } else if(cmd == "cpp.tu"){
            if(inv.args.empty()) throw std::runtime_error("cpp.tu <path>");
            std::string abs = normalize_path(cwd.path, inv.args[0]);
            auto tu = std::make_shared<CppTranslationUnit>(path_basename(abs));
            vfs_add(vfs, abs, tu, cwd.primary_overlay);
            std::cout << "cpp tu @ " << abs << "\n";

        } else if(cmd == "cpp.include"){
            if(inv.args.size() < 2) throw std::runtime_error("cpp.include <tu> <header> [angled]");
            std::string absTu = normalize_path(cwd.path, inv.args[0]);
            auto tu = expect_tu(vfs.resolveForOverlay(absTu, cwd.primary_overlay));
            int angled = 0;
            if(inv.args.size() > 2){
                try{
                    angled = std::stoi(inv.args[2]);
                } catch(const std::exception&){
                    angled = 0;
                }
            }
            auto inc = std::make_shared<CppInclude>("include", inv.args[1], angled != 0);
            tu->includes.push_back(inc);
            std::cout << "+include " << inv.args[1] << "\n";

        } else if(cmd == "cpp.func"){
            if(inv.args.size() < 3) throw std::runtime_error("cpp.func <tu> <name> <ret>");
            std::string absTu = normalize_path(cwd.path, inv.args[0]);
            auto tu = expect_tu(vfs.resolveForOverlay(absTu, cwd.primary_overlay));
            auto fn = std::make_shared<CppFunction>(inv.args[1], inv.args[2], inv.args[1]);
            std::string fnPath = join_path(absTu, inv.args[1]);
            vfs_add(vfs, fnPath, fn, cwd.primary_overlay);
            tu->funcs.push_back(fn);
            vfs_add(vfs, join_path(fnPath, "body"), fn->body, cwd.primary_overlay);
            std::cout << "+func " << inv.args[1] << "\n";

        } else if(cmd == "cpp.param"){
            if(inv.args.size() < 3) throw std::runtime_error("cpp.param <fn> <type> <name>");
            auto fn = expect_fn(vfs.resolveForOverlay(normalize_path(cwd.path, inv.args[0]), cwd.primary_overlay));
            fn->params.push_back(CppParam{inv.args[1], inv.args[2]});
            std::cout << "+param " << inv.args[1] << " " << inv.args[2] << "\n";

        } else if(cmd == "cpp.print"){
            if(inv.args.empty()) throw std::runtime_error("cpp.print <scope> <text>");
            auto block = expect_block(vfs.resolveForOverlay(normalize_path(cwd.path, inv.args[0]), cwd.primary_overlay));
            std::string text = unescape_meta(join_args(inv.args, 1));
            auto s = std::make_shared<CppString>("s", text);
            auto chain = std::vector<std::shared_ptr<CppExpr>>{ s, std::make_shared<CppId>("endl", "std::endl") };
            auto coutline = std::make_shared<CppStreamOut>("cout", chain);
            block->stmts.push_back(std::make_shared<CppExprStmt>("es", coutline));
            std::cout << "+print '" << text << "'\n";

        } else if(cmd == "cpp.returni"){
            if(inv.args.size() < 2) throw std::runtime_error("cpp.returni <scope> <int>");
            auto block = expect_block(vfs.resolveForOverlay(normalize_path(cwd.path, inv.args[0]), cwd.primary_overlay));
            long long value = std::stoll(inv.args[1]);
            block->stmts.push_back(std::make_shared<CppReturn>("ret", std::make_shared<CppInt>("i", value)));
            std::cout << "+return " << value << "\n";

        } else if(cmd == "cpp.return"){
            if(inv.args.empty()) throw std::runtime_error("cpp.return <scope> [expr]");
            auto block = expect_block(vfs.resolveForOverlay(normalize_path(cwd.path, inv.args[0]), cwd.primary_overlay));
            std::string trimmed = unescape_meta(trim_copy(join_args(inv.args, 1)));
            std::shared_ptr<CppExpr> expr;
            if(!trimmed.empty()) expr = std::make_shared<CppRawExpr>("rexpr", trimmed);
            block->stmts.push_back(std::make_shared<CppReturn>("ret", expr));
            std::cout << "+return expr\n";

        } else if(cmd == "cpp.expr"){
            if(inv.args.empty()) throw std::runtime_error("cpp.expr <scope> <expr>");
            auto block = expect_block(vfs.resolveForOverlay(normalize_path(cwd.path, inv.args[0]), cwd.primary_overlay));
            block->stmts.push_back(std::make_shared<CppExprStmt>("expr", std::make_shared<CppRawExpr>("rexpr", unescape_meta(join_args(inv.args, 1)))));
            std::cout << "+expr " << inv.args[0] << "\n";

        } else if(cmd == "cpp.vardecl"){
            if(inv.args.size() < 3) throw std::runtime_error("cpp.vardecl <scope> <type> <name> [init]");
            auto block = expect_block(vfs.resolveForOverlay(normalize_path(cwd.path, inv.args[0]), cwd.primary_overlay));
            std::string init = unescape_meta(trim_copy(join_args(inv.args, 3)));
            bool hasInit = !init.empty();
            block->stmts.push_back(std::make_shared<CppVarDecl>("var", inv.args[1], inv.args[2], init, hasInit));
            std::cout << "+vardecl " << inv.args[1] << " " << inv.args[2] << "\n";

        } else if(cmd == "cpp.stmt"){
            if(inv.args.empty()) throw std::runtime_error("cpp.stmt <scope> <stmt>");
            auto block = expect_block(vfs.resolveForOverlay(normalize_path(cwd.path, inv.args[0]), cwd.primary_overlay));
            block->stmts.push_back(std::make_shared<CppRawStmt>("stmt", unescape_meta(join_args(inv.args, 1))));
            std::cout << "+stmt " << inv.args[0] << "\n";

        } else if(cmd == "cpp.rangefor"){
            if(inv.args.size() < 2) throw std::runtime_error("cpp.rangefor <scope> <loop> decl | range");
            std::string rest = trim_copy(join_args(inv.args, 2));
            auto bar = rest.find('|');
            if(bar == std::string::npos) throw std::runtime_error("cpp.rangefor expects 'decl | range'");
            std::string decl = unescape_meta(trim_copy(rest.substr(0, bar)));
            std::string range = unescape_meta(trim_copy(rest.substr(bar + 1)));
            if(decl.empty() || range.empty()) throw std::runtime_error("cpp.rangefor missing decl or range");
            std::string absScope = normalize_path(cwd.path, inv.args[0]);
            auto block = expect_block(vfs.resolveForOverlay(absScope, cwd.primary_overlay));
            auto loop = std::make_shared<CppRangeFor>(inv.args[1], decl, range);
            block->stmts.push_back(loop);
            std::string loopPath = join_path(absScope, inv.args[1]);
            vfs_add(vfs, loopPath, loop, cwd.primary_overlay);
            vfs_add(vfs, join_path(loopPath, "body"), loop->body, cwd.primary_overlay);
            std::cout << "+rangefor " << inv.args[1] << "\n";

        } else if(cmd == "cpp.dump"){
            if(inv.args.size() < 2) throw std::runtime_error("cpp.dump <tu> <out>");
            std::string absTu = normalize_path(cwd.path, inv.args[0]);
            std::string absOut = normalize_path(cwd.path, inv.args[1]);
            cpp_dump_to_vfs(vfs, cwd.primary_overlay, absTu, absOut);
            std::cout << "dump -> " << absOut << "\n";

        } else if(cmd == "help"){
            help();

        } else if(cmd == "quit" || cmd == "exit"){
            result.exit_requested = true;

        } else if(cmd.empty()){
            // nothing

        } else {
            std::cout << _(UNKNOWN_COMMAND) << "\n";
            result.success = false;
        }

        result.output += capture.str();
        return result;
    };

    auto run_pipeline = [&](const CommandPipeline& pipeline, const std::string& initial_input) -> CommandResult {
        if(pipeline.commands.empty()) return CommandResult{};
        CommandResult last;
        std::string next_input = initial_input;
        for(size_t i = 0; i < pipeline.commands.size(); ++i){
            last = execute_single(pipeline.commands[i], next_input);
            if(last.exit_requested) return last;
            next_input = last.output;
        }

        // Handle output redirection
        if(!pipeline.output_redirect.empty()){
            std::string abs_path = normalize_path(cwd.path, pipeline.output_redirect);
            if(pipeline.redirect_append){
                // >> append to file
                std::string existing;
                try {
                    existing = vfs.read(abs_path, std::nullopt);
                } catch(...) {
                    // File doesn't exist yet, that's ok
                }
                vfs.write(abs_path, existing + last.output, cwd.primary_overlay);
            } else {
                // > overwrite file
                vfs.write(abs_path, last.output, cwd.primary_overlay);
            }
            last.output.clear(); // don't print to stdout when redirected
        }

        return last;
    };

    while(true){
        TRACE_LOOP("repl.iter", std::string("iter=") + std::to_string(repl_iter));
        ++repl_iter;
        bool have_line = false;
        if(interactive && input == &std::cin){
            if(!read_line_with_history("> ", line, history)){
                break;
            }
            have_line = true;
        } else {
            if(!std::getline(*input, line)){
                if(script_active && fallback_after_script){
                    script_active = false;
                    fallback_after_script = false;
                    scriptStream.reset();
                    input = &std::cin;
                    interactive = true;
                    if(!std::cin.good()) std::cin.clear();
                    continue;
                }
                break;
            }
            have_line = true;
        }

        if(!have_line) break;

        auto trimmed = trim_copy(line);
        if(trimmed.empty()) continue;
        // Skip comment lines starting with #
        if(!trimmed.empty() && trimmed[0] == '#') continue;
        try{
            auto tokens = tokenize_command_line(line);
            if(tokens.empty()) continue;
            bool simple_history = false;
            if(tokens[0] == "history"){
                simple_history = true;
                for(const auto& tok : tokens){
                    if(tok == "|" || tok == "&&" || tok == "||"){ simple_history = false; break; }
                }
            }
            bool record_line = !simple_history;
            if(record_line){
                history.push_back(line);
                history_dirty = true;
            }
            auto chain = parse_command_chain(tokens);
            bool exit_requested = false;
            bool last_success = true;
            for(const auto& entry : chain){
                if(entry.logical == "&&" && !last_success) continue;
                if(entry.logical == "||" && last_success) continue;
                CommandResult res = run_pipeline(entry.pipeline, "");
                if(!res.output.empty()){
                    std::cout << res.output;
                    std::cout.flush();
                }
                last_success = res.success;
                if(res.exit_requested){
                    exit_requested = true;
                    break;
                }
            }
            if(exit_requested) break;
        } catch(const std::exception& e){
            std::cout << "error: " << e.what() << "\n";
        }
    }
    if(solution.active && vfs.overlayDirty(solution.overlay_id)){
        while(true){
            std::cout << "Solution '" << solution.title << "' modified. Save changes? [y/N] ";
            std::string answer;
            if(!std::getline(std::cin, answer)){
                std::cout << '\n';
                break;
            }
            std::string trimmed = trim_copy(answer);
            if(trimmed.empty()){
                break;
            }
            char c = static_cast<char>(std::tolower(static_cast<unsigned char>(trimmed[0])));
            if(c == 'y'){
                solution_save(vfs, solution, false);
                break;
            }
            if(c == 'n'){
                break;
            }
            std::cout << "Please answer y or n.\n";
        }
    }
    if(history_dirty) save_history(history);
    return 0;
}
