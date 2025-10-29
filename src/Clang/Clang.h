#ifndef _Clang_Clang_h_
#define _Clang_Clang_h_

#include <Core/Core.h>
#include <atomic>                     // For std::atomic<bool>
#include "../VfsCore/VfsCore.h"   // For VfsNode and Vfs
#include "../VfsShell/Sexp.h"     // For AstNode, Value, Env
#include <clang-c/Index.h>
#include <functional>
#include <optional>
#include <filesystem>
#include <limits>
#include <unordered_map>
#include <mutex>
#include <chrono>

using namespace Upp;

// Forward declarations for types defined elsewhere
struct Vfs;
struct VfsNode;
struct CppExpr;
struct CppCompound;
struct SolutionContext;

// Include headers in dependency order
#include "CppAst.h"
#include "ClangParser.h"

#endif
