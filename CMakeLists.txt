cmake_minimum_required(VERSION 3.12)
project(VfsBoot VERSION 1.0 LANGUAGES CXX)

# C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)  # Allow GNU extensions

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# pkg-config for SVN libraries
find_package(PkgConfig REQUIRED)
pkg_check_modules(SVN_DELTA REQUIRED libsvn_delta)
pkg_check_modules(SVN_SUBR REQUIRED libsvn_subr)

# BLAKE3 library
find_library(BLAKE3_LIBRARY blake3 REQUIRED)

# Source files
set(VFSSHELL_SOURCES
    VfsShell/codex.cpp
    VfsShell/snippet_catalog.cpp
    VfsShell/utils.cpp
)

# Harness sources
set(HARNESS_SOURCES
    harness/scenario.cpp
    harness/runner.cpp
)

# Main executable
add_executable(codex ${VFSSHELL_SOURCES})

# Include directories
target_include_directories(codex PRIVATE
    ${SVN_DELTA_INCLUDE_DIRS}
    ${SVN_SUBR_INCLUDE_DIRS}
)

# Compiler definitions
target_compile_definitions(codex PRIVATE
    ${SVN_DELTA_CFLAGS_OTHER}
    ${SVN_SUBR_CFLAGS_OTHER}
)

# Link libraries
target_link_libraries(codex PRIVATE
    ${BLAKE3_LIBRARY}
    ${SVN_DELTA_LIBRARIES}
    ${SVN_SUBR_LIBRARIES}
    pthread
    dl
)

# Optional: Enable tracing
option(ENABLE_TRACE "Enable CODEX_TRACE logging" OFF)
if(ENABLE_TRACE)
    target_compile_definitions(codex PRIVATE CODEX_TRACE)
endif()

# Installation
install(TARGETS codex DESTINATION bin)

# Print configuration
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ flags: ${CMAKE_CXX_FLAGS}")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Debug flags: ${CMAKE_CXX_FLAGS_DEBUG}")
else()
    message(STATUS "Release flags: ${CMAKE_CXX_FLAGS_RELEASE}")
endif()

# Planner harness executables
add_executable(planner_demo harness/demo.cpp ${VFSSHELL_SOURCES} ${HARNESS_SOURCES})
add_executable(planner_train harness/train.cpp ${VFSSHELL_SOURCES} ${HARNESS_SOURCES})

# Include directories for harness binaries
target_include_directories(planner_demo PRIVATE
    ${SVN_DELTA_INCLUDE_DIRS}
    ${SVN_SUBR_INCLUDE_DIRS}
)
target_include_directories(planner_train PRIVATE
    ${SVN_DELTA_INCLUDE_DIRS}
    ${SVN_SUBR_INCLUDE_DIRS}
)

# Compiler definitions for harness binaries
target_compile_definitions(planner_demo PRIVATE
    ${SVN_DELTA_CFLAGS_OTHER}
    ${SVN_SUBR_CFLAGS_OTHER}
)
target_compile_definitions(planner_train PRIVATE
    ${SVN_DELTA_CFLAGS_OTHER}
    ${SVN_SUBR_CFLAGS_OTHER}
)

# Link libraries for harness binaries
target_link_libraries(planner_demo PRIVATE
    ${BLAKE3_LIBRARY}
    ${SVN_DELTA_LIBRARIES}
    ${SVN_SUBR_LIBRARIES}
    pthread
    dl
)
target_link_libraries(planner_train PRIVATE
    ${BLAKE3_LIBRARY}
    ${SVN_DELTA_LIBRARIES}
    ${SVN_SUBR_LIBRARIES}
    pthread
    dl
)
