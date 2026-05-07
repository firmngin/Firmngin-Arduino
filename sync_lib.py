#!/usr/bin/env python3
"""
Sync library files from src/ to examples/BasicExample/
Run this after editing library files in src/
"""

import shutil
from pathlib import Path

project_dir = Path(__file__).parent
src_dir = project_dir / "src"
example_dir = project_dir / "examples" / "BasicExample"

files_to_copy = ["firmnginKit.h", "firmnginKit.cpp"]

def sync():
    for filename in files_to_copy:
        src_file = src_dir / filename
        dst_file = example_dir / filename
        if src_file.exists():
            shutil.copy2(src_file, dst_file)
            print(f"✓ {filename}")
        else:
            print(f"✗ {filename} not found")
    print("Sync complete!")

if __name__ == "__main__":
    sync()

