#!/bin/bash

# Always run from the folder where build.sh lives
cd "$(dirname "$0")"

# Build
echo "Building..."
make -f Makefile

if [ $? -ne 0 ]; then
    echo ""
    echo "Build FAILED. See errors above."
    exit 1
fi

echo ""
echo "Build succeeded! Running build/main ..."
echo ""
./build/main

