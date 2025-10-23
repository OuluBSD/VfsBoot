#ifndef _VfsShell_VfsShell_h_
#define _VfsShell_VfsShell_h_

// Standard library includes
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cctype>
#include <cstddef>
#include <ctime>
#include <algorithm>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <thread>
#include <random>
#include <regex>
#include <atomic>
#include <chrono>
#include <system_error>
#include <limits>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <array>
#include <unordered_set>
#include <queue>
#include <cassert>
#include <cerrno>

// System includes
#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <poll.h>

// External library includes
#include <blake3.h>
#include <clang-c/Index.h>
#include <svn_delta.h>
#include <svn_pools.h>

// Web server
#include <libwebsockets.h>

/*
#include <iostream>
#include <sstream>
#include <cstring>

// POSIX headers for process management
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <poll.h>

*/

#include "vfs_common.h"
#include "tag_system.h"
#include "logic_engine.h"
#include "vfs_core.h"
#include "vfs_mount.h"
#include "sexp.h"
#include "cpp_ast.h"
#include "clang_parser.h"
#include "planner.h"
#include "ai_bridge.h"
#include "context_builder.h"
#include "build_graph.h"
#include "make.h"
#include "hypothesis.h"
#include "scope_store.h"
#include "feedback.h"
#include "snippet_catalog.h"
#include "utils.h"
#include "shell_commands.h"
#include "web_server.h"
#include "command.h"
#include "editor_functions.h"
#include "ui_backend.h"
#include "upp_assembly.h"
#include "upp_builder.h"
#include "cmd_qwen.h"
#include "qwen_protocol.h"
#include "qwen_client.h"
#include "qwen_state_manager.h"


#endif
