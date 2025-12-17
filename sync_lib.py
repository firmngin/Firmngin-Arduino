#!/usr/bin/env python3
"""
Sync library files from src/ to examples/ and lib/ folder
Usage:
  python3 sync_lib.py          # Sync to all examples
  python3 sync_lib.py --release # Sync to lib/ folder and create ZIP release
  python3 sync_lib.py BasicExample  # Sync to specific example only
"""

import shutil
import sys
import zipfile
from pathlib import Path

project_dir = Path(__file__).parent
src_dir = project_dir / "src"
examples_dir = project_dir / "examples"
lib_dir = project_dir / "lib" / "firmnginKit"

files_to_copy = ["firmnginKit.h", "firmnginKit.cpp"]

def sync_to_directory(dst_dir, label=""):
    """Sync files to a specific directory"""
    if not dst_dir.exists():
        dst_dir.mkdir(parents=True, exist_ok=True)
    
    success_count = 0
    for filename in files_to_copy:
        src_file = src_dir / filename
        dst_file = dst_dir / filename
        if src_file.exists():
            # Check if source and destination are the same file (symlink)
            try:
                if src_file.samefile(dst_file):
                    print(f"  ⊙ {filename} → {label} (same file, skipped)")
                    success_count += 1
                    continue
            except (OSError, ValueError):
                # Files don't exist or can't compare, proceed with copy
                pass
            
            shutil.copy2(src_file, dst_file)
            print(f"  ✓ {filename} → {label}")
            success_count += 1
        else:
            print(f"  ✗ {filename} not found in src/")
    
    return success_count == len(files_to_copy)

def sync_all_examples():
    """Sync to all example directories"""
    if not examples_dir.exists():
        print(f"✗ Examples directory not found: {examples_dir}")
        return False
    
    example_folders = [d for d in examples_dir.iterdir() if d.is_dir()]
    if not example_folders:
        print(f"✗ No example folders found in {examples_dir}")
        return False
    
    print(f"Syncing to {len(example_folders)} example(s)...")
    all_success = True
    
    for example_dir in example_folders:
        print(f"\n{example_dir.name}:")
        success = sync_to_directory(example_dir, example_dir.name)
        if not success:
            all_success = False
    
    return all_success

def get_version():
    """Get version for release (default 0.0.0, user will rename manually)"""
    # Default to 0.0.0 as requested - user will rename manually
    return "0.0.0"

def create_release_zip():
    """Create ZIP release package"""
    version = get_version()
    library_name = "firmnginKit"
    releases_dir = project_dir / "releases"
    releases_dir.mkdir(exist_ok=True)
    
    release_dir = releases_dir / f"{library_name}-{version}"
    zip_file = releases_dir / f"{library_name}-{version}.zip"
    
    # Remove existing release dir and zip
    if release_dir.exists():
        shutil.rmtree(release_dir)
    if zip_file.exists():
        zip_file.unlink()
    
    # Create release directory structure
    release_dir.mkdir(parents=True)
    (release_dir / "src").mkdir()
    (release_dir / "examples").mkdir()
    
    print(f"\nCreating release package: {library_name}-{version}.zip")
    
    # Copy source files
    print("Copying source files...")
    for filename in files_to_copy:
        src_file = src_dir / filename
        dst_file = release_dir / "src" / filename
        if src_file.exists():
            shutil.copy2(src_file, dst_file)
            print(f"  ✓ {filename}")
        else:
            print(f"  ✗ {filename} not found")
    
    # Copy examples
    print("Copying examples...")
    if examples_dir.exists():
        for example_folder in examples_dir.iterdir():
            if example_folder.is_dir():
                dst_example = release_dir / "examples" / example_folder.name
                shutil.copytree(example_folder, dst_example, dirs_exist_ok=True)
                print(f"  ✓ {example_folder.name}/")
    
    # Copy library.properties
    props_file = project_dir / "library.properties"
    if props_file.exists():
        shutil.copy2(props_file, release_dir / "library.properties")
        print("  ✓ library.properties")
    
    # Copy keywords.txt
    keywords_file = project_dir / "keywords.txt"
    if keywords_file.exists():
        shutil.copy2(keywords_file, release_dir / "keywords.txt")
        print("  ✓ keywords.txt")
    
    # Copy LICENSE
    license_file = project_dir / "LICENSE"
    if license_file.exists():
        shutil.copy2(license_file, release_dir / "LICENSE")
        print("  ✓ LICENSE")
    
    # Copy README.md
    readme_file = project_dir / "README.md"
    if readme_file.exists():
        shutil.copy2(readme_file, release_dir / "README.md")
        print("  ✓ README.md")
    
    # Create ZIP file
    print(f"\nCreating ZIP: {zip_file.name}...")
    with zipfile.ZipFile(zip_file, 'w', zipfile.ZIP_DEFLATED) as zipf:
        for file_path in release_dir.rglob('*'):
            if file_path.is_file():
                arcname = file_path.relative_to(release_dir)
                zipf.write(file_path, arcname)
                print(f"  ✓ {arcname}")
    
    # Cleanup release directory
    shutil.rmtree(release_dir)
    
    print(f"\n✓ Release package created: {zip_file}")
    return zip_file

def sync_to_lib():
    """Sync to lib/ folder for release (skip if symlink, use src/ directly)"""
    # Check if lib is symlink - if so, skip sync
    if lib_dir.exists():
        try:
            if lib_dir.is_symlink():
                print(f"⊙ lib/ is symlink, skipping sync (using src/ directly)")
            else:
                print(f"Syncing to lib/ folder...")
                sync_to_directory(lib_dir, "lib/firmnginKit")
        except Exception:
            # If check fails, try to sync anyway
            print(f"Syncing to lib/ folder...")
            sync_to_directory(lib_dir, "lib/firmnginKit")
    else:
        print(f"⊙ lib/ folder not found, skipping sync (using src/ directly)")
    
    # Always create ZIP release from src/
    create_release_zip()
    return True

def sync_to_example(example_name):
    """Sync to specific example directory"""
    example_dir = examples_dir / example_name
    if not example_dir.exists():
        print(f"✗ Example directory not found: {example_dir}")
        return False
    
    print(f"Syncing to {example_name}...")
    return sync_to_directory(example_dir, example_name)

def main():
    if len(sys.argv) > 1:
        arg = sys.argv[1]
        
        if arg == "--release" or arg == "-r":
            # Release mode: sync to lib/ folder
            success = sync_to_lib()
            if success:
                print("\n✓ Release sync complete!")
            else:
                print("\n✗ Release sync failed!")
                sys.exit(1)
        elif arg == "--all" or arg == "-a":
            # Sync to all examples
            success = sync_all_examples()
            if success:
                print("\n✓ All examples synced!")
            else:
                print("\n✗ Some examples failed to sync!")
                sys.exit(1)
        else:
            # Sync to specific example
            success = sync_to_example(arg)
            if success:
                print(f"\n✓ {arg} synced!")
            else:
                print(f"\n✗ {arg} sync failed!")
                sys.exit(1)
    else:
        # Default: sync to all examples
        success = sync_all_examples()
        if success:
            print("\n✓ All examples synced!")
        else:
            print("\n✗ Some examples failed to sync!")
            sys.exit(1)

if __name__ == "__main__":
    main()

