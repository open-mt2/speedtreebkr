# SpeedTree geometry baker

Offline authoring tool that turns every Metin2 `.spt` asset into the binary
geometry cache consumed by `SpeedTreeLib` at runtime:

```
bin/pack/**/*.spt  --(SpeedTreeRT.dll under Wine)-->  treecache/<fnv1a32>.tree
```

The client keeps the `.spt` files for texture names, tree identity and the
procedural fallback. The cache only replaces the proprietary geometry
generator, so the renderer gets the original branches, fronds, leaf cards,
billboards, bounding box and collision objects.

## Layout

```
speedtree_baker/
├── CMakeLists.txt                  cross build (32-bit Windows PE)
├── cmake/i686-w64-windows-gnu.cmake
├── build.sh                        thin CMake wrapper
├── bake.py                         orchestration CLI (incremental, verify, prune)
├── include/speedtree_baker/
│   ├── platform.h                  Win32 file I/O, logging, no-CRT libc
│   ├── speedtree_api.h             SpeedTreeRT.dll ABI + SDK structures
│   ├── tree_cache_writer.h         freestanding cache writer
│   ├── dumper.h                    per-tree bake pipeline
│   └── version.h
├── src/                            implementations + CLI entry point
└── tests/                          host-native writer unit test
```

The on-disk format is defined once in
[`src/SpeedTreeLib/include/SpeedTreeLib/TreeCacheFormat.h`](../../src/SpeedTreeLib/include/SpeedTreeLib/TreeCacheFormat.h)
and shared by the writer here and `CSpeedTreeRT::LoadBakedGeometry` in the
client.

## Requirements

- `clang`/`clang++` with the `i686-w64-windows-gnu` target and `ld.lld`
- Wine with 32-bit support and `/usr/lib/wine/i386-windows/libkernel32.a`
- CMake 3.20+ and Ninja
- Python 3.8+ for `bake.py`
- `bin/SpeedTreeRT.dll` (already part of the client distribution)

No MinGW sysroot or Windows SDK is required: the dumper has no CRT dependency
and imports only `kernel32`.

## Build

```bash
tools/speedtree_baker/build.sh            # build/speedtree_dumper.exe
tools/speedtree_baker/build.sh --clean    # from scratch
tools/speedtree_baker/build.sh --test     # build + host writer test
```

`WINE_LIBS` and `BUILD_DIR` can override the import-library directory and the
build directory.

## Bake

```bash
# Incremental: only missing/stale entries (uses treecache/.bake-stamp.json)
python3 tools/speedtree_baker/bake.py

# Force a full re-bake
python3 tools/speedtree_baker/bake.py --force

# Inspect the plan without touching anything
python3 tools/speedtree_baker/bake.py --dry-run

# Check existing cache entries without Wine
python3 tools/speedtree_baker/bake.py --verify

# Remove cache files that no longer match an .spt
python3 tools/speedtree_baker/bake.py --prune

# Rebuild from an empty cache
python3 tools/speedtree_baker/bake.py --clean
```

Useful options: `--out DIR`, `--dll FILE`, `--dumper FILE`, `--seed N`,
`--pattern 'bin/pack/**/*.spt'` (repeatable), `--wine PATH`, `-v`, `-q`.

The script hashes assets with FNV-1a in memory, deduplicates identical
content, drives the dumper once for the whole batch, then verifies the magic
and embedded hash of every expected entry.

Exit codes: `0` success, `1` configuration/asset error, `2` bake or
verification failure, `130` interrupted.

### When to re-bake

- the `.spt` set changes (new map or edited asset),
- the baker/format/seed changes,
- the cache was deleted.

The stamp stores the dumper SHA-256 and seed. A rebuilt dumper invalidates the
stamp automatically, so the next run re-bakes everything; that is intentional
because the cache is cheap to regenerate (~7 s for the full client).

`treecache/` is a build input, not a build output: `build.sh --skip-pack` only
copies `treecache/*.tree` into `dist/treecache/`. A missing cache entry makes
the client fall back to the procedural tree generator.

## How it works

`speedtree_dumper.exe` is a PE32/i386 executable built with no CRT. It:

1. loads `SpeedTreeRT.dll` and resolves the mangled C++ exports
   (`??0CSpeedTreeRT`, `?LoadTree`, `?Compute`, `?GetGeometry`,
   `?GetBoundingBox`, `?GetTextures`, `?GetCollisionObject*`),
2. calls `Authorize("")`, sets the camera and enables the Direct3D T-flip,
3. for each path in the list file: constructs the object, `LoadTree`,
   `Compute(seed=1)`, `GetGeometry(all)`, `GetTextures`, then writes a
   `<fnv1a32>.tree` file,
4. writes through `<name>.tree.tmp` and renames atomically, so an interrupted
   bake never leaves a truncated cache entry.

ABI notes: instance methods are `__thiscall`, static members (`Authorize`,
`SetCamera`, `SetTextureFlip`, `IsAuthorized`) are `__cdecl`, and the
SpeedTree object needs 16-byte-aligned storage. The tool uses a conservative
8 KiB object buffer (the class only holds pointers).

The client side is the inverse operation in
`CSpeedTreeRT::LoadBakedGeometry` (branches/fronds strips, leaf cards with real
composite-atlas UVs, bounding box, collisions).

## Tests

```bash
tools/speedtree_baker/build.sh --test
# or just the host test:
tools/speedtree_baker/tests/run.sh
```

`tests/format_test.cpp` serializes a synthetic tree and parses it back field by
field, including null-array defaults. It needs neither Wine nor the DLL.

## Troubleshooting

| Symptom | Fix |
| --- | --- |
| `dumper not found` | run `tools/speedtree_baker/build.sh` |
| `cannot load DLL` | check `--dll`; the path is passed to Wine as `Z:\...` |
| `kernel32` import library missing | install Wine i386 support or set `WINE_LIBS` |
| `missing SpeedTreeRT exports` | wrong DLL (the client ships 1.6) |
| cache keeps re-baking | the dumper was rebuilt; expected, or keep an older binary |
| paths with spaces fail | the dumper supports quoted paths; `bake.py` writes one path per line |
