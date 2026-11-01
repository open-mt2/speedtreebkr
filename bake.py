#!/usr/bin/env python3
"""Bake SpeedTree geometry with the original SpeedTreeRT.dll under Wine.

Scans the client packs for .spt assets, drives ``speedtree_dumper.exe`` once for
all of them, and writes one ``<fnv1a32>.tree`` cache file per unique asset.

Examples:
    tools/speedtree_baker/bake.py
    tools/speedtree_baker/bake.py --force
    tools/speedtree_baker/bake.py --verify
    tools/speedtree_baker/bake.py --dry-run --pattern 'bin/pack/**/*.spt'
    tools/speedtree_baker/bake.py --force --manifest treecache/MANIFEST.json
"""

from __future__ import annotations

import argparse
import glob
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

TOOL_VERSION = "1.0.0"
FORMAT_VERSION = 1
STAMP_NAME = ".bake-stamp.json"
TOOL_DIR = Path(__file__).resolve().parent
DEFAULT_ROOT = TOOL_DIR.parent.parent


def fnv1a32(data: bytes) -> int:
    value = 2166136261
    for byte in data:
        value ^= byte
        value = (value * 16777619) & 0xFFFFFFFF
    return value


def tree_name(digest: int) -> str:
    return f"{digest:08x}.tree"


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def to_windows_path(path: Path) -> str:
    """Map a Unix path into Wine's Z: drive. Native Windows paths pass through."""
    if os.name == "nt":
        return str(path.resolve())
    return "Z:" + str(path.resolve()).replace("/", "\\")


def load_stamp(path: Path) -> dict:
    try:
        with path.open("r", encoding="utf-8") as handle:
            data = json.load(handle)
        return data if isinstance(data, dict) else {}
    except (OSError, ValueError):
        return {}


def write_stamp(path: Path, data: dict) -> None:
    temp = path.with_suffix(path.suffix + ".tmp")
    with temp.open("w", encoding="utf-8") as handle:
        json.dump(data, handle, indent=2, sort_keys=True)
        handle.write("\n")
    os.replace(temp, path)


def write_manifest(path: Path, root: Path, out_dir: Path,
                   entries: dict[int, Path], dumper: Path, dumper_hash: str | None,
                   dll: Path, seed: int, patterns: list[str],
                   duplicate_count: int, manifest_prefix: Path | None) -> None:
    """Write the cache manifest consumed by the devkit's verify-treecache.py."""
    prefix = (root / manifest_prefix).resolve() if manifest_prefix else root
    records = []
    for digest, spt in sorted(entries.items(), key=lambda item: str(item[1])):
        tree = out_dir / tree_name(digest)
        try:
            relative = spt.resolve().relative_to(prefix)
        except ValueError:
            relative = spt.resolve().relative_to(root)
        records.append({
            "spt": relative.as_posix(),
            "file": tree.name,
            "fnv1a32": f"{digest:08x}",
            "spt_bytes": spt.stat().st_size,
            "bytes": tree.stat().st_size,
            "sha256": sha256_file(tree),
        })
    manifest = {
        "format": FORMAT_VERSION,
        "generator": {
            "tool": "open-mt2/speedtreebkr",
            "tool_version": TOOL_VERSION,
            "tree_format": FORMAT_VERSION,
            "seed": seed,
            "dumper": dumper.name,
            "dumper_sha256": dumper_hash,
            "dll_sha256": sha256_file(dll) if dll.is_file() else None,
            "baked_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        },
        "source": {
            "patterns": patterns,
            "assets": len(records),
            "duplicate_assets": duplicate_count,
        },
        "entries": records,
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    write_stamp(path, manifest)


def collect_spt(root: Path, patterns: list[str]) -> list[Path]:
    found: set[Path] = set()
    for pattern in patterns:
        expression = pattern if Path(pattern).is_absolute() else str(root / pattern)
        for match in glob.glob(expression, recursive=True):
            path = Path(match)
            if path.is_file() and path.suffix.lower() == ".spt":
                found.add(path.resolve())
    return sorted(found)


def verify_tree(path: Path, expected_hash: int) -> bool:
    try:
        with path.open("rb") as handle:
            header = handle.read(8)
    except OSError:
        return False
    if len(header) != 8:
        return False
    magic = int.from_bytes(header[0:4], "little")
    digest = int.from_bytes(header[4:8], "little")
    return magic == 0x4D543254 and digest == expected_hash


def remove_generated(out_dir: Path) -> list[Path]:
    removed = []
    if not out_dir.is_dir():
        return removed
    for path in sorted(out_dir.glob("*.tree")) + sorted(out_dir.glob("*.tree.tmp")):
        try:
            path.unlink()
            removed.append(path)
        except OSError as error:
            print(f"warning: cannot remove {path}: {error}", file=sys.stderr)
    stamp = out_dir / STAMP_NAME
    if stamp.exists():
        try:
            stamp.unlink()
        except OSError as error:
            print(f"warning: cannot remove {stamp}: {error}", file=sys.stderr)
    return removed


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Bake .spt assets into the treecache geometry cache.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__.split("Examples:", 1)[-1].strip(),
    )
    parser.add_argument("--root", type=Path, default=DEFAULT_ROOT,
                        help="client root (default: repository root)")
    parser.add_argument("--out", type=Path, default=None,
                        help="output directory (default: <root>/treecache)")
    parser.add_argument("--dll", type=Path, default=None,
                        help="SpeedTreeRT.dll (default: <root>/bin/SpeedTreeRT.dll)")
    parser.add_argument("--dumper", type=Path, default=None,
                        help="dumper executable (default: tools/speedtree_baker/build/speedtree_dumper.exe)")
    parser.add_argument("--pattern", action="append", default=None, metavar="GLOB",
                        help=".spt glob relative to --root (repeatable; "
                             "default: bin/pack/**/*.spt)")
    parser.add_argument("--seed", type=int, default=1,
                        help="geometry seed passed to SpeedTree Compute (default: 1)")
    parser.add_argument("--wine", default=os.environ.get("WINE", "wine"),
                        help="Wine executable (default: $WINE or 'wine')")
    parser.add_argument("--force", action="store_true",
                        help="re-bake every asset even if the cache is current")
    parser.add_argument("--prune", action="store_true",
                        help="remove .tree files that do not match a current .spt")
    parser.add_argument("--clean", action="store_true",
                        help="delete generated .tree files before baking")
    parser.add_argument("--verify", action="store_true",
                        help="only verify existing cache entries, do not bake")
    parser.add_argument("--manifest", type=Path, default=None, metavar="FILE",
                        help="write a JSON cache manifest (asset paths, hashes, "
                             "generator provenance) after baking")
    parser.add_argument("--manifest-prefix", type=Path, default=Path("bin/pack"),
                        metavar="DIR",
                        help="strip this directory from --manifest asset paths "
                             "(default: bin/pack)")
    parser.add_argument("--dry-run", action="store_true",
                        help="print what would be baked and exit")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="stream dumper output")
    parser.add_argument("-q", "--quiet", action="store_true",
                        help="only print warnings and errors")
    parser.add_argument("--version", action="version",
                        version=f"%(prog)s {TOOL_VERSION} (tree cache format {FORMAT_VERSION})")

    args = parser.parse_args(argv)
    if args.clean and args.verify:
        parser.error("--clean cannot be combined with --verify")
    return args


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    root: Path = args.root.resolve()
    out_dir: Path = (args.out or (root / "treecache")).resolve()
    dll: Path = (args.dll or (root / "bin" / "SpeedTreeRT.dll")).resolve()
    dumper: Path = (args.dumper or (TOOL_DIR / "build" / "speedtree_dumper.exe")).resolve()
    patterns = args.pattern or ["bin/pack/**/*.spt"]

    def note(message: str) -> None:
        if not args.quiet:
            print(message)

    if not root.is_dir():
        print(f"error: client root not found: {root}", file=sys.stderr)
        return 1

    spt_files = collect_spt(root, patterns)
    if not spt_files:
        print(f"error: no .spt files matched {patterns} under {root}", file=sys.stderr)
        return 1

    entries: dict[int, Path] = {}
    duplicate_count = 0
    for path in spt_files:
        try:
            digest = fnv1a32(path.read_bytes())
        except OSError as error:
            print(f"error: cannot read {path}: {error}", file=sys.stderr)
            return 1
        if digest in entries:
            duplicate_count += 1
            continue
        entries[digest] = path

    expected = {tree_name(digest): digest for digest in entries}

    cleaned = False
    if args.clean:
        if args.dry_run:
            stale = list(out_dir.glob("*.tree")) if out_dir.is_dir() else []
            note(f"would remove {len(stale)} cache file(s) from {out_dir}")
        else:
            removed = remove_generated(out_dir)
            cleaned = True
            note(f"removed {len(removed)} cache file(s) from {out_dir}")

    if args.verify:
        missing = []
        invalid = []
        for name, digest in sorted(expected.items()):
            path = out_dir / name
            if not path.exists():
                missing.append(name)
            elif not verify_tree(path, digest):
                invalid.append(name)
        for name in missing:
            print(f"missing: {name}", file=sys.stderr)
        for name in invalid:
            print(f"invalid: {name}", file=sys.stderr)
        note(f"verified: {len(expected) - len(missing) - len(invalid)}/{len(expected)} "
             f"({len(missing)} missing, {len(invalid)} invalid)")
        return 0 if not missing and not invalid else 2

    stamp_path = out_dir / STAMP_NAME
    stamp = load_stamp(stamp_path)
    try:
        tool_hash = sha256_file(dumper) if dumper.exists() else None
    except OSError as error:
        print(f"error: cannot hash dumper {dumper}: {error}", file=sys.stderr)
        return 1
    cache_current = (
        stamp.get("format") == FORMAT_VERSION
        and stamp.get("seed") == args.seed
        and stamp.get("dumper_sha256") == tool_hash
        and stamp.get("dumper") == dumper.name
    )

    to_bake = []
    up_to_date = 0
    for digest, path in sorted(entries.items(), key=lambda item: str(item[1])):
        output = out_dir / tree_name(digest)
        if (args.force or not cache_current or not output.exists()
                or not verify_tree(output, digest)):
            to_bake.append((digest, path))
        else:
            up_to_date += 1

    stale = []
    if out_dir.is_dir() and not cleaned:
        stale = sorted(path.name for path in out_dir.glob("*.tree") if path.name not in expected)

    if args.dry_run:
        note(f"assets: {len(spt_files)} ({duplicate_count} duplicate content)")
        note(f"cache: {up_to_date} up to date, {len(to_bake)} to bake, {len(stale)} stale")
        if not cache_current:
            note("stamp missing or tool/seed changed: a full re-bake is planned")
        for _digest, path in to_bake:
            note(f"  bake {path}")
        if args.prune:
            for name in stale:
                note(f"  prune {out_dir / name}")
        return 0

    if not dumper.exists():
        print(f"error: dumper not found: {dumper}", file=sys.stderr)
        print("build it first: tools/speedtree_baker/build.sh", file=sys.stderr)
        return 1
    if not dll.exists():
        print(f"error: SpeedTreeRT.dll not found: {dll}", file=sys.stderr)
        return 1

    prefix: list[str] = []
    if os.name != "nt":
        wine = shutil.which(args.wine)
        if not wine:
            print(f"error: Wine not found: {args.wine}", file=sys.stderr)
            print("install Wine or pass --wine <path>", file=sys.stderr)
            return 1
        prefix = [wine]

    out_dir.mkdir(parents=True, exist_ok=True)

    dumper_failed = False
    if not to_bake:
        note(f"cache is up to date ({up_to_date} assets)")
    else:
        note(f"baking {len(to_bake)}/{len(entries)} assets "
             f"({duplicate_count} duplicate content skipped)...")
        list_fd, list_name = tempfile.mkstemp(prefix="spt-list-", suffix=".txt")
        os.close(list_fd)
        list_path = Path(list_name)
        try:
            with list_path.open("w", encoding="utf-8", newline="\n") as handle:
                for _digest, path in to_bake:
                    handle.write(to_windows_path(path) + "\n")

            command = prefix + [
                str(dumper),
                "--list", to_windows_path(list_path),
                "--out", to_windows_path(out_dir),
                "--dll", to_windows_path(dll),
                "--seed", str(args.seed),
            ]
            if args.verbose:
                command.append("--verbose")
                print("+ " + " ".join(command))
                completed = subprocess.run(command, check=False)
            else:
                completed = subprocess.run(
                    command, check=False, text=True,
                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                if completed.returncode != 0:
                    sys.stdout.write(completed.stdout or "")
            if completed.returncode not in (0, 3):
                print(f"error: dumper exited with status {completed.returncode}", file=sys.stderr)
                return 2
            if completed.returncode == 3:
                print("error: the dumper reported failed assets", file=sys.stderr)
                dumper_failed = True
        finally:
            list_path.unlink(missing_ok=True)

    if dumper_failed:
        return 2

    # Verify every expected entry, including files that were already present.
    missing = []
    invalid = []
    for name, digest in sorted(expected.items()):
        path = out_dir / name
        if not path.exists():
            missing.append(name)
        elif not verify_tree(path, digest):
            invalid.append(name)

    if missing:
        for name in missing:
            print(f"missing: {name}", file=sys.stderr)
    if invalid:
        for name in invalid:
            print(f"invalid: {name}", file=sys.stderr)

    if missing or invalid:
        print(f"error: {len(missing)} missing, {len(invalid)} invalid cache file(s)",
              file=sys.stderr)
        return 2

    if args.prune and stale:
        for name in stale:
            try:
                (out_dir / name).unlink()
            except OSError as error:
                print(f"warning: cannot prune {name}: {error}", file=sys.stderr)
        note(f"pruned {len(stale)} stale cache file(s)")

    write_stamp(stamp_path, {
        "format": FORMAT_VERSION,
        "seed": args.seed,
        "dumper": dumper.name,
        "dumper_sha256": tool_hash,
        "assets": len(entries),
        "baked_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
    })

    if args.manifest:
        write_manifest(args.manifest, root, out_dir, entries, dumper, tool_hash,
                       dll, args.seed, patterns, duplicate_count,
                       args.manifest_prefix)
        note(f"manifest: {args.manifest}")

    note(f"cache ready: {len(entries)} assets in {out_dir}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except KeyboardInterrupt:
        sys.exit(130)
