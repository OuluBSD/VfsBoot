#pragma once


#include <string>
#include <map>
#include <memory>
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

#ifndef _VfsCore_VfsMount_h_
#define _VfsCore_VfsMount_h_

#include <Core/Core.h>
#include "VfsCore.h"  // Include the main package header which has VfsNode definition

using namespace Upp;

// Forward declarations for types defined elsewhere
struct WorkingDirectory;
struct SolutionContext;

#endif
#include <condition_variable>
#include <future>
#include <chrono>
#include <ratio>
#include <ctime>

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
