#!/bin/bash

# Build script for VfsBoot Text Editor
echo "Building VfsBoot Text Editor..."

# Create build directory if it doesn't exist
if [ ! -d "build" ]; then
    mkdir build
fi

# Change to build directory
cd build

# Run cmake
cmake ..

# Build the project
make

echo "Build completed!"
echo "To run the text editor demo: ./text_editor_demo"
echo "To run the UI backend test: ./test_ui_backend"
echo "To run the UI abstraction demo: ./demo_ui_abstraction"