#ifndef _VfsCore_VfsMount_h_
#define _VfsCore_VfsMount_h_

// All includes have been moved to VfsCore.h - the main header
// This header is not self-contained as per U++ convention
// For reference: This header needs VfsNode definition from VfsCore.h
// #include "VfsCore.h"  // Commented for U++ convention - included in main header

// Forward declarations for types defined elsewhere
struct WorkingDirectory;
struct SolutionContext;

struct MountNode : VfsNode {
    String host_path;
    mutable std::unordered_map<std::string, std::shared_ptr<VfsNode>> cache;
    MountNode(String n, String hp);
    bool isDir() const override;
    String read() const override;
    void write(const String& s) override;
    std::unordered_map<std::string, std::shared_ptr<VfsNode>>& children() override;
private:
    void populateCache() const;
    typedef MountNode CLASSNAME;  // Required for THISBACK macros if used
};

struct LibraryNode : VfsNode {
    String lib_path;
    void* handle;
    std::unordered_map<std::string, std::shared_ptr<VfsNode>> symbols;
    LibraryNode(String n, String lp);
    ~LibraryNode() override;
    bool isDir() const override { return true; }
    std::unordered_map<std::string, std::shared_ptr<VfsNode>>& children() override { return symbols; }
    typedef LibraryNode CLASSNAME;  // Required for THISBACK macros if used
};

struct LibrarySymbolNode : VfsNode {
    void* func_ptr;
    String signature;
    LibrarySymbolNode(String n, void* ptr, String sig);
    String read() const override { return signature; }
    typedef LibrarySymbolNode CLASSNAME;  // Required for THISBACK macros if used
};

struct RemoteNode : VfsNode {
    String host;
    int port;
    String remote_path;  // VFS path on remote server
    mutable int sock_fd;
    mutable std::unordered_map<std::string, std::shared_ptr<VfsNode>> cache;
    mutable bool cache_valid;
    mutable std::mutex conn_mutex;

    RemoteNode(String n, String h, int p, String rp);
    ~RemoteNode() override;

    bool isDir() const override;
    String read() const override;
    void write(const String& s) override;
    std::unordered_map<std::string, std::shared_ptr<VfsNode>>& children() override;

private:
    void ensureConnected() const;
    void disconnect() const;
    String execRemote(const String& command) const;
    void populateCache() const;
    typedef RemoteNode CLASSNAME;  // Required for THISBACK macros if used
};

#endif
