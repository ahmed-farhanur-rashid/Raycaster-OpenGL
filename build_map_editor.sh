#!/bin/bash

# Build map editor for Linux
cd "$(dirname "$0")"

echo "Building Map Editor..."
g++ -Iinclude -std=c++17 tools/map_editor.cpp src/glad.c -lglfw -lGL -ldl -lpthread -lm -o build/map_editor

if [ $? -eq 0 ]; then
    echo "Map Editor built successfully!"
    echo "Run with: ./build/map_editor [map_size]"
else
    echo "Build failed!"
    exit 1
fi
