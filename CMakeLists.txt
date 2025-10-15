cmake_minimum_required(VERSION 3.10)
project(VfsBootTextEditor)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Curses REQUIRED)

# Include directories
include_directories(${CURSES_INCLUDE_DIRS})

# Define an option to enable ncurses
option(USE_NCURSES "Enable ncurses support" ON)

# Compiler definitions based on options
if(USE_NCURSES AND CURSES_FOUND)
    add_definitions(-DCODEX_UI_NCURSES)
endif()

# Create a library for the editor functions
add_library(editor_functions editor_functions.cpp)

# Text editor demo executable
add_executable(text_editor_demo 
    demo_text_editor.cpp
    VfsShell/ui_backend.h
    VfsShell/file_browser.h
)

# Link libraries
if(CURSES_FOUND)
    target_link_libraries(text_editor_demo ${CURSES_LIBRARIES})
    target_link_libraries(editor_functions ${CURSES_LIBRARIES})
endif()

# UI backend test
add_executable(test_ui_backend 
    test_ui_backend.cpp
    VfsShell/ui_backend.h
)

if(CURSES_FOUND)
    target_link_libraries(test_ui_backend ${CURSES_LIBRARIES})
endif()

# Simple UI abstraction demo
add_executable(demo_ui_abstraction 
    demo_ui_abstraction.cpp
    VfsShell/ui_backend.h
)

if(CURSES_FOUND)
    target_link_libraries(demo_ui_abstraction ${CURSES_LIBRARIES})
endif()

# Additional test executables
add_executable(test_ncurses 
    test_ncurses.cpp
)

if(CURSES_FOUND)
    target_link_libraries(test_ncurses ${CURSES_LIBRARIES})
endif()

add_executable(test_ncurses_editor 
    test_ncurses_editor.cpp
)

if(CURSES_FOUND)
    target_link_libraries(test_ncurses_editor ${CURSES_LIBRARIES})
endif()

# Set compiler flags
target_compile_options(text_editor_demo PRIVATE -Wall -Wextra)
target_compile_options(test_ui_backend PRIVATE -Wall -Wextra)
target_compile_options(demo_ui_abstraction PRIVATE -Wall -Wextra)
target_compile_options(test_ncurses PRIVATE -Wall -Wextra)
target_compile_options(test_ncurses_editor PRIVATE -Wall -Wextra)