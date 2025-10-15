#include "VfsShell.h"

// Global callback for save shortcuts
std::function<void()> g_on_save_shortcut;

// Utility function implementations
void sort_unique(std::vector<size_t>& ids){
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
}

void maybe_extend_context(Vfs& vfs, WorkingDirectory& cwd){
    try{
        update_directory_context(vfs, cwd, cwd.path);
    } catch(const std::exception&){
        // keep existing overlays if path vanished; caller should handle
    }
}

void update_directory_context(Vfs& vfs, WorkingDirectory& cwd, const std::string& absPath){
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

size_t mount_overlay_from_file(Vfs& vfs, const std::string& name, const std::string& hostPath){
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

void save_overlay_to_file(Vfs& vfs, size_t overlayId, const std::string& hostPath){
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

bool is_solution_file(const std::filesystem::path& p){
    auto ext = p.extension().string();
    std::string lowered = ext;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return lowered == kPackageExtension || lowered == kAssemblyExtension;
}

std::optional<std::filesystem::path> auto_detect_vfs_path(){
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

std::optional<std::filesystem::path> auto_detect_solution_path(){
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

std::string make_unique_overlay_name(Vfs& vfs, std::string base){
    if(base.empty()) base = "solution";
    std::string candidate = base;
    int counter = 2;
    while(vfs.findOverlayByName(candidate)){
        candidate = base + "_" + std::to_string(counter++);
    }
    return candidate;
}

bool solution_save(Vfs& vfs, SolutionContext& sol, bool quiet){
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

void attach_solution_shortcut(Vfs& vfs, SolutionContext& sol){
    g_on_save_shortcut = [&vfs, &sol](){
        solution_save(vfs, sol, false);
    };
}

bool load_solution_from_file(Vfs& vfs, WorkingDirectory& cwd, SolutionContext& sol, const std::filesystem::path& file, bool auto_detected){
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

size_t mount_overlay_from_file(Vfs& vfs, const std::string& name, const std::string& hostPath);
std::string unescape_meta(const std::string& s){
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

std::string sanitize_component(const std::string& s){
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


uint64_t fnv1a64(const std::string& data){
    const uint64_t offset = 1469598103934665603ull;
    const uint64_t prime  = 1099511628211ull;
    uint64_t h = offset;
    for(unsigned char c : data){
        h ^= c;
        h *= prime;
    }
    return h;
}

std::string hash_hex(uint64_t value){
    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << value;
    return oss.str();
}


void cmd_parse_file(Vfs& vfs, const std::vector<std::string>& args) {
	int* i = 0; *i = 0; // TODO
}

void cmd_parse_dump(Vfs& vfs, const std::vector<std::string>& args) {
	int* i = 0; *i = 0; // TODO
}

void cmd_parse_generate(Vfs& vfs, const std::vector<std::string>& args) {
	int* i = 0; *i = 0; // TODO
}
std::shared_ptr<AstNode> deserialize_ast_node(const std::string& type,
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

std::pair<std::string, std::string> serialize_s_ast_node(const std::shared_ptr<AstNode>& node){
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

std::shared_ptr<AstNode> deserialize_s_ast_node(const std::string& type, const std::string& payload){
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

bool is_s_ast_type(const std::string& type){
    return type == "AstInt" || type == "AstBool" || type == "AstStr" ||
           type == "AstSym" || type == "AstIf" || type == "AstLambda" ||
           type == "AstCall";
}

bool is_s_ast_instance(const std::shared_ptr<AstNode>& node){
    return std::dynamic_pointer_cast<AstInt>(node) ||
           std::dynamic_pointer_cast<AstBool>(node) ||
           std::dynamic_pointer_cast<AstStr>(node) ||
           std::dynamic_pointer_cast<AstSym>(node) ||
           std::dynamic_pointer_cast<AstIf>(node) ||
           std::dynamic_pointer_cast<AstLambda>(node) ||
           std::dynamic_pointer_cast<AstCall>(node);
}

void deserialize_cpp_compound_into(const std::string& payload,
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

std::pair<std::string, std::string> serialize_ast_node(const std::shared_ptr<AstNode>& node){
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

std::shared_ptr<CppExpr> deserialize_cpp_expr(BinaryReader& r){
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

std::string serialize_cpp_compound_payload(const std::shared_ptr<CppCompound>& compound){
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

void serialize_cpp_expr(BinaryWriter& w, const std::shared_ptr<CppExpr>& expr){
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

