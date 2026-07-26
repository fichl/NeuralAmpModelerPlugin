#!/usr/bin/env python3
"""Update NAM On Steroids release version metadata.

Usage:
    python set_version.py 1.5.3
    python set_version.py 1.5.3 --dry-run

This intentionally does not modify:
  - the hardcoded Settings-page version 0.7.15;
  - historical serialization/changelog references;
  - Visual Studio project-format version fields.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parent
PROJECT = ROOT / "NeuralAmpModeler"

PLISTS = (
    "NeuralAmpModeler-AAX-Info.plist",
    "NeuralAmpModeler-AU-Info.plist",
    "NeuralAmpModeler-VST3-Info.plist",
    "NeuralAmpModeler-iOS-AUv3-Info.plist",
    "NeuralAmpModeler-iOS-AUv3Framework-Info.plist",
    "NeuralAmpModeler-iOS-Info.plist",
    "NeuralAmpModeler-macOS-AUv3-Info.plist",
    "NeuralAmpModeler-macOS-AUv3Framework-Info.plist",
    "NeuralAmpModeler-macOS-Info.plist",
)


def parse_version(value: str) -> tuple[int, int, int]:
    match = re.fullmatch(r"(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)", value)
    if not match:
        raise argparse.ArgumentTypeError("Version must use major.minor.patch, e.g. 1.5.3")

    parts = tuple(int(part) for part in match.groups())
    if any(part > 255 for part in parts):
        raise argparse.ArgumentTypeError("Each version component must be between 0 and 255")
    return parts


def replace_required(text: str, pattern: str, replacement: str, label: str) -> str:
    updated, count = re.subn(pattern, replacement, text, flags=re.MULTILINE)
    if count == 0:
        raise RuntimeError(f"Could not find {label}")
    return updated


def write_if_changed(path: Path, updated: str, dry_run: bool) -> bool:
    original = path.read_text(encoding="utf-8")
    if original == updated:
        return False
    if not dry_run:
        path.write_text(updated, encoding="utf-8", newline="")
    return True


def update_config(version: str, version_hex: str, dry_run: bool) -> bool:
    path = PROJECT / "config.h"
    text = path.read_text(encoding="utf-8")
    text = replace_required(
        text,
        r"^#define PLUG_VERSION_HEX\s+0x[0-9A-Fa-f]+$",
        f"#define PLUG_VERSION_HEX {version_hex}",
        "PLUG_VERSION_HEX in config.h",
    )
    text = replace_required(
        text,
        r'^#define PLUG_VERSION_STR\s+"[^"]+"$',
        f'#define PLUG_VERSION_STR "{version}"',
        "PLUG_VERSION_STR in config.h",
    )
    return write_if_changed(path, text, dry_run)


def update_installer(version: str, dry_run: bool) -> bool:
    path = PROJECT / "installer" / "NeuralAmpModeler.iss"
    text = path.read_text(encoding="utf-8")
    text = replace_required(text, r"^AppVersion=.*$", f"AppVersion={version}", "AppVersion")
    text = replace_required(
        text, r"^VersionInfoVersion=.*$", f"VersionInfoVersion={version}", "VersionInfoVersion"
    )
    return write_if_changed(path, text, dry_run)


def update_windows_resources(version: str, parts: tuple[int, int, int], dry_run: bool) -> bool:
    path = PROJECT / "resources" / "main.rc"
    numeric = f"{parts[0]},{parts[1]},{parts[2]},0"
    text = path.read_text(encoding="utf-8")
    text = replace_required(text, r"^ FILEVERSION .*$", f" FILEVERSION {numeric}", "FILEVERSION")
    text = replace_required(text, r"^ PRODUCTVERSION .*$", f" PRODUCTVERSION {numeric}", "PRODUCTVERSION")
    text = replace_required(
        text,
        r'VALUE "FileVersion", "[^"]+"',
        f'VALUE "FileVersion", "{version}"',
        "FileVersion string",
    )
    text = replace_required(
        text,
        r'VALUE "ProductVersion", "[^"]+"',
        f'VALUE "ProductVersion", "{version}"',
        "ProductVersion string",
    )
    return write_if_changed(path, text, dry_run)


def update_plist(path: Path, version: str, version_hex: str, version_int: int, dry_run: bool) -> bool:
    text = path.read_text(encoding="utf-8")

    text = re.sub(
        r"(<key>CFBundleGetInfoString</key>\s*<string>[^<]*?v)\d+\.\d+\.\d+([^<]*</string>)",
        rf"\g<1>{version}\g<2>",
        text,
    )

    for key in ("CFBundleShortVersionString", "CFBundleVersion"):
        pattern = rf"(<key>{key}</key>\s*<string>)[^<]*(</string>)"
        text = re.sub(pattern, rf"\g<1>{version}\g<2>", text)

    text = re.sub(
        r"(<key>AudioUnit Version</key>\s*<string>)[^<]*(</string>)",
        rf"\g<1>{version_hex}\g<2>",
        text,
    )
    text = re.sub(
        r"(<key>version</key>\s*<integer>)\d+(</integer>)",
        rf"\g<1>{version_int}\g<2>",
        text,
    )
    return write_if_changed(path, text, dry_run)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("version", help="New version in major.minor.patch format")
    parser.add_argument("--dry-run", action="store_true", help="Show affected files without writing them")
    args = parser.parse_args()

    parts = parse_version(args.version)
    version = ".".join(str(part) for part in parts)
    version_int = (parts[0] << 16) | (parts[1] << 8) | parts[2]
    version_hex = f"0x{version_int:08x}"

    changed: list[Path] = []
    operations = (
        (PROJECT / "config.h", lambda: update_config(version, version_hex, args.dry_run)),
        (PROJECT / "installer" / "NeuralAmpModeler.iss", lambda: update_installer(version, args.dry_run)),
        (
            PROJECT / "resources" / "main.rc",
            lambda: update_windows_resources(version, parts, args.dry_run),
        ),
    )

    for path, operation in operations:
        if operation():
            changed.append(path)

    for filename in PLISTS:
        path = PROJECT / "resources" / filename
        if update_plist(path, version, version_hex, version_int, args.dry_run):
            changed.append(path)

    action = "Would update" if args.dry_run else "Updated"
    if changed:
        print(f"{action} {len(changed)} files to version {version}:")
        for path in changed:
            print(f"  {path.relative_to(ROOT)}")
    else:
        print(f"All release metadata is already at version {version}.")

    print("Settings-page version 0.7.15 was intentionally left unchanged.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
