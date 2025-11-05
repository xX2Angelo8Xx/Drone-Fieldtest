#!/bin/bash
# Build script for the drone field testing project

set -e

PROJECT_ROOT="/home/angelo/Projects/Drone-Fieldtest"
BUILD_DIR="$PROJECT_ROOT/build"

echo "Building Drone Field Testing Project..."

# Wechsle zum Projektverzeichnis
cd "$PROJECT_ROOT"

# Erstelle Build-Verzeichnis falls nicht vorhanden
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Konfiguriere mit CMake
echo "Configuring with CMake..."
cmake ..

# Kompiliere
echo "Building..."
make -j$(nproc)

echo "Build completed successfully!"
echo "Executable location: $BUILD_DIR/apps/data_collector/data_collector"