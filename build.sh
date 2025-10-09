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

usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Build VfsBoot using available build system (umk > cmake > make)

OPTIONS:
    -d, --debug         Debug build (default: release)
    -r, --release       Release build
    -s, --system TYPE   Force build system: umk, cmake, or make
    -v, --verbose       Verbose output
    -c, --clean         Clean before build
    -h, --help          Show this help

EXAMPLES:
    $0                  # Auto-detect and build (release)
    $0 -d               # Debug build
    $0 -s umk -d        # Force U++ debug build
    $0 -s cmake -r      # Force CMake release build
    $0 -c -d            # Clean debug build

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
        "$umk_cmd" .,. VfsShell "$buildmethod" "$flags" ./codex
    else
        "$umk_cmd" .,~/upp/uppsrc VfsShell "$buildmethod" "$flags" ./codex
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
    if [ -f codex ]; then
        cp codex ..
        echo -e "${GREEN}Binary copied to: $(pwd)/../codex${NC}"
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

# Main build logic
main() {
    echo -e "${GREEN}=== VfsBoot Build Script ===${NC}"
    echo "Build type: ${BUILD_TYPE}"

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

    # Check if binary was created
    if [ -f "./codex" ]; then
        echo ""
        echo -e "${GREEN}✓ Build successful!${NC}"
        echo -e "Binary: ${BLUE}./codex${NC}"
        ls -lh ./codex
    else
        echo -e "${RED}✗ Build failed - binary not found${NC}" >&2
        exit 1
    fi
}

main "$@"
