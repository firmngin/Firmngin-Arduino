#!/bin/bash

# Path dari script ke root project
ROOT_DIR=".."
# Path ke folder library
LIB_DIR="$ROOT_DIR/lib/firmnginKit"
# Path ke file library.properties
LIBRARY_PROPS="$ROOT_DIR/library.properties"

# Periksa apakah file library.properties ada
if [ ! -f "$LIBRARY_PROPS" ]; then
    echo "Error: File library.properties tidak ditemukan di $LIBRARY_PROPS"
    echo "Menggunakan library.properties dari folder library sebagai fallback"
    LIBRARY_PROPS="$LIB_DIR/library.properties"
fi

# Pastikan output tetap di folder root/releases
RELEASE_OUTPUT_DIR="$ROOT_DIR/releases"
mkdir -p "$RELEASE_OUTPUT_DIR"

# Tampilkan isi library.properties untuk debugging
echo "Menggunakan library.properties dari: $LIBRARY_PROPS"
echo "Isi library.properties:"
cat "$LIBRARY_PROPS"
echo "-------------------"

# Baca versi dari library.properties
VERSION=$(grep "version=" "$LIBRARY_PROPS" | cut -d'=' -f2)

# Periksa apakah versi berhasil diambil
if [ -z "$VERSION" ]; then
    echo "Error: Tidak dapat membaca versi dari library.properties"
    echo "Menggunakan versi default 1.0.0"
    VERSION="1.0.0"
fi

LIBRARY_NAME="firmnginKit"
RELEASE_DIR="$RELEASE_OUTPUT_DIR/${LIBRARY_NAME}-${VERSION}"

echo "Membuat release untuk ${LIBRARY_NAME} versi ${VERSION}"

# Buat direktori release
rm -rf "$RELEASE_DIR"
mkdir -p "$RELEASE_DIR/src"
mkdir -p "$RELEASE_DIR/examples/BasicExample"

# Copy file-file utama
cp "$LIBRARY_PROPS" "$RELEASE_DIR/library.properties"

# Copy file dari folder root jika tersedia
cp "$ROOT_DIR/keywords.txt" "$RELEASE_DIR/" 2>/dev/null || echo "keywords.txt tidak ditemukan di root"
cp "$ROOT_DIR/License" "$RELEASE_DIR/" 2>/dev/null || echo "License tidak ditemukan di root" 
cp "$ROOT_DIR/readme.md" "$RELEASE_DIR/" 2>/dev/null || echo "readme.md tidak ditemukan di root"
cp "$ROOT_DIR/README.md" "$RELEASE_DIR/" 2>/dev/null || echo "README.md tidak ditemukan di root"

# Copy file dari folder library sebagai fallback jika tidak ada di root
[ ! -f "$RELEASE_DIR/README.md" ] && [ -f "$LIB_DIR/README.md" ] && cp "$LIB_DIR/README.md" "$RELEASE_DIR/" || echo "README.md tidak ditemukan"
[ ! -f "$RELEASE_DIR/keywords.txt" ] && [ -f "$LIB_DIR/keywords.txt" ] && cp "$LIB_DIR/keywords.txt" "$RELEASE_DIR/" || echo "keywords.txt tidak ditemukan"
cp "$LIB_DIR/CHANGELOG.md" "$RELEASE_DIR/" 2>/dev/null || echo "CHANGELOG.md tidak ditemukan"

# Copy source files - memperbaiki format perintah cp
cp "$ROOT_DIR/src/firmnginKit.h" "$RELEASE_DIR/src/" 2>/dev/null || echo "firmnginKit.h tidak ditemukan"
cp "$ROOT_DIR/src/firmnginKit.cpp" "$RELEASE_DIR/src/" 2>/dev/null || echo "firmnginKit.cpp tidak ditemukan"

# Copy examples
echo "Menyalin examples..."
# Coba ambil examples dari root project dulu
if [ -d "$ROOT_DIR/examples" ]; then
    echo "Menggunakan examples dari folder root"
    cp -r "$ROOT_DIR/examples/"* "$RELEASE_DIR/examples/" 2>/dev/null || echo "Gagal menyalin examples dari root"
# Jika tidak ada di root, coba dari lib
elif [ -d "$LIB_DIR/examples" ]; then
    echo "Menggunakan examples dari folder library"
    cp -r "$LIB_DIR/examples/"* "$RELEASE_DIR/examples/" 2>/dev/null || echo "Gagal menyalin examples dari library"
else
    echo "Folder examples tidak ditemukan di root atau library!"
    # Buat contoh kosong supaya struktur valid
    echo "// Contoh dasar penggunaan firmnginKit" > "$RELEASE_DIR/examples/BasicExample/BasicExample.ino"
    echo "void setup() {}" >> "$RELEASE_DIR/examples/BasicExample/BasicExample.ino"
    echo "void loop() {}" >> "$RELEASE_DIR/examples/BasicExample/BasicExample.ino"
fi

# Buat ZIP
ZIP_FILE="$RELEASE_OUTPUT_DIR/${LIBRARY_NAME}-${VERSION}.zip"
rm -f "$ZIP_FILE"
(cd "$RELEASE_OUTPUT_DIR" && zip -r "${LIBRARY_NAME}-${VERSION}.zip" "${LIBRARY_NAME}-${VERSION}")

echo "Release package dibuat: $ZIP_FILE"

# Cleanup
rm -rf "$RELEASE_DIR"

echo "Selesai!"