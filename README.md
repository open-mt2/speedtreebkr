# SpeedTree geometry baker

Offline tool that turns Metin2 `.spt` assets into the geometry cache consumed by
`SpeedTreeLib`:

```
bin/pack/**/*.spt  --(SpeedTreeRT.dll under Wine)-->  treecache/<fnv1a32>.tree
```

The devkit already vendors `treecache/`; use this tool only when adding or
editing trees. A missing cache entry makes the client fall back to its
procedural generator.

## Requirements

- `clang`/`clang++` targeting `i686-w64-windows-gnu`, plus `ld.lld`
- Wine with 32-bit support (`/usr/lib/wine/i386-windows/libkernel32.a`)
- CMake 3.20+ and Ninja
- Python 3.8+
- `bin/SpeedTreeRT.dll`
- `SpeedTreeLib/TreeCacheFormat.h` — `build.sh` and `tests/run.sh` expect the
  client monorepo layout (`../../src/SpeedTreeLib/include`); for a standalone
  checkout configure CMake with `-DSPEEDTREELIB_INCLUDE_DIR=<dir>`.

## Build

```bash
./build.sh              # build/speedtree_dumper.exe
./build.sh --clean
./build.sh --test       # build + host writer test
```

`WINE_LIBS` and `BUILD_DIR` override the import-library and build directories.

## Usage

```bash
# Incremental bake (uses treecache/.bake-stamp.json)
python3 bake.py

# Full re-bake and the manifest consumed by the devkit
python3 bake.py --force --manifest treecache/MANIFEST.json

# Verify the cache without Wine
python3 bake.py --verify

# Inspect the plan, prune stale files, start from scratch
python3 bake.py --dry-run
python3 bake.py --prune
python3 bake.py --clean
```

Options: `--root`, `--out`, `--dll`, `--dumper`, `--seed`, `--pattern`,
`--manifest`, `--manifest-prefix`, `--wine`, `-v`, `-q`.

Exit codes: `0` success, `1` configuration/asset error, `2` bake or
verification failure, `130` interrupted.

## Tests

```bash
./build.sh --test
# or just the host test:
tests/run.sh
```
