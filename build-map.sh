#!/bin/bash

# Always run from the folder where build-map.sh lives
cd "$(dirname "$0")"

# Build map editor
echo "Building map editor..."
make -f Makefile editor

if [ $? -ne 0 ]; then
    echo ""
    echo "Build FAILED. See errors above."
    exit 1
fi

echo ""
echo "Build succeeded! Running build/map_editor ..."
echo ""
./build/map_editor
