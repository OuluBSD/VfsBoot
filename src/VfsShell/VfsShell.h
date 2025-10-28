#ifndef _VfsShell_VfsShell_h_
#define _VfsShell_VfsShell_h_

// Standard library includes for UPP compatibility
#include <Core/Core.h>

// Additional standard includes (converted to UPP equivalents where possible)
#include <Core/TimeDate.h>
#include <Core/Charset.h>
#include <Core/Xmlize.h>
#include <Core/CommandLine.h>
#include <Core/Environment.h>
#include <Core/Event.h>
#include <Core/Thread.h>

// Explicitly qualify U++ types to avoid conflicts with standard types
// Instead of using declarations, we'll use full qualification where needed
// using Upp::Date;
// using Upp::Stream;
// using Upp::Nuller;

// Standard C++ includes still needed for specific functionality
#include <iomanip>  // For QwenProtocol.cpp and potentially other files

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

// Forward declarations to avoid circular dependencies and reduce includes
class VfsCore;
class VfsMount;
class TagSystem;
class LogicEngine;
class Sexp;
class CppAst;
class ClangParser;
class Planner;
class AiBridge;
class ContextBuilder;
class BuildGraph;
class Make;
class Hypothesis;
class ScopeStore;
class Feedback;
class SnippetCatalog;
class Utils;
class ShellCommands;
class WebServer;
class Command;
class EditorFunctions;
class UiBackend;
class UppAssembly;
class UppBuilder;
class CmdQwen;
class QwenProtocol;
class QwenClient;
class QwenStateManager;
class QwenTcpServer;
class QwenManager;

/*
// This section shows original includes that are now handled via forward declarations or main header
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
*/

// Main header includes - all package headers are included here
#include "Config.h"
#include "../Logic/TagSystem.h"
#include "../Logic/LogicEngine.h"
#include "Sexp.h"
#include "Planner.h"
#include "AiBridge.h"
#include "ContextBuilder.h"
#include "BuildGraph.h"
#include "Make.h"
#include "Hypothesis.h"
#include "ScopeStore.h"
#include "Feedback.h"
#include "SnippetCatalog.h"
#include "Utils.h"
#include "ShellCommands.h"
#include "Command.h"
#include "EditorFunctions.h"
#include "UiBackend.h"

// Include headers from other packages
#include "../VfsCore/VfsCommon.h"
#include "../VfsCore/VfsCore.h"
#include "../VfsCore/VfsMount.h"
#include "../Clang/CppAst.h"
#include "../Clang/ClangParser.h"
#include "../WebServer/WebServer.h"
#include "../UppCompat/UppAssembly.h"
#include "../UppCompat/UppBuilder.h"
#include "../Qwen/CmdQwen.h"
#include "../Qwen/QwenProtocol.h"
#include "../Qwen/QwenClient.h"
#include "../Qwen/QwenStateManager.h"
#include "../Qwen/QwenTcpServer.h"
#include "../Qwen/QwenManager.h"

// Using Upp::String instead of std::string
USING_NAMESPACE_UPP

#endif
