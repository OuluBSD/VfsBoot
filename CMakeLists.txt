cmake_minimum_required(VERSION 3.10)
project(VfsBoot VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)  # Allow gnu++17

# Find required packages
find_package(PkgConfig REQUIRED)

# Find libsvn
pkg_check_modules(SVN REQUIRED libsvn_delta libsvn_subr)

# Find libwebsockets
pkg_check_modules(LIBWEBSOCKETS REQUIRED libwebsockets)

# Find ncurses (optional)
pkg_check_modules(NCURSES ncurses)

# LLVM/Clang configuration
set(LLVM_DIR "/usr/lib/llvm/21" CACHE PATH "LLVM installation directory")
set(LIBCLANG_INCLUDE_DIR "${LLVM_DIR}/include")
set(LIBCLANG_LIBRARY_DIR "${LLVM_DIR}/lib64")
if(NOT EXISTS "${LIBCLANG_LIBRARY_DIR}")
    set(LIBCLANG_LIBRARY_DIR "${LLVM_DIR}/lib")
endif()

# Check for BLAKE3
find_library(BLAKE3_LIBRARY NAMES blake3 REQUIRED)

# VfsShell source files
set(VFSSHELL_SOURCES
    VfsShell/vfs_common.cpp
    VfsShell/tag_system.cpp
    VfsShell/logic_engine.cpp
    VfsShell/vfs_core.cpp
    VfsShell/vfs_mount.cpp
    VfsShell/sexp.cpp
    VfsShell/cpp_ast.cpp
    VfsShell/clang_parser.cpp
    VfsShell/planner.cpp
    VfsShell/ai_bridge.cpp
    VfsShell/context_builder.cpp
    VfsShell/build_graph.cpp
    VfsShell/make.cpp
    VfsShell/hypothesis.cpp
    VfsShell/scope_store.cpp
    VfsShell/feedback.cpp
    VfsShell/shell_commands.cpp
    VfsShell/repl.cpp
    VfsShell/main.cpp
    VfsShell/snippet_catalog.cpp
    VfsShell/utils.cpp
    VfsShell/web_server.cpp
    VfsShell/upp_assembly.cpp
    VfsShell/upp_builder.cpp
    VfsShell/upp_workspace_build.cpp
    VfsShell/command.cpp
    VfsShell/daemon.cpp
    VfsShell/registry.cpp
    VfsShell/qwen_protocol.cpp
    VfsShell/qwen_client.cpp
    VfsShell/qwen_state_manager.cpp
    VfsShell/qwen_manager.cpp
    VfsShell/qwen_tcp_server.cpp
    VfsShell/cmd_qwen.cpp
)

# Main vfsh executable
add_executable(vfsh ${VFSSHELL_SOURCES})

# Include directories
target_include_directories(vfsh PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LIBCLANG_INCLUDE_DIR}
    ${SVN_INCLUDE_DIRS}
    ${LIBWEBSOCKETS_INCLUDE_DIRS}
)

# Compiler flags
target_compile_options(vfsh PRIVATE
    -Wall
    -Wextra
    -pedantic
)

# Add ncurses support if available
if(NCURSES_FOUND)
    target_compile_definitions(vfsh PRIVATE CODEX_UI_NCURSES)
    target_include_directories(vfsh PRIVATE ${NCURSES_INCLUDE_DIRS})
endif()

# Link directories
target_link_directories(vfsh PRIVATE
    ${LIBCLANG_LIBRARY_DIR}
    ${SVN_LIBRARY_DIRS}
    ${LIBWEBSOCKETS_LIBRARY_DIRS}
)

# Link libraries
target_link_libraries(vfsh PRIVATE
    ${BLAKE3_LIBRARY}
    clang
    ${SVN_LIBRARIES}
    ${LIBWEBSOCKETS_LIBRARIES}
    pthread
)

# Add ncurses if available
if(NCURSES_FOUND)
    target_link_libraries(vfsh PRIVATE ${NCURSES_LIBRARIES})
endif()

# Install target
install(TARGETS vfsh DESTINATION bin)

# Print configuration summary
message(STATUS "=== VfsBoot CMake Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "LLVM/Clang dir: ${LLVM_DIR}")
message(STATUS "libclang library: ${LIBCLANG_LIBRARY_DIR}/libclang.so")
message(STATUS "BLAKE3: ${BLAKE3_LIBRARY}")
message(STATUS "Subversion: ${SVN_LIBRARIES}")
message(STATUS "libwebsockets: ${LIBWEBSOCKETS_LIBRARIES}")
if(NCURSES_FOUND)
    message(STATUS "ncurses: ${NCURSES_LIBRARIES} (enabled)")
else()
    message(STATUS "ncurses: not found (disabled)")
endif()
message(STATUS "==================================")
