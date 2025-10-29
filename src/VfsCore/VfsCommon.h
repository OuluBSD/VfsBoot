#ifndef _VfsCore_VfsCommon_h_
#define _VfsCore_VfsCommon_h_

#include <Core/Core.h>
#include "VfsCore.h"  // Include the main package header

using namespace Upp;

#include <string>
#include <vector>

#endif

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
struct VfsNode;
struct Vfs;
struct WorkingDirectory;

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

// Forward declarations
struct Vfs;
struct VfsNode;
bool run_ncurses_editor(Vfs& vfs, const std::string& vfs_path,
                        std::vector<std::string>& lines,
                        bool file_exists, size_t overlay_id);

void vfs_add(Vfs& vfs, const std::string& path, std::shared_ptr<VfsNode> node, size_t overlayId);
