# Voxel Lab: reusable technology and asset research

Researched 2026-10-20. This is a research and architecture proposal, not an implemented engine. Links point to upstream projects; pin exact revisions and retain their notices when integrating them.

## Recommendation

Build a portable C++20 world library with SQLite persistence and two terrain representations: a scalar density field for Transvoxel terrain, and discrete block IDs for placed construction. Keep rendering, editor UI, scripting, and platform code outside that library.

For a standalone experimental client, evaluate SDL3 + bgfx + Dear ImGui. For faster editor/gameplay iteration, evaluate Godot 4 + Voxel Tools first. The same engine-independent world library can support either, but adapting Voxel Tools internals into a standalone library is significant work. Start with one frontend, not both.

My preference for your stated flexibility is the standalone C++ core, with Godot/Voxel Tools as a reference implementation and an optional prototype frontend. If getting playable terrain quickly becomes the priority, Godot/Voxel Tools is the shorter route.

## What is actually in this workspace

- `assets/` contains textures, models, audio, Godot resources, and several Java Minecraft mod repositories.
- `build/bin/voxel-server.exe` exists. No first-party C++ source, root game project, or CMake configuration was found in this inspection. Asset README references to absent tools do not establish that those tools are available.
- `Worlds/Wayfinder.sqlite` exists alongside WAL and SHM files. Its schema was inspected through a read-only connection; no migration or gameplay changes were made.
- `PRAGMA user_version` is `1`. Tables:

```sql
CREATE TABLE blocks(x INTEGER,y INTEGER,z INTEGER,id TEXT NOT NULL,
                    PRIMARY KEY(x,y,z));
CREATE TABLE density(x INTEGER,y INTEGER,z INTEGER,value REAL NOT NULL,
                     PRIMARY KEY(x,y,z));
CREATE TABLE metadata(key TEXT PRIMARY KEY,value TEXT NOT NULL);
CREATE TABLE players(identity TEXT PRIMARY KEY,profile TEXT NOT NULL);
```

This already separates blocks and density, which fits a hybrid design. The schema alone does not establish density sign, units, generator version, coordinate interpretation, or whether rows represent complete terrain or sparse edits. Resolve those before implementing a compatible loader. SQLite files from Voxel Tools are not automatically compatible with this schema.

## Transvoxel and meshing sources

| Source | What we can reuse | Fit and limitations |
|---|---|---|
| [Eric Lengyel: Transvoxel](https://transvoxel.org/) | Algorithm explanation, dissertation links, transition-cell design | Primary reference for smooth terrain LOD seams. Not a complete world engine. |
| [Official Transvoxel tables](https://github.com/EricLengyel/Transvoxel) | C++ regular/transition lookup tables, MIT license | The supplied `Transvoxel.cpp` is lookup data, not a complete mesher. Preserve its license. |
| [Zylann Voxel Tools](https://github.com/Zylann/godot_voxel) | C++ smooth/block terrain implementation, streaming, editing, collision integration | MIT project, integrated with Godot. Excellent working reference. Match Godot and Voxel Tools versions when testing module/extension builds. |
| [Voxel Tools smooth terrain guide](https://voxel-tools.readthedocs.io/en/latest/smooth_terrain/) | Density representation, texturing and LOD behavior | Useful implementation guidance; distinguish SDF convention from the existing world's unknown convention. |
| [Binary Greedy Meshing](https://github.com/cgerikj/binary-greedy-meshing) | MIT C++ meshing for block terrain | Separate from Transvoxel. Uses packed occupancy data; documented size limit includes neighboring samples. Adapt chunk dimensions and vertex format. Published timings are not our benchmarks. |
| [meshoptimizer](https://github.com/zeux/meshoptimizer) | Mesh cache/fetch optimization, compression, simplification | Optional later optimization. Simplifying chunks independently does not replace Transvoxel seam handling. |

Transvoxel connects meshes sampled at different resolutions; it does not provide procedural generation, materials, lighting, liquids, persistence, physics, or a block-building system. Start with adjacent levels differing by a factor of two. Share density values on borders, define one sign convention, handle all six transition faces, and invalidate dependent neighbor/LOD meshes after edits. Density fields need not be mathematically exact signed distance functions for isosurface extraction.

For Minecraft-inspired play, use smooth hills/caves plus grid-aligned buildings. Decide how a placed block interacts with terrain: allow overlap initially, then add an explicit terrain cut or placement rule. Transvoxel does not automatically join cube structures to smooth terrain. Use triplanar terrain texturing and conventional face UVs for blocks; keep material IDs separate from density.

## Framework choices

| Route | Advantages | Cost |
|---|---|---|
| C++20 + SDL3 + bgfx + ImGui | Direct control, replaceable systems, portable graphics backends, good experimentation tools | We must build scene management, animation, asset pipeline, editor tools, and terrain integration. |
| Godot 4 + C++ + Voxel Tools | Existing scene editor, asset imports, gameplay scripting, integrated voxel systems | Engine integration and matching builds; engine-independent core needs a deliberate boundary. |

Sources: [SDL3](https://github.com/libsdl-org/SDL), [bgfx](https://github.com/bkaradzic/bgfx), [Dear ImGui](https://github.com/ocornut/imgui), [Voxel Tools](https://github.com/Zylann/godot_voxel).

SDL handles platform/window/input services; bgfx handles rendering. Their integration still needs native window setup and shader compilation. ImGui is particularly useful for development panels: density brushes, chunk/LOD overlays, generator controls, material selection, and save queue metrics.

Target Windows, Linux and macOS desktop first. Mobile, web and consoles have additional renderer, input, deployment, and dependency constraints; desktop portability does not guarantee those targets.

## Reusable C++ building blocks

| Component | Candidate and upstream source | Proposed use |
|---|---|---|
| Noise | [FastNoiseLite](https://github.com/Auburn/FastNoiseLite) | Small portable noise library for hills, caves, biome fields. MIT. |
| Physics | [Jolt Physics](https://github.com/jrouwe/JoltPhysics) | Character movement and rigid bodies; rebuild collision only for affected chunks. MIT. Avoid collision builds blocking the main thread. |
| Entities | [EnTT](https://github.com/skypjack/entt) | Optional ECS for creatures, items, machines; avoid creating an entity per terrain voxel. MIT. |
| Model loading | [fastgltf](https://github.com/spnda/fastgltf) | glTF/GLB parsing in a custom client. MIT. Rendering, animation evaluation and skinning remain our responsibility. |
| Compression | [Zstandard](https://github.com/facebook/zstd) | Chunk payload compression; select and retain the applicable upstream license. |
| Gameplay scripting | [Lua through sol2](https://github.com/ThePhD/sol2) | Optional recipes, item behavior, tool commands, fast gameplay iteration. |
| Python integration | [pybind11](https://github.com/pybind/pybind11) | Optional offline generation, world analysis, importer experiments. Keep Python out of the required shipping runtime initially. |
| Dependency management | [vcpkg](https://github.com/microsoft/vcpkg) | Manifest dependencies and pinned baseline for Windows/Linux/macOS builds. |

Avoid adopting everything at once. First integrate meshing, SQLite, noise and one frontend. Add physics, scripting and ECS when a playable feature needs them. The table is a shortlist, not a tested compatible dependency set.

## SQLite storage and compatibility

Keep SQLite as the world format. Put an asynchronous persistence interface between gameplay and SQL, with a dedicated writer and batched transactions. WAL allows readers and a writer to overlap, but SQLite still permits only one writer at a time. Keep databases on local storage. Sources: [SQLite WAL](https://www.sqlite.org/wal.html), [SQLite backup API](https://www.sqlite.org/backup.html).

For the existing version-1 world, first implement a read-only adapter and characterize its contents. Per-voxel rows can be suitable for sparse modifications; they become expensive if we store every generated sample. Benchmark before choosing a migration.

For a future format, consider compressed chunk blobs with separate density/material/block channels, a stable block registry, generator ID/version/seed, payload encoding version, and a revision counter. Store authoritative edits at the base resolution and regenerate coarse LODs and meshes. A generated base plus sparse edits saves space only when the exact generator behavior remains reproducible; preserve generator versions or bake data when retiring them.

Candidate schema for a NEW database or explicit migration only:

```sql
CREATE TABLE chunks (
    x INTEGER NOT NULL,
    y INTEGER NOT NULL,
    z INTEGER NOT NULL,
    channel INTEGER NOT NULL,
    encoding_version INTEGER NOT NULL,
    revision INTEGER NOT NULL,
    payload BLOB NOT NULL,
    PRIMARY KEY (x, y, z, channel)
) WITHOUT ROWID;
```

Chunk encoding must specify dimensions, sample ordering, halo ownership, density sign/scale, numeric types, byte order, compression codec and uncompressed size. Do not dump C++ structs to disk: padding and ABI vary. Bound decompression size and validate payloads on load. SQLite handles its own portable file format; custom blobs need our own portable specification. See [SQLite file format](https://www.sqlite.org/fileformat.html).

When transferring an open WAL world, copying just `Wayfinder.sqlite` can omit committed data. Use SQLite's backup API for a consistent export, or shut down all connections cleanly and verify checkpoint completion before copying. Do not delete WAL/SHM files to make a world look portable.

[VoxelStreamSQLite](https://voxel-tools.readthedocs.io/en/latest/api/VoxelStreamSQLite/) is a ready-made option when using Voxel Tools, but its coordinate formats and serialized voxel payloads differ from our current world. Use an importer/adapter rather than pointing it directly at the existing file.

## Assets already available

The following status is based on local license files, not directory names. It is a partial audit; unreviewed packs remain unverified.

| Local path under `assets/` | Useful content | Evidence/status |
|---|---|---|
| `kenney_survival-kit/` | Props and survival environment models | Local `License.txt` states CC0; suitable first integration candidate. |
| `kenney_impact-sounds/` | Impact audio for tools and interactions | Local `License.txt` states CC0; suitable first integration candidate. |
| `KayKit_Adventurers_2.0_FREE/` | Characters, animations, accessories | Local `License.txt` states CC0; suitable first integration candidate. Verify animation/rig import. |
| `kenney_particle-pack/`, `kenney_blaster-kit_2.1/` | Effects and equipment | License files present; read exact files before promotion into runtime content. |
| `KayKit_Dungeon_Pack_1.1_FREE/`, `KayKit_Skeletons_1.1_FREE/` | Environment and enemies | License files present; read exact files before integration. |
| `block_textures/` | Existing atlas, mapping JSON, previews | Its README traces artwork to `minecraft-inspired-textures-free/`. That source README says it contains Minecraft textures; no reuse license was found in the inspected pack. Treat atlas and derivatives as unverified. |
| `block_breaking/` | Crack sprites and atlas | README documents generated provenance. Useful prototype candidate; record generation/export provenance in the asset manifest. |
| `Create-mc1.21.1-dev/.../` | Java gameplay concepts/source | Local license separates MIT code from all-rights-reserved artwork. Java code requires a port/design adaptation for C++; do not import its artwork under the code license. |
| `Naturalist-1.21.1-NeoForge/.../` | Java creature behavior/source | Local license separates MIT code from all-rights-reserved resources. Inspect paths carefully; artwork is not cleared by the code license. |
| Alex's Mobs, Thaumcraft, IC2Classic, BuildCraft and other packs | Possible references | No blanket approval from this research. Review each code/art license separately before copying. |

Use a small curated runtime asset directory rather than recursively importing the entire collection. Java repositories, source archives, duplicate imports and unused textures should not become shipping content automatically.

## Additional asset sources

- [Kenney Voxel Pack](https://kenney.nl/assets/voxel-pack): CC0 2D tiles/items, useful for icons and a texture exploration pass. This is a 2D pack, not a complete 3D world kit; check tile design before using it on block faces.
- [KayKit Adventurers](https://kaylousberg.itch.io/kaykit-adventurers): official source for the existing character pack and import references.
- [Poly Haven](https://polyhaven.com/license): CC0 models, materials and HDRIs. Good for sky lighting and smooth terrain material experiments; realistic art may need processing to match the intended style.
- [Kenney asset catalog](https://kenney.nl/assets): additional UI, audio and model packs. Retain the license shipped with each downloaded pack.

Normalize models to glTF/GLB when practical; use PNG for textures and retain lossless audio source masters. Record units, up axis, texture color space, filtering, animation clips and collision settings. Preserve raw sources separately from converted runtime assets. Use texture arrays or padded mip-safe atlases for blocks; a previous README's no-mipmap choice should not determine a large-distance terrain renderer.

Suggested manifest entry fields: stable asset ID, relative source path, author, upstream URL, license ID, license file, source hash, conversion settings, and verification status. Keep restricted/unverified assets out of the default runtime set.

## Original implementation sketches

These small sketches were written for this project. They are design examples, not copied upstream meshing code or a compiled implementation.

Use a revision to discard stale asynchronous mesh results:

```cpp
struct ChunkKey { std::int64_t x, y, z; };
struct MeshTicket {
    ChunkKey key;
    std::uint64_t revision;
    std::uint8_t lod;
};
// Capture an immutable chunk + neighbor snapshot and its revision.
// Worker generates a mesh. Main thread uploads it only if the current
// chunk revision and LOD still match this ticket.
```

For chunk addressing, floor division matters at negative world coordinates:

```cpp
// Precondition: chunk_size > 0.
constexpr std::int64_t chunk_coord(std::int64_t voxel,
                                   std::int64_t chunk_size) {
    const auto q = voxel / chunk_size;
    const auto r = voxel % chunk_size;
    return q - (r < 0 ? 1 : 0);
}
// voxel=-1, chunk_size=32 -> chunk=-1, not 0.
```

Keep world access separate from the renderer:

```cpp
struct WorldSample {
    float density;             // Proposal: negative means inside terrain.
    std::uint32_t material_id;
    std::uint32_t block_id;     // 0 reserved for no placed block.
};
// The existing density sign and block registry must be mapped explicitly.
// A dense mesher requests chunk snapshots, not one SQL query per sample.
```

For optional Python/Lua/other language bindings, expose opaque handles, fixed-width values and explicit buffer ownership through a versioned C ABI. Avoid exposing STL containers or C++ exceptions across the ABI. Keep high-frequency per-sample generation and meshing in C++; expose bulk chunk operations and commands to scripts. Godot bindings can sit above the same core. Arbitrary native plugins are platform-specific binaries and need rebuilding per target.

## Portable project layout and first experiment

```text
core/                 C++ world data, generation, edits, scheduling
meshing/              regular cells, transitions, block faces
storage/              SQLite adapters, codecs, migrations
frontends/            one initial native or Godot frontend
bindings/             optional C ABI, Lua, Python, Godot adapters
tools/                asset conversion and world inspection
content/              curated definitions and verified runtime assets
docs/                 formats, research, licenses
tests/                seam, persistence and coordinate fixtures
```

Use CMake presets and pinned dependency versions for a custom C++ build. Godot engine/module builds use their own build workflow; do not force the whole engine through our CMake project. Support MSVC on Windows and Clang/GCC on other desktop platforms. Add Linux/macOS CI early. Use filesystem paths rather than embedded drive letters, explicit UTF-8 boundaries, fixed-width serialization, CPU fallbacks instead of mandatory AVX2, and relative asset IDs. Keep saves in the platform's user data directory. Permit a separate headless server build without the renderer.

First proof of concept:

1. Load a backed-up copy of the version-1 SQLite world through a documented compatibility adapter.
2. Render a known density shape with two neighboring LODs and verify all six transition orientations.
3. Add one digging brush, neighbor invalidation and asynchronous remeshing/collision.
4. Place a block using a verified texture/model and define its overlap behavior with terrain.
5. Save/reload edits and export a consistent SQLite snapshot.
6. Build the same core on Windows and Linux, then validate macOS rendering.

Measure frame time, meshing latency, queue depth, collision rebuild time and storage throughput. Before expanding the world, exercise negative coordinates, chunk corners, fast LOD changes, edits across boundaries, save interruption and database round trips. No performance or cross-platform build claims have been validated in this research pass.
