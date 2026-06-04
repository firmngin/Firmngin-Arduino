#!/bin/bash

# Absolute path from script directory to project root
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
# Path to library.properties
LIBRARY_PROPS="$ROOT_DIR/library.properties"

# Check whether library.properties exists
if [ ! -f "$LIBRARY_PROPS" ]; then
    echo "Error: library.properties not found at $LIBRARY_PROPS"
    echo "Using root project library.properties as fallback"
    LIBRARY_PROPS="$ROOT_DIR/library.properties"
fi

# Keep output inside root/releases
RELEASE_OUTPUT_DIR="$ROOT_DIR/releases"
mkdir -p "$RELEASE_OUTPUT_DIR"

# Show library.properties contents for debugging
echo "Using library.properties from: $LIBRARY_PROPS"
echo "library.properties contents:"
cat "$LIBRARY_PROPS"
echo "-------------------"

# Read version from library.properties
VERSION=$(grep "version=" "$LIBRARY_PROPS" | cut -d'=' -f2)

# Check whether version was read successfully
if [ -z "$VERSION" ]; then
    echo "Error: unable to read version from library.properties"
    echo "Using default version 1.0.0"
    VERSION="1.0.0"
fi

LIBRARY_NAME="firmngin"
RELEASE_DIR="$RELEASE_OUTPUT_DIR/${LIBRARY_NAME}"

echo "Creating release for ${LIBRARY_NAME} version ${VERSION}"

# Create release directory
rm -rf "$RELEASE_DIR"
mkdir -p "$RELEASE_DIR/src"
mkdir -p "$RELEASE_DIR/examples/BasicExample"

# Copy main files
cp "$LIBRARY_PROPS" "$RELEASE_DIR/library.properties"

# Copy files from root if available
cp "$ROOT_DIR/keywords.txt" "$RELEASE_DIR/" 2>/dev/null || echo "keywords.txt not found in root"
cp "$ROOT_DIR/License" "$RELEASE_DIR/" 2>/dev/null || echo "License not found in root" 
cp "$ROOT_DIR/readme.md" "$RELEASE_DIR/" 2>/dev/null || echo "readme.md not found in root"
cp "$ROOT_DIR/README.md" "$RELEASE_DIR/" 2>/dev/null || echo "README.md not found in root"

# Copy files from root project as fallback if missing
[ ! -f "$RELEASE_DIR/README.md" ] && [ -f "$ROOT_DIR/README.md" ] && cp "$ROOT_DIR/README.md" "$RELEASE_DIR/" || echo "README.md not found"
[ ! -f "$RELEASE_DIR/keywords.txt" ] && [ -f "$ROOT_DIR/keywords.txt" ] && cp "$ROOT_DIR/keywords.txt" "$RELEASE_DIR/" || echo "keywords.txt not found"
cp "$ROOT_DIR/CHANGELOG.md" "$RELEASE_DIR/" 2>/dev/null || echo "CHANGELOG.md not found"

# Copy source files
SOURCE_FILES=(
    "firmngin.h"
    "firmngin.cpp"
    "json.h"
    "crypto.h"
    "crypto.cpp"
    "hmac.h"
    "hmac.cpp"
    "ota.cpp"
    "ota_progress.h"
    "ota_progress.cpp"
    "queue.cpp"
    "queue_format.h"
    "queue_format.cpp"
)

for source_file in "${SOURCE_FILES[@]}"; do
    cp "$ROOT_DIR/src/$source_file" "$RELEASE_DIR/src/" 2>/dev/null || echo "$source_file not found"
done

# Copy bundled PubSubClient
mkdir -p "$RELEASE_DIR/src/PubSubClient"
cp "$ROOT_DIR/src/PubSubClient/PubSubClient.h" "$RELEASE_DIR/src/PubSubClient/" 2>/dev/null || echo "PubSubClient.h not found"
cp "$ROOT_DIR/src/PubSubClient/PubSubClient.cpp" "$RELEASE_DIR/src/PubSubClient/" 2>/dev/null || echo "PubSubClient.cpp not found"

# Copy examples
echo "Copying examples..."
# Try to use examples from the root project first
if [ -d "$ROOT_DIR/examples" ]; then
    echo "Using examples from the root folder"
    (cd "$ROOT_DIR" && tar \
        --exclude='*/keys.h' \
        --exclude='*/.DS_Store' \
        --exclude='*/.!*.DS_Store' \
        -cf - examples) | (cd "$RELEASE_DIR" && tar -xf -) || echo "Failed to copy examples from root"
else
    echo "Examples folder not found in root!"
    # Create a minimal example so the package structure stays valid
    echo "// Basic Firmngin usage example" > "$RELEASE_DIR/examples/BasicExample/BasicExample.ino"
    echo "void setup() {}" >> "$RELEASE_DIR/examples/BasicExample/BasicExample.ino"
    echo "void loop() {}" >> "$RELEASE_DIR/examples/BasicExample/BasicExample.ino"
fi

# Validate that all primary examples are present
EXPECTED_EXAMPLES=(
    "BasicExample/BasicExample.ino"
    "BasicMonetizeExample/BasicMonetizeExample.ino"
    "BatchStateExample/BatchStateExample.ino"
    "DisplayPINExample/DisplayPINExample.ino"
    "EthernetExample/EthernetExample.ino"
    "PushStateExample/PushStateExample.ino"
    "ReceiveStateExample/ReceiveStateExample.ino"
    "SensorExample/SensorExample.ino"
)

for example_file in "${EXPECTED_EXAMPLES[@]}"; do
    if [ ! -f "$RELEASE_DIR/examples/$example_file" ]; then
        echo "Warning: example not found: $example_file"
    fi
done

# Create ZIP
ZIP_FILE="$RELEASE_OUTPUT_DIR/${LIBRARY_NAME}-${VERSION}.zip"
rm -f "$ZIP_FILE"
(cd "$RELEASE_OUTPUT_DIR" && zip -r "${LIBRARY_NAME}-${VERSION}.zip" "${LIBRARY_NAME}")

echo "Release package created: $ZIP_FILE"

# install_arduino_lib — extract to Arduino libraries so Arduino IDE picks it up
# Set ARDUINO_LIBRARIES_DIR env to override default ($HOME/Documents/Arduino/libraries)
ARDUINO_DIR="${ARDUINO_LIBRARIES_DIR:-$HOME/Documents/Arduino/libraries}"
INSTALL_DIR="$ARDUINO_DIR/$LIBRARY_NAME"

if [ -d "$INSTALL_DIR" ]; then
    echo "Removing existing library at $INSTALL_DIR"
    rm -rf "$INSTALL_DIR"
fi

echo "Installing to $INSTALL_DIR"
unzip -q "$ZIP_FILE" -d "$ARDUINO_DIR"

rm -rf "$RELEASE_DIR"

echo "Done! Library installed at $INSTALL_DIR"
