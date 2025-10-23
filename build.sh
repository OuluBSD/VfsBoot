#!/bin/bash
# VfsBoot build script - supports U++ (umk), CMake, and Make

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Parse arguments
BUILD_TYPE="release"
BUILD_SYSTEM=""
VERBOSE=0
BOOTSTRAP=0
STATIC_ANALYSIS=""  # "yes", "no", or "" (prompt)

usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Build VfsBoot using available build system (umk > cmake > make)

OPTIONS:
    -d, --debug           Debug build (default: release)
    -r, --release         Release build
    -s, --system TYPE     Force build system: umk, cmake, or make
    -v, --verbose         Verbose output
    -c, --clean           Clean before build
    -b, --bootstrap       Bootstrap build (compile internal make, then use it)
    --static-analysis     Run static analysis after successful build (clang-tidy)
    --no-static-analysis  Skip static analysis prompt
    -h, --help            Show this help

EXAMPLES:
    $0                  # Auto-detect and build (release)
    $0 -d               # Debug build
    $0 -b               # Bootstrap build (build internal make first)
    $0 --static-analysis  # Build and run static analysis
    $0 -s umk -d        # Force U++ debug build
    $0 -s cmake -r      # Force CMake release build
    $0 -s make -c -d    # Force Unix Makefile clean debug build

ENVIRONMENT:
    UMK                 Path to umk executable (default: search PATH)
    CMAKE               Path to cmake executable (default: search PATH)
EOF
    exit 0
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -d|--debug)
            BUILD_TYPE="debug"
            shift
            ;;
        -r|--release)
            BUILD_TYPE="release"
            shift
            ;;
        -s|--system)
            BUILD_SYSTEM="$2"
            shift 2
            ;;
        -v|--verbose)
            VERBOSE=1
            shift
            ;;
        -c|--clean)
            CLEAN=1
            shift
            ;;
        -b|--bootstrap)
            BOOTSTRAP=1
            shift
            ;;
        --static-analysis)
            STATIC_ANALYSIS="yes"
            shift
            ;;
        --no-static-analysis)
            STATIC_ANALYSIS="no"
            shift
            ;;
        -h|--help)
            usage
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            usage
            ;;
    esac
done

# Detect build system
detect_build_system() {
    if [ -n "$BUILD_SYSTEM" ]; then
        echo "$BUILD_SYSTEM"
        return
    fi

    # Check for umk (U++ Ultimate Maker)
    if command -v umk &> /dev/null || [ -n "$UMK" ]; then
        echo "umk"
        return
    fi

    # Check for CMake
    if command -v cmake &> /dev/null || [ -n "$CMAKE" ]; then
        echo "cmake"
        return
    fi

    # Fallback to make
    if command -v make &> /dev/null; then
        echo "make"
        return
    fi

    echo -e "${RED}Error: No build system found (umk, cmake, or make)${NC}" >&2
    exit 1
}

# Build with U++ umk
build_umk() {
    local umk_cmd="${UMK:-umk}"

    if ! command -v "$umk_cmd" &> /dev/null; then
        echo -e "${RED}Error: umk not found${NC}" >&2
        exit 1
    fi

    echo -e "${BLUE}Building with U++ (umk)...${NC}"

    # umk syntax: umk assembly,includes package buildmethod flags output
    # -d = debug, -r = release, -s = shared, -v = verbose
    local flags=""
    local buildmethod="CLANG.bm"

    if [ "$BUILD_TYPE" = "debug" ]; then
        flags="-ds"  # debug + shared
    else
        flags="-rs"  # release + shared
    fi

    if [ "$VERBOSE" = "1" ]; then
        flags="${flags}v"
    fi

    # Check if ~/upp/uppsrc exists
    if [ ! -d ~/upp/uppsrc ]; then
        echo -e "${YELLOW}Warning: ~/upp/uppsrc not found, using current directory only${NC}"
        "$umk_cmd" .,. VfsShell "$buildmethod" "$flags" ./vfsh
    else
        "$umk_cmd" .,~/upp/uppsrc VfsShell "$buildmethod" "$flags" ./vfsh
    fi
}

# Build with CMake
build_cmake() {
    local cmake_cmd="${CMAKE:-cmake}"

    if ! command -v "$cmake_cmd" &> /dev/null; then
        echo -e "${RED}Error: cmake not found${NC}" >&2
        exit 1
    fi

    echo -e "${BLUE}Building with CMake...${NC}"

    # Create build directory
    local build_dir="build"
    if [ "$BUILD_TYPE" = "debug" ]; then
        build_dir="build-debug"
    fi

    mkdir -p "$build_dir"
    cd "$build_dir"

    # Configure
    local cmake_build_type="Release"
    if [ "$BUILD_TYPE" = "debug" ]; then
        cmake_build_type="Debug"
    fi

    local cmake_flags="-DCMAKE_BUILD_TYPE=$cmake_build_type"
    if [ "$VERBOSE" = "1" ]; then
        cmake_flags="$cmake_flags -DCMAKE_VERBOSE_MAKEFILE=ON"
    fi

    "$cmake_cmd" .. $cmake_flags

    # Build
    local make_flags="-j$(nproc 2>/dev/null || echo 4)"
    if [ "$VERBOSE" = "1" ]; then
        make_flags="$make_flags VERBOSE=1"
    fi

    "$cmake_cmd" --build . $make_flags

    # Copy binary to root
    if [ -f vfsh ]; then
        cp vfsh ..
        echo -e "${GREEN}Binary copied to: $(pwd)/../vfsh${NC}"
    fi

    cd ..
}

# Build with Make
build_make() {
    if ! command -v make &> /dev/null; then
        echo -e "${RED}Error: make not found${NC}" >&2
        exit 1
    fi

    echo -e "${BLUE}Building with Make...${NC}"

    # Check dependencies first
    if ! check_dependencies; then
        exit 1
    fi

    # Clean if requested
    if [ -n "$CLEAN" ]; then
        echo "Cleaning..."
        make clean
    fi

    # Build
    local make_target="all"
    if [ "$BUILD_TYPE" = "debug" ]; then
        make_target="debug"
    elif [ "$BUILD_TYPE" = "release" ]; then
        make_target="release"
    fi

    local make_flags="-j$(nproc 2>/dev/null || echo 4)"
    if [ "$VERBOSE" = "1" ]; then
        make $make_flags $make_target V=1
    else
        make $make_flags $make_target
    fi
}

# Check for required libraries
check_dependencies() {
    echo -e "${BLUE}Checking dependencies...${NC}"
    local missing_deps=()
    local missing_hints=()

    # Check for C++ compiler
    local CXX="${CXX:-g++}"
    if ! command -v "$CXX" &> /dev/null; then
        CXX="c++"
        if ! command -v "$CXX" &> /dev/null; then
            missing_deps+=("C++ compiler (g++ or c++)")
            missing_hints+=("  Install: emerge sys-devel/gcc")
        fi
    fi

    # Check for pkg-config
    if ! command -v pkg-config &> /dev/null; then
        missing_deps+=("pkg-config")
        missing_hints+=("  Install: emerge dev-util/pkgconfig")
    fi

    # Check for BLAKE3 library
    if ! pkg-config --exists blake3 2>/dev/null && ! ldconfig -p 2>/dev/null | grep -q libblake3; then
        missing_deps+=("BLAKE3 library")
        missing_hints+=("  Install: emerge dev-libs/blake3")
    fi

    # Check for Subversion development libraries
    if ! pkg-config --exists libsvn_delta 2>/dev/null; then
        missing_deps+=("Subversion development libraries (libsvn_delta)")
        missing_hints+=("  Install: emerge dev-vcs/subversion")
    fi

    if ! pkg-config --exists libsvn_subr 2>/dev/null; then
        missing_deps+=("Subversion development libraries (libsvn_subr)")
        missing_hints+=("  Install: emerge dev-vcs/subversion")
    fi

    # Check for libclang
    local LLVM_VERSION="${LLVM_VERSION:-21}"
    if [ ! -f "/usr/lib/llvm/${LLVM_VERSION}/lib64/libclang.so" ] && \
       [ ! -f "/usr/lib/llvm/${LLVM_VERSION}/lib/libclang.so" ] && \
       ! ldconfig -p 2>/dev/null | grep -q libclang; then
        missing_deps+=("libclang (LLVM ${LLVM_VERSION})")
        missing_hints+=("  Install: emerge sys-devel/clang")
    fi

    # Check for libwebsockets
    if ! pkg-config --exists libwebsockets 2>/dev/null && ! ldconfig -p 2>/dev/null | grep -q libwebsockets; then
        missing_deps+=("libwebsockets")
        missing_hints+=("  Install: emerge net-libs/libwebsockets")
    fi

    # Check for pthread (usually part of glibc)
    if ! echo '#include <pthread.h>' | ${CXX:-g++} -E -x c++ - &>/dev/null; then
        missing_deps+=("pthread library")
        missing_hints+=("  Usually provided by glibc - check your system installation")
    fi

    # Report missing dependencies
    if [ ${#missing_deps[@]} -gt 0 ]; then
        echo -e "${RED}✗ Missing dependencies:${NC}"
        for dep in "${missing_deps[@]}"; do
            echo -e "  ${RED}✗${NC} $dep"
        done
        echo ""
        echo -e "${YELLOW}Installation hints (Gentoo):${NC}"
        for hint in "${missing_hints[@]}"; do
            echo -e "$hint"
        done
        echo ""
        echo -e "${YELLOW}Note: For other distributions:${NC}"
        echo "  Debian/Ubuntu: apt-get install libblake3-dev libsvn-dev libclang-dev libwebsockets-dev"
        echo "  Fedora/RHEL:   dnf install blake3-devel subversion-devel clang-devel libwebsockets-devel"
        echo "  Arch:          pacman -S blake3 subversion clang libwebsockets"
        return 1
    fi

    echo -e "${GREEN}✓ All dependencies found${NC}"
    return 0
}

# Bootstrap build: compile internal make, then use it to build codex
build_bootstrap() {
    echo -e "${BLUE}=== Bootstrap Build ===${NC}"

    # Check dependencies first
    if ! check_dependencies; then
        exit 1
    fi

    echo "Step 1: Compiling internal make utility..."

    # Detect C++ compiler
    local CXX="${CXX:-g++}"
    if ! command -v "$CXX" &> /dev/null; then
        CXX="c++"
        if ! command -v "$CXX" &> /dev/null; then
            echo -e "${RED}Error: No C++ compiler found (tried g++ and c++)${NC}" >&2
            exit 1
        fi
    fi

    echo "Using compiler: $CXX"

    # Compile make_main.cpp to ./vfsh-make
    local cxx_flags="-std=c++17 -O2 -Wall -Wextra"
    if [ "$BUILD_TYPE" = "debug" ]; then
        cxx_flags="-std=c++17 -g -O0 -Wall -Wextra"
    fi

    if [ "$VERBOSE" = "1" ]; then
        echo "$CXX $cxx_flags make_main.cpp -o vfsh-make"
    fi

    $CXX $cxx_flags make_main.cpp -o vfsh-make

    if [ ! -f "./vfsh-make" ]; then
        echo -e "${RED}Error: Failed to compile internal make${NC}" >&2
        exit 1
    fi

    echo -e "${GREEN}✓ Internal make compiled successfully${NC}"
    ls -lh ./vfsh-make

    echo ""
    echo "Step 2: Using internal make to build vfsh..."

    # Use our make to build vfsh
    local make_target="all"
    if [ "$BUILD_TYPE" = "debug" ]; then
        make_target="debug"
    elif [ "$BUILD_TYPE" = "release" ]; then
        make_target="release"
    fi

    if [ "$VERBOSE" = "1" ]; then
        ./vfsh-make -v $make_target
    else
        ./vfsh-make $make_target
    fi

    if [ ! -f "./vfsh" ]; then
        echo -e "${RED}Error: vfsh binary not created${NC}" >&2
        exit 1
    fi

    echo -e "${GREEN}✓ Bootstrap build successful!${NC}"
}

# Run static analysis with clang-tidy
run_static_analysis() {
    echo ""
    echo -e "${BLUE}=== Static Analysis ===${NC}"

    # Check if clang-tidy is available
    if ! command -v clang-tidy &> /dev/null; then
        echo -e "${YELLOW}Warning: clang-tidy not found${NC}"
        echo "  Install: emerge sys-devel/clang (Gentoo)"
        echo "           apt-get install clang-tidy (Debian/Ubuntu)"
        return 1
    fi

    echo "Running clang-tidy on VfsShell sources..."
    echo ""

    # Create a compile_commands.json if it doesn't exist
    if [ ! -f compile_commands.json ]; then
        echo -e "${YELLOW}Note: No compile_commands.json found, using manual flags${NC}"
    fi

    # Define source files to analyze
    local sources=(
        "VfsShell/vfs_common.cpp"
        "VfsShell/tag_system.cpp"
        "VfsShell/logic_engine.cpp"
        "VfsShell/vfs_core.cpp"
        "VfsShell/vfs_mount.cpp"
        "VfsShell/sexp.cpp"
        "VfsShell/cpp_ast.cpp"
        "VfsShell/clang_parser.cpp"
        "VfsShell/planner.cpp"
        "VfsShell/ai_bridge.cpp"
        "VfsShell/context_builder.cpp"
        "VfsShell/make.cpp"
        "VfsShell/hypothesis.cpp"
        "VfsShell/scope_store.cpp"
        "VfsShell/feedback.cpp"
        "VfsShell/shell_commands.cpp"
        "VfsShell/repl.cpp"
        "VfsShell/main.cpp"
        "VfsShell/snippet_catalog.cpp"
        "VfsShell/utils.cpp"
        "VfsShell/web_server.cpp"
    )

    # Run clang-tidy on each source file
    local warnings=0
    for src in "${sources[@]}"; do
        echo -e "${BLUE}Analyzing $src...${NC}"

        # Run clang-tidy with common checks
        # Redirect stderr to stdout to capture all warnings
        if ! clang-tidy "$src" \
            --checks='*,-fuchsia-*,-google-*,-llvm-*,-llvmlibc-*,-altera-*,-android-*' \
            -- -std=c++17 \
            -I/usr/lib/llvm/21/include \
            $(pkg-config --cflags libsvn_delta libsvn_subr 2>/dev/null) \
            2>&1 | tee ".static-analysis-$(basename "$src").log"; then
            ((warnings++))
        fi
        echo ""
    done

    echo -e "${GREEN}Static analysis complete${NC}"
    echo "Full logs saved to: .static-analysis-*.log"

    if [ $warnings -gt 0 ]; then
        echo -e "${YELLOW}Found issues in $warnings file(s)${NC}"
        return 1
    else
        echo -e "${GREEN}No critical issues found${NC}"
        return 0
    fi
}

# Prompt user to run static analysis
prompt_static_analysis() {
    # Check command-line flag
    if [ "$STATIC_ANALYSIS" = "yes" ]; then
        run_static_analysis
        return
    elif [ "$STATIC_ANALYSIS" = "no" ]; then
        return
    fi

    # Default: prompt user
    echo ""
    echo -e "${YELLOW}Would you like to run static analysis (clang-tidy)?${NC}"
    echo "  This provides comprehensive code quality checks beyond normal compiler warnings."
    echo -n "Run static analysis? [y/N]: "

    # Only prompt if interactive
    if [ -t 0 ]; then
        read -r response
        case "$response" in
            [yY][eE][sS]|[yY])
                run_static_analysis
                ;;
            *)
                echo "Skipping static analysis."
                ;;
        esac
    else
        echo "Skipping (non-interactive mode)"
    fi
}

# Main build logic
main() {
    echo -e "${GREEN}=== VfsBoot Build Script ===${NC}"
    echo "Build type: ${BUILD_TYPE}"

    # Bootstrap build uses internal make
    if [ "$BOOTSTRAP" = "1" ]; then
        echo "Build mode: Bootstrap (internal make)"
        echo ""
        build_bootstrap
    else
        local system=$(detect_build_system)
        echo "Build system: ${system}"
        echo ""

        case "$system" in
            umk)
                build_umk
                ;;
            cmake)
                build_cmake
                ;;
            make)
                build_make
                ;;
            *)
                echo -e "${RED}Unknown build system: $system${NC}" >&2
                exit 1
                ;;
        esac
    fi

    # Check if binary was created
    if [ -f "./vfsh" ]; then
        echo ""
        echo -e "${GREEN}✓ Build successful!${NC}"
        echo -e "Binary: ${BLUE}./vfsh${NC}"
        ls -lh ./vfsh

        # Prompt for static analysis after successful build
        prompt_static_analysis
    else
        echo -e "${RED}✗ Build failed - binary not found${NC}" >&2
        exit 1
    fi
}

main "$@"