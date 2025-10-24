#pragma once


struct MountNode : VfsNode {
    std::string host_path;
    mutable std::map<std::string, std::shared_ptr<VfsNode>> cache;
    MountNode(std::string n, std::string hp);
    bool isDir() const override;
    std::string read() const override;
    void write(const std::string& s) override;
    std::map<std::string, std::shared_ptr<VfsNode>>& children() override;
private:
    void populateCache() const;
};

struct LibraryNode : VfsNode {
    std::string lib_path;
    void* handle;
    std::map<std::string, std::shared_ptr<VfsNode>> symbols;
    LibraryNode(std::string n, std::string lp);
    ~LibraryNode() override;
    bool isDir() const override { return true; }
    std::map<std::string, std::shared_ptr<VfsNode>>& children() override { return symbols; }
};

struct LibrarySymbolNode : VfsNode {
    void* func_ptr;
    std::string signature;
    LibrarySymbolNode(std::string n, void* ptr, std::string sig);
    std::string read() const override { return signature; }
};

struct RemoteNode : VfsNode {
    std::string host;
    int port;
    std::string remote_path;  // VFS path on remote server
    mutable int sock_fd;
    mutable std::map<std::string, std::shared_ptr<VfsNode>> cache;
    mutable bool cache_valid;
    mutable std::mutex conn_mutex;

    RemoteNode(std::string n, std::string h, int p, std::string rp);
    ~RemoteNode() override;

    bool isDir() const override;
    std::string read() const override;
    void write(const std::string& s) override;
    std::map<std::string, std::shared_ptr<VfsNode>>& children() override;

private:
    void ensureConnected() const;
    void disconnect() const;
    std::string execRemote(const std::string& command) const;
    void populateCache() const;
};
