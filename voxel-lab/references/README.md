# Downloaded game development references

Downloaded on 2026-10-20 as shallow Git checkouts. Exact source URLs and commit hashes are in [manifest.json](manifest.json). All eight working trees were verified clean after download. Original licenses and Git history for the downloaded tip are retained.

These are source references, not installed dependencies or a tested engine build. No downloaded programs, build scripts or samples were executed. Git submodules were not fetched; their pinned revisions are recorded in the manifest. Additional dependencies may be needed to build examples. Do not recursively add this whole directory to a game build or asset importer.

## Start here

| Project | Why it is useful | Local starting points |
|---|---|---|
| [Zylann Voxel Tools](https://github.com/Zylann/godot_voxel) | Main C++ reference for smooth/block terrain, Transvoxel LOD, streaming and persistence | `godot_voxel/meshers/transvoxel/transvoxel.cpp`, `godot_voxel/meshers/transvoxel/voxel_mesher_transvoxel.cpp`, `godot_voxel/terrain/variable_lod/` |
| [Zylann voxelgame](https://github.com/Zylann/voxelgame) | Example integration, editing, block gameplay, materials and generators | `voxelgame/project/smooth_terrain/`, `voxelgame/project/blocky_game/`, `voxelgame/project/smooth_materials/`, `voxelgame/project/multipass_generator/` |
| [Official Transvoxel tables](https://github.com/EricLengyel/Transvoxel) | Canonical regular/transition lookup data | `Transvoxel/Transvoxel.cpp` and `Transvoxel/LICENSE` |
| [Binary Greedy Meshing](https://github.com/cgerikj/binary-greedy-meshing) | Efficient cube-face meshing for placed blocks | `binary-greedy-meshing/src/mesher.h`, `binary-greedy-meshing/src/main.cpp` |
| [FastNoiseLite](https://github.com/Auburn/FastNoiseLite) | Terrain/biome noise; also includes other language implementations for experiments | `FastNoiseLite/Cpp/FastNoiseLite.h` |
| [Jolt Physics](https://github.com/jrouwe/JoltPhysics) | Collision, rigid bodies and character movement examples | `JoltPhysics/HelloWorld/`, `JoltPhysics/Samples/Tests/Character/CharacterVirtualTest.cpp` |
| [SQLiteCpp](https://github.com/SRombauts/SQLiteCpp) | C++ SQLite RAII, statements and transaction examples; bundled SQLite source | `SQLiteCpp/examples/example1/main.cpp`, `SQLiteCpp/include/SQLiteCpp/`, `SQLiteCpp/sqlite3/` |
| [fastgltf](https://github.com/spnda/fastgltf) | glTF/GLB model parsing for existing assets | `fastgltf/include/fastgltf/`, `fastgltf/examples/gl_viewer/` |

## Particularly useful Voxel Tools files

- `godot_voxel/streams/sqlite/voxel_stream_sqlite.cpp`: asynchronous terrain persistence integration.
- `godot_voxel/doc/source/specs/sqlite_format_v1.md`: upstream world storage format. It is different from `Worlds/Wayfinder.sqlite`; use a deliberate adapter/importer.
- `godot_voxel/tests/voxel/test_transvoxel.cpp`: meshing test reference.
- `godot_voxel/tests/voxel/test_stream_sqlite.cpp`: persistence test reference.
- `godot_voxel/meshers/transvoxel/transvoxel_tables.cpp`: lookup tables as integrated into the engine.

Read Voxel Tools first for an end-to-end implementation, then the official tables for algorithm details. Its Godot integration must be adapted if we use a standalone C++ renderer. The official table repository alone is not a complete mesher.

## Licensing and integration notes

The eight projects provide MIT license text for their main code. Keep original notices when reusing substantial portions. Bundled dependencies, example artwork and other third-party files can have their own terms; the root code license does not replace those. In particular, Jolt includes separate asset licenses.

No project is automatically promoted into the runtime dependency set. Before integration, choose an exact revision, review its required dependencies and build configuration, and copy/link only the components needed. Keep our modifications outside these reference checkouts where possible, so upstream comparisons remain straightforward.

Known uninitialized submodules: Binary Greedy Meshing's FastNoise/GLFW/GLM, and SQLiteCpp's googletest. These do not prevent reading the source. The `manifest.json` submodule entries beginning with `-` identify uninitialized submodules.

The native game implementation additionally downloaded `raylib/` (zlib license, with separately licensed bundled dependencies) and `json/` (MIT). Their exact commits are recorded in the same manifest. The game currently compiles the Transvoxel tables, FastNoiseLite, bundled SQLite amalgamation, JSON header and raylib; the other repositories remain references.

The manifest is a download-time snapshot, not an automatic updater. It records exact revisions for reproducibility; it does not claim that the independently downloaded projects form a compatible build set.
