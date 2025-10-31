#!/usr/bin/env bash

# Parse command-line arguments
USE_MALLOC=0
for arg in "$@"; do
    if [ "$arg" = "--malloc" ]; then
        USE_MALLOC=1
    fi
done

# Build flags - conditionally include USEMALLOC
if [ $USE_MALLOC -eq 1 ]; then
    FLAGS="DEBUG_FULL,USEMALLOC"
    echo "Building with USEMALLOC (for Valgrind testing)"
else
    FLAGS="DEBUG_FULL"
    echo "Building without USEMALLOC (default)"
fi

umk src,$HOME/Dev/ai-upp/uppsrc VfsShell $HOME/.config/u++/theide/CLANG.bm -ds +$FLAGS bin/vfsh
