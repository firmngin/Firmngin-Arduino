#!/usr/bin/env python3
"""Sync VERSION file to library.properties, library.json, and firmware headers."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
VERSION_FILE = ROOT / "VERSION"
LIBRARY_PROPS = ROOT / "library.properties"
LIBRARY_JSON = ROOT / "library.json"
VERSION_HEADER = ROOT / "src" / "firmngin_version.h"


def read_version() -> str:
    if not VERSION_FILE.exists():
        raise SystemExit(f"Missing {VERSION_FILE}")
    version = VERSION_FILE.read_text(encoding="utf-8").strip()
    if not re.fullmatch(r"\d+\.\d+\.\d+", version):
        raise SystemExit(f"Invalid semver in VERSION: {version!r}")
    return version


def sync_properties(version: str) -> None:
    text = LIBRARY_PROPS.read_text(encoding="utf-8")
    if not re.search(r"^version=", text, flags=re.MULTILINE):
        raise SystemExit("version= not found in library.properties")
    text = re.sub(r"^version=.*$", f"version={version}", text, count=1, flags=re.MULTILINE)
    LIBRARY_PROPS.write_text(text, encoding="utf-8")


def sync_json(version: str) -> None:
    data = json.loads(LIBRARY_JSON.read_text(encoding="utf-8"))
    data["version"] = version
    LIBRARY_JSON.write_text(json.dumps(data, indent=4) + "\n", encoding="utf-8")


def sync_version_header(version: str) -> None:
    VERSION_HEADER.write_text(
        "#ifndef FIRMNGIN_VERSION_H\n"
        "#define FIRMNGIN_VERSION_H\n\n"
        f'#define FIRMNGIN_VERSION "{version}"\n\n'
        "#endif\n",
        encoding="utf-8",
    )


def check_sync(version: str) -> bool:
    ok = True
    props = LIBRARY_PROPS.read_text(encoding="utf-8")
    match = re.search(r"^version=(.+)$", props, flags=re.MULTILINE)
    if not match or match.group(1).strip() != version:
        print(f"library.properties version mismatch (expected {version})")
        ok = False
    data = json.loads(LIBRARY_JSON.read_text(encoding="utf-8"))
    if data.get("version") != version:
        print(f"library.json version mismatch (expected {version})")
        ok = False
    if not VERSION_HEADER.exists():
        print(f"{VERSION_HEADER.relative_to(ROOT)} missing")
        ok = False
    else:
        header = VERSION_HEADER.read_text(encoding="utf-8")
        match = re.search(r'^#define\s+FIRMNGIN_VERSION\s+"([^"]+)"$', header, flags=re.MULTILINE)
        if not match or match.group(1) != version:
            print(f"src/firmngin_version.h version mismatch (expected {version})")
            ok = False
    return ok


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", help="exit 1 if manifests are out of sync")
    args = parser.parse_args()

    version = read_version()
    if args.check:
        return 0 if check_sync(version) else 1

    sync_properties(version)
    sync_json(version)
    sync_version_header(version)
    print(f"Synced version {version}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
