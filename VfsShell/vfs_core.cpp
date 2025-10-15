#include "VfsShell.h"

// ====== VFS ======
Vfs* G_VFS = nullptr;

// Global feedback pipeline objects (initialized in main)
MetricsCollector* G_METRICS_COLLECTOR = nullptr;
RulePatchStaging* G_PATCH_STAGING = nullptr;
FeedbackLoop* G_FEEDBACK_LOOP = nullptr;

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



bool run_ncurses_editor(Vfs& vfs, const std::string& vfs_path, std::vector<std::string>& lines,
                        bool file_exists, size_t overlay_id) {
    return false;
}

Vfs::Vfs() : logic_engine(&tag_registry) {
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

// Advanced tree visualization with options
std::string Vfs::formatTreeNode(VfsNode* node, const std::string& /* path */, const TreeOptions& opts){
    std::ostringstream oss;

    // Node type/kind indicator
    if(opts.show_node_kind){
        oss << type_char(std::shared_ptr<VfsNode>(node, [](VfsNode*){})) << " ";
    }

    // Node name with optional color
    if(opts.use_colors){
        const char* color = "\033[0m";  // Default
        switch(node->kind){
            case VfsNode::Kind::Dir:      color = "\033[34m"; break; // Blue
            case VfsNode::Kind::File:     color = "\033[0m";  break; // Default
            case VfsNode::Kind::Ast:      color = "\033[35m"; break; // Magenta
            case VfsNode::Kind::Mount:    color = "\033[36m"; break; // Cyan
            case VfsNode::Kind::Library:  color = "\033[33m"; break; // Yellow
            default:                      color = "\033[37m"; break; // White
        }
        oss << color << node->name << "\033[0m";
    } else {
        oss << node->name;
    }

    // Show size/token estimate
    if(opts.show_sizes && !node->isDir()){
        std::string content = node->read();
        size_t tokens = ContextEntry::estimate_tokens(content);
        oss << " (" << tokens << " tok)";
    }

    // Show tags
    if(opts.show_tags){
        const TagSet* tags = tag_storage.getTags(node);
        if(tags && !tags->empty()){
            oss << " [";
            bool first = true;
            for(TagId tid : *tags){
                if(!first) oss << ",";
                oss << tag_registry.getTagName(tid);
                first = false;
            }
            oss << "]";
        }
    }

    return oss.str();
}

void Vfs::treeAdvanced(std::shared_ptr<VfsNode> n, const std::string& path,
                      const TreeOptions& opts, int depth, bool is_last){
    TRACE_FN("path=", path, ", depth=", depth);

    if(!n) return;
    if(opts.max_depth >= 0 && depth > opts.max_depth) return;

    // Apply filter
    if(!opts.filter_pattern.empty()){
        if(path.find(opts.filter_pattern) == std::string::npos){
            return;
        }
    }

    // Build prefix with box-drawing characters
    std::string prefix;
    if(depth > 0){
        if(opts.use_box_chars){
            prefix = (is_last ? "└─ " : "├─ ");
        } else {
            prefix = std::string(depth * 2, ' ');
        }
    }

    // Print current node
    std::cout << prefix << formatTreeNode(n.get(), path, opts) << "\n";

    // Recurse into children
    if(n->isDir()){
        auto& ch = n->children();
        std::vector<std::pair<std::string, std::shared_ptr<VfsNode>>> entries(ch.begin(), ch.end());

        // Sort if requested
        if(opts.sort_entries){
            std::sort(entries.begin(), entries.end(),
                [](const auto& a, const auto& b){ return a.first < b.first; });
        }

        for(size_t i = 0; i < entries.size(); ++i){
            const auto& [name, child] = entries[i];
            std::string child_path = path;
            if(child_path.back() != '/') child_path += '/';
            child_path += name;

            bool child_is_last = (i == entries.size() - 1);

            // Recurse into child
            treeAdvanced(child, child_path, opts, depth + 1, child_is_last);
        }
    }
}

void Vfs::treeAdvanced(const std::string& path, const TreeOptions& opts){
    TRACE_FN("path=", path);
    auto node = resolve(path);
    if(!node){
        std::cout << "error: path not found: " << path << "\n";
        return;
    }
    treeAdvanced(node, path, opts, 0, true);
}

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

