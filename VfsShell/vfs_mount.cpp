#include "VfsShell.h"

// ====== Mount Nodes ======

// Helper function to determine node kind based on host path
VfsNode::Kind determineMountNodeKind(const std::string& host_path) {
    namespace fs = std::filesystem;
    try {
        if(fs::is_directory(host_path)) {
            return VfsNode::Kind::Dir;    // Use Dir kind for directories (so they can have children)
        } else if(fs::is_regular_file(host_path)) {
            return VfsNode::Kind::File;   // Use File kind for regular files
        } else {
            return VfsNode::Kind::Mount;  // Default to Mount for other types
        }
    } catch(...) {
        return VfsNode::Kind::Mount;  // Default to Mount if we can't determine the type
    }
}

MountNode::MountNode(std::string n, std::string hp)
    : VfsNode(std::move(n), determineMountNodeKind(hp)), host_path(std::move(hp)) {}

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

std::optional<std::string> Vfs::mapToHostPath(const std::string& vfs_path) const {
    if(vfs_path.empty() || vfs_path.front() != '/') {
        return std::nullopt;
    }

    std::optional<std::string> best_match;
    size_t best_len = 0;

    for(const auto& mount : mounts) {
        if(mount.type != MountType::Filesystem) continue;
        const auto& mount_path = mount.vfs_path;
        if(mount_path.empty()) continue;

        if(vfs_path == mount_path ||
           (vfs_path.compare(0, mount_path.size(), mount_path) == 0 &&
            (vfs_path.size() == mount_path.size() || vfs_path[mount_path.size()] == '/'))) {
            if(mount_path.size() < best_len) continue;

            std::filesystem::path host_path = mount.host_path;
            if(vfs_path.size() > mount_path.size()) {
                auto suffix = vfs_path.substr(mount_path.size());
                if(!suffix.empty() && suffix.front() == '/') {
                    suffix.erase(0, 1);
                }
                if(!suffix.empty()) {
                    host_path /= suffix;
                }
            }

            best_match = host_path.lexically_normal().string();
            best_len = mount_path.size();
        }
    }

    return best_match;
}

std::optional<std::string> Vfs::mapFromHostPath(const std::string& host_path) const {
    if(host_path.empty()) {
        return std::nullopt;
    }

    // Normalize the host path first
    std::filesystem::path normalized_host = std::filesystem::path(host_path).lexically_normal();

    std::optional<std::string> best_match;
    size_t best_len = 0;

    for(const auto& mount : mounts) {
        if(mount.type != MountType::Filesystem) continue;

        std::filesystem::path mount_host = std::filesystem::path(mount.host_path).lexically_normal();

        // Check if the host_path is under this mount's host_path
        auto rel_path = normalized_host.lexically_relative(mount_host);
        if(!rel_path.empty() && rel_path.native()[0] != '.') {
            // The path is under this mount
            if(mount.host_path.size() > best_len) {
                // Build the VFS path
                std::filesystem::path vfs_result = mount.vfs_path;
                if(rel_path != ".") {
                    vfs_result /= rel_path;
                }
                best_match = vfs_result.lexically_normal().string();
                best_len = mount.host_path.size();
            }
        } else if(normalized_host == mount_host) {
            // Exact match with mount point
            if(mount.host_path.size() > best_len) {
                best_match = mount.vfs_path;
                best_len = mount.host_path.size();
            }
        }
    }

    return best_match;
}

std::string overlay_suffix(const Vfs& vfs, const std::vector<size_t>& overlays, size_t primary){
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

size_t select_overlay(const Vfs& vfs, const WorkingDirectory& cwd, const std::vector<size_t>& overlays){
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

void adjust_context_after_unmount(Vfs& vfs, WorkingDirectory& cwd, size_t removedId){
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
